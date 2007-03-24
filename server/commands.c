/*  $Id$
**
**  Running commands.
**
**  These are the functions for running external commands under remctld and
**  calling the appropriate protocol functions to deal with the output.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on work by Anton Ushakov
**  Copyright 2002, 2003, 2004, 2005, 2006, 2007
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <server/internal.h>
#include <util/util.h>

/* Data structure used to hold details about a running process. */
struct process {
    int reaped;                 /* Whether we've reaped the process. */
    int fds[2];                 /* Array of file descriptors for output. */
    pid_t pid;                  /* Process ID of child. */
    int status;                 /* Exit status. */
};


/*
**  Processes the output from an external program.  Takes the client struct,
**  an array of file descriptors representing the output streams from the
**  client, and a count of streams.  Reads from all the streams as output is
**  available, stopping when they all reach EOF.
**
**  For protocol v2 and higher, we can send the output immediately as we get
**  it.  For protocol v1, we instead accumulate the output in the buffer
**  stored in our client struct, and will send it out later in conjunction
**  with the exit status.
**
**  Returns true on success, false on failure.
*/
static int
server_process_output(struct client *client, struct process *process)
{
    char junk[BUFSIZ];
    char *p;
    size_t left = MAXBUFFER;
    ssize_t status[2];
    int i, maxfd, fd, result;
    fd_set fdset;
    struct timeval timeout;

    /* If we haven't allocated an output buffer, do so now. */
    if (client->output == NULL)
        client->output = xmalloc(MAXBUFFER);
    p = client->output;

    /* Initialize read status for standard output and standard error. */
    status[0] = -1;
    status[1] = -1;

    /* Now, loop while we have input.  We no longer have input if the return
       status of read is 0 on all file descriptors.  At that point, we break
       out of the loop.

       Exceptionally, however, we want to catch the case where our child
       process ran some other command that didn't close its inherited standard
       output and error and then exited itself.  This is not uncommon with
       init scripts that start poorly-written daemons.  Once our child process
       is finished, we're done, even if standard output and error from the
       child process aren't closed yet.  To catch this case, call waitpid with
       the WNOHANG flag each time through the select loop and decide we're
       done as soon as our child has exited. */
    while (!process->reaped) {
        FD_ZERO(&fdset);
        maxfd = -1;
        for (i = 0; i < 2; i++) {
            if (status[i] != 0) {
                FD_SET(process->fds[i], &fdset);
                if (process->fds[i] > maxfd)
                    maxfd = process->fds[i];
            }
        }
        if (maxfd == -1)
            break;

        /* We want to wait until either our child exits or until we get data
           on its output file descriptors.  Normally, the SIGCHLD signal from
           the child exiting would break us out of our select loop.  However,
           the child could exit between the waitpid call and the select call,
           in which case select could block forever since there's nothing to
           wake it up.

           The POSIX-correct way of doing this is to block SIGCHLD and then
           use pselect instead of select with a signal mask that allows
           SIGCHLD.  This allows SIGCHLD from the exiting child process to
           reliably interrupt pselect without race conditions from the child
           exiting before pselect is called.

           Unfortunately, Linux didn't implement a proper pselect until 2.6.16
           and the glibc wrapper that emulates it leaves us open to exactly
           the race condition we're trying to avoid.  This unfortunately
           leaves us with no choice but to set a timeout and wake up every
           five seconds to see if our child died.  (The wait time is arbitrary
           but makes the test suite less annoying.)

           If we see that the child has already exited, do one final poll of
           our output file descriptors and then call the command finished. */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        if (waitpid(process->pid, &process->status, WNOHANG) > 0) {
            process->reaped = 1;
            timeout.tv_sec = 0;
        }
        result = select(maxfd + 1, &fdset, NULL, NULL, &timeout);
        if (result < 0 && errno != EINTR) {
            syswarn("select failed");
            server_send_error(client, ERROR_INTERNAL, "Internal failure");
            free(status);
            return 0;
        }

        /* Iterate through each set file descriptor and read its output.   If
           we're using protocol version one, we append all the output together
           into the buffer.  Otherwise, we send an output token for each bit
           of output as we see it. */
        for (i = 0; i < 2; i++) {
            fd = process->fds[i];
            if (!FD_ISSET(fd, &fdset))
                continue;
            if (client->protocol == 1) {
                if (left > 0) {
                    status[i] = read(fd, p, left);
                    if (status[i] < 0 && (errno != EINTR && errno != EAGAIN))
                        goto readfail;
                    else if (status[i] > 0) {
                        p += status[i];
                        left -= status[i];
                    }
                } else {
                    status[i] = read(fd, junk, sizeof(junk));
                    if (status[i] < 0 && (errno != EINTR && errno != EAGAIN))
                        goto readfail;
                }
            } else {
                status[i] = read(fd, client->output, MAXBUFFER);
                if (status[i] < 0 && (errno != EINTR && errno != EAGAIN))
                    goto readfail;
                if (status[i] > 0) {
                    client->outlen = status[i];
                    if (!server_v2_send_output(client, i + 1))
                        goto fail;
                }
            }
        }
    }
    if (client->protocol == 1)
        client->outlen = p - client->output;
    return 1;

readfail:
    syswarn("read failed");
    server_send_error(client, ERROR_INTERNAL, "Internal failure");
fail:
    return 0;
}


/*
**  Process an incoming command.  Check the configuration files and the ACL
**  file, and if appropriate, forks off the command.  Takes the argument
**  vector and the user principal, and a buffer into which to put the output
**  from the executable or any error message.  Returns 0 on success and a
**  negative integer on failure.
**
**  Using the type and the service, the following argument, a lookup in the
**  conf data structure is done to find the command executable and acl file.
**  If the conf file, and subsequently the conf data structure contains an
**  entry for this type with service equal to "ALL", that is a wildcard match
**  for any given service.  The first argument is then replaced with the
**  actual program name to be executed.
**
**  After checking the acl permissions, the process forks and the child
**  execv's the command with pipes arranged to gather output. The parent waits
**  for the return code and gathers stdout and stderr pipes.
*/
void
server_run_command(struct client *client, struct config *config,
                   struct vector *argv)
{
    char *program;
    char *path = NULL;
    const char *type, *service, *user;
    struct confline *cline = NULL;
    int stdout_pipe[2], stderr_pipe[2];
    char **req_argv = NULL;
    size_t i;
    int ok, fd;
    struct process process = { 0, { 0, 0 }, -1, 0 };

    /* We refer to these a lot, so give them good aliases. */
    type = argv->strings[0];
    service = argv->strings[1];
    user = client->user;

    /* Look up the command and the ACL file from the conf file structure in
       memory. */
    for (i = 0; i < config->count; i++) {
        cline = config->rules[i];
        if (strcmp(cline->type, type) == 0) {
            if (strcmp(cline->service, "ALL") == 0
                || strcmp(cline->service, service) == 0) {
                path = cline->program;
                break;
            }
        }
    }

    /* log after we look for command so we can get potentially get logmask */
    server_log_command(argv, path == NULL ? NULL : cline, user);

    /* Check the command, aclfile, and the authorization of this client to
       run this command. */
    if (path == NULL) {
        notice("unknown command %s %s from user %s", type, service, user);
        server_send_error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
        goto done;
    }
    if (!server_config_acl_permit(cline, user)) {
        notice("access denied: user %s, command %s %s", user, type, service);
        server_send_error(client, ERROR_ACCESS, "Access denied");
        goto done;
    }

    /* Assemble the argv, envp, and fork off the child to run the command. */
    req_argv = xmalloc((argv->count + 1) * sizeof(char *));

    /* Get the real program name, and use it as the first argument in argv
       passed to the command. */
    program = strrchr(path, '/');
    if (program == NULL)
        program = path;
    else
        program++;
    req_argv[0] = strdup(program);
    for (i = 1; i < argv->count; i++) {
        req_argv[i] = strdup(argv->strings[i]);
    }
    req_argv[i] = NULL;

    /* These pipes are used for communication with the child process that 
       actually runs the command. */
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        syswarn("cannot create pipes");
        server_send_error(client, ERROR_INTERNAL, "Internal failure");
        goto done;
    }

    /* Flush output before forking, mostly in case -S was given and we've
       therefore been writing log messages to standard output that may not
       have been flushed yet. */
    fflush(stdout);
    process.pid = fork();
    switch (process.pid) {
    case -1:
        syswarn("cannot fork");
        server_send_error(client, ERROR_INTERNAL, "Internal failure");
        goto done;

    /* In the child. */
    case 0:
        dup2(stdout_pipe[1], 1);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        dup2(stderr_pipe[1], 2);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        /* Child doesn't need stdin at all, but just closing it causes
           problems for puppet.  Reopen on /dev/null instead.  Ignore failure
           here, since it probably won't matter and worst case is that we
           leave stdin closed. */
        close(0);
        fd = open("/dev/null", O_RDONLY);
        if (fd > 0)
            dup2(fd, 0);

        /* Put the authenticated principal and other connection information in
           the environment.  REMUSER is for backwards compatibility with
           earlier versions of remctl. */
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

        /* Run the command. */
        execv(path, req_argv);

        /* This happens only if the exec fails.  Print out an error message to
           the stderr pipe and fail; that's the best that we can do. */
        fprintf(stderr, "Cannot execute: %s\n", strerror(errno));
        exit(-1);

    /* In the parent. */
    default:
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Unblock the read ends of the pipes, to enable us to read from both
           iteratevely. */
        fcntl(stdout_pipe[0], F_SETFL, 
              fcntl(stdout_pipe[0], F_GETFL) | O_NONBLOCK);
        fcntl(stderr_pipe[0], F_SETFL, 
              fcntl(stderr_pipe[0], F_GETFL) | O_NONBLOCK);

        /* This collects output from both pipes iteratively, while the child
           is executing, and processes it. */
        process.fds[0] = stdout_pipe[0];
        process.fds[1] = stderr_pipe[0];
        ok = server_process_output(client, &process);
        if (!process.reaped)
            waitpid(process.pid, &process.status, 0);
        if (WIFEXITED(process.status))
            process.status = (signed int) WEXITSTATUS(process.status);
        else
            process.status = -1;
        if (ok) {
            if (client->protocol == 1)
                server_v1_send_output(client, process.status);
            else
                server_v2_send_status(client, process.status);
        }
        close(process.fds[0]);
        close(process.fds[1]);
    }

 done:
    if (req_argv != NULL) {
        i = 0;
        while (req_argv[i] != '\0') {
            free(req_argv[i]);
            i++;
        }
        free(req_argv);
    }
}
