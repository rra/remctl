/*
 * Running a child process.
 *
 * This file contains the code to run a subprocess and manage its output and
 * exit status.  It uses libevent heavily to manage the various interactions
 * with the child process.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
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
#include <util/xmalloc.h>

/*
 * We would like to use event_base_loopbreak and event_base_got_break, but the
 * latter was introduced in libevent 2.x.  For right now, until we can rely on
 * libevent 2.x, set a flag in the process struct instead.  We still call
 * event_base_loopbreak where we can, to keep from processing more data than
 * we have to.
 *
 * This portability glue is specific to this file and assumes that the process
 * variable is in scope to replace the event_base_got_break functionality.
 */
#ifndef HAVE_EVENT_BASE_LOOPBREAK
# define event_base_loopbreak(base) /* empty */
#endif
#ifndef HAVE_EVENT_BASE_GOT_BREAK
# define event_base_got_break(base) process->saw_error
#endif


/*
 * Callback for events in input or output handling while running a process.
 * This means either an error or EOF.  On EOF or an EPIPE or ECONNRESET error,
 * just deactivate the bufferevent.  On other errors, send an error message to
 * the client and then break out of the event loop.
 *
 * This has to be public so that it can be referenced by the setup code for
 * the various protocols.
 */
void
server_handle_io_event(struct bufferevent *bev, short events, void *data)
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
    client->error(client, ERROR_INTERNAL, "Internal failure");
    process->saw_error = true;
    event_base_loopbreak(process->loop);
}


/*
 * Callback when all stdin data has been sent.  We only have a callback to
 * shut down our end of the socketpair so that the process gets EOF on its
 * next read.  Also has to be public so that it can be referenced in the
 * per-protocol startup callbacks.
 */
void
server_handle_input_end(struct bufferevent *bev, void *data)
{
    struct process *process = data;

    bufferevent_disable(bev, EV_WRITE);
    if (shutdown(process->stdinout_fd, SHUT_WR) < 0)
        sysdie("cannot shut down input side of process socket pair");
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
    socket_type stdinout_fds[2] = { INVALID_SOCKET, INVALID_SOCKET };
    socket_type stderr_fds[2]   = { INVALID_SOCKET, INVALID_SOCKET };
    socket_type fd;
    struct sigaction sa;
    char *expires;

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
         * Restore the default SIGPIPE handler.  The server sets it to
         * SIG_IGN, which is inherited by children.  We want the child to have
         * a default set of signal handlers.
         */
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        if (sigaction(SIGPIPE, &sa, NULL) < 0)
            sysdie("cannot clear SIGPIPE handler");

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
        xasprintf(&expires, "%lu", (unsigned long) client->expires);
        if (setenv("REMOTE_EXPIRES", expires, 1) < 0)
            sysdie("cannot set REMOTE_EXPIRES in environment");
        free(expires);

        /* Drop privileges if requested. */
        if (process->rule->user != NULL && process->rule->uid > 0) {
            if (initgroups(process->rule->user, process->rule->gid) != 0)
                sysdie("cannot initgroups for %s\n", process->rule->user);
            if (setgid(process->rule->gid) != 0)
                sysdie("cannot setgid to %d\n", process->rule->gid);
            if (setuid(process->rule->uid) != 0)
                sysdie("cannot setuid to %d\n", process->rule->uid);
        }

        /*
         * Run the command.  On error, we intentionally don't reveal
         * information about the command we ran.
         */
        if (execv(process->rule->program, process->argv) < 0)
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
     * Set up bufferevents to send input to and consume output from the
     * process.  There are two possibilities here.
     *
     * For protocol version two, we use two bufferevents, one for standard
     * input and output and one for standard error, that turn each chunk of
     * data into a MESSAGE_OUTPUT token to the client.
     *
     * For protocol version one, we use a single bufferevent, which sends
     * standard intput and collects both standard output and standard error,
     * queuing it to send on process exit.  In this case, stdinout_fd gets
     * both streams, since there's no point in distinguishing, and we only
     * need one bufferevent.
     */
    fdflag_nonblocking(stdinout_fds[0], true);
    process->inout = bufferevent_socket_new(loop, process->stdinout_fd, 0);
    if (process->inout == NULL)
        die("internal error: cannot create stdin/stdout bufferevent");
    if (process->input == NULL)
        bufferevent_enable(process->inout, EV_READ);
    else {
        bufferevent_enable(process->inout, EV_READ | EV_WRITE);
        if (bufferevent_write_buffer(process->inout, process->input) < 0)
            die("internal error: cannot queue input for process");
    }
    if (client->protocol > 1) {
        fdflag_nonblocking(stderr_fds[0], true);
        process->err = bufferevent_socket_new(loop, process->stderr_fd, 0);
        if (process->err == NULL)
            die("internal error: cannot create stderr bufferevent");
    }

    /* Set up the event hooks for the different protocols. */
    client->setup(process);
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
    client->error(client, ERROR_INTERNAL, "Internal failure");
    process->saw_error = true;
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
