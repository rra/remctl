

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include <gssapi/gssapi_generic.h>
#include "gss-utils.h"

int verbose = 0;
int READFD = 0;
int WRITEFD = 1;

void usage()
{
     fprintf(stderr, "Usage: \n");
     fprintf(stderr, "remctl [-p port] [-d] [-v] host remctl_service_name type service <arguments>\n");
     fprintf(stderr, "       -d delegate (only works with a proxiable TGT)\n");
     fprintf(stderr, "       -v verbose\n");
     fprintf(stderr, "       remctl_service_name is the K5 principal the remctld server runs under.\n");
 fprintf(stderr, "       It's format should be: service/instance or service/host.domain.edu\n");
     exit(1);
}



static void parse_oid(char *mechanism, gss_OID *oid)
{
    char	*mechstr = 0, *cp;
    gss_buffer_desc tok;
    OM_uint32 maj_stat, min_stat;
    
    if (isdigit(mechanism[0])) {
	mechstr = malloc(strlen(mechanism)+5);
	if (!mechstr) {
	    fprintf(stderr, "Couldn't allocate mechanism scratch!\n");
	    return;
	}
	sprintf(mechstr, "{ %s }", mechanism);
	for (cp = mechstr; *cp; cp++)
	    if (*cp == '.')
		*cp = ' ';
	tok.value = mechstr;
    } else
	tok.value = mechanism;
    tok.length = strlen(tok.value);
    maj_stat = gss_str_to_oid(&min_stat, &tok, oid);
    if (maj_stat != GSS_S_COMPLETE) {
	display_status("while str_to_oid", maj_stat, min_stat);
	return;
    }
    if (mechstr)
	free(mechstr);
}




/*
 * Function: connect_to_server
 *
 * Purpose: Opens a TCP connection to the name host and port.
 *
 * Arguments:
 *
 * 	host		(r) the target host name
 * 	port		(r) the target port, in host byte order
 *
 * Returns: the established socket file desciptor, or -1 on failure
 *
 * Effects:
 *
 * The host name is resolved with gethostbyname(), and the socket is
 * opened and connected.  If an error occurs, an error message is
 * displayed and -1 is returned.
 */
int connect_to_server(host, port)
     char *host;
     u_short port;
{
     struct sockaddr_in saddr;
     struct hostent *hp;
     int s;
     
     if ((hp = gethostbyname(host)) == NULL) {
	  fprintf(stderr, "Unknown host: %s\n", host);
	  return -1;
     }
     
     saddr.sin_family = hp->h_addrtype;
     memcpy((char *)&saddr.sin_addr, hp->h_addr, sizeof(saddr.sin_addr));
     saddr.sin_port = htons(port);

     if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror("creating socket");
	  return -1;
     }
     if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
	  perror("connecting to server");
	  (void) close(s);
	  return -1;
     }
     return s;
}

/*
 * Function: client_establish_context
 *
 * Purpose: establishes a GSS-API context with a specified service and
 * returns the context handle
 *
 * Arguments:
 *
 * 	s		(r) an established TCP connection to the service
 * 	service_name	(r) the ASCII service name of the service
 *	deleg_flag	(r) GSS-API delegation flag (if any)
 *	auth_flag	(r) whether to actually do authentication
 *	oid		(r) OID of the mechanism to use
 * 	context		(w) the established GSS-API context
 *	ret_flags	(w) the returned flags from init_sec_context
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * service_name is imported as a GSS-API name and a GSS-API context is
 * established with the corresponding service; the service should be
 * listening on the TCP connection s.  The default GSS-API mechanism
 * is used, and mutual authentication and replay detection are
 * requested.
 * 
 * If successful, the context handle is returned in context.  If
 * unsuccessful, the GSS-API error messages are displayed on stderr
 * and -1 is returned.
 */
int client_establish_context(service_name, deleg_flag, 
			     gss_context, ret_flags)
     char *service_name;
     OM_uint32 deleg_flag;
     gss_ctx_id_t *gss_context;
     OM_uint32 *ret_flags;
{

     gss_OID oid = GSS_C_NULL_OID;
     gss_buffer_desc send_tok, recv_tok, *token_ptr;
     gss_name_t target_name;
     OM_uint32 maj_stat, min_stat, init_sec_min_stat;
     int token_flags;

     parse_oid("1.2.840.113554.1.2.2", &oid);

       /*
	* Import the name into target_name.  Use send_tok to save
	* local variable space.
	*/
       send_tok.value = service_name;
       send_tok.length = strlen(service_name) + 1;
       maj_stat = gss_import_name(&min_stat, &send_tok,
				  (gss_OID) gss_nt_user_name, &target_name);
       if (maj_stat != GSS_S_COMPLETE) {
	 display_status("while parsing name", maj_stat, min_stat);
	 return -1;
       }
     
       if (send_token(TOKEN_NOOP|TOKEN_CONTEXT_NEXT, empty_token) < 0) {
	 (void) gss_release_name(&min_stat, &target_name);
	 return -1;
       }

       /*
	* Perform the context-establishement loop.
	*
	* On each pass through the loop, token_ptr points to the token
	* to send to the server (or GSS_C_NO_BUFFER on the first pass).
	* Every generated token is stored in send_tok which is then
	* transmitted to the server; every received token is stored in
	* recv_tok, which token_ptr is then set to, to be processed by
	* the next call to gss_init_sec_context.
	* 
	* GSS-API guarantees that send_tok's length will be non-zero
	* if and only if the server is expecting another token from us,
	* and that gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if
	* and only if the server has another token to send us.
	*/
     
       token_ptr = GSS_C_NO_BUFFER;
       *gss_context = GSS_C_NO_CONTEXT;

       do {
	 maj_stat =
	   gss_init_sec_context(&init_sec_min_stat,
				GSS_C_NO_CREDENTIAL,
				gss_context,
				target_name,
				oid,
				GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG |
				deleg_flag,
				0,
				NULL,	/* no channel bindings */
				token_ptr,
				NULL,	/* ignore mech type */
				&send_tok,
				ret_flags,
				NULL);	/* ignore time_rec */

	 if (token_ptr != GSS_C_NO_BUFFER)
	   (void) gss_release_buffer(&min_stat, &recv_tok);

	 if (send_tok.length != 0) {
	   if (verbose)
	     printf("Sending init_sec_context token (size=%d)...\n",
		    send_tok.length);
	   if (send_token(TOKEN_CONTEXT, &send_tok) < 0) {
	     (void) gss_release_buffer(&min_stat, &send_tok);
	     (void) gss_release_name(&min_stat, &target_name);
	     return -1;
	   }
	 }

	 (void) gss_release_buffer(&min_stat, &send_tok);
 
	 if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
	      display_status("while initializing context", maj_stat,
			     init_sec_min_stat);
	      (void) gss_release_name(&min_stat, &target_name);
	      if (*gss_context == GSS_C_NO_CONTEXT)
		      gss_delete_sec_context(&min_stat, gss_context,
					     GSS_C_NO_BUFFER);
	      return -1;
	 }

	 if (maj_stat == GSS_S_CONTINUE_NEEDED) {
	   if (verbose)
	     printf("continue needed...");
	   if (recv_token(&token_flags, &recv_tok) < 0) {
	     (void) gss_release_name(&min_stat, &target_name);
	     return -1;
	   }
	   token_ptr = &recv_tok;
	 }
	 if (verbose)
	   printf("\n");
       } while (maj_stat == GSS_S_CONTINUE_NEEDED);

       (void) gss_release_name(&min_stat, &target_name);
       (void) gss_release_oid(&min_stat, &oid);

     return 0;
}



/*
 * Function: process_response
 *
 * Purpose: get the response back from the server, containg the result message
 *
 * Arguments:
 *
 * 	context		(r) the established gssapi context
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * Calls a utility function gss_recvmsg to get the token, then disassembles
 *  the data payload, which is in the format of 
 * [<length><data of that length>] and then displays the return code and 
 * message.
 * */
int process_response(context)
     gss_ctx_id_t context;
{

     char *cp;
     int token_flags;
     int errorcode;
     int length;
     char* body;
     char* msg;
     int msglength;

     if (gss_recvmsg(context, &token_flags, &msg, &msglength) < 0)
       return(-1);

     cp = msg;

     if (token_flags & TOKEN_ERROR) {
       bcopy(cp, &errorcode, sizeof(int)); cp+=sizeof(int);
       printf("Error code: %d\n", errorcode);
     }

     bcopy(cp, &length, sizeof(int)); cp+=sizeof(int);

     if (length > MAXCMDLINE || length < 0) length = MAXCMDLINE;
     if (length > msglength) {
       fprintf(stderr, "Data unpacking error");
       return(-1);
     }

     body = malloc(length*sizeof(char)+1);
     bcopy(cp, body, length);
     body[length] = '\0';

     printf("Message: %s\n", body);
     free(msg);

     return(0);

}




/*
 * Function: process_request
 *
 * Purpose: send the request data over to the server
 *
 * Arguments:
 *
 * 	context		(r) the established gssapi context
 * 	argc		(r) number of agruments to send
 * 	argv		(r) the end of the main's argv containing only the 
 *                          things representing the remote command to send 
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * Assembles the data payload in the format of [<length><data of that length>]*
 * then calls a utility function gss_sendmsg to send the token over
 * */
int process_request(context, argc, argv)
     gss_ctx_id_t context;
     int argc;
     char** argv;
{
     int length = 0;
     char* msg;
     int msglength=0;
     char** cp;
     char* mp;
     int i;

     cp = argv;
     for (i=0; i<argc; i++) {
       msglength += strlen(*cp++);
     }
     msglength += argc * sizeof(int);

     msg = malloc(msglength * (sizeof(char)));

     mp = msg;
     cp = argv;
     while (argc != 0) {
       length = strlen(*cp);
       bcopy(&length, mp, sizeof(int)); mp+=sizeof(int);
       bcopy(*cp, mp, length); mp+=length;
       argc--;
       cp++;
     }

     if (gss_sendmsg(context, TOKEN_DATA, msg, msglength) < 0)
       return(-1);

     return(0);

}



/*
 * The main just parses the arguments, establishes the gssapi context and
 * calls the effector functions to request and process response
 */
int main(argc, argv)
     int argc;
     char **argv;
{
     char *service_name, *server_host;
     u_short port = 4444;
     OM_uint32 deleg_flag = 0, ret_flags, maj_stat, min_stat;
     int s;
     gss_ctx_id_t context;
     gss_buffer_desc out_buf;

     display_file = stdout;

     /* Parse arguments. */
     argc--; argv++;
     while (argc) {
	  if (strcmp(*argv, "-p") == 0) {
	       argc--; argv++;
	       if (!argc) usage();
	       port = atoi(*argv);
	   } else if (strcmp(*argv, "-v") == 0) {
	       verbose = 1;
	   } else if (strcmp(*argv, "-d") == 0) {
	       deleg_flag = GSS_C_DELEG_FLAG;
	  } else 
	       break;
	  argc--; argv++;
     }
     if (argc < 3)
	  usage();

     server_host = *argv++;
     argc--;
     service_name = *argv++;
     argc--;

     /* Open connection */
     if ((s = connect_to_server(server_host, port)) < 0)
	  return -1;

     READFD = s;
     WRITEFD = s;

     /* Establish context */
     if (client_establish_context(service_name, deleg_flag, &context, 
				  &ret_flags) < 0) {
	  (void) close(s);
	  return -1;
     }

     /* display the flags */
     if (verbose)
       display_ctx_flags(ret_flags);

     if (process_request(context, argc, argv) < 0)
       exit(1);

     if (process_response(context) < 0)
       exit(1);

     /* Delete context */
     maj_stat = gss_delete_sec_context(&min_stat, &context, &out_buf);
     if (maj_stat != GSS_S_COMPLETE) {
       display_status("while deleting context", maj_stat, min_stat);
       (void) close(s);
       (void) gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
       return -1;
     }
     
     (void) gss_release_buffer(&min_stat, &out_buf);
     (void) close(s);
	 
     return 0;
}
