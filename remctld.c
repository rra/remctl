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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <gssapi/gssapi_generic.h>

#include "gss-utils.h"
#include "messages.h"
#include "vector.h"
#include "xmalloc.h"

int verbose;       /* Turns on debugging output. */
int use_syslog;    /* Toggle for sysctl vs stdout/stderr. */

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
usage(void)
{
    static const char usage[] = "\
Usage: remctld <options>\n\
Options:\n\
\t-s service       K5 servicename to run as (default host/machine.stanford.edu)\
\t-f conffile      the default is ./remctl.conf\n\
\t-v               debugging level of output\n\
\t-m               standalone single connection mode, meant for testing only\n\
\t-p port          only for standalone mode. default 4444\n";

    fprintf(stderr, usage);
    syslog(LOG_ERR, "invalid usage");
    exit(1);
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

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("creating socket");
        return -1;
    }
    /* Let the socket be reused right away */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        perror("binding socket");
        close(s);
        return -1;
    }
    if (listen(s, 5) < 0) {
        perror("listening on socket");
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

    maj_stat = gss_import_name(&min_stat, &name_buf,
                               (gss_OID) gss_nt_user_name, &server_name);

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
    gss_buffer_desc oid_name;
    int token_flags;

    if (recv_token(&token_flags, &recv_tok) < 0)
        return -1;

    gss_release_buffer(&min_stat, &recv_tok);
    if (!(token_flags & TOKEN_NOOP)) {
        syslog(LOG_ERR, "Expected NOOP token, got %d token instead\n",
               token_flags);
        return -1;
    }

    *context = GSS_C_NO_CONTEXT;

    if (!(token_flags & TOKEN_CONTEXT_NEXT)) {
        client_name->length = *ret_flags = 0;
        syslog(LOG_DEBUG,"Accepted unauthenticated connection.\n");
    } else {
        do {
            if (recv_token(&token_flags, &recv_tok) < 0)
                return -1;

            if (verbose) {
                syslog(LOG_DEBUG, "Received token (size=%d): \n", 
                       recv_tok.length);
                print_token(&recv_tok);
            }

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
                if (verbose) {
                    syslog(LOG_DEBUG,
                           "Sending accept_sec_context token (size=%d):\n",
                           send_tok.length);
                    print_token(&send_tok);
                }
                if (send_token(TOKEN_CONTEXT, &send_tok) < 0) {
                    syslog(LOG_ERR, "failure sending token\n");
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

            if (verbose) {
                if (maj_stat == GSS_S_CONTINUE_NEEDED)
                    syslog(LOG_DEBUG,"continue needed...\n");
                else
                    syslog(LOG_DEBUG,"\n");
            }
        } while (maj_stat == GSS_S_CONTINUE_NEEDED);


        if (verbose) {
            display_ctx_flags(*ret_flags);

            maj_stat = gss_oid_to_str(&min_stat, doid, &oid_name);
            if (maj_stat != GSS_S_COMPLETE) {
                display_status("while converting oid->string", maj_stat,
                               min_stat);
                return -1;
            }
            syslog(LOG_DEBUG,"Accepted connection using mechanism OID %.*s.\n",
                   (int)oid_name.length, (char *)oid_name.value);
            gss_release_buffer(&min_stat, &oid_name);
        }

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
**  Reads a file into a character buffer, returning the newly allocated buffer
**  or NULL on failure.
*/
static char *
read_file(const char *file_name)
{
    int fd, count;
    struct stat stat_buf;
    int length;
    char* buf;

    if ((fd = open(file_name, O_RDONLY, 0)) < 0) {
        syslog(LOG_ERR, "Can't open file %s: %m\n", file_name);
        return NULL;
    }
    if (fstat(fd, &stat_buf) < 0) {
        return NULL;
    }
    length = stat_buf.st_size;

    if (length == 0) {
        return NULL;
    }

    buf = xmalloc(length + 1);
    count = read(fd, buf, length);
    if (count < 0) {
        syslog(LOG_ERR, "Problem during read in read_file\n");
        return NULL;
    }
    if (count < length) {
        syslog(LOG_ERR, "Error: only read in %d bytes, expected %d\n",
               count, length);
        return NULL;
    }

    buf[length] = '\0';
    close(fd);

    return buf;
}


/*
**  Reads the configuration file and parses every line, populating a data
**  structure that will be traversed on each request to translate a command
**  type into an executable path and ACL file.
**
**  config is populated with the parsed configuration file.  Empty lines and
**  lines beginning with # are ignored.  Each line is divided into fields,
**  separated by spaces.  The fields are defined by struct confline.  Lines
**  ending in backslash are continued on the next line.
**
**  As a special case, include <file> will call read_conf_file recursively to
**  parse an included file (or, if <file> is a directory, every file in that
**  directory that doesn't contain a period).
**
**  Returns 0 on success and -1 on error, reporting an error message.
*/
static int
read_conf_file(struct config *config, const char *name)
{
    FILE *file;
    char *buffer, *p;
    size_t bufsize, length, size, count, i, arg_i;
    struct vector *line;
    struct confline *confline;
    size_t lineno = 0;

    bufsize = 1024;
    buffer = xmalloc(bufsize);
    file = fopen(name, "r");
    if (file == NULL) {
        free(buffer);
        syslog(LOG_ERR, "cannot open config file %s: %m", name);
        return -1;
    }
    while (fgets(buffer, bufsize, file) != NULL) {
        length = strlen(buffer);

        /* Allow for long lines and continuation lines.  As long as we've
           either filled the buffer or have a line ending in a backslash, we
           keep reading more data.  If we filled the buffer, increase it by
           another 1KB; otherwise, back up and write over the backslash and
           newline. */
        while (length > 2 && (buffer[length - 1] != '\n'
                              || buffer[length - 2] == '\\')) {
            if (buffer[length - 1] != '\n') {
                bufsize += 1024;
                buffer = xrealloc(buffer, bufsize);
            } else {
                length -= 2;
            }
            if (fgets(buffer + length, bufsize - length, file) == NULL)
                goto done;
            length = strlen(buffer);
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
            const char *included = line->strings[1];
            struct stat st;

            if (line->count != 2 || strcmp(line->strings[0], "include") != 0) {
                syslog(LOG_ERR, "%s:%d: config parse error", name, lineno);
                vector_free(line);
                free(buffer);
                fclose(file);
                return -1;
            }
            if (strcmp(included, name) == 0) {
                syslog(LOG_ERR, "%s:%d: %s recursively included", name,
                       lineno, name);
                vector_free(line);
                free(buffer);
                fclose(file);
                return -1;
            }
            if (stat(included, &st) < 0) {
                syslog(LOG_ERR, "%s:%d: included file %s not found", name,
                       lineno, included);
                vector_free(line);
                free(buffer);
                fclose(file);
                return -1;
            }
            if (S_ISDIR(st.st_mode)) {
                DIR *dir;
                struct dirent *entry;

                dir = opendir(included);
                while ((entry = readdir(dir)) != NULL) {
                    char *path;

                    if (strchr(entry->d_name, '.') != NULL)
                        continue;
                    length = strlen(included) + 1 + strlen(entry->d_name) + 1;
                    path = xmalloc(length);
                    snprintf(path, length, "%s/%s", included, entry->d_name);
                    if (read_conf_file(config, path) < 0) {
                        closedir(dir);
                        free(path);
                        free(buffer);
                        fclose(file);
                        return -1;
                    }
                    free(path);
                }
                closedir(dir);
                vector_free(line);
            } else {
                if (read_conf_file(config, included) < 0) {
                    vector_free(line);
                    fclose(file);
                    free(buffer);
                    return -1;
                }
                vector_free(line);
            }
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
        confline = xmalloc(sizeof(struct confline));
        memset(confline, 0, sizeof(struct confline));
        config->rules[config->count] = confline;
        config->count++;
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
            syslog(LOG_ERR, "%s:%d: config parse error", name, lineno);
            fclose(file);
            free(buffer);
            return -1;
        }

        /* Grab the list of ACL files. */
        count = line->count - arg_i + 1;
        confline->acls = xmalloc(count * sizeof(char *));
        for (i = 0; i < line->count - arg_i; i++)
            confline->acls[i] = line->strings[i + arg_i];
        confline->acls[i] = NULL;
    }

    /* Free allocated memory and return success if fgets succeeded. */
done:
    free(buffer);
    if (ferror(file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}


/*
**  Check to see if a given principal is in a given list of ACL files.  The
**  list of ACL files should be NULL-terminated.  Empty lines and lines
**  beginning with # in the ACL file are ignored.
**
**  Returns 0 on success and -1 on failure.
*/
static int
acl_check(char *userprincipal, char **acls)
{
    char *buf;
    char *line;
    int authorized = 0;
    int i = 0;

    if (strcmp(acls[i], "ANYUSER") == 0)
        return 0;

    while (acls[i] != NULL) {
        if ((buf = read_file(acls[i])) == NULL)
            return -1;

        for (line = strtok(buf, "\n"); line != NULL; line = strtok(NULL, "\n")){
            if (*line == '\0' || *line == '#')
                continue;
            if (strcmp(line, userprincipal) == 0) {
                authorized = 1;
                break;
            }
        }
        
        free(buf);

        if (authorized)
            return 0;
        else
            i++;
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

    if (verbose)
        syslog(LOG_DEBUG, "argc is: %d\n", req_argc);
    if (req_argc <= 0 || req_argc > MAXCMDARGS) {
        strcpy(ret_message, "Invalid argc in request message\n");
        syslog(LOG_DEBUG, ret_message);
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
            strcpy (ret_message, 
                    "Data unpacking error in getting arguments\n");
            syslog(LOG_DEBUG, ret_message);
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
    char log_message[MAXBUFFER];
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

    /* This block logs the requested command. */
    snprintf(log_message, MAXBUFFER, "COMMAND from %s: ", userprincipal);
    strncat(log_message, command, MAXBUFFER - strlen(log_message));
    strcat(log_message, "\n");
    syslog(LOG_INFO, "%s", log_message);
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
    OM_uint32 ret_code;
    int pid, pipebuffer;
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
        syslog(LOG_ERR, "%s", ret_message);
        ret_code = -1;
        goto done;
    }

    switch (pid = fork()) {
    case -1:
        strcpy(ret_message, "Can't fork\n");
        syslog(LOG_ERR, "%s", ret_message);
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
            syslog(LOG_ERR, "%s%m", ret_message);
            fprintf(stderr, ret_message);
            exit(-1);
        }

        /* Backward compat */
        snprintf(scprincipal, MAXBUFFER, "SCPRINCIPAL=%s", userprincipal);
        if (putenv(scprincipal) < 0) {
            strcpy(ret_message,
                   "Can't set SCPRINCIPAL environment variable\n");
            syslog(LOG_ERR, "%s%m", ret_message);
            fprintf(stderr, ret_message);
            exit(-1);   
        }

        execv(command, req_argv);

        /* This here happens only if the exec failed. In that case this passed
           the errno information back up the stderr pipe to the parent, and
           dies. */
        strcpy(ret_message, strerror(errno));
        fprintf(stderr, "%s\n", ret_message);
        exit(-1);

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
        if (WIFEXITED(ret_code)) {
            ret_code = (signed int) WEXITSTATUS(ret_code);
        }
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
    int ret_flags;
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
        syslog(LOG_ERR, "Unauthenticated connection.\n");
        return -1;
    }

    /* This is who just connected to us. */
    userprincipal = xmalloc(client_name.length + 1);
    memcpy(userprincipal, client_name.value, client_name.length);
    userprincipal[client_name.length] = '\0';

    if (verbose)
        syslog(LOG_DEBUG, "Accepted connection from \"%.*s\"\n",
               (int)client_name.length, (char *)client_name.value);
    
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
    char service_name[500];
    char host_name[500];
    struct hostent* hostinfo;
    gss_cred_id_t server_creds;
    OM_uint32 min_stat;
    unsigned short port = 4444;
    int s, stmp;
    int do_standalone = 0;
    const char *conffile = CONFIG_FILE;
    struct config *config;

    service_name[0] = '\0';
    verbose = 0;
    use_syslog = 1;

    /* Since we are called from tcpserver, prevent clients from holding on to 
       us forever, and die after an hour. */
    alarm(60 * 60);

    /* Establish identity and set up logging. */
    message_program_name = "remctld";
    openlog("remctld", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    message_handlers_notice(1, message_log_syslog_info);
    message_handlers_warn(1, message_log_syslog_warning);
    message_handlers_die(1, message_log_syslog_err);

    argc--;
    argv++;
    while (argc) {
        if (strcmp(*argv, "-p") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            port = atoi(*argv);
        } else if (strcmp(*argv, "-v") == 0) {
            message_handlers_debug(1, message_log_syslog_debug);
        } else if (strcmp(*argv, "-f")    == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            conffile = *argv;
        } else if (strcmp(*argv, "-s")    == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            strncpy(service_name, *argv, sizeof(service_name));
        } else if (strcmp(*argv, "-m")   == 0) {
            do_standalone = 1;
        } else
            break;
        argc--;
        argv++;
    }

    config = xmalloc(sizeof(struct config));
    if (read_conf_file(config, conffile) != 0) {
        syslog(LOG_ERR, "%s%m\n", "Can't read conf file: ");
        exit(1);
    }

    if (strlen(service_name) == 0) {
        if (gethostname(host_name, sizeof(host_name)) < 0) {
            syslog(LOG_ERR, "%s%m\n", "Can't get hostname: ");
            exit(1);
        }
        
        if ((hostinfo = gethostbyname(host_name)) == NULL) {
            syslog(LOG_ERR, "%s%s %m\n", "Can't resolve given hostname: ", 
                   host_name);
            exit(1);
        }

        /* service_name is for example host/zver.stanford.edu */
        lowercase(hostinfo->h_name);
        strcpy(service_name, "host/");
        strcat(service_name, hostinfo->h_name);
    }

    if (verbose)
        syslog(LOG_DEBUG, "service_name: %s\n", service_name);

    /* This specifies the service name, checks out the keytab, etc. */
    if (server_acquire_creds(service_name, &server_creds) < 0)
        return -1;

    /* Here we do standalone operation and listen on a socket. */
    if (do_standalone) {
        alarm(0);
        if ((stmp = create_socket(port)) >= 0) {
            do {
                /* Accept a TCP connection. */
                if ((s = accept(stmp, NULL, 0)) < 0) {
                    perror("accepting connection");
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
