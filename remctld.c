/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <gssapi/gssapi_generic.h>
#include "gss-utils.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

FILE *log;

int verbose = 0;
int INETD = 0;
int READFD = 0;
int WRITEFD = 1;

typedef struct confline {
  char* line;
  char* elem[4];
} confline; 
confline* confbuffer;
int confsize;


void usage()
{
     fprintf(stderr, "Usage: remctl [-port port] [-verbose] \n");
     fprintf(stderr, "       [-logfile file] [service_name]\n");
     exit(1);
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
int server_acquire_creds(service_name, server_creds)
     char *service_name;
     gss_cred_id_t *server_creds;
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

     (void) gss_release_name(&min_stat, &server_name);

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
 * 	s		(r) an established TCP connection to the client
 * 	service_creds	(r) server credentials, from gss_acquire_cred
 * 	context		(w) the established GSS-API context
 * 	client_name	(w) the client's ASCII name
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Any valid client request is accepted.  If a context is established,
 * its handle is returned in context and the client name is returned
 * in client_name and 0 is returned.  If unsuccessful, an error
 * message is displayed and -1 is returned.
 */
int server_establish_context(server_creds, context, client_name, ret_flags)
     gss_cred_id_t server_creds;
     gss_ctx_id_t *context;
     gss_buffer_t client_name;
     OM_uint32 *ret_flags;
{
     gss_buffer_desc send_tok, recv_tok;
     gss_name_t client;
     gss_OID doid;
     OM_uint32 maj_stat, min_stat, acc_sec_min_stat;
     gss_buffer_desc	oid_name;
     int token_flags;

     if (recv_token(&token_flags, &recv_tok) < 0)
       return -1;

     (void) gss_release_buffer(&min_stat, &recv_tok);
     if (! (token_flags & TOKEN_NOOP)) {
       if (log)
	 fprintf(log, "Expected NOOP token, got %d token instead\n",
		 token_flags);
       return -1;
     }

     *context = GSS_C_NO_CONTEXT;

     if (token_flags & TOKEN_CONTEXT_NEXT) {
       do {
	 if (recv_token(&token_flags, &recv_tok) < 0)
	   return -1;

	 if (verbose && log) {
	   fprintf(log, "Received token (size=%d): \n", recv_tok.length);
	   print_token(&recv_tok);
	 }

	 maj_stat =
	   gss_accept_sec_context(&acc_sec_min_stat,
				  context,
				  server_creds,
				  &recv_tok,
				  GSS_C_NO_CHANNEL_BINDINGS,
				  &client,
				  &doid,
				  &send_tok,
				  ret_flags,
				  NULL, 	/* ignore time_rec */
				  NULL); 	/* ignore del_cred_handle */

	 (void) gss_release_buffer(&min_stat, &recv_tok);

	 if (send_tok.length != 0) {
	   if (verbose && log) {
	     fprintf(log,
		     "Sending accept_sec_context token (size=%d):\n",
		     send_tok.length);
	     print_token(&send_tok);
	   }
	   if (send_token(TOKEN_CONTEXT, &send_tok) < 0) {
	     if (log)
	       fprintf(log, "failure sending token\n");
	     return -1;
	   }

	   (void) gss_release_buffer(&min_stat, &send_tok);
	 }
	 if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
	      display_status("while accepting context", maj_stat,
			     acc_sec_min_stat);
	      if (*context == GSS_C_NO_CONTEXT)
		      gss_delete_sec_context(&min_stat, context,
					     GSS_C_NO_BUFFER);
	      return -1;
	 }
 
	 if (verbose && log) {
	   if (maj_stat == GSS_S_CONTINUE_NEEDED)
	     fprintf(log, "continue needed...\n");
	   else
	     fprintf(log, "\n");
	   fflush(log);
	 }
       } while (maj_stat == GSS_S_CONTINUE_NEEDED);


       if (verbose && log) {
	 display_ctx_flags(*ret_flags);

	 maj_stat = gss_oid_to_str(&min_stat, doid, &oid_name);
	 if (maj_stat != GSS_S_COMPLETE) {
	   display_status("while converting oid->string", maj_stat, min_stat);
	   return -1;
	 }
	 fprintf(log, "Accepted connection using mechanism OID %.*s.\n",
		 (int) oid_name.length, (char *) oid_name.value);
	 (void) gss_release_buffer(&min_stat, &oid_name);
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
     else {
       client_name->length = *ret_flags = 0;

       if (log)
	 fprintf(log, "Accepted unauthenticated connection.\n");
     }

     return 0;
}

/*
 * Function: create_socket
 *
 * Purpose: Opens a listening TCP socket.
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
int create_socket(port)
     u_short port;
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
     (void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
     if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
	  perror("binding socket");
	  (void) close(s);
	  return -1;
     }
     if (listen(s, 5) < 0) {
	  perror("listening on socket");
	  (void) close(s);
	  return -1;
     }
     return s;
}



int process_response(context, code, blob, bloblength)
     gss_ctx_id_t context;
     int code;
     char* blob;
     int bloblength;
{
  int msglength;
  char* msg;
  int flags;

  if (code == 0) { /* positive response */

    msg = malloc((sizeof(int) + bloblength)*(sizeof(char)));
    msglength = bloblength + sizeof(int);

    bcopy(&bloblength, msg, sizeof(int));
    bcopy(blob, msg + sizeof(int), bloblength);
    
    flags = TOKEN_DATA;

  } else { /* negative response */

    msg = malloc((2 * sizeof(int) + bloblength)*(sizeof(char)));
    msglength = bloblength + 2 * sizeof(int);

    bcopy(&code, msg, sizeof(int));
    bcopy(&bloblength, msg+sizeof(int), sizeof(int));
    bcopy(blob, msg + 2*sizeof(int), bloblength);

    flags = TOKEN_ERROR;
  }

  if (gss_sendmsg(context, flags, msg, msglength) < 0)
    return(-1);

  return(0);
}



/*
 * Function: sign_server
 *
 * Purpose: Performs the "sign" service.
 *
 * Arguments:
 *
 * 	s		(r) a TCP socket on which a connection has been
 *			accept()ed
 * 	service_name	(r) the ASCII name of the GSS-API service to
 * 			establish a context as
 *	export		(r) whether to test context exporting
 * 
 * Returns: -1 on error
 *
 * Effects:
 *
 * sign_server establishes a context, and performs a single sign request.
 *
 * A sign request is a single GSS-API sealed token.  The token is
 * unsealed and a signature block, produced with gss_sign, is returned
 * to the sender.  The context is the destroyed and the connection
 * closed.
 *
 * If any error occurs, -1 is returned.
 */
int process_request(context, req_argc, req_argv)
     gss_ctx_id_t context;
     int* req_argc;
     char*** req_argv; /* just a pointer to a passed argv pointer */
{
     char	*cp;
     int token_flags;
     int length;
     char* msg;
     int msglength;
     int argvsize;

     if (gss_recvmsg(context, &token_flags, &msg, &msglength) < 0)
       return(-1);

     /* Let's see how many arguments are packed into the message, 
	i.e. what is the argc - use the info to allocate argv */

     cp = msg;
     argvsize = 0;
     while (1) {
       if (cp - msg >= msglength) break;

       bcopy(cp, &length, sizeof(int)); cp+=sizeof(int);
       if (length > MAXCMDLINE || length < 0) length = MAXCMDLINE;
       if (length > msglength) {
	 fprintf(log, "Data unpacking error");
	 return(-1);
       }
       cp+=length;
       argvsize++;
     }


     /* Now we actually know argc, and can populate the allocated argv */

     *req_argv = malloc(argvsize * sizeof(char*));

     cp = msg;
     (*req_argc) = 0;
     length = 0;
     while (1) {
       if (cp - msg >= msglength) break;

       bcopy(cp, &length, sizeof(int)); cp+=sizeof(int);
       if (length > MAXCMDLINE || length < 0) length = MAXCMDLINE;
       if (length > msglength) {
	 fprintf(log, "Data unpacking error");
	 return(-1);
       }

       (*req_argv)[*req_argc] = malloc(length+1);
       bcopy(cp, (*req_argv)[*req_argc], length); cp+=length;
       (*req_argv)[*req_argc][length] = '\0';

       (*req_argc)++;
     }
     (*req_argv)[*req_argc] = '\0';

     if (log)
       fflush(log);
     
     return(0);
}


int read_file(file_name, buf)
    char* file_name;
    char** buf;
{
    int fd, count;
    struct stat stat_buf;
    int length;

    if ((fd = open(file_name, O_RDONLY, 0)) < 0) {
	return(-2);
    }
    if (fstat(fd, &stat_buf) < 0) {
	perror("fstat");
	exit(1);
    }
    length = stat_buf.st_size;

    if (length == 0) {
	*buf = NULL;
	return;
    }

    if ((*buf = malloc(length+1)) == 0) {
	fprintf(stderr, "Couldn't allocate %d byte buffer for reading file\n",
		length);
	exit(1);
    }

    /* this code used to check for incomplete reads, but you can't get
       an incomplete read on any file for which fstat() is meaningful */

    count = read(fd, *buf, length);
    if (count < 0) {
	perror("read");
	exit(1);
    }
    if (count < length)
	fprintf(stderr, "Warning, only read in %d bytes, expected %d\n",
		count, length);

    (*buf)[length] = '\0';

    return(0);
}

void read_conf_file() {

  char* buf;
  char* word;
  char* line;
  int linenum;
  int wordnum;
  int i = 0;

  read_file("remctl.conf", &buf);

  linenum = 0;
  line = strtok (strdup(buf), "\n");
  while (line != NULL) {
    if ((strlen(line) > 0) && (line[0] != '#'))
      linenum++;
    line = strtok (NULL, "\n");
  }

  confbuffer = malloc(linenum * sizeof(confline));
  confsize = linenum;

  line = strtok (buf,"\n");
  linenum = 0;
  while (line != NULL) {
    if ((strlen(line) == 0) || (line[0] == '#')) {
      line = strtok (NULL, "\n");
      continue;
    }

    (*(confbuffer+linenum)).line = line;
    line = strtok (NULL, "\n");
    linenum++;
  }

  for(i=0; i<linenum; i++) {
    wordnum = 0;
    word = strtok (strdup((*(confbuffer+i)).line), " ");
    while (word != NULL) {
      (*(confbuffer+i)).elem[wordnum] = word;
      word = strtok (NULL, " ");
      wordnum++;
    }
    if (wordnum != 4) {
      fprintf(log, "Parse error in the conf file on this line: %s\n", 
	      (*(confbuffer+i)).line);
      exit(-1);
    }
  }

}

int acl_check(userprincipal, aclfile) 
     char* userprincipal;
     char* aclfile;
{

  char* buf;
  int buflen;
  char* line;
  int ret;

  if (ret = read_file(aclfile, &buf, &buflen) != 0)
    return(ret);

  line = strtok (buf,"\n");
  while (line != NULL) {
    if ((strlen(line) == 0) || (line[0] == '#')) {
      line = strtok(NULL, "\n");
      continue;
    }

    if (!strcmp(line, userprincipal)) 
      return(0);

    line = strtok (NULL, "\n");
  }

  return(-1);

}


int process_connection(server_creds)
     gss_cred_id_t server_creds;
{

  gss_buffer_desc client_name;
  gss_ctx_id_t context;
  OM_uint32 maj_stat, min_stat;
  int i, ret_flags;
  int req_argc;
  char** req_argv;
  int ret_code;
  char ret_message[MAXCMDLINE];
  int ret_length;

  char* command;
  char* aclfile;
  char* userprincipal;
  char* program;
  char* cp;
  int pipefd[2];

  ret_code = 0;
  bzero(ret_message, MAXCMDLINE);
  ret_length = 0;

  /* Establish a context with the client */
  if (server_establish_context(server_creds, &context,
			       &client_name, &ret_flags) < 0)
    return(-1);

  userprincipal = malloc(client_name.length * sizeof(char)+1);
  bcopy(client_name.value, userprincipal, client_name.length);
  userprincipal[client_name.length] = '\0';
	 
  if (context == GSS_C_NO_CONTEXT) {
    fprintf(log, "Unauthenticated connection.\n");
    return(-1);
  }

  if (verbose) {
    fprintf(log, "Accepted connection: \"%.*s\"\n",
	    (int) client_name.length, (char *) client_name.value);
  }

  (void) gss_release_buffer(&min_stat, &client_name);

  /* Establish a context with the client */
  
  process_request(context, &req_argc, &req_argv);

  if (verbose) {
  fprintf(log, "\nCOMMAND: ");
  i=0;
  while (req_argv[i] != '\0') {
    fprintf(log, "%s ", req_argv[i]);
    i++;
  }
  fprintf(log, "\n");
  }


  command = NULL;
  aclfile = NULL;
  for (i=0; i<confsize; i++) {
    if (!strcmp((*(confbuffer+i)).elem[0], req_argv[0])) {
      if (!strcmp((*(confbuffer+i)).elem[1], "all") || 
	  !strcmp((*(confbuffer+i)).elem[1], req_argv[1])) {
	command = (*(confbuffer+i)).elem[2];
	aclfile = (*(confbuffer+i)).elem[3];
	break;
      }
    }
  }


  if (command == NULL) {
    ret_code = -1;
    strcpy(ret_message, "Command not defined");
  } else if (aclfile == NULL) {
    ret_code = -1;
    strcpy(ret_message, "Aclfile not defined");
  } else {
    ret_code = acl_check(userprincipal, aclfile);

    if (ret_code == -1)
      strcpy(ret_message, "Access denied: principal not on the acl list");
    else if (ret_code == -2)
      strcpy(ret_message, "Problem reading acl list file: can't open file");
  }

  ret_length = strlen(ret_message);

  fprintf(log, "PRELIM code: %d\nmessage: %s\n\n", ret_code, ret_message);

  if (ret_code == 0) {

    /* parse out the real program name, for the first argument in argv */
    cp = strtok (strdup(command),"/");
    while (cp != NULL) {
      program = cp;
      cp = strtok (NULL, "/");
    }
    req_argv[0] = program;


    pipe (pipefd);
    switch(fork()){
    case -1: fprintf(log, "Can't fork\n");
      exit(-1);
      
    case 0 : // this is the code the child runs 
      close(1);      // close stdout
      dup (pipefd[1]); // points pipefd at file descriptor
      close (pipefd[0]);
      execv (command, req_argv);
      strcpy(ret_message, strerror(errno));
      fprintf(stdout, ret_message);
      exit(-1);
    default: // this is the code the parent runs 
      
      close (pipefd[1]); // the parent isn't going to write to the pipe
      // Now read from the pipe:
      ret_length = read(pipefd[0], ret_message, MAXCMDLINE); 
      ret_message[ret_length] = '\0';
      wait(&ret_code);

      if (WIFEXITED(ret_code)) {
	ret_code = WEXITSTATUS(ret_code);
	if (ret_code != 0 && ret_length == 0) {
	  strcpy(ret_message, strerror(errno));
	  ret_length = strlen(ret_message);
	}
      }
    }
  }

  process_response(context, ret_code, ret_message, ret_length);

  /* destroy the context */
  maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
  if (maj_stat != GSS_S_COMPLETE) {
    display_status("while deleting context", maj_stat, min_stat);
    return(-1);
  }

  if (log)
    fflush(log);

  return(0);

}




int
main(argc, argv)
     int argc;
     char **argv;
{
     char *service_name;
     gss_cred_id_t server_creds;
     OM_uint32 min_stat;
     u_short port = 4444;
     int s;
     int do_inetd = 0;
     time_t    timevar;
     char*     timestr;
     int stmp;

     read_conf_file();

     /* Since we are called from tcpserver, prevent clients from holding on
	to us forever, and die after an hour */
     alarm(60 * 60);

     log = stdout;
     display_file = stdout;
     argc--; argv++;
     while (argc) {
	  if (strcmp(*argv, "-port") == 0) {
	       argc--; argv++;
	       if (!argc) usage();
	       port = atoi(*argv);
	  } else if (strcmp(*argv, "-verbose") == 0) {
	      verbose = 1;
	  } else if (strcmp(*argv, "-inetd") == 0) {
	      do_inetd = 1;
	  } else if (strcmp(*argv, "-logfile") == 0) {
	      argc--; argv++;
	      if (!argc) usage();

	      log = fopen(*argv, "w");
	      display_file = log;
	      if (!log) {
		perror(*argv);
		exit(1);
	      }

	      timevar = time(NULL);
	      timestr = ctime(&timevar);
	      timestr[strlen(timestr)-1]='\0';
	      if (verbose) {
		fprintf(log,"[%s] start ",timestr);
		fprintf(log, "===========================================\n");
		fflush(log);
	      }
	      
	  } else
	    break;
	  argc--; argv++;
     }
     if (argc != 1)
	  usage();

     if ((*argv)[0] == '-')
	  usage();

     service_name = *argv;

     if (do_inetd) {
       INETD = 1;
       READFD = 0;
       WRITEFD = 1;
       process_connection(server_creds);
       close(0);
       close(1);
     } else {

       INETD = 0;

       /* This specifies the service name, checks out the keytab, etc */
       if (server_acquire_creds(service_name, &server_creds) < 0)
	 return -1;
       
       if ((stmp = create_socket(port)) >= 0) {
	 do {
	   /* Accept a TCP connection */
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
     }

     (void) gss_release_cred(&min_stat, &server_creds);

     return 0;
}
