
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

/* these are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver */
int READFD = 0;
int WRITEFD = 1;


/* this is used for caching the conf file in memory after first reading it */
typedef struct confline {
  /* this if for a particular line in the conf file */
  char* line;

  /* The following holds these strings:
     0 type
     1 service
     2 full file name of the executable, corresponding to this type
     3..N full file names of the acl files that have the principals, authorized
         to run this command
  */
  char** settings;

} confline; 
confline* confbuffer;
int confsize;


void usage()
{
     fprintf(stderr, "Usage: remctld [-port port] [-verbose] [-inetd]\n");
     fprintf(stderr, "       [-logfile file] [-conf conffile] [service_name]\n");
     fprintf(stderr, "       service_name is service/instance or service/host.domain.edu\n");
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
 * 	bloblength	(r) the the length of the output message
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * Packs the return message payload with it's length in front of it, in case of
 * error also packing in the error code and designating the error status with 
 * a token flag. Calls a utility function gss_sendmsg to send the token.
 * */
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

    msg = smalloc((sizeof(int) + bloblength)*(sizeof(char)));
    msglength = bloblength + sizeof(int);

    bcopy(&bloblength, msg, sizeof(int));
    bcopy(blob, msg + sizeof(int), bloblength);
    
    flags = TOKEN_DATA;

  } else { /* negative response */

    msg = smalloc((2 * sizeof(int) + bloblength)*(sizeof(char)));
    msglength = bloblength + 2 * sizeof(int);

    bcopy(&code, msg, sizeof(int));
    bcopy(&bloblength, msg+sizeof(int), sizeof(int));
    bcopy(blob, msg + 2*sizeof(int), bloblength);

    flags = TOKEN_ERROR;
  }

  if (gss_sendmsg(context, flags, msg, msglength) < 0)
    return(-1);

  free(msg);

  return(0);
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

     *req_argv = smalloc((argvsize+1) * sizeof(char*));

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

       (*req_argv)[*req_argc] = smalloc(length+1);
       bcopy(cp, (*req_argv)[*req_argc], length); cp+=length;
       (*req_argv)[*req_argc][length] = '\0';

       (*req_argc)++;
     }
     (*req_argv)[*req_argc] = '\0';

     /* was allocated in gss_recvmsg */
     free(msg);

     if (log)
       fflush(log);
     
     return(0);
}


/*
 * Function: read_file
 *
 * Purpose: reads a file into a char buffer
 *
 * Returns: 0 on success, -2 on failure, on purpose, to distinguish with a 
 * -1 that could happen in a calling function
 * */
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
	return(-2);
    }
    length = stat_buf.st_size;

    if (length == 0) {
	*buf = NULL;
	return(-2);
    }

    if ((*buf = smalloc(length+1)) == 0) {
	fprintf(log, "Couldn't allocate %d byte buffer for reading file\n",
		length);
	return(-2);
    }

    count = read(fd, *buf, length);
    if (count < 0) {
      fprintf(log, "Problem during read\n");
      return(-2);
    }
    if (count < length)
	fprintf(log, "Warning, only read in %d bytes, expected %d\n",
		count, length);

    (*buf)[length] = '\0';
    close(fd);

    return(0);
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
void read_conf_file(char* filename) {

  char* buf;
  char* word;
  char* line;
  char* temp;
  int linenum;
  int wordnum;
  int i, j;

  read_file(filename, &buf);

  linenum = 0;
  temp = strdup(buf);
  line = strtok (temp, "\n");
  while (line != NULL) {
    if ((strlen(line) > 0) && (line[0] != '#'))
      linenum++;
    line = strtok (NULL, "\n");
  }
  free(temp);

  confbuffer = smalloc(linenum * sizeof(confline));
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
    temp = strdup((*(confbuffer+i)).line);
    word = strtok (temp, " ");
    while (word != NULL) {
      word = strtok (NULL, " ");
      wordnum++;
    }
    free(temp);

    if (wordnum < 4) {
      fprintf(log, "Parse error in the conf file on this line: %s\n", 
	      (*(confbuffer+i)).line);
      exit(-1);
    }

    /* the settings member needs to have pointers to type, service, executable,
       1 or more aclfiles, and finally one more spot for the terminator. */
    (*(confbuffer+i)).settings = smalloc((wordnum+1)*sizeof(char*));

    (*(confbuffer+i)).settings[0] = strtok (strdup((*(confbuffer+i)).line), " ");
    (*(confbuffer+i)).settings[1] = strtok (NULL, " ");
    (*(confbuffer+i)).settings[2] = strtok (NULL, " ");

    j=3;
    word = strtok (NULL, " ");
    do {
      (*(confbuffer+i)).settings[j] = word;
      word = strtok (NULL, " ");
      j++;
    } while (word != NULL);
    (*(confbuffer+i)).settings[j] = 0;

    (*(confbuffer+i)).line = 0;
  }

  free(buf);

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
int acl_check(userprincipal, settings) 
     char* userprincipal;
     char** settings;
{

  char* buf;
  int buflen;
  char* line;
  int ret;
  int authorized = 0;
  int i;

  i=3;
  if (!strcmp(settings[i], "anyuser")) return(0);

  while(settings[i] != 0) {

    if ((ret = read_file(settings[i], &buf, &buflen)) != 0)
      return(ret);

    line = strtok (buf,"\n");
    while (line != NULL) {
      if ((strlen(line) == 0) || (line[0] == '#')) {
	line = strtok(NULL, "\n");
	continue;
      }

      if (!strcmp(line, userprincipal)) {
	authorized = 1;
	break;
      }

      line = strtok (NULL, "\n");
    }

    free(buf);

    if (authorized)
      return(0);
    else i++;
  }

  return(-1);
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
 * Establishes a context with this client. 
 *
 * Calls process_request to get the argv from the client. The first string in 
 * argv is expected to be a "type" which determines a category of commands - 
 * often points to a particular command executable. 
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
 * the return code and gathers stdout and stderr pipes. It then calls 
 * process_response with the message generated by the command execution and
 * cleans up the context.
 * 
 * */
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
  char err_message[MAXCMDLINE];
  int ret_length;

  char* tempcommand;
  char* command;
  char** settings;
  char* userprincipal;
  char* program;
  char* cp;
  int stdout_pipe[2];
  int stderr_pipe[2];

  ret_code = 0;
  ret_length = 0;
  bzero(ret_message, MAXCMDLINE);


  /* Establish a context with the client */
  if (server_establish_context(server_creds, &context,
			       &client_name, &ret_flags) < 0)
    return(-1);

  /* This is who just connected to us */
  userprincipal = smalloc(client_name.length * sizeof(char)+1);
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


  /*Interchange to get the data that makes up the request - basically an argv*/
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


  /*Lookup the command and the aclfile from the conf file structure in memory*/
  command = NULL;
  for (i=0; i<confsize; i++) {
    if (!strcmp((*(confbuffer+i)).settings[0], req_argv[0])) {
      if (!strcmp((*(confbuffer+i)).settings[1], "all") || 
	  !strcmp((*(confbuffer+i)).settings[1], req_argv[1])) {
	command = (*(confbuffer+i)).settings[2];
	settings = (*(confbuffer+i)).settings;
	break;
      }
    }
  }


  /* Check the command, aclfile, and the authorization of this client to run 
     this command */

  if (command == NULL) {
    ret_code = -1;
    strcpy(ret_message, "Command not defined");
  } else {
    ret_code = acl_check(userprincipal, settings);

    if (ret_code == -1)
      sprintf(ret_message, "Access denied: %s principal not on the acl list", userprincipal);
    else if (ret_code == -2)
      strcpy(ret_message, "Problem reading acl list file: can't open file");

  }
  free(userprincipal);

  ret_length = strlen(ret_message);


  if (ret_code == 0) {

    /* parse out the real program name, and splice it in as the first argument
       in argv passed to the command */
    tempcommand = smalloc(strlen(command)+1);
    strcpy(tempcommand, command);
    cp = strtok (tempcommand,"/");
    while (cp != NULL) {
      program = cp;
      cp = strtok (NULL, "/");
    }
    free(req_argv[0]);
    req_argv[0] = smalloc(strlen(program)+1);
    strcpy(req_argv[0], program);
    free(tempcommand);


    /* These pipes are used for communication with the child process that 
       actually runs the command */
    if(pipe (stdout_pipe) != 0 || 
       pipe (stderr_pipe) != 0) {
      fprintf(log, "Can't create pipes\n");
      exit(-1);
    }

    switch(fork()){
    case -1: fprintf(log, "Can't fork\n");
      exit(-1);
      
    case 0 : /* this is the code the child runs */

      /* Close the Child process' STDOUT read end*/
      close(1);
      dup(stdout_pipe[1]);
      close(stdout_pipe[0]);
      
      /* Close the Child process' STDERR read end*/
      close(2);
      dup(stderr_pipe[1]);
      close(stderr_pipe[0]);

      /* Child doesn't need STDIN at all */
      close(0);
      
      execv (command, req_argv);

      /* This here happens only if the exec failed. In that case this passed 
	 the errno information back up the stderr pipe to the parent, and 
	 dies */
      strcpy(ret_message, strerror(errno));
      fprintf(stderr, ret_message);

      exit(-1);
    default: /* this is the code the parent runs */
      
      close (stdout_pipe[1]);
      close (stderr_pipe[1]);

      wait(&ret_code);

      /* Now read from the pipe: */
      ret_length = read(stdout_pipe[0], ret_message, MAXCMDLINE/2); 
      ret_message[ret_length] = '\0';

      ret_length = read(stderr_pipe[0], err_message, MAXCMDLINE/2); 
      err_message[ret_length] = '\0';

      /* both streams could have useful info */
      strcat(ret_message, err_message);
      ret_length = strlen(ret_message);

      /* this does the crazy >>8 thing to get the real error code */
      if (WIFEXITED(ret_code)) {
	ret_code = (signed) WEXITSTATUS(ret_code);
      }

    }
  }


  /* send back the stuff returned from the exec */
  process_response(context, ret_code, ret_message, ret_length);


  /* destroy the context */
  maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
  if (maj_stat != GSS_S_COMPLETE) {
    display_status("while deleting context", maj_stat, min_stat);
    return(-1);
  }


  i=0;
  while (req_argv[i] != '\0') {
    free(req_argv[i]);
    i++;
  }
  free(req_argv);

  if (log)
    fflush(log);

  return(0);

}



/* The main serves to gather arguments and determine the standalone vs inetd
   mode. It also calls to set up a tcp connection on standalone mode; the 
   transaction handling is then handed off to process_connection
*/

int main(argc, argv)
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
     char* conffile = "remctl.conf";

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
	  } else if (strcmp(*argv, "-conf") == 0) {
	      conffile = *argv;
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

     read_conf_file(conffile);

     /* This specifies the service name, checks out the keytab, etc */
     service_name = *argv;
     if (server_acquire_creds(service_name, &server_creds) < 0)
       return -1;

     if (do_inetd) {
       READFD = 0;
       WRITEFD = 1;
       process_connection(server_creds);
       close(0);
       close(1);
     } else {

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
