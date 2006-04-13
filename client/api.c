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

#include <client/remctl.h>
#include <util/util.h>

/* Private structure that holds the details of an open remctl connection. */
struct remctl {
    int fd;
    char *error;
    struct remctl_output *output;
};


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
    return NULL;
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

    if (r->error != NULL)
        free(r->error);
    va_start(args, format);
    vasprintf(&r->error, format, args);
    va_end(args);
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
*/
int
remctl_command(struct remctl *r, const char **command, int finished)
{
    _remctl_set_error(r, "Not yet implemented");
    return 0;
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
