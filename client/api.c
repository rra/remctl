/*  $Id$
**
**  Entry points for the remctl library API.
**
**  All of the high-level entry points for the remctl library API, defined in
**  remctl.h, are found here.  The detailed implementation of these APIs are
**  in other source files.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on work by Anton Ushakov
**  Copyright 2002, 2003, 2004, 2005, 2006
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>

#include <errno.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <client/remctl.h>
#include <util/util.h>

/* Private structure that holds the details of an open remctl connection. */
struct remctl {
    int protocol;
    int fd;
    gss_ctx_id_t context;
    char *error;
    struct remctl_output *output;
    int status;
};


/*
**  Handle an internal failure for the simplified interface.  We try to grab
**  the remctl error and put it into the error field in the remctl result, but
**  if that fails, we free the remctl result and return NULL to indicate a
**  fatal error.
*/
static struct remctl_result *
_remctl_fail(struct remctl *r, struct remctl_result *result)
{
    const char *error;

    error = remctl_error(r);
    if (error == NULL) {
        remctl_close(r);
        remctl_result_free(result);
        return NULL;
    } else {
        result->error = strdup(error);
        remctl_close(r);
        if (result->error != NULL)
            return result;
        else {
            remctl_result_free(result);
            return NULL;
        }
    }
}


/*
**  Given a struct remctl_result into which we're accumulating output and a
**  struct remctl_output that contains a fragment of output, append the output
**  to the appropriate slot in the result.  This is not particularly
**  efficient.  Returns false if something fails and tries to set
**  result->error; if we can't even do that, make sure it's set to NULL.
*/
static int
_remctl_output_append(struct remctl_result *result,
                      struct remctl_output *output)
{
    char **buffer = NULL;
    size_t *length = NULL;
    char *old, *newbuf;
    size_t oldlen;
    int status;

    if (output->type == REMCTL_OUT_ERROR)
        buffer = &result->error;
    else if (output->type == REMCTL_OUT_OUTPUT && output->stream == 1) {
        buffer = &result->stdout;
        length = &result->stdout_len;
    } else if (output->type == REMCTL_OUT_OUTPUT && output->stream == 2) {
        buffer = &result->stderr;
        length = &result->stderr_len;
    } else if (output->type == REMCTL_OUT_OUTPUT) {
        if (result->error != NULL)
            free(result->error);
        status = asprintf(&result->error, "bad output stream %d",
                          output->stream);
        if (status < 0)
            result->error = NULL;
        return 0;
    } else {
        if (result->error != NULL)
            free(result->error);
        result->error = strdup("internal error: bad output type");
        return 0;
    }

    /* We've done our setup, so now we can do the actual manipulation. */
    old = *buffer;
    if (length != NULL)
        oldlen = *length;
    else
        oldlen = strlen(old);
    *length = oldlen + output->length;
    if (output->type == REMCTL_OUT_ERROR)
        *length++;
    newbuf = realloc(*buffer, *length);
    if (newbuf == NULL) {
        if (result->error != NULL)
            free(result->error);
        result->error = strdup("cannot allocate memory");
        return 0;
    }
    *buffer = newbuf;
    memcpy(*buffer + oldlen, output->data, output->length);
    if (output->type == REMCTL_OUT_ERROR)
        *buffer[*length - 1] = '\0';
    return 1;
}


/*
**  The simplified interface.  Given a host, a port, and a command (as a
**  null-terminated argv-style vector), run the command on that host and port
**  and return a struct remctl_result.  The result should be freed with
**  remctl_result_free.
*/
struct remctl_result *
remctl(const char *host, unsigned short port, const char *principal,
       const char **command)
{
    struct remctl *r = NULL;
    struct remctl_result *result = NULL;
    enum remctl_output_type type;

    result = calloc(1, sizeof(struct remctl_result));
    if (result == NULL)
        return NULL;
    r = remctl_open(host, port, principal);
    if (r == NULL)
        return _remctl_fail(r, result);
    if (!remctl_command(r, command, 1))
        return _remctl_fail(r, result);
    do {
        struct remctl_output *output;

        output = remctl_output(r);
        if (output == NULL)
            return _remctl_fail(r, result);
        type = output->type;
        if (type == REMCTL_OUT_OUTPUT || type == REMCTL_OUT_ERROR) {
            if (!_remctl_output_append(result, output)) {
                if (result->error != NULL)
                    return result;
                else {
                    remctl_result_free(result);
                    return NULL;
                }
            }
        } else if (type == REMCTL_OUT_STATUS) {
            result->status = output->status;
        } else {
            remctl_close(r);
            return result;
        }
    } while (type == REMCTL_OUT_OUTPUT);
    remctl_close(r);
    return result;
}


/*
**  Free a struct remctl_result returned by remctl.
*/
void
remctl_result_free(struct remctl_result *result)
{
    if (result != NULL) {
        if (result->error != NULL)
            free(result->error);
        if (result->stdout != NULL)
            free(result->stdout);
        if (result->stderr != NULL)
            free(result->stderr);
        free(result);
    }
}


/*
**  Internal function to set the error message, freeing an old error message
**  if one is present.
*/
void
_remctl_set_error(struct remctl *r, const char *format, ...)
{
    va_list args;
    int status;

    if (r->error != NULL)
        free(r->error);
    va_start(args, format);
    status = vasprintf(&r->error, format, args);
    va_end(args);

    /* If vasprintf fails, there isn't much we can do, but make sure that at
       least the error is in a consistent state. */
    if (status < 0)
        r->error = NULL;
}


/*
**  Open a new persistant remctl connection to a server, given the host, port,
**  and principal.
*/
struct remctl *
remctl_open(const char *host, unsigned short port, const char *principal)
{
    struct remctl *r;

    r = xmalloc(sizeof(struct remctl));
    r->fd = -1;
    r->context = NULL;
    r->error = NULL;
    r->output = NULL;
    return r;
}


/*
**  Close a persistant remctl connection.
*/
void
remctl_close(struct remctl *r)
{
    if (r != NULL) {
        if (r->fd != -1)
            close(r->fd);
        if (r->error != NULL)
            free(r->error);
        if (r->output != NULL)
            free(r->output);
        free(r);
    }
}


/*
**  Send a complete remote command.  Returns true on success, false on
**  failure.  On failure, use remctl_error to get the error.  command is a
**  NULL-terminated array of nul-terminated strings.  finished is a boolean
**  flag that should be false when the command is not yet complete and true
**  when this is the final (or only) segment.
**
**  Implement in terms of remctl_commandv.
*/
int
remctl_command(struct remctl *r, const char **command, int finished)
{
    struct iovec *vector;
    size_t count, i;
    int status;

    for (count = 0; command[count] != NULL; count++)
        ;
    vector = malloc(sizeof(struct iovec) * count);
    if (vector == NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        return 0;
    }
    for (i = 0; i < count; i++) {
        vector[i].iov_base = (void *) command[i];
        vector[i].iov_len = strlen(command[i]);
    }
    status = remctl_commandv(r, vector, count, finished);
    free(vector);
    return status;
}


/*
**  Same as remctl_command, but take the command as an array of struct iovecs
**  instead.  Use this form for binary data.
*/
int
remctl_commandv(struct remctl *r, const struct iovec *command, size_t count,
                int finished)
{
    _remctl_set_error(r, "Not yet implemented");
    return 0;
}


/*
**  Retrieve output from the remote server.  Each call to this function on the
**  same connection invalidates the previous returned remctl_output struct.
**
**  This function will return zero or more REMCTL_OUT_OUTPUT types followed by
**  a REMCTL_OUT_STATUS type, *or* a REMCTL_OUT_ERROR type.  In either case,
**  any subsequent call before sending a new command will return
**  REMCTL_OUT_DONE.  If the function returns NULL, an internal error
**  occurred; call remctl_error to retrieve the error message.
**
**  The remctl_output struct should *not* be freed by the caller.  It will be
**  invalidated after another call to remctl_output or to remctl_close on the
**  same connection.
*/
struct remctl_output *
remctl_output(struct remctl *r)
{
    _remctl_set_error(r, "Not yet implemented");
    return NULL;
}


/*
**  Returns the internal error message after a failure or "No error" if the
**  last command completed successfully.  This should generally only be called
**  after a failure.
*/
const char *
remctl_error(struct remctl *r)
{
    return (r->error != NULL) ? r->error : "No error";
}
