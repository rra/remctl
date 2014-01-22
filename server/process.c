/*
 * Running a child process.
 *
 * This file contains the code to run a subprocess and manage its output and
 * exit status.  It uses libevent heavily to manage the various interactions
 * with the child process.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013,
 *     2014 The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/event.h>
#include <portable/system.h>

#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <server/internal.h>
#include <util/fdflag.h>
#include <util/macros.h>
#include <util/messages.h>
#include <util/protocol.h>


/*
 * Callback when all stdin data has been sent.  We only have a callback to
 * shut down our end of the socketpair so that the process gets EOF on its
 * next read.
 */
static void
handle_input_end(struct bufferevent *bev, void *data)
{
    struct process *process = data;

    bufferevent_disable(bev, EV_WRITE);
    if (shutdown(process->stdinout_fd, SHUT_WR) < 0)
        sysdie("cannot shut down input side of process socket pair");
}


/*
 * Callback used to handle output from a process (protocol version two or
 * later).  We use the same handler for both standard output and standard
 * error and check the bufferevent to determine which stream we're seeing.
 *
 * When called, note that we saw some output, which is a flag to continue
 * processing when running the event loop after the child has exited.
 */
static void
handle_output(struct bufferevent *bev, void *data)
{
    int stream;
    struct evbuffer *buf;
    struct process *process = data;

    process->saw_output = true;
    stream = (bev == process->inout) ? 1 : 2;
    buf = bufferevent_get_input(bev);
    if (!server_v2_send_output(process->client, stream, buf))
        event_base_loopbreak(process->loop);
}


/*
 * Discard all data in the evbuffer.  This handler is used with protocol
 * version one when we've already read as much data as we can return to the
 * remctl client.
 */
static void
handle_output_discard(struct bufferevent *bev, void *data UNUSED)
{
    size_t length;
    struct evbuffer *buf;

    buf = bufferevent_get_input(bev);
    length = evbuffer_get_length(buf);
    if (evbuffer_drain(buf, length) < 0)
        sysdie("internal error: cannot discard extra output");
}


/*
 * Callback used to handle filling the output buffer with protocol version
 * one.  When this happens, we pull all of the data out into a separate
 * evbuffer and then change our read callback to handle_output_discard, which
 * just drains (discards) all subsequent data from the process.
 */
static void
handle_output_full(struct bufferevent *bev, void *data)
{
    struct process *process = data;
    bufferevent_data_cb writecb;

    process->output = evbuffer_new();
    if (process->output == NULL)
        die("internal error: cannot create discard evbuffer");
    if (bufferevent_read_buffer(bev, process->output) < 0)
        die("internal error: cannot read data from output buffer");

    /*
     * Change the output callback.  We need to be sure not to dump our input
     * callback if it exists.
     */
    writecb = (process->input == NULL) ? NULL : handle_input_end;
    bufferevent_setcb(bev, handle_output_discard, writecb, NULL, data);
}


/*
 * Callback for events in input or output handling.  This means either an
 * error or EOF.  On EOF or an EPIPE or ECONNRESET error, just deactivate the
 * bufferevent.  On other errors, send an error message to the client and then
 * break out of the event loop.
 */
static void
handle_io_event(struct bufferevent *bev, short events, void *data)
{
    struct process *process = data;
    struct client *client = process->client;

    /* Check for EOF, after which we should stop trying to listen. */
    if (events & BEV_EVENT_EOF) {
        bufferevent_disable(bev, EV_READ);
        return;
    }

    /*
     * If we get ECONNRESET or EPIPE, the client went away without bothering
     * to read our data.  This is the same as EOF except that we should also
     * stop trying to write data.
     */
    if (events & BEV_EVENT_ERROR)
        if (socket_errno == ECONNRESET || socket_errno == EPIPE) {
            bufferevent_disable(bev, EV_READ | EV_WRITE);
            return;
        }

    /* Everything else is some sort of error. */
    if (events & BEV_EVENT_READING)
        syswarn("read from process failed");
    else
        syswarn("write to standard input failed");
    server_send_error(client, ERROR_INTERNAL, "Internal failure");
    event_base_loopbreak(process->loop);
}


/*
 * Called when the process has exited.  Here we reap the status and then tell
 * the event loop to complete.  Ignore SIGCHLD if our child process wasn't the
 * one that exited.
 */
static void
handle_exit(evutil_socket_t sig UNUSED, short what UNUSED, void *data)
{
    struct process *process = data;

    if (waitpid(process->pid, &process->status, WNOHANG) > 0) {
        process->reaped = true;
        event_del(process->sigchld);
        event_base_loopexit(process->loop, NULL);
    }
}


/*
 * Called on fatal errors in the child process before exec.  This callback
 * exists only to change the exit status for fatal internal errors in the
 * child process before exec to -1 instead of the default of 1.
 */
static int
child_die_handler(void)
{
    return -1;
}


/*
 * Start the child process.  This runs as a one-time event inside the event
 * loop, forks off the child process, and sets up the events that process
 * output from the child and send it back to the remctl client.
 */
static void
start(evutil_socket_t junk UNUSED, short what UNUSED, void *data)
{
    struct process *process = data;
    struct client *client = process->client;
    struct event_base *loop = process->loop;
    bufferevent_data_cb writecb = NULL;
    socket_type stdinout_fds[2] = { INVALID_SOCKET, INVALID_SOCKET };
    socket_type stderr_fds[2]   = { INVALID_SOCKET, INVALID_SOCKET };
    socket_type fd;

    /*
     * Socket pairs are used for communication with the child process that
     * actually runs the command.  We have to use sockets rather than pipes
     * because libevent's buffevents require sockets.
     *
     * For protocol version one, we can use one socket pair for eerything,
     * since we don't distinguish between streams.  For protocol version two,
     * we use one socket pair for standard intput and standard output, and a
     * separate read-only one for standard error so that we can keep the
     * stream separate.
     */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, stdinout_fds) < 0) {
        syswarn("cannot create stdin and stdout socket pair");
        goto fail;
    }
    if (client->protocol > 1)
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, stderr_fds) < 0) {
            syswarn("cannot create stderr socket pair");
            goto fail;
        }

    /*
     * Flush output before forking, mostly in case -S was given and we've
     * therefore been writing log messages to standard output that may not
     * have been flushed yet.
     */
    fflush(stdout);
    process->pid = fork();
    switch (process->pid) {
    case -1:
        syswarn("cannot fork");
        goto fail;

    /* In the child. */
    case 0:
        message_fatal_cleanup = child_die_handler;

        /* Close the server sides of the sockets. */
        close(stdinout_fds[0]);
        stdinout_fds[0] = INVALID_SOCKET;
        if (stderr_fds[0] != INVALID_SOCKET) {
            close(stderr_fds[0]);
            stderr_fds[0] = INVALID_SOCKET;
        }

        /*
         * Set up stdin if we have input data.  If we don't have input data,
         * reopen on /dev/null instead so that the process gets immediate EOF.
         * Ignore failure here, since it probably won't matter and worst case
         * is that we leave stdin closed.
         */
        if (process->input != NULL)
            dup2(stdinout_fds[1], 0);
        else {
            close(0);
            fd = open("/dev/null", O_RDONLY);
            if (fd > 0) {
                dup2(fd, 0);
                close(fd);
            }
        }

        /* Set up stdout and stderr. */
        dup2(stdinout_fds[1], 1);
        if (client->protocol == 1)
            dup2(stdinout_fds[1], 2);
        else {
            dup2(stderr_fds[1], 2);
            close(stderr_fds[1]);
        }
        close(stdinout_fds[1]);

        /*
         * Older versions of MIT Kerberos left the replay cache file open
         * across exec.  Newer versions correctly set it close-on-exec, but
         * close our low-numbered file descriptors anyway for older versions.
         * We're just trying to get the replay cache, so we don't have to go
         * very high.
         */
        for (fd = 3; fd < 16; fd++)
            close(fd);

        /*
         * Put the authenticated principal and other connection and command
         * information in the environment.  REMUSER is for backwards
         * compatibility with earlier versions of remctl.
         */
        if (setenv("REMUSER", client->user, 1) < 0)
            sysdie("cannot set REMUSER in environment");
        if (setenv("REMOTE_USER", client->user, 1) < 0)
            sysdie("cannot set REMOTE_USER in environment");
        if (setenv("REMOTE_ADDR", client->ipaddress, 1) < 0)
            sysdie("cannot set REMOTE_ADDR in environment");
        if (client->hostname != NULL)
            if (setenv("REMOTE_HOST", client->hostname, 1) < 0)
                sysdie("cannot set REMOTE_HOST in environment");
        if (setenv("REMCTL_COMMAND", process->command, 1) < 0)
            sysdie("cannot set REMCTL_COMMAND in environment");

        /* Drop privileges if requested. */
        if (process->config->user != NULL && process->config->uid > 0) {
            if (initgroups(process->config->user, process->config->gid) != 0)
                sysdie("cannot initgroups for %s\n", process->config->user);
            if (setgid(process->config->gid) != 0)
                sysdie("cannot setgid to %d\n", process->config->gid);
            if (setuid(process->config->uid) != 0)
                sysdie("cannot setuid to %d\n", process->config->uid);
        }

        /*
         * Run the command.  On error, we intentionally don't reveal
         * information about the command we ran.
         */
        if (execv(process->config->program, process->argv) < 0)
            sysdie("cannot execute command");

    /* In the parent.  Close the other sides of the socket pairs. */
    default:
        close(stdinout_fds[1]);
        stdinout_fds[1] = INVALID_SOCKET;
        process->stdinout_fd = stdinout_fds[0];
        if (client->protocol > 1) {
            close(stderr_fds[1]);
            stderr_fds[1] = INVALID_SOCKET;
            process->stderr_fd = stderr_fds[0];
        }
    }

    /*
     * Set up a bufferevent to consume output from the process.
     *
     * There are two possibilities here.  For protocol version two, we use two
     * bufferevents, one for standard input and output and one for standard
     * error, that turn each chunk of data into a MESSAGE_OUTPUT token to the
     * client.  For protocol version one, we use a single bufferevent, which
     * sends standard intput and collects both standard output and standard
     * error, queuing it to send on process exit.  In this case, stdinout_fd
     * gets both streams, since there's no point in distinguishing, and we
     * only need one bufferevent.
     */
    fdflag_nonblocking(stdinout_fds[0], true);
    process->inout = bufferevent_socket_new(loop, process->stdinout_fd, 0);
    if (process->inout == NULL)
        die("internal error: cannot create stdin/stdout bufferevent");
    if (process->input == NULL)
        bufferevent_enable(process->inout, EV_READ);
    else {
        writecb = handle_input_end;
        bufferevent_enable(process->inout, EV_READ | EV_WRITE);
        if (bufferevent_write_buffer(process->inout, process->input) < 0)
            die("internal error: cannot queue input for process");
    }
    if (client->protocol == 1) {
        bufferevent_setcb(process->inout, handle_output_full, writecb,
                          handle_io_event, process);
        bufferevent_setwatermark(process->inout, EV_READ, TOKEN_MAX_OUTPUT_V1,
                                 TOKEN_MAX_OUTPUT_V1);
    } else {
        bufferevent_setcb(process->inout, handle_output, writecb,
                          handle_io_event, process);
        bufferevent_setwatermark(process->inout, EV_READ, 0, TOKEN_MAX_OUTPUT);
        fdflag_nonblocking(stderr_fds[0], true);
        process->err = bufferevent_socket_new(loop, process->stderr_fd, 0);
        if (process->err == NULL)
            die("internal error: cannot create stderr bufferevent");
        bufferevent_enable(process->err, EV_READ);
        bufferevent_setcb(process->err, handle_output, NULL,
                          handle_io_event, process);
        bufferevent_setwatermark(process->err, EV_READ, 0, TOKEN_MAX_OUTPUT);
    }
    return;

fail:
    if (stdinout_fds[0] != INVALID_SOCKET)
        close(stdinout_fds[0]);
    if (stdinout_fds[1] != INVALID_SOCKET)
        close(stdinout_fds[1]);
    if (stderr_fds[0] != INVALID_SOCKET)
        close(stderr_fds[0]);
    if (stderr_fds[1] != INVALID_SOCKET)
        close(stderr_fds[1]);
    server_send_error(client, ERROR_INTERNAL, "Internal failure");
    event_base_loopbreak(process->loop);
}


/*
 * Runs a process as a child to completion, capturing its output and
 * processing it according to the negotiated remctl client protocol.
 *
 * Takes the client, the short name for the command, an argument list, the
 * configuration line for that command, and the process.  Returns true on
 * success and false on failure.
 */
bool
server_process_run(struct process *process)
{
    bool success;
    struct event_base *loop;
    struct client *client = process->client;
    const struct timeval immediate = { 0, 0 };

    /* Create the event base that we use for the event loop. */
    loop = event_base_new();
    process->loop = loop;

    /*
     * Create the event to handle SIGCHLD when the child process exits.  We
     * have to register this event first and then make sure that we create the
     * child process inside the event loop, since otherwise we race the child
     * process in setting up the event loop and may miss SIGCHLD and not
     * realize the child has already exited.
     */
    process->sigchld = evsignal_new(loop, SIGCHLD, handle_exit, process);
    if (process->sigchld == NULL)
        die("internal error: cannot create SIGCHLD processing event");
    if (event_add(process->sigchld, NULL) < 0)
        die("internal error: cannot add SIGCHLD processing event");

    /*
     * Prepare to spawn the process itself via a one-time event.  This event
     * will run once, immediately, and create and add further bufferevents to
     * handle the output from the process.  It will then self-destruct.
     */
    if (event_base_once(loop, -1, EV_TIMEOUT, start, process, &immediate) < 0)
        die("internal error: cannot create event to spawn the process");

    /*
     * Run the event loop.  This will continue until handle_exit is called or
     * we encounter some fatal error, in which case we'll break out of the
     * loop.
     */
    if (event_base_dispatch(loop) < 0)
        die("internal error: process event loop failed");

    /*
     * We have some more work to do after client exit since there may still be
     * output from the child sitting in system buffers.  Therefore, we now
     * repeatedly run the event loop in EVLOOP_NONBLOCK mode, only continuing
     * if process->saw_output remains true and we didn't break out of the loop
     * (indicating an error).  The saw_output flag will be set by the event
     * handlers if we see any output from the process.
     */
    process->saw_output = true;
    while (process->saw_output && !event_base_got_break(loop)) {
        process->saw_output = false;
        if (event_base_loop(loop, EVLOOP_NONBLOCK) < 0)
            die("internal error: process event loop failed");
    }

    /* Close down the file descriptors now that we have all the data. */
    close(process->stdinout_fd);
    if (client->protocol > 1)
        close(process->stderr_fd);

    /*
     * If we aborted on error, still wait for the child process to exit.  We
     * don't want to just exit and orphan the process since, if spawned from
     * something like xinetd, the lifetime of the remctld process controls the
     * rate limiting.  We shouldn't deadlock here since client will get broken
     * pipe errors or EOF when trying to talk to the now-closed sockets.
     *
     * An alternative would be to kill the child, but that could cause other
     * problems if the child is doing something that shouldn't be arbitrarily
     * interrupted.  This approach seems safer, although has the disadvantage
     * of keeping the remctld process around until the child completes.
     */
    if (event_base_got_break(loop)) {
        if (!process->reaped)
            waitpid(process->pid, &process->status, 0);
        return false;
    }

    /*
     * For protocol version one, if the process sent more than the max output,
     * we already pulled out the output we care about into process->output.
     * Otherwise, we need to pull the output from the bufferevent before we
     * free it.
     */
    if (client->protocol == 1 && process->output == NULL) {
        process->output = evbuffer_new();
        if (process->output == NULL)
            die("internal error: cannot create output buffer");
        if (bufferevent_read_buffer(process->inout, process->output) < 0)
            die("internal error: cannot read data from output buffer");
    }

    /* Free resources and return. */
    success = !event_base_got_break(loop);
    bufferevent_free(process->inout);
    if (process->err != NULL)
        bufferevent_free(process->err);
    event_free(process->sigchld);
    event_base_free(loop);
    return success;
}
