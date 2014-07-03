/*
 * Internal support functions for the remctld daemon.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2007, 2008, 2009, 2010, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H 1

#include <config.h>
#include <portable/gssapi.h>
#include <portable/macros.h>
#include <portable/socket.h>
#include <portable/stdbool.h>

#include <sys/types.h>

#include <util/protocol.h>

/* Forward declarations to avoid extra includes. */
struct bufferevent;
struct evbuffer;
struct event;
struct event_base;
struct iovec;

/*
 * The maximum size of argc passed to the server (4K arguments), and the
 * maximum size of a single command including all of its arguments (100MB).
 * These are arbitrary limit to protect against memory-based denial of service
 * attacks on the server.
 */
#define COMMAND_MAX_ARGS (4 * 1024)
#define COMMAND_MAX_DATA (100UL * 1024 * 1024)

/*
 * The timeout.  We won't wait for longer than this number of seconds for more
 * data from the client.  This needs to be configurable.
 */
#define TIMEOUT (60 * 60)

/* Holds the information about a client connection. */
struct client {
    int fd;                     /* File descriptor of client connection. */
    char *hostname;             /* Hostname of client (if available). */
    char *ipaddress;            /* IP address of client as a string. */
    int protocol;               /* Protocol version number. */
    gss_ctx_id_t context;       /* GSS-API context. */
    char *user;                 /* Name of the client as a string. */
    OM_uint32 flags;            /* Connection flags. */
    bool keepalive;             /* Whether keep-alive was set. */
    bool fatal;                 /* Whether a fatal error has occurred. */
};

/* Holds the configuration for a single command. */
struct rule {
    char *file;                 /* Config file name. */
    int lineno;                 /* Config file line number. */
    struct vector *line;        /* The split configuration line. */
    char *command;              /* Command (first argument). */
    char *subcommand;           /* Subcommand (second argument). */
    char *program;              /* Full file name of executable. */
    unsigned int *logmask;      /* Zero-terminated list of args to mask. */
    long stdin_arg;             /* Arg to pass on stdin, -1 for last. */
    char *user;                 /* Run executable as user. */
    uid_t uid;                  /* Run executable with this UID. */
    gid_t gid;                  /* Run executable with this GID. */
    char *summary;              /* Argument that gives a command summary. */
    char *help;                 /* Argument that gives help for a command. */
    char **acls;                /* Full file names of ACL files. */
};

/* Holds the complete parsed configuration for remctld. */
struct config {
    struct rule **rules;
    size_t count;
    size_t allocated;
};

/*
 * Holds details about a running process.  The events we hook into the event
 * loop are also stored here so that the event handlers can use this as their
 * data and have their pointers so that they can remove themselves when
 * needed.
 */
struct process {
    struct client *client;      /* Pointer to corresponding remctl client. */

    /* Command input. */
    char *command;              /* The remctl command run by the user. */
    char **argv;                /* argv for running the command. */
    struct rule *rule;          /* Configuration rule for the command. */
    struct evbuffer *input;     /* Buffer of input to process. */

    /* Command output. */
    struct evbuffer *output;    /* Buffer of output from process. */
    int status;                 /* Exit status. */

    /* Everything below this point is used internally by the process loop. */

    /* Process data. */
    socket_type stdinout_fd;    /* File descriptor for input and output. */
    socket_type stderr_fd;      /* File descriptor for standard error. */
    pid_t pid;                  /* Process ID of child. */

    /* Event loop. */
    struct event_base *loop;    /* Event base for the process event loop. */
    struct bufferevent *inout;  /* Input and output from process. */
    struct bufferevent *err;    /* Standard error from process. */
    struct event *sigchld;      /* Handle the SIGCHLD signal for exit. */

    /* State flags. */
    bool reaped;                /* Whether we've reaped the process. */
    bool saw_error;             /* Whether we encountered some error. */
    bool saw_output;            /* Whether we saw process output. */
};

BEGIN_DECLS

/* Logging functions. */
void warn_gssapi(const char *, OM_uint32 major, OM_uint32 minor);
void warn_token(const char *, int status, OM_uint32 major, OM_uint32 minor);
void server_log_command(struct iovec **, struct rule *, const char *user);

/* Configuration file functions. */
struct config *server_config_load(const char *file);
void server_config_free(struct config *);
bool server_config_acl_permit(const struct rule *, const char *user);
void server_config_set_gput_file(char *file);

/* Running commands. */
void server_run_command(struct client *, struct config *, struct iovec **);

/* Freeing the command structure. */
void server_free_command(struct iovec **);

/* Running processes. */
bool server_process_run(struct process *process);

/* Generic protocol functions. */
struct client *server_new_client(int fd, gss_cred_id_t creds);
void server_free_client(struct client *);
struct iovec **server_parse_command(struct client *, const char *, size_t);
bool server_send_error(struct client *, enum error_codes, const char *);

/* Protocol v1 functions. */
bool server_v1_send_output(struct client *, struct evbuffer *, int status);
void server_v1_handle_messages(struct client *, struct config *);

/* Protocol v2 functions. */
bool server_v2_send_output(struct client *, int stream, struct evbuffer *);
bool server_v2_send_status(struct client *, int);
bool server_v2_send_error(struct client *, enum error_codes, const char *);
void server_v2_handle_messages(struct client *, struct config *);

END_DECLS

#endif /* !SERVER_INTERNAL_H */
