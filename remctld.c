/*
   $Id$

   The deamon for a "K5 sysctl" - a service for remote execution of 
   predefined commands. Access is authenticated via GSSAPI Kerberos 5, 
   authorized via aclfiles. Runs as a inetd/tcpserver deamon or a standalone
   program.

   Written by Anton Ushakov <antonu@stanford.edu>
   Vector library contributed by Russ Allbery <rra@stanford.edu> 
   Copyright 2002 Board of Trustees, Leland Stanford Jr. University

*/

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>

#include <gssapi/gssapi_generic.h>
#include "gss-utils.h"
#include "vector.h"

int verbose;       /* Turns on debugging output. */
int use_syslog;    /* Toggle for sysctl vs stdout/stderr. */

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
int READFD = 0;
int WRITEFD = 1;

/* This is used for caching the conf file in memory after first reading it. */
struct confline {
    char *type;             /* Service type (ss in ss:create). */
    char *service;          /* Service name (create in ss:create). */
    char *program;          /* Full file name of executable. */
    char **acls;            /* Full file names of ACL files. */
};

/* Pointer to the global structure that will hold the parsed conf file. */
struct confline **confbuffer;

/* The environment */
extern char **environ; 

void
usage()
{
    static const char usage[] = "\
Usage: remctld <options>\n\
Options:\n\
\t-s service       K5 servicename to run as (default host/machine.stanford.edu)
\t-f conffile      the default is ./remctl.conf\n\
\t-v               debugging level of output\n\
\t-m               standalone single connection mode, meant for testing only\n\
\t-p port          only for standalone mode. default 4444\n";

    fprintf(stderr, usage);
    syslog(LOG_ERR, usage);
    exit(1);
}



/*
 * Function: create_socket
 *
 * Purpose: Opens a listening TCP socket. Only for Standalone Mode
 *
 * Arguments:
 *
 * 	port		(r) the port number on which to listen
 *
 * Returns: the listening socket file descriptor, or -1 on failure
 *
 * Effects:
 *
 * A listening socket on the specified port and created and returned.
 * On error, an error message is displayed and -1 is returned.
 */
int
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
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
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
 * Function: server_acquire_creds
 *
 * Purpose: imports a service name and acquires credentials for it
 *
 * Arguments:
 *
 * 	service_name	(r) the ASCII service name
 * 	server_creds	(w) the GSS-API service credentials
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * The service name is imported with gss_import_name, and service
 * credentials are acquired with gss_acquire_cred.  If either opertion
 * fails, an error message is displayed and -1 is returned; otherwise,
 * 0 is returned.
 */
int
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
 * Function: server_establish_context
 *
 * Purpose: establishses a GSS-API context as a specified service with
 * an incoming client, and returns the context handle and associated
 * client name
 *
 * Arguments:
 *
 * 	service_creds	(r) server credentials, from gss_acquire_cred
 *      context         (w) gssapi context to be established
 * 	client_name	(w) identity of the connecting client
 * 	ret_flags	(w) flags describing the connection, returned by gssapi
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Any valid client request is accepted.  If a context is established,
 * its handle is returned in "context", the type of the conncetion established 
 * is described in the "ret_flags",  and the client name is returned
 * in "client_name" and 0 is returned.  If unsuccessful, an error
 * message is displayed and -1 is returned.
 */
int
server_establish_context(gss_cred_id_t server_creds, gss_ctx_id_t* context, gss_buffer_t client_name, OM_uint32 *ret_flags)
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
 * Function: read_file
 *
 * Purpose: reads a file into a char buffer
 *
 * Returns: 0 on success, -2 on failure, on purpose, to distinguish with a 
 * -1 that could happen in a calling function
 * */
char*
read_file(char *file_name)
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

    if ((buf = smalloc(length + 1)) == 0) {
        syslog(LOG_ERR, "Couldn't allocate %d byte buffer for reading file\n",
               length);
        return NULL;
    }

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
 * Function: read_conf_file
 *
 * Purpose: reads the conf file and parses every line to populate a data
 *          structure that will be traversed on each request to translate a
 *          command type into an executable command filepath and aclfile.
 *
 * Arguments:
 *
 * 	filename	(r) conf file name
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * The confbuffer is a global, this read the conf file into that structure once
 * at startup of this process. It checks for empty and comment '#' lines in the
 * conf file and expects the fields to be space-separated. Struct confline
 * declaration on top of this file details the fields. 
 *
 * */
int
read_conf_file(char *filename)
{

    char** vlp;
    char *buf;
    int linenum, i, j;
    struct vector *vline;
    struct vector *vbuf;
    struct confline* cp;

    if ((buf = read_file(filename)) == NULL) {
        return -1;
    }

    vbuf = vector_new();
    vbuf = vector_split(buf, '\n', vbuf);
    if (vbuf->count == 0) {
            syslog(LOG_ERR, "Empty conf file\n");
            return -1;
    }

    /* How many real content lines are there. */
    linenum = 0;
    for (i=0; i < vbuf->count; i++) {
        vlp = vbuf->strings + i;
        if (*vlp[0] == '\0' || *vlp[0] == '#')
            continue;
        linenum++;
    }

    confbuffer = smalloc((linenum+1) * sizeof(struct confline*));
    confbuffer[linenum] = NULL;

    linenum = 0; /* Its value no longer needed, now it's a confline counter */

    /* Fill in the confbuffer */
    vline = vector_new();
    for (i=0; i < vbuf->count; i++) {
        vlp = vbuf->strings + i;
        if (*vlp[0] == '\0' || *vlp[0] == '#')
            continue;

        vline = vector_split_space(*vlp, vline);
        if (vline->count < 4){
            syslog(LOG_ERR, "Parse error in the conf file on line %d\n", i);
            return -1;
        }

        confbuffer[linenum] = smalloc(sizeof(struct confline));
        cp = confbuffer[linenum];
        cp->type    = strdup(vline->strings[0]);
        cp->service = strdup(vline->strings[1]);
        cp->program = strdup(vline->strings[2]);
        cp->acls    = smalloc((vline->count-2) * sizeof(char*));
        for (j=0; j < vline->count - 3; j++) {
            cp->acls[j] = strdup(vline->strings[j+3]);
        }
        cp->acls[j] = NULL;
        linenum++;
    }

    free(buf);
    vector_free(vbuf);
    vector_free(vline);
    return 0;

}


/*
 * Function: acl_check
 *
 * Purpose: checks of a given principal is in a given aclfile
 *
 * Arguments:
 *
 * 	userprincipal    (r) K5 principal in form   user@stanford.edu
 * 	settings	 (r) acl files are in this structure
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * Just sees if this userprincipal is one of the lines in the file. Ignores
 * empty lines and comments '#'
 *
 * */
int
acl_check(char *userprincipal, char **acls)
{

    char *buf;
    char *line;
    int authorized = 0;
    int i = 0;

    if (strcmp(acls[i], "ANYUSER") == 0)
        return 0;

    while (acls[i] != 0) {

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
 * Function: process_request
 *
 * Purpose: receives a request token payload and builds an argv stucture for it
 *
 * Arguments:
 *
 * 	context		(r) the established gssapi context
 * 	req_argc	(w) pointer to the number of arguments
 * 	req_argv	(w) pointer to the array of argument strings
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * Gets the packed message payload, unpacks it and proceeds to allocate and 
 * populate an argv structure for the parameters found in the message
 * */
struct vector*
process_request(gss_ctx_id_t context, char ret_message[])
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
    
    syslog(LOG_INFO, "argc is: %d\n", req_argc);
    if (req_argc <= 0 || req_argc > MAXCMDARGS) {
        strcpy (ret_message, "Invalid argc in request message\n");
        syslog(LOG_DEBUG, ret_message);
        return NULL;
    }
        
    req_argv = vector_new();
    vector_resize(req_argv, req_argc);

    /* Parse out the arguments and store them into a vector */
    /* Arguments are packed: (<arglength><argument>)+       */
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
 * Function: process_response
 *
 * Purpose: send back the response to the clientr, containg the result message
 *
 * Arguments:
 *
 * 	context		(r) the established gssapi context
 * 	code		(r) the return code from running the command
 * 	blob		(r) the output message gathered from the command
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * Packs the return message payload with it's length in front of it, in case of
 * error also packing in the error code and designating the error status with 
 * a token flag. Calls a utility function gss_sendmsg to send the token.
 * */
int
process_response(gss_ctx_id_t context, OM_uint32 code, char *blob)
{
    OM_uint32 bloblength;
    OM_uint32 msglength;
    OM_uint32 network_order;
    char *msg;
    OM_uint32 flags;
    bloblength = strlen(blob);

    flags = TOKEN_DATA;

    msg = smalloc((2 * sizeof(OM_uint32) + bloblength));
    msglength = bloblength + 2 * sizeof(OM_uint32);

    network_order = htonl(code);
    memcpy(msg, &code, sizeof(OM_uint32));
    network_order = htonl(bloblength);
    memcpy(msg + sizeof(int), &network_order, sizeof(OM_uint32));
    memcpy(msg + 2 * sizeof(OM_uint32), blob, bloblength);
    
    if (gss_sendmsg(context, flags, msg, msglength) < 0)
        return -1;

    free(msg);

    return 0;
}



/*
 * Function: process_command
 *
 * Purpose: the function that determines the executable for this request,
 * checks the aclfiles and forks off to execute the commend
 *
 * Arguments:
 *
 *      argvector     (r) the arguments vector
 *      userprincipal (r) the identify of the user, making the request
 *      ret_message   (w) used to return the collected output message from
 *                        the executable
 *
 * Returns: 0 on success, negative integer on failure
 *
 * Effects:
 *
 * Using the "type" and the "service" - the following argument - a lookup in
 * the conf data structure is done to find the command executable and acl file.
 * If the conf file, and subsequently the conf data structure contains an entry
 * for this type, with service equal to "all" that is a wildcard match for any
 * given service. The first argument is then replaced with the actual program
 * name to be executed.
 *
 * After checking the acl permissions, the process forks and the child execv's
 * the said command with pipes arranged to gather output. The parent waits for
 * the return code and gathers stdout and stderr pipes. 
*/

OM_uint32
process_command(struct vector *argvector, char *userprincipal, char ret_message[])
{
    char *command;
    char *program;
    char **acls;
    struct confline* cline;
    int stdout_pipe[2];
    int stderr_pipe[2];
    char **req_argv;
    char remuser[100];
    int ret_length;
    char err_message[MAXBUFFER];
    OM_uint32 ret_code;
    int i, pid, pipebuffer;

    /* This block logs the requested command. */
    command = vector_join(argvector, " ");
    strcpy(ret_message, "COMMAND: ");
    strncat(ret_message, command, MAXBUFFER - strlen(ret_message));
    strcat(ret_message, "\n");
    syslog(LOG_INFO,ret_message);

    memset(ret_message, 0, MAXBUFFER);
    memset(err_message, 0, MAXBUFFER);
    /* This is how much is read from each pipe - stderr and stdout. The total
       size of what we send back has to be not more than MAXBUFFER */
    pipebuffer = MAXBUFFER/2;

    command = NULL;
    req_argv = NULL;

    /* Lookup the command and the aclfile from the conf file structure in
       memory. */
    
    for (i=0; confbuffer[i] != NULL; i++) {
        cline = confbuffer[i];
        if (strcmp(cline->type, argvector->strings[0]) == 0) {
            if (strcmp(cline->service, "ALL") == 0 ||
                strcmp(cline->service, argvector->strings[1]) == 0) {
                    command = cline->program;
                    acls    = cline->acls;
                    break;
            }
        }
    }


    /* Check the command, aclfile, and the authorization of this client to
       run this command. */

    if (command == NULL) {
        ret_code = -1;
        strcpy(ret_message, "Command not defined");
    } else {
        if ((ret_code = acl_check(userprincipal, acls)) != 0)
            snprintf(ret_message, MAXBUFFER,
                    "Access denied: %s principal not on the acl list",
                    userprincipal);
    }


    /* Assemble the argv, envp, and fork off the child to run the command. */
    if (ret_code == 0) {


        /* ARGV: */
        req_argv = smalloc((argvector->count + 1) * sizeof(char*));

        /* Get the real program name, and use it as the first
           argument in argv passed to the command. */
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
            syslog(LOG_ERR, ret_message);
            return -1;
        }


        switch (pid = fork()) {
        case -1:
            strcpy(ret_message, "Can't fork\n");
            syslog(LOG_ERR, ret_message);
            return -1;

        case 0:                /* This is the code the child runs. */

            /* Close the Child process' STDOUT read end */
            dup2(stdout_pipe[1], 1);
            close(stdout_pipe[0]);

            /* Close the Child process' STDERR read end */
            dup2(stderr_pipe[1], 2);
            close(stderr_pipe[0]);

            /* Child doesn't need STDIN at all */
            close(0);

            /* Tell the exec'ed program who requested it */
            sprintf(remuser, "REMUSER=%s", userprincipal);
            if (putenv(remuser) < 0) {
                strcpy(ret_message, 
                       "Cant's set REMUSER environment variable \n");
                syslog(LOG_ERR, "%s%m", ret_message);
                fprintf(stderr, ret_message);
                exit(-1);   
            }

            /* Backward compat */
            sprintf(remuser, "SCPRINCIPAL=%s", userprincipal);
            if (putenv(remuser) < 0) {
                strcpy(ret_message, 
                       "Cant's set SCPRINCIPAL environment variable \n");
                syslog(LOG_ERR, "%s%m", ret_message);
                fprintf(stderr, ret_message);
                exit(-1);   
            }

            execv(command, req_argv);

            /* This here happens only if the exec failed. In that case this
               passed the errno information back up the stderr pipe to the
               parent, and dies */
            strcpy(ret_message, strerror(errno));
            fprintf(stderr, ret_message);

            exit(-1);
        default:               /* This is the code the parent runs. */

            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            /* Unblock the read ends of the pipes, to enable us to read from
               both iteratevely */
            fcntl(stdout_pipe[0], F_SETFL, 
                  fcntl(stdout_pipe[0], F_GETFL) | O_NONBLOCK);
            fcntl(stderr_pipe[0], F_SETFL, 
                  fcntl(stderr_pipe[0], F_GETFL) | O_NONBLOCK);

            /* This collects output from both pipes iteratively, while the
               child is executing */
            if ((ret_length = read_two(stdout_pipe[0], stderr_pipe[0], 
                                       ret_message, err_message, 
                                       pipebuffer, pipebuffer)) < 0)
                ret_message = "No output from the command\n";

            /* Both streams could have useful info. */
            strcat(ret_message, err_message);
            ret_length = strlen(ret_message);

            waitpid(pid, &ret_code, 0);

            /* This does the crazy >>8 thing to get the real error code. */
            if (WIFEXITED(ret_code)) {
                ret_code = (signed)WEXITSTATUS(ret_code);
            }
        }
    }

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
 * Function: process_connection
 *
 * Purpose: the main fucntion that processes the entire interaction with one
 *          client
 *
 * Arguments:
 *
 * 	server_creds	(r) gssapi server credentials
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * This is the main line of control for a particular client interaction. First 
 * establishes a context with this client. 
 *
 * Calls process_request to get the argv from the client. The first string in 
 * argv is expected to be a "type" which determines a category of commands - 
 * often points to a particular command executable. 
 *
 * Calls process_command to figure out the executable for this request, and
 * check the pertaining aclfiles. The command is executed and the response
 * message and return code are returned. 
 *
 * It then calls process_response with the message generated by process_command
 * cleans up the context.
 * 
 * */
int
process_connection(gss_cred_id_t server_creds)
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
    userprincipal = smalloc(client_name.length + 1);
    memcpy(userprincipal, client_name.value, client_name.length);
    userprincipal[client_name.length] = '\0';

    syslog(LOG_INFO, "Accepted connection from \"%.*s\"\n",
           (int)client_name.length, (char *)client_name.value);
    
    gss_release_buffer(&min_stat, &client_name);


    /* Interchange to get the data that makes up the request - basically an
       argv. */
    if ((req_argv = process_request(context, ret_message)) == NULL)
        ret_code = -1;

    /* Check the acl, the existence of the command, run the command and
       gather the output. */
    if (ret_code == 0)
        ret_code = process_command(req_argv, userprincipal, ret_message);

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



/* The main serves to gather arguments and determine the standalone vs inetd
   mode. It also calls to set up a tcp connection on standalone mode; the 
   transaction handling is then handed off to process_connection.
*/

int
main(int argc, char **argv)
{
    char service_name[500];
    char host_name[500];
    struct hostent* hostinfo;
    gss_cred_id_t server_creds;
    OM_uint32 min_stat;
    unsigned short port = 4444;
    int s, stmp;
    int do_standalone = 0;
    char *conffile = "remctl.conf";

    service_name[0] = '\0';
    verbose = 0;
    use_syslog = 1;

    /* Since we are called from tcpserver, prevent clients from holding on to 
       us forever, and die after an hour. */
    alarm(60 * 60);
    openlog("remctld", LOG_PID | LOG_NDELAY, LOG_DAEMON);

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
            verbose = 1;
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

    if (read_conf_file(conffile) != 0) {
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
        if ((stmp = create_socket(port)) >= 0) {
            do {
                /* Accept a TCP connection. */
                if ((s = accept(stmp, NULL, 0)) < 0) {
                    perror("accepting connection");
                    continue;
                }

                READFD = s;
                WRITEFD = s;

                if (process_connection(server_creds) < 0)
                    close(s);
            } while (1);
        }
    } else { /* Here we read from stdin, and write to stdout to communicate 
                with the network. */

        READFD = 0;
        WRITEFD = 1;
        process_connection(server_creds);
        close(0);
        close(1);
    }

    gss_release_cred(&min_stat, &server_creds);

    return 0;
}

/*
** Local variables:
** mode: c
** c-basic-offset: 4
** indent-tabs-mode: nil
** end:
*/
