/*  $Id$
**
**  The deamon for a "K5 sysctl" - a service for remote execution of
**  predefined commands.  Access is authenticated via GSSAPI Kerberos 5,
**  authorized via ACL files.  Runs as a inetd/tcpserver deamon or a
**  standalone program.
**
**  Written by Anton Ushakov <antonu@stanford.edu>
**  Vector library contributed by Russ Allbery <rra@stanford.edu>
**
**  See README for copyright and licensing information.
*/

#include <config.h>
#include <system.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include "gss-utils.h"
#include <util/util.h>

/* Handle compatibility to older versions of MIT Kerberos. */
#ifndef HAVE_GSS_RFC_OIDS
# define GSS_C_NT_USER_NAME gss_nt_user_name
#endif

/* Usage message. */
static const char usage_message[] = "\
Usage: remctld <options>\n\
\n\
Options:\n\
    -d            Log debugging information to syslog\n\
    -f <file>     Config file (default: " CONFIG_FILE ")\n\
    -h            Display this help\n\
    -m            Stand-alone daemon mode, meant mostly for testing\n\
    -P <file>     Write PID to file, only useful with -m\n\
    -p <port>     Port to use, only for standalone mode (default: 4444)\n\
    -s <service>  Service principal to use (default: host/<host>)\n\
    -v            Display the version of remctld\n";

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
int READFD = 0;
int WRITEFD = 1;

/* This is used for caching the conf file in memory after first reading it. */
struct confline {
    struct vector *line;        /* The split configuration line. */
    char *type;                 /* Service type. */
    char *service;              /* Service name. */
    char *program;              /* Full file name of executable. */
    struct cvector *logmask;    /* What args to mask in the log, if any. */
    char **acls;                /* Full file names of ACL files. */
};

/* Holds the complete parsed configuration for remctl. */
struct config {
    struct confline **rules;
    size_t count;
    size_t allocated;
};


/*
**  Display the usage message for remctld.
*/
static void
usage(int status)
{
    fprintf((status == 0) ? stdout : stderr, usage_message);
    if (status == 0)
        exit(0);
    else
        die("invalid usage");
}


/*
**  Given the port number on which to listen, open a listening TCP socket.
**  Returns the file descriptor or -1 on failure, logging an error message.
**  This is only used in standalone mode.
*/
static int
create_socket(unsigned short port)
{
    struct sockaddr_in saddr;
    int s;
    int on = 1;

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        syswarn("error creating socket");
        return -1;
    }

    /* Let the socket be reused right away */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        syswarn("error binding socket");
        close(s);
        return -1;
    }
    if (listen(s, 5) < 0) {
        syswarn("error listening on socket");
        close(s);
        return -1;
    }
    return s;
}


/*
**  Given a service name, imports it and acquires credentials for it, storing
**  them in the second argument.  Returns 0 on success and -1 on failure,
**  logging an error message.
*/
static int
server_acquire_creds(char *service_name, gss_cred_id_t *server_creds)
{
    gss_buffer_desc name_buf;
    gss_name_t server_name;
    OM_uint32 maj_stat, min_stat;

    name_buf.value = service_name;
    name_buf.length = strlen(name_buf.value) + 1;

    maj_stat = gss_import_name(&min_stat, &name_buf, GSS_C_NT_USER_NAME,
                               &server_name);

    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while importing name", maj_stat, min_stat);
        return -1;
    }

    maj_stat = gss_acquire_cred(&min_stat, server_name, 0,
                                GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
                                server_creds, NULL, NULL);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while acquiring credentials", maj_stat, min_stat);
        return -1;
    }

    gss_release_name(&min_stat, &server_name);

    return 0;
}


/*
**  Establish a GSS-API context as a specified service with an incoming
**  client, and returns the context handle, associated client name, and
**  associated connection flags.  Returns 0 on success and -1 on failure,
**  logging an error message.
**
**  Any valid client request is accepted.  If a context is established, its
**  handle is returned in context, the type of the conncetion established is
**  described in the ret_flags, and the client name is returned in
**  client_name.
*/
static int
server_establish_context(gss_cred_id_t server_creds, gss_ctx_id_t *context,
                         gss_buffer_t client_name, OM_uint32 *ret_flags)
{
    gss_buffer_desc send_tok, recv_tok;
    gss_name_t client;
    gss_OID doid;
    OM_uint32 maj_stat, min_stat, acc_sec_min_stat;
    int token_flags;

    if (recv_token(&token_flags, &recv_tok) < 0)
        return -1;

    gss_release_buffer(&min_stat, &recv_tok);
    if (!(token_flags & TOKEN_NOOP)) {
        warn("expected NOOP token, got %d token instead", token_flags);
        return -1;
    }

    *context = GSS_C_NO_CONTEXT;

    if (!(token_flags & TOKEN_CONTEXT_NEXT)) {
        client_name->length = *ret_flags = 0;
        debug("accepted unauthenticated connection");
    } else {
        do {
            if (recv_token(&token_flags, &recv_tok) < 0)
                return -1;
            debug("received token (size=%d)", recv_tok.length);
            print_token(&recv_tok);
            maj_stat = gss_accept_sec_context(&acc_sec_min_stat, 
                                              context, 
                                              server_creds, 
                                              &recv_tok, 
                                              GSS_C_NO_CHANNEL_BINDINGS, 
                                              &client, 
                                              &doid, 
                                              &send_tok, 
                                              ret_flags, 
                                              NULL,     /* ignore time_rec */
                                              NULL);    /* ignore 
                                                           del_cred_handle */

            gss_release_buffer(&min_stat, &recv_tok);

            if (send_tok.length != 0) {
                debug("sending accept_sec_context token (size=%d)",
                      send_tok.length);
                print_token(&send_tok);
                if (send_token(TOKEN_CONTEXT, &send_tok) < 0) {
                    warn("failure sending token");
                    gss_release_buffer(&min_stat, &send_tok);
                    return -1;
                }
                gss_release_buffer(&min_stat, &send_tok);
            }
            if (maj_stat != GSS_S_COMPLETE
                && maj_stat != GSS_S_CONTINUE_NEEDED) {
                display_status("while accepting context", maj_stat,
                               acc_sec_min_stat);
                if (*context == GSS_C_NO_CONTEXT)
                    gss_delete_sec_context(&min_stat, context,
                                           GSS_C_NO_BUFFER);
                return -1;
            }

            if (maj_stat == GSS_S_CONTINUE_NEEDED)
                debug("continue needed");
        } while (maj_stat == GSS_S_CONTINUE_NEEDED);

        display_ctx_flags(*ret_flags);

        maj_stat = gss_display_name(&min_stat, client, client_name, &doid);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("while displaying name", maj_stat, min_stat);
            return -1;
        }
        maj_stat = gss_release_name(&min_stat, &client);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("while releasing name", maj_stat, min_stat);
            return -1;
        }
    }

    return 0;
}


/*
**  Handles an include request for either read_conf_file or acl_check_file.
**  Takes the vector that represents the include directive, the current file,
**  the line number, the function to call for each included file, and a piece
**  of data to pass to that function.  Handles including either files or
**  directories.
**
**  If the function returns a value less than -1, return its return code.
**  Otherwise, return the greatest of all status codes returned by the
**  functions.  Also returns -2 for any error in processing the include
**  directive.
*/
static int
handle_include(struct vector *line, const char *file, int lineno,
               int (*function)(void *, const char *), void *data)
{
    const char *included = line->strings[1];
    struct stat st;

    /* Validate the directive. */
    if (line->count != 2 || strcmp(line->strings[0], "include") != 0) {
        warn("%s:%d: parse error", file, lineno);
        return -2;
    }
    if (strcmp(included, file) == 0) {
        warn("%s:%d: %s recursively included", file, lineno, file);
        return -2;
    }
    if (stat(included, &st) < 0) {
        warn("%s:%d: included file %s not found", file, lineno, included);
        return -2;
    }

    /* If it's a directory, include everything in the directory that doesn't
       contain a period.  Otherwise, just include the one file. */
    if (!S_ISDIR(st.st_mode)) {
        return (*function)(data, included);
    } else {
        DIR *dir;
        struct dirent *entry;
        int status = -1;
        int last;

        dir = opendir(included);
        while ((entry = readdir(dir)) != NULL) {
            char *path;
            size_t length;

            if (strchr(entry->d_name, '.') != NULL)
                continue;
            length = strlen(included) + 1 + strlen(entry->d_name) + 1;
            path = xmalloc(length);
            snprintf(path, length, "%s/%s", included, entry->d_name);
            last = (*function)(data, path);
            free(path);
            if (last < -1) {
                closedir(dir);
                return status;
            }
            if (last > status)
                status = last;
        }
        closedir(dir);
        return status;
    }
}


/*
**  Reads the configuration file and parses every line, populating a data
**  structure that will be traversed on each request to translate a command
**  type into an executable path and ACL file.
**
**  config is populated with the parsed configuration file.  Empty lines and
**  lines beginning with # are ignored.  Each line is divided into fields,
**  separated by spaces.  The fields are defined by struct confline.  Lines
**  ending in backslash are continued on the next line.  config is passed in
**  as a void * so that read_conf_file and acl_check_file can use common
**  include handling code.
**
**  As a special case, include <file> will call read_conf_file recursively to
**  parse an included file (or, if <file> is a directory, every file in that
**  directory that doesn't contain a period).
**
**  Returns 0 on success and -2 on error, reporting an error message.
*/
static int
read_conf_file(void *data, const char *name)
{
    struct config *config = data;
    FILE *file;
    char *buffer, *p;
    size_t bufsize, length, size, count, i, arg_i;
    int s;
    struct vector *line = NULL;
    struct confline *confline = NULL;
    size_t lineno = 0;
    DIR *dir = NULL;

    bufsize = 1024;
    buffer = xmalloc(bufsize);
    file = fopen(name, "r");
    if (file == NULL) {
        free(buffer);
        syswarn("cannot open config file %s", name);
        return -2;
    }
    while (fgets(buffer, bufsize, file) != NULL) {
        length = strlen(buffer);
        if (length == 2 && buffer[length - 1] != '\n') {
            warn("%s:%d: no final newline", name, lineno);
            goto fail;
        }
        if (length < 2)
            continue;

        /* Allow for long lines and continuation lines.  As long as we've
           either filled the buffer or have a line ending in a backslash, we
           keep reading more data.  If we filled the buffer, increase it by
           another 1KB; otherwise, back up and write over the backslash and
           newline. */
        p = buffer + length - 2;
        while (length > 2 && (p[1] != '\n' || p[0] == '\\')) {
            if (p[1] != '\n') {
                bufsize += 1024;
                buffer = xrealloc(buffer, bufsize);
            } else {
                length -= 2;
                lineno++;
            }
            if (fgets(buffer + length, bufsize - length, file) == NULL) {
                warn("%s:%d: no final line or newline", name, lineno);
                goto fail;
            }
            length = strlen(buffer);
            p = buffer + length - 2;
        }
        if (length > 0)
            buffer[length - 1] = '\0';
        lineno++;

        /* Skip blank lines or commented-out lines.  Note that because of the
           above logic, comments can be continued on the next line, so be
           careful. */
        p = buffer;
        while (isspace((int) *p))
            p++;
        if (*p == '\0' || *p == '#')
            continue;

        /* We have a valid configuration line.  Do a quick syntax check and
           handle include. */
        line = vector_split_space(buffer, NULL);
        if (line->count < 4) {
            s = handle_include(line, name, lineno, read_conf_file, config);
            if (s < -1)
                goto fail;
            vector_free(line);
            line = NULL;
            continue;
        }

        /* Okay, we have a regular configuration line.  Make sure there's
           space for it in the config struct and stuff the vector into
           place. */
        if (config->count == config->allocated) {
            if (config->allocated < 4)
                config->allocated = 4;
            else
                config->allocated *= 2;
            size = config->allocated * sizeof(struct confline *);
            config->rules = xrealloc(config->rules, size);
        }
        confline = xcalloc(1, sizeof(struct confline));
        confline->line    = line;
        confline->type    = line->strings[0];
        confline->service = line->strings[1];
        confline->program = line->strings[2];

        /* Change this to a while vline->string[n] has an "=" in it to support
           multiple x=y options. */
        if (strncmp(line->strings[3], "logmask=", 8) == 0) {
            confline->logmask = cvector_new();
            cvector_split(line->strings[3] + 8, ',', confline->logmask);
            arg_i = 4;
        } else
            arg_i = 3;

        /* One more syntax error possibility here: a line that only has a
           logmask setting but no ACL files. */
        if (line->count <= arg_i) {
            warn("%s:%d: config parse error", name, lineno);
            goto fail;
        }

        /* Grab the list of ACL files. */
        count = line->count - arg_i + 1;
        confline->acls = xmalloc(count * sizeof(char *));
        for (i = 0; i < line->count - arg_i; i++)
            confline->acls[i] = line->strings[i + arg_i];
        confline->acls[i] = NULL;

        /* Success.  Put the configuration line in place. */
        config->rules[config->count] = confline;
        config->count++;
        confline = NULL;
        line = NULL;
    }

    /* Free allocated memory and return success. */
    free(buffer);
    fclose(file);
    return 0;

    /* Abort with an error. */
fail:
    if (dir != NULL)
        closedir(dir);
    if (line != NULL)
        vector_free(line);
    if (confline != NULL) {
        if (confline->logmask != NULL)
            cvector_free(confline->logmask);
        free(confline);
    }
    free(buffer);
    fclose(file);
    return -2;
}


/*
**  Check to see if a given principal is in a given file.  This function is
**  recursive to handle included ACL files and only does a simple check to
**  prevent infinite recursion, so be careful.  The first argument is the user
**  to check, which is passed in as a void * so that acl_check_file and
**  read_conf_file can share common include-handling code.
**
**  Returns 0 if the user is authorized, -1 if they aren't, and -2 on some
**  sort of failure (such as failure to read a file or a syntax error).
*/
static int
acl_check_file(void *data, const char *aclfile)
{
    const char *user = data;
    FILE *file = NULL;
    char buffer[BUFSIZ];
    char *p;
    int lineno, s;
    size_t length;
    struct vector *line = NULL;

    file = fopen(aclfile, "r");
    if (file == NULL) {
        syswarn("cannot open ACL file %s", aclfile);
        return -2;
    }
    lineno = 0;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lineno++;
        length = strlen(buffer);
        if (length >= sizeof(buffer) - 1) {
            warn("%s:%d: ACL file line too long", aclfile, lineno);
            goto fail;
        }

        /* Skip blank lines or commented-out lines and remove trailing
         * whitespace. */
        p = buffer + length - 1;
        while (isspace((int) *p))
            p--;
        p[1] = '\0';
        p = buffer;
        while (isspace((int) *p))
            p++;
        if (*p == '\0' || *p == '#')
            continue;

        /* Parse the line. */
        if (strchr(p, ' ') != NULL) {
            line = vector_split_space(buffer, NULL);
            s = handle_include(line, aclfile, lineno, acl_check_file, data);
            vector_free(line);
            line = NULL;
            if (s != -1) {
                fclose(file);
                return s;
            }
        } else if (strcmp(p, user) == 0) {
            fclose(file);
            return 0;
        }
    }
    return -1;

fail:
    if (line != NULL)
        vector_free(line);
    if (file != NULL)
        fclose(file);
    return -2;
}


/*
**  Check to see if a given principal is in a given list of ACL files.  The
**  list of ACL files should be NULL-terminated.  Special-case "ANYUSER" as
**  the first filename.
**
**  Returns 0 on success and -1 on failure.
*/
static int
acl_check(char *userprincipal, char **acls)
{
    int status, i;

    if (strcmp(acls[0], "ANYUSER") == 0)
        return 0;
    for (i = 0; acls[i] != NULL; i++) {
        status = acl_check_file(userprincipal, acls[i]);
        if (status != -1)
            return status;
    }
    return -1;
}


/*
**  Receives a request token payload and builds an argv structure for it,
**  returning that as a vector.  Takes the GSSAPI context and a buffer into
**  which to put an error message.  If there are any problems with the request
**  structure, puts the error message in ret_message, logs it, and returns -1.
**  Otherwise, returns 0.
*/
static struct vector *
process_request(gss_ctx_id_t context, char *ret_message)
{
    char *msg;
    OM_uint32 msglength;
    OM_uint32 req_argc;
    struct vector *req_argv;
    OM_uint32 arglength;
    char *cp;
    int token_flags;  /* Not used but required by gss_recvmsg for other uses.*/
    OM_uint32 network_order;
    char argbuf[MAXBUFFER];

    if (gss_recvmsg(context, &token_flags, &msg, &msglength) < 0)
        return NULL;

    cp = msg;

    /* Get the argc of the request. */
    memcpy(&network_order, cp, sizeof(OM_uint32));
    req_argc = ntohl(network_order);
    cp += sizeof(OM_uint32);

    debug("argc is %d", req_argc);
    if (req_argc <= 0 || req_argc > MAXCMDARGS) {
        strcpy(ret_message, "Invalid argc in request message\n");
        warn("invalid argc in request message");
        return NULL;
    }
        
    req_argv = vector_new();
    vector_resize(req_argv, req_argc);

    /* Parse out the arguments and store them into a vector.  Arguments are
       packed: (<arglength><argument>)+. */
    while (cp < msg + msglength) {
        memcpy(&network_order, cp, sizeof(OM_uint32));
        arglength = ntohl(network_order);
        cp += sizeof(OM_uint32);
        if (arglength > MAXBUFFER
            || arglength <= 0
            || cp + arglength > msg + msglength) {
            strcpy(ret_message, 
                   "Data unpacking error in getting arguments\n");
            warn("data unpacking error in getting arguments");
            return NULL;
        }
        memcpy(argbuf, cp, arglength);
        argbuf[arglength] = '\0';
        vector_add(req_argv, argbuf);
        cp += arglength;
    }

    /* was allocated in gss_recvmsg */
    free(msg);

    return req_argv;
}


/*
**  Send a response back to the client.  Takes the GSSAPI context, the return
**  code from running the command, and the output from the command.  Packs the
**  return message payload, adds the error code and flag if needed, and then
**  sends the response back via GSSAPI.
*/
static int
process_response(gss_ctx_id_t context, OM_uint32 code, char *blob)
{
    OM_uint32 bloblength;
    OM_uint32 msglength;
    OM_uint32 network_order;
    char *msg;
    OM_uint32 flags;
    bloblength = strlen(blob);

    flags = TOKEN_DATA;

    msg = xmalloc((2 * sizeof(OM_uint32) + bloblength));
    msglength = bloblength + 2 * sizeof(OM_uint32);

    network_order = htonl(code);
    memcpy(msg, &network_order, sizeof(OM_uint32));
    network_order = htonl(bloblength);
    memcpy(msg + sizeof(int), &network_order, sizeof(OM_uint32));
    memcpy(msg + 2 * sizeof(OM_uint32), blob, bloblength);
    
    if (gss_sendmsg(context, flags, msg, msglength) < 0)
        return -1;

    free(msg);

    return 0;
}


/*
**  Log a command.  Takes the argument vector, the configuration line that
**  matched the command, and the principal running the command.
*/
static void
log_command(struct vector *argvector, struct confline *cline,
            char *userprincipal)
{
    char *command;
    unsigned int i, j;

    if (cline != NULL && cline->logmask != NULL) {
        struct cvector *v = cvector_new();
        for (i = 0; i < argvector->count; i++) {
            const char *a = argvector->strings[i];
            for (j = 0; j < cline->logmask->count; j++) {
                if (atoi(cline->logmask->strings[j]) == (int) i) {
                    a = "**MASKED**";
                    break;
                }
            }
            cvector_add(v, a);
        }
        command = cvector_join(v, " ");
        cvector_free(v);
    } else {
        command = vector_join(argvector, " ");
    }
    notice("COMMAND from %s: %s", userprincipal, command);
    free(command);
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
static OM_uint32
process_command(struct config *config, struct vector *argvector,
                char *userprincipal, char *ret_message)
{
    char *command;
    char *program;
    char **acls = NULL;
    struct confline *cline = NULL;
    int stdout_pipe[2];
    int stderr_pipe[2];
    char **req_argv;
    char remuser[100];
    char scprincipal[100];
    int ret_length;
    char err_message[MAXBUFFER];
    int ret_code, pipebuffer;
    pid_t pid;
    unsigned int i;

    memset(ret_message, 0, MAXBUFFER);
    memset(err_message, 0, MAXBUFFER);

    /* This is how much is read from each pipe, stderr and stdout.  The total
       size of what we send back has to be not more than MAXBUFFER. */
    pipebuffer = MAXBUFFER / 2;

    command = NULL;
    req_argv = NULL;

    /* Look up the command and the ACL file from the conf file structure in
       memory. */
    for (i = 0; i < config->count; i++) {
        cline = config->rules[i];
        if (strcmp(cline->type, argvector->strings[0]) == 0) {
            if (strcmp(cline->service, "ALL") == 0 ||
                strcmp(cline->service, argvector->strings[1]) == 0) {
                    command = cline->program;
                    acls    = cline->acls;
                    break;
            }
        }
    }

    /* log after we look for command so we can get potentially get logmask */
    log_command(argvector, command == NULL ? NULL : cline, userprincipal);

    /* Check the command, aclfile, and the authorization of this client to
       run this command. */
    if (command == NULL) {
        ret_code = -1;
        strcpy(ret_message, "Command not defined\n");
        goto done;
    }
    ret_code = acl_check(userprincipal, acls);
    if (ret_code != 0) {
        snprintf(ret_message, MAXBUFFER,
                 "Access denied: %s principal not on the acl list\n",
                 userprincipal);
        notice("access denied: %s not on ACL list", userprincipal);
        goto done;
    }

    /* Assemble the argv, envp, and fork off the child to run the command. */
    req_argv = xmalloc((argvector->count + 1) * sizeof(char *));

    /* Get the real program name, and use it as the first argument in argv
       passed to the command. */
    program = strrchr(command, '/');
    if (program == NULL)
        program = command;
    else
        program++;
    req_argv[0] = strdup(program);
    for (i = 1; i < argvector->count; i++) {
        req_argv[i] = strdup(argvector->strings[i]);
    }
    req_argv[i] = '\0';

    /* These pipes are used for communication with the child process that 
       actually runs the command. */
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        strcpy(ret_message, "Can't create pipes\n");
        syswarn("cannot create pipes");
        ret_code = -1;
        goto done;
    }

    switch (pid = fork()) {
    case -1:
        strcpy(ret_message, "Can't fork\n");
        syswarn("cannot fork");
        ret_code = -1;
        goto done;

    /* In the child. */
    case 0:
        dup2(stdout_pipe[1], 1);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        dup2(stderr_pipe[1], 2);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        /* Child doesn't need stdin at all. */
        close(0);

        /* Tell the exec'ed program who requested it */
        snprintf(remuser, MAXBUFFER, "REMUSER=%s", userprincipal);
        if (putenv(remuser) < 0) {
            strcpy(ret_message, "Can't set REMUSER environment variable\n");
            syswarn("cannot set REMUSER environment variable");
            fprintf(stderr, ret_message);
            exit(-1);
        }

        /* Backward compat */
        snprintf(scprincipal, MAXBUFFER, "SCPRINCIPAL=%s", userprincipal);
        if (putenv(scprincipal) < 0) {
            strcpy(ret_message,
                   "Can't set SCPRINCIPAL environment variable\n");
            syswarn("cannot set SCPRINCIPAL environment variable");
            fprintf(stderr, ret_message);
            exit(-1);   
        }

        execv(command, req_argv);

        /* This here happens only if the exec failed. In that case this passed
           the errno information back up the stderr pipe to the parent, and
           dies. */
        strcpy(ret_message, strerror(errno));
        sysdie("exec failed");

    /* In the parent. */
    default:
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Unblock the read ends of the pipes, to enable us to read from both
           iteratevely */
        fcntl(stdout_pipe[0], F_SETFL, 
              fcntl(stdout_pipe[0], F_GETFL) | O_NONBLOCK);
        fcntl(stderr_pipe[0], F_SETFL, 
              fcntl(stderr_pipe[0], F_GETFL) | O_NONBLOCK);

        /* This collects output from both pipes iteratively, while the child
           is executing */
        if ((ret_length = read_two(stdout_pipe[0], stderr_pipe[0], 
                                   ret_message, err_message, 
                                   pipebuffer, pipebuffer)) < 0)
            strcpy(ret_message, "No output from the command\n");

        /* Both streams could have useful info. */
        strcat(ret_message, err_message);
        ret_length = strlen(ret_message);

        waitpid(pid, &ret_code, 0);

        /* This does the crazy >>8 thing to get the real error code. */
        if (WIFEXITED(ret_code))
            ret_code = (signed int) WEXITSTATUS(ret_code);
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
    return ret_code;
}


/*
**  Handle the interaction with the client.  Takes the server credentials and
**  establishes a context with the client.  Then, calls process_request to get
**  the command and arguments from the client and calls process_command to
**  find the executable, check the ACL files, and run the command.  Once the
**  response and exit code have been determined, calls process_response to
**  send the response back to the client and then cleans up the context.
*/
static int
process_connection(struct config *config, gss_cred_id_t server_creds)
{
    gss_buffer_desc client_name;
    gss_ctx_id_t context;
    OM_uint32 maj_stat, min_stat;
    OM_uint32 ret_flags;
    struct vector *req_argv;
    OM_uint32 ret_code;
    char ret_message[MAXBUFFER];
    int ret_length;

    char *userprincipal;

    ret_code = 0;
    ret_length = 0;
    req_argv=0;
    memset(ret_message, '\0', MAXBUFFER);

    /* Establish a context with the client. */
    if (server_establish_context(server_creds, &context,
                                 &client_name, &ret_flags) < 0)
        return -1;

    if (context == GSS_C_NO_CONTEXT) {
        warn("unauthenticated connection");
        return -1;
    }

    /* This is who just connected to us. */
    userprincipal = xmalloc(client_name.length + 1);
    memcpy(userprincipal, client_name.value, client_name.length);
    userprincipal[client_name.length] = '\0';

    debug("accepted connection from %.*s",
          (int) client_name.length, (char *) client_name.value);
    
    gss_release_buffer(&min_stat, &client_name);

    /* Interchange to get the data that makes up the request - basically an
       argv. */
    if ((req_argv = process_request(context, ret_message)) == NULL)
        ret_code = -1;

    /* Check the acl, the existence of the command, run the command and
       gather the output. */
    if (ret_code == 0)
        ret_code = process_command(config, req_argv, userprincipal,
                                   ret_message);

    /* Send back the stuff returned from the exec. */
    process_response(context, ret_code, ret_message);
    free(userprincipal);

    /* Destroy the context. */
    maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while deleting context", maj_stat, min_stat);
        return -1;
    }

    return 0;
}


/*
**  Main routine.  Parses command-line arguments, determines whether we're
**  running in stand-alone or inetd mode, and does the connection handling if
**  running in standalone mode.  User connections are handed off to
**  process_connection.
*/
int
main(int argc, char *argv[])
{
    char *service_name = NULL;
    const char *pid_path = NULL;
    FILE *pid_file;
    char host_name[500];
    int option;
    struct hostent *hostinfo;
    gss_cred_id_t server_creds;
    OM_uint32 min_stat;
    unsigned short port = 4444;
    int s, stmp;
    int do_standalone = 0;
    const char *conffile = CONFIG_FILE;
    struct config *config;

    /* Since we are normally called from tcpserver or inetd, prevent clients
       from holding on to us forever by dying after an hour. */
    alarm(60 * 60);

    /* Establish identity and set up logging. */
    message_program_name = "remctld";
    openlog("remctld", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    message_handlers_notice(1, message_log_syslog_info);
    message_handlers_warn(1, message_log_syslog_warning);
    message_handlers_die(1, message_log_syslog_err);

    /* Parse options. */
    while ((option = getopt(argc, argv, "df:hmP:p:s:v")) != EOF) {
        switch (option) {
        case 'd':
            message_handlers_debug(1, message_log_syslog_debug);
            break;
        case 'f':
            conffile = optarg;
            break;
        case 'h':
            usage(0);
            break;
        case 'm':
            do_standalone = 1;
            break;
        case 'P':
            pid_path = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            service_name = optarg;
            break;
        case 'v':
            printf("remctld %s\n", PACKAGE_VERSION);
            exit(0);
            break;
        default:
            usage(1);
            break;
        }
    }

    /* Read the configuration file. */
    config = xcalloc(1, sizeof(struct config));
    if (read_conf_file(config, conffile) != 0)
        die("cannot read configuration file %s", conffile);

    /* This specifies the service name, checks out the keytab, etc. */
    if (service_name == NULL) {
        if (gethostname(host_name, sizeof(host_name)) < 0)
            sysdie("cannot get hostname");
        hostinfo = gethostbyname(host_name);
        if (hostinfo == NULL)
            die("cannot resolve hostname %s", host_name);
        service_name = xmalloc(strlen("host/") + strlen(hostinfo->h_name) + 1);
        strcpy(service_name, "host/");
        strcat(service_name, hostinfo->h_name);
        lowercase(service_name);
    }
    debug("service name: %s", service_name);
    if (server_acquire_creds(service_name, &server_creds) < 0)
        return -1;

    /* Here we do standalone operation and listen on a socket. */
    if (do_standalone) {
        alarm(0);
        if ((stmp = create_socket(port)) >= 0) {
            if (pid_path != NULL) {
                pid_file = fopen(pid_path, "w");
                if (pid_file == NULL)
                    sysdie("cannot create PID file %s", pid_path);
                fprintf(pid_file, "%ld\n", (long) getpid());
                fclose(pid_file);
            }
            do {
                /* Accept a TCP connection. */
                s = accept(stmp, NULL, 0);
                if (s < 0) {
                    syswarn("error accepting connection");
                    continue;
                }
                READFD = s;
                WRITEFD = s;
                if (process_connection(config, server_creds) < 0)
                    close(s);
            } while (1);
        }

    /* Here we read from stdin, and write to stdout to communicate with the
       network. */
    } else {
        READFD = 0;
        WRITEFD = 1;
        process_connection(config, server_creds);
        close(0);
        close(1);
    }

    /* Clean up and exit.  We only reach here in regular mode. */
    gss_release_cred(&min_stat, &server_creds);
    return 0;
}


/*
**  Local variables:
**  mode: c
**  c-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/
