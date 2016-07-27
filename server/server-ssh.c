/*
 * ssh protocol, server implementation.
 *
 * Implements remctl over ssh using regular commands and none of the remctl
 * protocol.  The only part of the normal remctl server reused here is the
 * configuration and command running code.
 *
 * Written by Russ Allbery
 * Copyright 2016 Dropbox, Inc.
 *
 * See LICENSE for licensing terms.
 */
 
#include <config.h>
#include <portable/event.h>
#include <portable/system.h>

#include <ctype.h>

#include <server/internal.h>
#include <util/buffer.h>
#include <util/macros.h>
#include <util/messages.h>
#include <util/vector.h>
#include <util/xmalloc.h>
#include <util/xwrite.h>


/*
 * Parse a command string into a remctl command.  This is much more complex
 * for remctl-shell than it is for remctld since we get the command as a
 * string with shell quoting and have to understand and undo the quoting.
 *
 * Implements single and double quotes, with backslash escaping any character.
 */
struct iovec **
server_ssh_parse_command(const char *command)
{
    struct vector *args;
    struct buffer *arg;
    struct iovec **argv;
    const char *p;
    size_t i, length;
    char quote = '\0';
    enum state {
        SEPARATOR,
        ARG,
        QUOTE
    } state;

    /*
     * Parse the string using a state engine.  We can be in one of three
     * states: in the separator between arguments, or inside a quoted string.
     * If inside a quoted string, the quote used to terminate the string is
     * stored in quote.
     *
     * Backslash escapes any character inside or outside quotes.  If backslash
     * is at the end of the string, we just treat it as a literal backslash.
     */
    args = vector_new();
    arg = buffer_new();
    state = SEPARATOR;
    for (p = command; p != '\0'; p++) {
        if (*p == '\\' && p[1] != '\0') {
            buffer_append(arg, p + 1, 1);
            if (state == SEPARATOR)
                state = ARG;
            continue;
        }
        switch (state) {
        case SEPARATOR:
            if (!isspace((int) *p)) {
                switch (*p) {
                case '\'':
                case '"':
                    state = QUOTE;
                    quote = *p;
                    break;
                default:
                    state = ARG;
                    buffer_append(arg, p, 1);
                    break;
                }
            }
            break;
        case QUOTE:
            if (*p == quote)
                state = ARG;
            else
                buffer_append(arg, p, 1);
            break;
        case ARG:
            if (isspace((int) *p)) {
                vector_addn(args, arg->data, arg->left);
                buffer_set(arg, NULL, 0);
                state = SEPARATOR;
            } else {
                switch (*p) {
                case '\'':
                case '"':
                    state = QUOTE;
                    quote = *p;
                    break;
                default:
                    buffer_append(arg, p, 1);
                    break;
                }
            }
            break;
        }
    }

    /*
     * Ending inside a quoted string is an error.  Otherwise, recover the last
     * argument and clean up.
     */
    if (state == QUOTE) {
        warn("unterminated %c quote in command", quote);
        goto fail;
    } else if (state == ARG) {
        vector_addn(args, arg->data, arg->left);
    }
    buffer_free(arg);

    /* Turn the vector into the iovec we need for everything else. */
    argv = xcalloc(args->count + 1, sizeof(struct iovec *));
    for (i = 0; i < args->count; i++) {
        argv[i] = xcalloc(1, sizeof(struct iovec));
        length = strlen(args->strings[i]);
        argv[i]->iov_base = xmalloc(length);
        memcpy(argv[i]->iov_base, args->strings[i], length);
        argv[i]->iov_len = length;
    }
    argv[args->count] = NULL;
    vector_free(args);
    return argv;

fail:
    vector_free(args);
    buffer_free(arg);
    return NULL;
}


/*
 * Handle one block of output from the running command.
 */
static void
handle_output(struct bufferevent *bev, void *data)
{
    int fd;
    struct evbuffer *buf;
    struct process *process = data;
    struct client *client = process->client;

    process->saw_output = true;
    fd = (bev == process->inout) ? client->fd : client->stderr_fd;
    buf = bufferevent_get_input(bev);
    if (evbuffer_write(buf, fd) < 0) {
        syswarn("error sending output");
        client->fatal = true;
        process->saw_error = true;
        event_base_loopbreak(process->loop);
    }
}


/*
 * Set up to execute a command.  For the ssh protocol, all we need to do is
 * install output handlers for both stdout and stderr that just send the
 * output back to our stdout and stderr.
 */
static void
command_setup(struct process *process)
{
    bufferevent_data_cb writecb;

    writecb = (process->input == NULL) ? NULL : server_handle_input_end;
    bufferevent_setcb(process->inout, handle_output, writecb,
                      server_handle_io_event, process);
    bufferevent_setwatermark(process->inout, EV_READ, 0, TOKEN_MAX_OUTPUT);
    bufferevent_enable(process->err, EV_READ);
    bufferevent_setcb(process->err, handle_output, NULL,
                      server_handle_io_event, process);
    bufferevent_setwatermark(process->err, EV_READ, 0, TOKEN_MAX_OUTPUT);
}


/*
 * Handle the end of the command.  For the ssh protocol, we do nothing, since
 * the main program will collect the exit status and exit with the appropriate
 * status in order to communicate it back to the caller.
 */
static bool
command_finish(struct client *client UNUSED, struct evbuffer *output UNUSED,
               int exit_status UNUSED)
{
    return true;
}


/*
 * Send an error back over an ssh channel.  This just writes the error message
 * with a trailing newline to standard error.
 */
static bool
send_error(struct client *client, enum error_codes code UNUSED,
           const char *message)
{
    ssize_t status;

    status = xwrite(client->stderr_fd, message, strlen(message));
    if (status >= 0)
        status = xwrite(client->stderr_fd, "\n", 1);
    if (status < 0) {
        syswarn("error sending error message");
        client->fatal = true;
        return false;
    }
    return true;
}


/*
 * Create a client struct for a remctl-shell invocation based on the ssh
 * environment.  Abort here if the expected ssh environment variables aren't
 * set.  Caller is responsible for freeing the allocated client struct.
 */
struct client *
server_ssh_new_client(void)
{
    struct client *client;
    const char *ssh_client, *user;
    struct vector *client_info;

    /* Parse client identity from ssh environment variables. */
    user = getenv("REMCTL_USER");
    if (user == NULL)
        die("REMCTL_USER must be set in the environment via authorized_keys");
    ssh_client = getenv("SSH_CLIENT");
    if (ssh_client == NULL)
        die("SSH_CLIENT not set (remctl-shell must be run via ssh)");
    client_info = vector_split_space(ssh_client, NULL);

    /* Create basic client struct. */
    client = xcalloc(1, sizeof(struct client));
    client->fd = STDOUT_FILENO;
    client->stderr_fd = STDERR_FILENO;
    client->ipaddress = xstrdup(client_info->strings[0]);
    client->protocol = 3;
    client->user = xstrdup(user);

    /* Add ssh protocol callbacks. */
    client->setup = command_setup;
    client->finish = command_finish;
    client->error = send_error;

    /* Free allocated data and return. */
    vector_free(client_info);
    return client;
}


/*
 * Free a client struct, including any resources that it holds.  This is a
 * subset of server_free_client that doesn't do the GSS-API actions.
 */
void
server_ssh_free_client(struct client *client)
{
    if (client == NULL)
        return;
    if (client->fd >= 0)
        close(client->fd);
    if (client->stderr_fd >= 0)
        close(client->stderr_fd);
    free(client->user);
    free(client->hostname);
    free(client->ipaddress);
    free(client);
}
