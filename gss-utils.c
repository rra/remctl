/*
   $Id$

   The shared functions for a "K5 sysctl" - a service for remote execution 
   of predefined commands. Access is authenticated via GSSAPI Kerberos 5, 
   authorized via aclfiles. 

   Written by Anton Ushakov <antonu@stanford.edu>
   vsnprintf implementation contributed by Russ Allbery <rra@stanford.edu>
   Copyright 2002 Board of Trustees, Leland Stanford Jr. University

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include <gssapi/gssapi_generic.h>
#include "gss-utils.h"


extern int use_syslog;   /* Toggle for sysctl vs stdout/stderr. */
extern int verbose;      /* Turns on debugging output. */

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
extern int READFD;
extern int WRITEFD;


/* This is used for all error and debugging printouts. It directs the output
   to whatever log is set to, which is either stdout, a file, or NULL, in which
   case the program is in "quiet" mode. */
static void
write_log(int priority, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    if (use_syslog == 1) {
        char *buffer;
        size_t length;

        length = vsnprintf(NULL, 0, format, args);
        buffer = malloc(length + 1);
        if (buffer == NULL) {
            fprintf(stderr, "ERROR: Cannot allocate memory: %s\n",
                    strerror(errno));
            exit(-1);
        }
        vsnprintf(buffer, length + 1, format, args);

        syslog(priority, buffer);
        free(buffer);
    } else {
        if (priority == LOG_DEBUG)
            vfprintf(stdout, format, args);
        else
            vfprintf(stderr, format, args);
    }
    va_end(args);
}

void 
lowercase(char string[])
{

 char *p;

    for (p = string; *p != '\0'; p++)
        *p = tolower(*p);

    return;
}

/* Safe realloc, that will check if the allocation succeeded. */
void* 
srealloc(void* ptr, size_t size) {

    void *temp;

    temp = realloc(ptr, size);
    if (temp == NULL) {
        write_log(LOG_ERR, "ERROR: Cannot reallocate memory: %s\n", strerror(errno));
        exit(-1);
    }

    return temp;

}

/* Safe malloc, that will check if the allocation succeeded. */
void *
smalloc(int size)
{

    void *temp;

    if ((temp = malloc(size)) == NULL) {
        write_log(LOG_ERR, "ERROR: Cannot allocate memory: %s\n", strerror(errno));
        exit(-1);
    }

    return temp;
}

/* Safe strdup, that will check if the allocation succeeded. */
char *
sstrdup(const char* s1)
{

    void *temp;

    if ((temp = strdup(s1)) == NULL) {
        write_log(LOG_ERR, "ERROR: Cannot allocate memory for strdup: %s\n", strerror(errno));
        exit(-1);
    }

    return temp;
}


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
int
gss_sendmsg(gss_ctx_id_t context, int flags, char *msg, OM_uint32 msglength)
{

    gss_buffer_desc in_buf, out_buf;
    int state;
    OM_uint32 maj_stat, min_stat;
    gss_qop_t qop_state;
    int token_flags;
    char *cp;

    in_buf.value = msg;
    in_buf.length = msglength;

    if (verbose) {
        write_log(LOG_DEBUG, "Sending message of length: %d\n", in_buf.length);
        print_token(&in_buf);
        write_log(LOG_DEBUG, "\n");
    }


    maj_stat = gss_wrap(&min_stat, context, 1, GSS_C_QOP_DEFAULT,
                        &in_buf, &state, &out_buf);

    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while wrapping message", maj_stat, min_stat);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
    } else if (!state) {
        write_log(LOG_ERR, "Error in gss_sendmsg: Message not encrypted.\n");
        return -1;
    }

    /* Send to server */
    if (send_token((flags | TOKEN_SEND_MIC), &out_buf) < 0) {
        close(WRITEFD);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
    }

    gss_release_buffer(&min_stat, &out_buf);

    /* Read signature block into out_buf */
    if (recv_token(&token_flags, &out_buf) < 0) {
        close(READFD);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
    }

    /* Verify signature block */
    maj_stat = gss_verify_mic(&min_stat, context, &in_buf,
                              &out_buf, &qop_state);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while verifying signature", maj_stat, min_stat);
        close(READFD);
        close(WRITEFD);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
    }

    gss_release_buffer(&min_stat, &out_buf);

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
 *      msglength       (w) length of data token
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
int
gss_recvmsg(gss_ctx_id_t context, int *token_flags, char **msg, OM_uint32 *msglength)
{
    gss_buffer_desc xmit_buf, msg_buf;
    OM_uint32 maj_stat, min_stat;
    int conf_state;
    char *cp;

    /* Receive the message token */
    if (recv_token(token_flags, &xmit_buf) < 0)
        return (-1);

    maj_stat = gss_unwrap(&min_stat, context, &xmit_buf, &msg_buf,
                          &conf_state, (gss_qop_t *) NULL);
    gss_release_buffer(&min_stat, &xmit_buf);

    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while unwrapping message", maj_stat, min_stat);
        return (-1);
    } else if (!conf_state) {

        write_log(LOG_ERR, "Error in gss_recvmsg: Message not encrypted.\n");
        return (-1);
    }


    if (verbose) {
        write_log(LOG_DEBUG, "Received message of length: %d\n", 
                  msg_buf.length);
        print_token(&msg_buf);
    }


    /* fill in the return value */
    *msg = malloc(msg_buf.length);
    memcpy(*msg, msg_buf.value, msg_buf.length);
    *msglength = msg_buf.length;


    /* send back a MIC checksum */

    if (*token_flags & TOKEN_SEND_MIC) {
        /* Produce a signature block for the message */
        maj_stat = gss_get_mic(&min_stat, context, GSS_C_QOP_DEFAULT,
                               &msg_buf, &xmit_buf);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("signing message", maj_stat, min_stat);
            return (-1);
        }

        /* Send the signature block to the client */
        if (send_token(TOKEN_MIC, &xmit_buf) < 0)
            return (-1);

        if (verbose)
            write_log(LOG_DEBUG, "MIC signature sent\n");

        gss_release_buffer(&min_stat, &xmit_buf);
    }

    gss_release_buffer(&min_stat, &msg_buf);

    return (0);

}

/*
 * Function: send_token
 *
 * Purpose: Writes a token to a file descriptor.
 *
 * Arguments:
 *
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
int
send_token(int flags, gss_buffer_t tok)
{
    OM_uint32 len, ret;
    unsigned char char_flags = (unsigned char)flags;

    ret = write_all(WRITEFD, &char_flags, 1);
    if (ret != 1) {
        write_log(LOG_ERR, "Error while sending token flags\n");
        return -1;
    }

    len = htonl(tok->length);

    ret = write_all(WRITEFD, &len, sizeof(OM_uint32));
    if (ret < 0) {
        write_log(LOG_ERR, "Error while sending token length\n");
        return -1;
    } else if (ret != sizeof(OM_uint32)) {
        write_log(LOG_ERR, 
                  "sending token length: only %d of %d bytes written\n", 
                  sizeof(OM_uint32));
        return -1;
    }

    ret = write_all(WRITEFD, tok->value, tok->length);
    if (ret < 0) {
        write_log(LOG_ERR, "Error while sending token data\n");
        return -1;
    } else if (ret != tok->length) {
        write_log(LOG_ERR, "sending token data: only %d of %d bytes written\n",
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
int
recv_token(int *flags, gss_buffer_t tok)
{
    OM_uint32 ret, len;
    unsigned char char_flags;

    ret = read_all(READFD, &char_flags, 1);
    if (ret < 0) {
            write_log(LOG_ERR, "reading token flags\n");
        return -1;
    } else if (!ret) {
        write_log(LOG_ERR, "reading token flags: 0 bytes read\n");
        return -1;
    } else {
        *flags = (int)char_flags;
    }

    ret = read_all(READFD, &len, sizeof(OM_uint32));
    if (ret < 0) {
        write_log(LOG_ERR, "Error while reading token length\n");
        return -1;
    } else if (ret != sizeof(OM_uint32)) {
        write_log(LOG_ERR, "reading token length: %d of %d bytes read\n",
                  ret, sizeof(OM_uint32));
        return -1;
    }

    tok->length = ntohl(len);

    if (tok->length > MAXENCRYPT || tok->length < 0) {
        write_log(LOG_ERR, "Illegal token length: %d\n", tok->length);
        return -1;
    }

    tok->value = (char *)malloc(tok->length);
    if (tok->length && tok->value == NULL) {
        write_log(LOG_ERR, "Out of memory allocating token data\n");
        return -1;
    }

    ret = read_all(READFD, (char *)tok->value, tok->length);
    if (ret < 0) {
        write_log(LOG_ERR, "Error while reading token data");
        free(tok->value);
        return -1;
    } else if (ret != tok->length) {
        write_log(LOG_ERR, "sending token data: only %d of %d bytes written\n",
                 ret, tok->length);
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
 *      fd              file descriptor to write to
 * 	buffer		a buffer fromwhich to write
 *      size	        number of bytes to write
 *
 */
ssize_t
write_all(int fd, const void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    if (size == 0)
        return 0;

    /* Abort the write if we try ten times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 10)
            break;
        status = write(fd, (const char *) buffer + total, size - total);
        if (status > 0)
            count = 0;
        if (status < 0) {
            if (errno != EINTR)
                break;
            status = 0;
        }
    }
    return (total < size) ? -1 : (ssize_t) total;
}

/*
 * Function: read_all
 *
 * Purpose: standard socket/file descriptor read to a particular length
 *
 * Arguments:
 *
 *      fd              file descriptor to read from
 * 	buffer		a buffer into which to read
 *      size	        number of bytes to read
 *
 */
ssize_t
read_all(int fd, void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    if (size == 0)
        return 0;

    /* Abort the read if we try ten times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 10)
            break;
        status = read(fd, buffer + total, size - total);
        if (status > 0)
            count = 0;
        if (status < 0) {
            if (errno != EINTR)
                break;
            status = 0;
        }
    }
    return (total < size) ? -1 : (ssize_t) total;
}

/*
 * Function: read_two
 *
 * Purpose: reads from two streams simultaneously. Stops reading when both 
 *          streams reach EOF.
 *
 * Arguments:
 *
 *      readfd1-2       file descriptor to read from
 * 	buf1-2	        buffer into which to read
 *      nbyte1-2        number of bytes to read
 *
 */
ssize_t
read_two(int readfd1, int readfd2, void *buf1, void *buf2, size_t nbyte1, 
         size_t nbyte2)
{
    void *ptr1, *ptr2;
    ssize_t ret1, ret2;

    ptr1 = buf1;
    ptr2 = buf2;
    ret1 = ret2 = 1;

    while(nbyte1 != 0 && nbyte2 != 0 && (ret1 != 0 || ret2 != 0)) {

        if (ret1 != 0) {
            if ((ret1 = read(readfd1, ptr1, nbyte1)) < 0) {
                if (errno == EINTR)
                    continue;
                else
                    return (ret1);
            }
            ptr1 += ret1;
            nbyte1 -= ret1;
        }

        if (ret2 != 0) {
            if ((ret2 = read(readfd2, ptr2, nbyte2)) < 0) {
                if (errno == EINTR)
                    continue;
                else
                    return (ret2);
            }
            ptr2 += ret2;
            nbyte2 -= ret2;
        }

    }

    * ((char *)ptr1) = '\0';
    * ((char *)ptr2) = '\0';
    return (ptr1 - buf1 + ptr2 - buf2);
}


/* Utility subfunction to display GSSAPI status */
static void
display_status_1(char *m, OM_uint32 code, int type)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx;

    msg_ctx = 0;
    while (1) {
        maj_stat = gss_display_status(&min_stat, code,
                                      type, GSS_C_NULL_OID, &msg_ctx, &msg);
        write_log(LOG_ERR, "GSS-API error %s: %s\n", m,(char *)msg.value);
        gss_release_buffer(&min_stat, &msg);

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
void
display_status(char *msg, OM_uint32 maj_stat,OM_uint32  min_stat)
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

void
display_ctx_flags(OM_uint32 flags)
{

    if (flags & GSS_C_DELEG_FLAG)
        write_log(LOG_DEBUG, "context flag: GSS_C_DELEG_FLAG\n");
    if (flags & GSS_C_MUTUAL_FLAG)
        write_log(LOG_DEBUG, "context flag: GSS_C_MUTUAL_FLAG\n");
    if (flags & GSS_C_REPLAY_FLAG)
        write_log(LOG_DEBUG, "context flag: GSS_C_REPLAY_FLAG\n");
    if (flags & GSS_C_SEQUENCE_FLAG)
        write_log(LOG_DEBUG, "context flag: GSS_C_SEQUENCE_FLAG\n");
    if (flags & GSS_C_CONF_FLAG)
        write_log(LOG_DEBUG, "context flag: GSS_C_CONF_FLAG \n");
    if (flags & GSS_C_INTEG_FLAG)
        write_log(LOG_DEBUG, "context flag: GSS_C_INTEG_FLAG \n");
}

/* Prints the contents of a raw token in hex, byte by byte */
void
print_token(gss_buffer_t tok)
{
    int i;
    unsigned char *p = tok->value;
    char tempbuf[3*MAXBUFFER] = "";
    size_t offset;

    for (offset = 0, i = 0; i < tok->length; i++, p++) {
        offset += snprintf(tempbuf + offset, 3*MAXBUFFER - offset, "%02x ", *p);
        if ((i % 16) == 15) {
            strcat(tempbuf, "\n");
            offset++;
        }
    }

    strcat(tempbuf, "\n");
    write_log(LOG_DEBUG, tempbuf);
}


    /*
    **  Local variables:
    **  mode: c
    **  c-basic-offset: 4
    **  indent-tabs-mode: nil
    **  end:
    */

