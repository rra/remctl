/*
 * Running commands.
 *
 * These are the functions for running external commands under remctld and
 * calling the appropriate protocol functions to deal with the output.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013,
 *     2014 The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/event.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>

#include <server/internal.h>
#include <util/buffer.h>
#include <util/fdflag.h>
#include <util/macros.h>
#include <util/messages.h>
#include <util/protocol.h>
#include <util/xmalloc.h>

/*
 * Data structure used to hold details about a running process.  The events we
 * hook into the event loop are also stored here so that the event handlers
 * can use this as their data and have their pointers so that they can remove
 * themselves when needed.
 */
struct process {
    struct event_base *events;  /* Event base for the process event loop. */
    struct event *in;           /* Send data to process standard input. */
    struct event *out;          /* Handle stdout from process. */
    struct event *err;          /* Handle stderr from process. */
    struct event *sigchld;      /* Handle the SIGCHLD signal for exit. */
    struct client *client;      /* Pointer to corresponding remctl client. */
    int fds[2];                 /* Array of file descriptors for output. */
    int stdin_fd;               /* File descriptor for standard input. */
    struct iovec *input;        /* Data to pass on standard input. */
    size_t offset;              /* Current offset into standard input data. */
    pid_t pid;                  /* Process ID of child. */
    int status;                 /* Exit status. */
    bool reaped;                /* Whether we've reaped the process. */
    bool saw_output;            /* Whether we saw process output. */
};


/*
 * Callback used to handle output from a process.  We do not check why we were
 * triggered since only EV_READ is possible.  We have to check the triggered
 * file descriptor against process->fds to figure out if we saw standard
 * output or standard error so that we can use the same callback for both.
 *
 * Note that for protocol version two and later, we immediately send the
 * partial output to the remctl client and block our event loop in the
 * process.  We may eventually hook this into the event loop.
 */
static void
handle_output(evutil_socket_t fd, short what UNUSED, void *data)
{
    int stream;
    char junk[BUFSIZ];
    ssize_t status;
    struct process *process = data;
    struct client *client = process->client;
    struct buffer *output = client->output;
    struct event *event;

    /* Determine our remctl output stream and which event called us. */
    stream = (fd == process->fds[0]) ? 1 : 2;
    event = (stream == 1) ? process->out : process->err;

    /*
     * Read the data.  If we're using protocol one, and hence accumulating
     * everyting into a single buffer, and we've run out of space, just throw
     * everything away.  Otherwise, read it into our client buffer.
     */
    if (client->protocol == 1 && output->left >= output->size)
        status = read(fd, junk, sizeof(junk));
    else
        status = buffer_read(output, fd);
    if (status < 0 && (errno != EINTR && errno != EAGAIN)) {
        syswarn("read failed");
        server_send_error(client, ERROR_INTERNAL, "Internal failure");
        event_base_loopbreak(process->events);
        return;
    }

    /* For protocol two and later, immediately send the output. */
    if (status > 0) {
        process->saw_output = true;
        if (client->protocol != 1) {
            if (!server_v2_send_output(client, stream))
                event_base_loopbreak(process->events);
            buffer_set(client->output, "", 0);
        }
    }

    /* If we saw EOF, remove this event from the event loop. */
    if (status == 0)
        event_del(event);
}


/*
 * Callback to send data to the process standard input.  We just keep sending
 * as much as possible until we've gotten through all of the input data, and
 * then remove ourselves from the event loop.
 */
static void
send_input(evutil_socket_t fd, short what UNUSED, void *data)
{
    ssize_t status;
    struct process *process = data;
    struct client *client = process->client;

    /* Write as much data as possible. */
    status = write(fd, (char *) process->input->iov_base + process->offset,
                   process->input->iov_len - process->offset);

    /*
     * On EPIPE, we assume that the process just doesn't care about the rest
     * of the data and treat this as equivalent to having written all of the
     * data.  On any other error other than EINTR or EAGAIN, send an error to
     * the remctl client and abort the event loop.
     */
    if (status < 0) {
        if (errno == EPIPE) {
            process->offset = process->input->iov_len;
            status = 0;
        } else if (errno != EINTR && errno != EAGAIN) {
            syswarn("write failed");
            server_send_error(client, ERROR_INTERNAL, "Internal failure");
            event_base_loopbreak(process->events);
            return;
        }
    }

    /* Update bookkeeping and see if we're done. */
    process->offset += status;
    if (process->offset >= process->input->iov_len) {
        close(process->stdin_fd);
        process->stdin_fd = -1;
        event_del(process->in);
    }
}


/*
 * Called when the process has exited.  Here we reap the status and then tell
 * the event loop to complete.  Ignore SIGCHLD if our child process wasn't the
 * one that exited.
 */
static void
handle_child_exit(evutil_socket_t sig UNUSED, short what UNUSED, void *data)
{
    struct process *process = data;

    if (waitpid(process->pid, &process->status, WNOHANG) > 0) {
        process->reaped = true;
        event_del(process->sigchld);
        event_base_loopexit(process->events, NULL);
    }
}


/*
 * Processes the input to and output from an external program.  Takes the
 * client struct and a struct representing the running process.  Feeds input
 * data to the process on standard input and reads from all the streams as
 * output is available, stopping when they all reach EOF.
 *
 * For protocol v2 and higher, we can send the output immediately as we get
 * it.  For protocol v1, we instead accumulate the output in the buffer stored
 * in our client struct, and will send it out later in conjunction with the
 * exit status.
 *
 * Returns true on success, false on failure.
 */
static int
server_process_output(struct client *client, struct process *process)
{
    struct event_base *events;
    bool success;

    /* Clear the output buffer. */
    buffer_set(client->output, "", 0);

    /* Create the event base that we use for the event loop. */
    events = event_base_new();
    process->events = events;

    /* Create events for consuming the output from the process. */
    process->out = event_new(events, process->fds[0], EV_READ | EV_PERSIST,
                             handle_output, process);
    process->err = event_new(events, process->fds[1], EV_READ | EV_PERSIST,
                             handle_output, process);
    if (event_add(process->out, NULL) < 0)
        die("internal error: cannot add stdout processing event");
    if (event_add(process->err, NULL) < 0)
        die("internal error: cannot add stderr processing event");

    /* If we have input for the process, create an event to send that input. */
    if (process->input != NULL) {
        process->in = event_new(events, process->stdin_fd,
                                EV_WRITE | EV_PERSIST, send_input, process);
        if (event_add(process->in, NULL) < 0)
            die("internal error: cannot add input processing event");
    }

    /* Create the event to handle SIGCHLD when the child process exits. */
    process->sigchld = evsignal_new(events, SIGCHLD, handle_child_exit,
                                    process);
    if (event_add(process->sigchld, NULL) < 0)
        die("internal error: cannot add SIGCHLD processing event");

    /*
     * Run the event loop.  This will continue until handle_child_exit is
     * called, unless we encounter some fatal error.  If handle_child_exit was
     * successfully called, process->reaped will be set to true.
     */
    if (event_base_dispatch(events) < 0)
        die("internal error: process event loop failed");
    if (event_base_got_break(events))
        return false;

    /*
     * We cannot simply decide the child is done as soon as we get an exit
     * status since we may still have buffered output from the child sitting
     * in system buffers.  Therefore, we now repeatedly run the event loop in
     * EVLOOP_NONBLOCK mode, only continuing if process->saw_output remains
     * true, indicating we processed some output from the process.
     */
    do {
        process->saw_output = false;
        if (event_base_loop(events, EVLOOP_NONBLOCK) < 0)
            die("internal error: process event loop failed");
    } while (process->saw_output && !event_base_got_break(events));

    /* Free resources and return. */
    success = !event_base_got_break(events);
    if (process->in != NULL)
        event_free(process->in);
    event_free(process->out);
    event_free(process->err);
    event_free(process->sigchld);
    event_base_free(events);
    return success;
}


/*
 * Given a configuration line, a command, and a subcommand, return true if
 * that command and subcommand match that configuration line and false
 * otherwise.  Handles the ALL and NULL special cases.
 *
 * Empty commands are not yet supported by the rest of the code, but this
 * function copes in case that changes later.
 */
static bool
line_matches(struct confline *cline, const char *command,
             const char *subcommand)
{
    bool okay = false;

    if (strcmp(cline->command, "ALL") == 0)
        okay = true;
    if (command != NULL && strcmp(cline->command, command) == 0)
        okay = true;
    if (command == NULL && strcmp(cline->command, "EMPTY") == 0)
        okay = true;
    if (okay) {
        if (strcmp(cline->subcommand, "ALL") == 0)
            return true;
        if (subcommand != NULL && strcmp(cline->subcommand, subcommand) == 0)
            return true;
        if (subcommand == NULL && strcmp(cline->subcommand, "EMPTY") == 0)
            return true;
    }
    return false;
}


/*
 * Look up the matching configuration line for a command and subcommand.
 * Takes the configuration, a command, and a subcommand to match against
 * Returns the matching config line or NULL if none match.
 */
static struct confline *
find_config_line(struct config *config, char *command, char *subcommand)
{
    size_t i;

    for (i = 0; i < config->count; i++)
        if (line_matches(config->rules[i], command, subcommand))
            return config->rules[i];
    return NULL;
}


/*
 * Runs a given command via exec.  This forks a child process, sets
 * environment and changes ownership if needed, then runs the command and
 * sends the output back to the remctl client.
 *
 * Takes the client, the short name for the command, an argument list, the
 * configuration line for that command, and the process.  Returns true on
 * success and false on failure.
 */
static bool
server_exec(struct client *client, char *command, char **req_argv,
            struct confline *cline, struct process *process)
{
    int stdin_pipe[2] = { -1, -1 };
    int stdout_pipe[2] = { -1, -1 };
    int stderr_pipe[2] = { -1, -1 };
    bool ok = false;
    int fd;

    /*
     * These pipes are used for communication with the child process that
     * actually runs the command.
     */
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        syswarn("cannot create pipes");
        server_send_error(client, ERROR_INTERNAL, "Internal failure");
        goto done;
    }
    if (process->input != NULL && pipe(stdin_pipe) != 0) {
        syswarn("cannot create stdin pipe");
        server_send_error(client, ERROR_INTERNAL, "Internal failure");
        goto done;
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
        server_send_error(client, ERROR_INTERNAL, "Internal failure");
        goto done;

    /* In the child. */
    case 0:
        dup2(stdout_pipe[1], 1);
        close(stdout_pipe[0]);
        stdout_pipe[0] = -1;
        close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
        dup2(stderr_pipe[1], 2);
        close(stderr_pipe[0]);
        stderr_pipe[0] = -1;
        close(stderr_pipe[1]);
        stderr_pipe[1] = -1;

        /*
         * Set up stdin pipe if we have input data.
         *
         * If we don't have input data, child doesn't need stdin at all, but
         * just closing it causes problems for puppet.  Reopen on /dev/null
         * instead.  Ignore failure here, since it probably won't matter and
         * worst case is that we leave stdin closed.
         */
        if (process->input != NULL) {
            dup2(stdin_pipe[0], 0);
            close(stdin_pipe[0]);
            stdin_pipe[0] = -1;
            close(stdin_pipe[1]);
            stdin_pipe[0] = -1;
        } else {
            close(0);
            fd = open("/dev/null", O_RDONLY);
            if (fd > 0) {
                dup2(fd, 0);
                close(fd);
            }
        }

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
        if (setenv("REMUSER", client->user, 1) < 0) {
            syswarn("cannot set REMUSER in environment");
            exit(-1);
        }
        if (setenv("REMOTE_USER", client->user, 1) < 0) {
            syswarn("cannot set REMOTE_USER in environment");
            exit(-1);
        }
        if (setenv("REMOTE_ADDR", client->ipaddress, 1) < 0) {
            syswarn("cannot set REMOTE_ADDR in environment");
            exit(-1);
        }
        if (client->hostname != NULL) {
            if (setenv("REMOTE_HOST", client->hostname, 1) < 0) {
                syswarn("cannot set REMOTE_HOST in environment");
                exit(-1);
            }
        }
        if (setenv("REMCTL_COMMAND", command, 1) < 0) {
            syswarn("cannot set REMCTL_COMMAND in environment");
            exit(-1);
        }

        /* Drop privileges if requested. */
        if (cline->user != NULL && cline->uid > 0) {
            if (initgroups(cline->user, cline->gid) != 0) {
                syswarn("cannot initgroups for %s\n", cline->user);
                exit(-1);
            }
            if (setgid(cline->gid) != 0) {
                syswarn("cannot setgid to %d\n", cline->gid);
                exit(-1);
            }
            if (setuid(cline->uid) != 0) {
                syswarn("cannot setuid to %d\n", cline->uid);
                exit(-1);
            }
        }

        /* Run the command. */
        execv(cline->program, req_argv);

        /*
         * This happens only if the exec fails.  Print out an error message to
         * the stderr pipe and fail; that's the best that we can do.
         */
        fprintf(stderr, "Cannot execute: %s\n", strerror(errno));
        exit(-1);

    /* In the parent. */
    default:
        close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
        close(stderr_pipe[1]);
        stderr_pipe[1] = -1;
        if (process->input != NULL) {
            close(stdin_pipe[0]);
            stdin_pipe[0] = -1;
        }

        /*
         * Unblock the read ends of the output pipes, to enable us to read
         * from both iteratively, and unblock the write end of the input pipe
         * if we have one so that we don't block when feeding data to our
         * child.
         */
        fdflag_nonblocking(stdout_pipe[0], true);
        fdflag_nonblocking(stderr_pipe[0], true);
        if (process->input != NULL)
            fdflag_nonblocking(stdin_pipe[1], true);

        /*
         * This collects output from both pipes iteratively, while the child
         * is executing, and processes it.  It also sends input data if we
         * have any.
         */
        process->fds[0] = stdout_pipe[0];
        process->fds[1] = stderr_pipe[0];
        if (process->input != NULL)
            process->stdin_fd = stdin_pipe[1];
        ok = server_process_output(client, process);
        close(process->fds[0]);
        close(process->fds[1]);
        if (process->input != NULL && process->stdin_fd >= 0)
            close(process->stdin_fd);
        if (!process->reaped)
            waitpid(process->pid, &process->status, 0);
        if (WIFEXITED(process->status))
            process->status = (signed int) WEXITSTATUS(process->status);
        else
            process->status = -1;
    }

 done:
    if (stdout_pipe[0] != -1)
        close(stdout_pipe[0]);
    if (stdout_pipe[1] != -1)
        close(stdout_pipe[1]);
    if (stderr_pipe[0] != -1)
        close(stderr_pipe[0]);
    if (stderr_pipe[1] != -1)
        close(stderr_pipe[1]);
    if (stdin_pipe[0] != -1)
        close(stdin_pipe[0]);
    if (stdin_pipe[1] != -1)
        close(stdin_pipe[1]);

    return ok;
}

/*
 * Find the summary of all commands the user can run against this remctl
 * server.  We do so by checking all configuration lines for any that
 * provide a summary setup that the user can access, then running that
 * line's command with the given summary sub-command.
 *
 * Takes a client object, the user requesting access, and the list of all
 * valid configurations.
 */
static void
server_send_summary(struct client *client, const char *user,
                    struct config *config)
{
    char *path = NULL;
    char *program;
    struct confline *cline = NULL;
    size_t i;
    char **req_argv = NULL;
    bool ok;
    bool ok_any = false;
    int status_all = 0;
    struct process process;

    /*
     * Check each line in the config to find any that are "<command> ALL"
     * lines, the user is authorized to run, and which have a summary field
     * given.
     */
    for (i = 0; i < config->count; i++) {
        memset(&process, 0, sizeof(process));
        process.client = client;
        cline = config->rules[i];
        if (strcmp(cline->subcommand, "ALL") != 0)
            continue;
        if (!server_config_acl_permit(cline, user))
            continue;
        if (cline->summary == NULL)
            continue;
        ok_any = true;

        /*
         * Get the real program name, and use it as the first argument in
         * argv passed to the command.  Then add the summary command to the
         * argv and pass off to be executed.
         */
        path = cline->program;
        req_argv = xmalloc(3 * sizeof(char *));
        program = strrchr(path, '/');
        if (program == NULL)
            program = path;
        else
            program++;
        req_argv[0] = program;
        req_argv[1] = cline->summary;
        req_argv[2] = NULL;
        ok = server_exec(client, cline->summary, req_argv, cline, &process);
        if (ok && process.status != 0)
            status_all = process.status;
        free(req_argv);
    }

    /*
     * Sets the last process status to 0 if all succeeded, or the last
     * failed exit status if any commands gave non-zero.  Return that
     * we had output successfully if any command gave it.
     */
    if (ok_any) {
        if (client->protocol == 1)
            server_v1_send_output(client, status_all);
        else
            server_v2_send_status(client, status_all);
    } else {
        notice("summary request from user %s, but no defined summaries",
               user);
        server_send_error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
    }
}

/*
 * Create the argv we will pass along to a program at a full command
 * request.  This will be created from the full command and arguments given
 * via the remctl client.
 *
 * Takes the command and optional sub-command to run, the config line for this
 * command, the process, and the existing argv from remctl client.  Returns
 * a newly-allocated argv array that the caller is responsible for freeing.
 */
static char **
create_argv_command(struct confline *cline, struct process *process,
                    struct iovec **argv)
{
    size_t count, i, j, stdin_arg;
    char **req_argv = NULL;
    const char *program;

    /* Get ready to assemble the argv of the command. */
    for (count = 0; argv[count] != NULL; count++)
        ;
    req_argv = xmalloc((count + 1) * sizeof(char *));

    /*
     * Get the real program name, and use it as the first argument in argv
     * passed to the command.  Then build the rest of the argv for the
     * command, splicing out the argument we're passing on stdin (if any).
     */
    program = strrchr(cline->program, '/');
    if (program == NULL)
        program = cline->program;
    else
        program++;
    req_argv[0] = xstrdup(program);
    if (cline->stdin_arg == -1)
        stdin_arg = count - 1;
    else
        stdin_arg = (size_t) cline->stdin_arg;
    for (i = 1, j = 1; i < count; i++) {
        if (i == stdin_arg) {
            process->input = argv[i];
            continue;
        }
        if (argv[i]->iov_len == 0)
            req_argv[j] = xstrdup("");
        else
            req_argv[j] = xstrndup(argv[i]->iov_base, argv[i]->iov_len);
        j++;
    }
    req_argv[j] = NULL;
    return req_argv;
}


/*
 * Create the argv we will pass along to a program in response to a help
 * request.  This is fairly simple, created off of the specific command
 * we want help with, along with any sub-command given for specific help.
 *
 * Takes the path of the program to run and the command and optional
 * sub-command to run.  Returns a newly allocated argv array that the caller
 * is responsible for freeing.
 */
static char **
create_argv_help(const char *path, const char *command, const char *subcommand)
{
    char **req_argv = NULL;
    const char *program;

    if (subcommand == NULL)
        req_argv = xmalloc(3 * sizeof(char *));
    else
        req_argv = xmalloc(4 * sizeof(char *));

    /* The argv to pass along for a help command is very simple. */
    program = strrchr(path, '/');
    if (program == NULL)
        program = path;
    else
        program++;
    req_argv[0] = xstrdup(program);
    req_argv[1] = xstrdup(command);
    if (subcommand == NULL)
        req_argv[2] = NULL;
    else {
        req_argv[2] = xstrdup(subcommand);
        req_argv[3] = NULL;
    }
    return req_argv;
}


/*
 * Process an incoming command.  Check the configuration files and the ACL
 * file, and if appropriate, forks off the command.  Takes the argument vector
 * and the user principal, and a buffer into which to put the output from the
 * executable or any error message.  Returns 0 on success and a negative
 * integer on failure.
 *
 * Using the command and the subcommand, the following argument, a lookup in
 * the conf data structure is done to find the command executable and acl
 * file.  If the conf file, and subsequently the conf data structure contains
 * an entry for this command with subcommand equal to "ALL", that is a
 * wildcard match for any given subcommand.  The first argument is then
 * replaced with the actual program name to be executed.
 *
 * After checking the acl permissions, the process forks and the child execv's
 * the command with pipes arranged to gather output. The parent waits for the
 * return code and gathers stdout and stderr pipes.
 */
void
server_run_command(struct client *client, struct config *config,
                   struct iovec **argv)
{
    char *command = NULL;
    char *subcommand = NULL;
    char *helpsubcommand = NULL;
    struct confline *cline = NULL;
    char **req_argv = NULL;
    size_t i;
    bool ok = false;
    bool help = false;
    const char *user = client->user;
    struct process process;

    /* Start with an empty process. */
    memset(&process, 0, sizeof(process));
    process.client = client;

    /*
     * We need at least one argument.  This is also rejected earlier when
     * parsing the command and checking argc, but may as well be sure.
     */
    if (argv[0] == NULL) {
        notice("empty command from user %s", user);
        server_send_error(client, ERROR_BAD_COMMAND, "Invalid command token");
        goto done;
    }

    /* Neither the command nor the subcommand may ever contain nuls. */
    for (i = 0; argv[i] != NULL && i < 2; i++) {
        if (memchr(argv[i]->iov_base, '\0', argv[i]->iov_len)) {
            notice("%s from user %s contains nul octet",
                   (i == 0) ? "command" : "subcommand", user);
            server_send_error(client, ERROR_BAD_COMMAND,
                              "Invalid command token");
            goto done;
        }
    }

    /* We need the command and subcommand as nul-terminated strings. */
    command = xstrndup(argv[0]->iov_base, argv[0]->iov_len);
    if (argv[1] != NULL)
        subcommand = xstrndup(argv[1]->iov_base, argv[1]->iov_len);

    /*
     * Find the program path we need to run.  If we find no matching command
     * at first and the command is a help command, then we either dispatch
     * to the summary command if no specific help was requested, or if a
     * specific help command was listed, check for that in the configuration
     * instead.
     */
    cline = find_config_line(config, command, subcommand);
    if (cline == NULL && strcmp(command, "help") == 0) {

        /* Error if we have more than a command and possible subcommand. */
        if (argv[1] != NULL && argv[2] != NULL && argv[3] != NULL) {
            notice("help command from user %s has more than three arguments",
                   user);
            server_send_error(client, ERROR_TOOMANY_ARGS,
                              "Too many arguments for help command");
        }

        if (subcommand == NULL) {
            server_send_summary(client, user, config);
            goto done;
        } else {
            help = true;
            if (argv[2] != NULL)
                helpsubcommand = xstrndup(argv[2]->iov_base,
                                          argv[2]->iov_len);
            cline = find_config_line(config, subcommand, helpsubcommand);
        }
    }

    /*
     * Arguments may only contain nuls if they're the argument being passed on
     * standard input.
     */
    for (i = 1; argv[i] != NULL; i++) {
        if (cline != NULL) {
            if (help == false && (long) i == cline->stdin_arg)
                continue;
            if (argv[i + 1] == NULL && cline->stdin_arg == -1)
                continue;
        }
        if (memchr(argv[i]->iov_base, '\0', argv[i]->iov_len)) {
            notice("argument %lu from user %s contains nul octet",
                   (unsigned long) i, user);
            server_send_error(client, ERROR_BAD_COMMAND,
                              "Invalid command token");
            goto done;
        }
    }

    /* Log after we look for command so we can get potentially get logmask. */
    server_log_command(argv, cline, user);

    /*
     * Check the command, aclfile, and the authorization of this client to
     * run this command.
     */
    if (cline == NULL) {
        notice("unknown command %s%s%s from user %s", command,
               (subcommand == NULL) ? "" : " ",
               (subcommand == NULL) ? "" : subcommand, user);
        server_send_error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
        goto done;
    }
    if (!server_config_acl_permit(cline, user)) {
        notice("access denied: user %s, command %s%s%s", user, command,
               (subcommand == NULL) ? "" : " ",
               (subcommand == NULL) ? "" : subcommand);
        server_send_error(client, ERROR_ACCESS, "Access denied");
        goto done;
    }

    /*
     * Check for a specific command help request with the cline and do error
     * checking and arg massaging.
     */
    if (help) {
        if (cline->help == NULL) {
            notice("command %s from user %s has no defined help",
                   command, user);
            server_send_error(client, ERROR_NO_HELP,
                              "No help defined for command");
            goto done;
        } else {
            subcommand = xstrdup(cline->help);
        }
    }

    /* Assemble the argv for the command we're about to run. */
    if (help)
        req_argv = create_argv_help(cline->program, subcommand, helpsubcommand);
    else
        req_argv = create_argv_command(cline, &process, argv);

    /* Now actually execute the program. */
    ok = server_exec(client, command, req_argv, cline, &process);
    if (ok) {
        if (client->protocol == 1)
            server_v1_send_output(client, process.status);
        else
            server_v2_send_status(client, process.status);
    }

 done:
    if (command != NULL)
        free(command);
    if (subcommand != NULL)
        free(subcommand);
    if (helpsubcommand != NULL)
        free(helpsubcommand);
    if (req_argv != NULL) {
        for (i = 0; req_argv[i] != NULL; i++)
            free(req_argv[i]);
        free(req_argv);
    }
}


/*
 * Free a command, represented as a NULL-terminated array of pointers to iovec
 * structs.
 */
void
server_free_command(struct iovec **command)
{
    struct iovec **arg;

    for (arg = command; *arg != NULL; arg++) {
        if ((*arg)->iov_base != NULL)
            free((*arg)->iov_base);
        free(*arg);
    }
    free(command);
}
