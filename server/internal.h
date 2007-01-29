/*  $Id$
**
**  Internal support functions for the remctld daemon.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Copyright 2006 Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H 1

#include <config.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <util/util.h>

/* BEGIN_DECLS is used at the beginning of declarations so that C++
   compilers don't mangle their names.  END_DECLS is used at the end. */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
# define BEGIN_DECLS    extern "C" {
# define END_DECLS      }
#else
# define BEGIN_DECLS    /* empty */
# define END_DECLS      /* empty */
#endif

/* Used as the default max buffer for the argv passed into the server, and for 
   the return message from the server. */
#define MAXBUFFER       64000  

/* The maximum size of argc passed to the server.  This is an arbitrary limit
   to protect against memory-based denial of service attacks on the server. */
#define MAXCMDARGS      (4 * 1024)

BEGIN_DECLS

/* Holds the information about a client connection. */
struct client {
    int fd;                     /* File descriptor of client connection. */
    char *hostname;             /* Hostname of client (if available). */
    char *ipaddress;            /* IP address of client as a string. */
    int protocol;               /* Protocol version number. */
    gss_ctx_id_t context;       /* GSS-API context. */
    char *user;                 /* Name of the client as a string. */
    OM_uint32 flags;            /* Connection flags. */
    int keepalive;              /* Whether keep-alive was set. */
    char *output;               /* Stores output to send to the client. */
    size_t outlen;              /* Length of output to send to client. */
    int fatal;                  /* Whether a fatal error has occurred. */
};

/* Holds the configuration for a single command. */
struct confline {
    struct vector *line;        /* The split configuration line. */
    char *type;                 /* Service type. */
    char *service;              /* Service name. */
    char *program;              /* Full file name of executable. */
    struct cvector *logmask;    /* What args to mask in the log, if any. */
    char **acls;                /* Full file names of ACL files. */
};

/* Holds the complete parsed configuration for remctld. */
struct config {
    struct confline **rules;
    size_t count;
    size_t allocated;
};

/* Logging functions. */
void warn_gssapi(const char *, OM_uint32 major, OM_uint32 minor);
void warn_token(const char *, int status, OM_uint32 major, OM_uint32 minor);
void server_log_command(struct vector *, struct confline *, const char *user);

/* Configuration file functions. */
struct config *server_config_load(const char *file);
int server_config_acl_permit(struct confline *, const char *user);

/* Running commands. */
void server_run_command(struct client *, struct config *, struct vector *);

/* Generic protocol functions. */
struct client *server_new_client(int fd, gss_cred_id_t creds);
void server_free_client(struct client *);
struct vector *server_parse_command(struct client *, const char *, size_t);
int server_send_error(struct client *, enum error_codes, const char *);

/* Protocol v1 functions. */
int server_v1_send_output(struct client *, int status);
void server_v1_handle_commands(struct client *, struct config *);

/* Protocol v2 functions. */
int server_v2_send_output(struct client *, int stream);
int server_v2_send_status(struct client *, int);
int server_v2_send_error(struct client *, enum error_codes, const char *);
void server_v2_handle_commands(struct client *, struct config *);

END_DECLS

#endif /* !SERVER_INTERNAL_H */
