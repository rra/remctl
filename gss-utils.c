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
#include <netinet/in.h>
#endif
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

#include <gssapi/gssapi_generic.h>
#include "gss-utils.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern char *malloc();
#endif

extern int verbose;

extern int READFD;
extern int WRITEFD;

FILE *display_file;

gss_buffer_desc empty_token_buf = { 0, (void *) "" };
gss_buffer_t empty_token = &empty_token_buf;

static void display_status_1
	PROTOTYPE( (char *m, OM_uint32 code, int type) );


/*
 * Function: gss_sendmsg
 *
 * Purpose: wraps, encrypts and sends a data payload token, gets back a MIC 
 *          checksum
 *
 * Arguments:
 *
 * 	context		(r) established gssapi context
 *	flags	        (r) the flags received with the incoming token
 * 	msg		(r) the data token, packed [<len><data of len>]*
 * 	msglength	(r) the data token length
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Calls gss api to wrap the token and sends it. Receives a MIC checksum
 * and verifies it. 
 * This is not proof of message integrity, as that is inherent in the 
 * underlying K5 mechanism -  the MIC is not mandatory to assure secure 
 * transfer, but present here nonetheless for completeness
 */
int gss_sendmsg(context, flags, msg, msglength)
     gss_ctx_id_t context;
     int flags;
     char* msg;
     int msglength;
{

     gss_buffer_desc in_buf, out_buf;
     int state;
     OM_uint32 maj_stat, min_stat;
     gss_qop_t		qop_state;

     int token_flags;

     in_buf.value = msg;
     in_buf.length = msglength;

     maj_stat = gss_wrap(&min_stat, context, 1, GSS_C_QOP_DEFAULT,
			 &in_buf, &state, &out_buf);

     if (maj_stat != GSS_S_COMPLETE) {
       display_status("while wrapping message", maj_stat, min_stat);
       (void) gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
       return -1;
     } else if (! state) {
       if (display_file)
	 fprintf(display_file, "Error in gss_sendmsg: Message not encrypted.\n");
       return -1;
     }

       /* Send to server */
     if (send_token((flags | TOKEN_SEND_MIC), &out_buf) < 0) {
       (void) close(WRITEFD);
       (void) gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
       return -1;
     }

     (void) gss_release_buffer(&min_stat, &out_buf);

     /* Read signature block into out_buf */
     if (recv_token(&token_flags, &out_buf) < 0) {
       (void) close(READFD);
       (void) gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
       return -1;
     }

     /* Verify signature block */
     maj_stat = gss_verify_mic(&min_stat, context, &in_buf,
			       &out_buf, &qop_state);
     if (maj_stat != GSS_S_COMPLETE) {
       display_status("while verifying signature", maj_stat, min_stat);
       (void) close(READFD);
       (void) close(WRITEFD);
       (void) gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
       return -1;
     }

     (void) gss_release_buffer(&min_stat, &out_buf);

     return 0;

}

/*
 * Function: gss_recvmsg
 *
 * Purpose: receives and an wraps a data payload token, send back a MIC 
 *          checksum
 *
 * Arguments:
 *
 * 	context		(r) established gssapi context
 *	token_flags	(w) the flags received with the incoming token
 * 	msg		(w) the data token, packed [<len><data of len>]*
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Calls lower utility to get the token as it was sent to us. Unwraps and 
 * unencrypts the payload. Allocates some memory for the payload and passes
 * it back by reference. 
 * Also send back a MIC checksum the proves the encryption was in place on the
 * wire and that we decrypted the token. This is not proof of message 
 * integrity, as that is inherent in the underlying K5 mechanism -  the MIC is
 * not mandatory to assure secure transfer, but present here nonetheless for
 * completeness
 */
int gss_recvmsg(context, token_flags, msg, msglength) 
     gss_ctx_id_t context;
     int* token_flags;
     char** msg;
     int* msglength;
{
     gss_buffer_desc xmit_buf, msg_buf;
     OM_uint32 maj_stat, min_stat;
     int conf_state;
     char	*cp;

     /* Receive the message token */
     if (recv_token(token_flags, &xmit_buf) < 0)
       return(-1);

     if (verbose) {
       print_token(&xmit_buf);
     }

     maj_stat = gss_unwrap(&min_stat, context, &xmit_buf, &msg_buf,
			   &conf_state, (gss_qop_t *) NULL);

     if (maj_stat != GSS_S_COMPLETE) {
       display_status("while unwrapping message", maj_stat, min_stat);
       (void) gss_release_buffer(&min_stat, &xmit_buf);
       return(-1);
     } else if (! conf_state) { 

       if (display_file)
	 fprintf(display_file, "Error in gss_recvmsg: Message not encrypted.\n");
       (void) gss_release_buffer(&min_stat, &xmit_buf);
       return(-1);
     }
       

     if (verbose && display_file) {
       fprintf(display_file, "Received message: ");
       cp = msg_buf.value;
       if ((isprint(cp[0]) || isspace(cp[0])) &&
	   (isprint(cp[1]) || isspace(cp[1]))) {
	 fprintf(display_file, "\"%.*s\"\n", (int)msg_buf.length, (char*)msg_buf.value);
       } else {
	 fprintf(display_file, "\n");
	 print_token(&msg_buf);
       }
     }
     

     *msg = malloc(msg_buf.length * sizeof(char));
     bcopy(msg_buf.value, *msg, msg_buf.length);
     *msglength = msg_buf.length;

     /* send back a MIC checksum */
     
     if (*token_flags & TOKEN_SEND_MIC) {
       /* Produce a signature block for the message */
       maj_stat = gss_get_mic(&min_stat, context, GSS_C_QOP_DEFAULT,
			      &msg_buf, &xmit_buf);
       if (maj_stat != GSS_S_COMPLETE) {
	 display_status("signing message", maj_stat, min_stat);
	 return(-1);
       }
       
       /* Send the signature block to the client */
       if (send_token(TOKEN_MIC, &xmit_buf) < 0)
	 return(-1);

       if (verbose)
	 fprintf(display_file, "MIC signature sent\n");
       
       (void) gss_release_buffer(&min_stat, &xmit_buf);
     }

     (void) gss_release_buffer(&min_stat, &msg_buf);

     return(0);

}

/*
 * Function: send_token
 *
 * Purpose: Writes a token to a file descriptor.
 *
 * Arguments:
 *
 * 	s		(r) an open file descriptor
 *	flags		(r) the flags to write
 * 	tok		(r) the token to write
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * send_token writes the token flags (a single byte, even though
 * they're passed in in an integer), then the token length (as a
 * network long) and then the token data to the file descriptor s.  It
 * returns 0 on success, and -1 if an error occurs or if it could not
 * write all the data.
 */
int send_token(flags, tok)
     int flags;
     gss_buffer_t tok;
{
     int len, ret;
     unsigned char char_flags = (unsigned char) flags;

     ret = write_all((char *)&char_flags, 1);
     if (ret != 1) {
       if (display_file) fprintf(display_file, "sending token flags");
       return -1;
     }

     len = htonl(tok->length);

     ret = write_all((char *) &len, 4);
     if (ret < 0) {
	  if (display_file) fprintf(display_file, "sending token length");
	  return -1;
     } else if (ret != 4) {
	 if (display_file)
	     fprintf(display_file, 
		     "sending token length: %d of %d bytes written\n", 
		     ret, 4);
	  return -1;
     }

     ret = write_all(tok->value, tok->length);
     if (ret < 0) {
	  if (display_file) fprintf(display_file, "sending token data");
	  return -1;
     } else if (ret != tok->length) {
	 if (display_file)
	     fprintf(display_file, 
		     "sending token data: %d of %d bytes written\n", 
		     ret, tok->length);
	 return -1;
     }
     
     return 0;
}

/*
 * Function: recv_token
 *
 * Purpose: Reads a token from a file descriptor.
 *
 * Arguments:
 *
 * 	s		(r) an open file descriptor
 *	flags		(w) the read flags
 * 	tok		(w) the read token
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * recv_token reads the token flags (a single byte, even though
 * they're stored into an integer, then reads the token length (as a
 * network long), allocates memory to hold the data, and then reads
 * the token data from the file descriptor s.  It blocks to read the
 * length and data, if necessary.  On a successful return, the token
 * should be freed with gss_release_buffer.  It returns 0 on success,
 * and -1 if an error occurs or if it could not read all the data.
 */
int recv_token(flags, tok)
     int *flags;
     gss_buffer_t tok;
{
     int ret;
     unsigned char char_flags;

     ret = read_all((char *) &char_flags, 1);
     if (ret < 0) {
       if (display_file) fprintf(display_file, "reading token flags");
       return -1;
     } else if (! ret) {
       if (display_file) fprintf(display_file, "reading token flags: 0 bytes read\n");
       return -1;
     } else {
       *flags = (int) char_flags;
     }

     ret = read_all((char *) &tok->length, 4);
     if (ret < 0) {
	  if (display_file) fprintf(display_file, "reading token length");
	  return -1;
     } else if (ret != 4) {
	 if (display_file)
	     fprintf(display_file, 
		     "reading token length: %d of %d bytes read\n", 
		     ret, 4);
	 return -1;
     }
	  
     tok->length = ntohl(tok->length);
     tok->value = (char *) malloc(tok->length);
     if (tok->length && tok->value == NULL) {
	 if (display_file)
	     fprintf(display_file, 
		     "Out of memory allocating token data\n");
	  return -1;
     }

     ret = read_all((char *) tok->value, tok->length);
     if (ret < 0) {
	  if (display_file) fprintf(display_file, "reading token data");
	  free(tok->value);
	  return -1;
     } else if (ret != tok->length) {
	 if (display_file)
	  fprintf(display_file, 
		  "sending token data: %d of %d bytes written\n", ret, 
		  tok->length);
	  free(tok->value);
	  return -1;
     }

     return 0;
}


/*
 * Function: write_all
 *
 * Purpose: standard socket/file descriptor write
 *
 * Arguments:
 *
 * 	buf		a buffer fromwhich to write
 *      nbyte	        number of bytes to write
 *
 */
static int write_all(char *buf, unsigned int nbyte)
{
     int ret;
     char *ptr;

     for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {

       ret = write(WRITEFD, ptr, nbyte);

	  if (ret < 0) {
	       if (errno == EINTR)
		    continue;
	       return(ret);
	  } else if (ret == 0) {
	       return(ptr-buf);
	  }
     }

     return(ptr-buf);
}

/*
 * Function: read_all
 *
 * Purpose: standard socket/file descriptor read to a particular length
 *
 * Arguments:
 *
 * 	buf		a buffer into which to read
 *      nbyte	        number of bytes to read
 *
 */
static int read_all(char *buf, unsigned int nbyte)
{
     int ret;
     char *ptr;

     for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {

       ret = read(READFD, ptr, nbyte);

	  if (ret < 0) {
	       if (errno == EINTR)
		    continue;
	       return(ret);
	  } else if (ret == 0) {
	       return(ptr-buf);
	  }
     }

     return(ptr-buf);
}



static void display_status_1(m, code, type)
     char *m;
     OM_uint32 code;
     int type;
{
     OM_uint32 maj_stat, min_stat;
     gss_buffer_desc msg;
     OM_uint32 msg_ctx;
     
     msg_ctx = 0;
     while (1) {
	  maj_stat = gss_display_status(&min_stat, code,
				       type, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
	  if (display_file)
	      fprintf(display_file, "GSS-API error %s: %s\n", m,
		      (char *)msg.value); 
	  (void) gss_release_buffer(&min_stat, &msg);
	  
	  if (!msg_ctx)
	       break;
     }
}

/*
 * Function: display_status
 *
 * Purpose: displays GSS-API messages
 *
 * Arguments:
 *
 * 	msg		a string to be displayed with the message
 * 	maj_stat	the GSS-API major status code
 * 	min_stat	the GSS-API minor status code
 *
 * Effects:
 *
 * The GSS-API messages associated with maj_stat and min_stat are
 * displayed on stderr, each preceeded by "GSS-API error <msg>: " and
 * followed by a newline.
 */
void display_status(msg, maj_stat, min_stat)
     char *msg;
     OM_uint32 maj_stat;
     OM_uint32 min_stat;
{
     display_status_1(msg, maj_stat, GSS_C_GSS_CODE);
     display_status_1(msg, min_stat, GSS_C_MECH_CODE);
}

/*
 * Function: display_ctx_flags
 *
 * Purpose: displays the flags returned by context initation in
 *	    a human-readable form
 *
 * Arguments:
 *
 * 	int		ret_flags
 *
 * Effects:
 *
 * Strings corresponding to the context flags are printed on
 * stdout, preceded by "context flag: " and followed by a newline
 */

void display_ctx_flags(flags)
     OM_uint32 flags;
{

    if (!display_file)
	return;

     if (flags & GSS_C_DELEG_FLAG)
	  fprintf(display_file, "context flag: GSS_C_DELEG_FLAG\n");
     if (flags & GSS_C_MUTUAL_FLAG)
	  fprintf(display_file, "context flag: GSS_C_MUTUAL_FLAG\n");
     if (flags & GSS_C_REPLAY_FLAG)
	  fprintf(display_file, "context flag: GSS_C_REPLAY_FLAG\n");
     if (flags & GSS_C_SEQUENCE_FLAG)
	  fprintf(display_file, "context flag: GSS_C_SEQUENCE_FLAG\n");
     if (flags & GSS_C_CONF_FLAG )
	  fprintf(display_file, "context flag: GSS_C_CONF_FLAG \n");
     if (flags & GSS_C_INTEG_FLAG )
	  fprintf(display_file, "context flag: GSS_C_INTEG_FLAG \n");
}

void print_token(tok)
     gss_buffer_t tok;
{
    int i;
    unsigned char *p = tok->value;

    if (!display_file)
	return;
    for (i=0; i < tok->length; i++, p++) {
	fprintf(display_file, "%02x ", *p);
	if ((i % 16) == 15) {
	    fprintf(display_file, "\n");
	}
    }
    fprintf(display_file, "\n");
    fflush(display_file);
}

#ifdef _WIN32
#include <sys\timeb.h>
#include <time.h>

int gettimeofday (struct timeval *tv, void *ignore_tz)
{
    struct _timeb tb;
    _tzset();
    _ftime(&tb);
    if (tv) {
	tv->tv_sec = tb.time;
	tv->tv_usec = tb.millitm * 1000;
    }
    return 0;
}
#endif /* _WIN32 */
