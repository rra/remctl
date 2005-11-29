/*  $Id$
**
**  Utility functions for remctl.
**
**  Written by Anton Ushakov <antonu@stanford.edu>
**
**  Defines various utility functions used by both remctl and remctld to
**  handle the GSSAPI conversation, to encode and decode tokens, to read the
**  output from programs, to report GSSAPI problems, and similar purposes.
*/

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <gssapi/gssapi_generic.h>
#include "gss-utils.h"

extern int use_syslog;   /* Toggle for sysctl vs stdout/stderr. */
extern int verbose;      /* Turns on debugging output. */

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
extern int READFD;
extern int WRITEFD;


/*
**  Used for all error and debugging printouts.  It directs the output to
**  standard output or error or to syslog as is appropriate.
*/
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


/*
**  Lowercase a string in place.
*/
void 
lowercase(char *string)
{
    char *p;

    for (p = string; *p != '\0'; p++)
        *p = tolower(*p);
}


/*
**  Wraps, encrypts, and sends a data payload token, getting back a MIC
**  checksum that it verifies.  Takes the GSSAPI context, the flags received
**  with the incoming token, the data token, and the length of the token.
**  Returns 0 on success, -1 on failure.  Writes a log message on errors.
**
**  This is not proof of message integrity, as that is inherent in the
**  underlying K5 mechanism.  The MIC is not mandatory to ensure secure
**  transfer, but is present here nonetheless for completeness.
*/
int
gss_sendmsg(gss_ctx_id_t context, int flags, char *msg, OM_uint32 msglength)
{
    gss_buffer_desc in_buf, out_buf;
    int state;
    OM_uint32 maj_stat, min_stat;
    gss_qop_t qop_state;
    int token_flags;

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

    /* Send to server. */
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
**  Receives and wraps a data payload token, sending back a MIC checksum.
**  Takes the GSSAPI context and writes the flags, message, and length of the
**  message into the provided parameters, returning 0 on success and -1 on
**  failure.  On failure, a log message is also recorded.
**
**  The MIC checksum is not proof of message integrity, as that is inherent in
**  the underlying K5 mechanism.  The MIC is not mandatory to ensure secure
**  transfer, but is present here nonetheless for completeness.
*/
int
gss_recvmsg(gss_ctx_id_t context, int *token_flags, char **msg,
            OM_uint32 *msglength)
{
    gss_buffer_desc xmit_buf, msg_buf;
    OM_uint32 maj_stat, min_stat;
    int conf_state;

    /* Receive the message token. */
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

    /* Send back a MIC checksum. */
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
    return 0;
}


/*
**  Send a token to a file descriptor.  Takes the flags (a single byte, even
**  though they're passed in as an integer) and the token and writes them to
**  WRITEFD, returning 0 on success and -1 on error or if all the data could
**  not be written.  A log message is recorded on error.
*/
int
send_token(int flags, gss_buffer_t tok)
{
    OM_uint32 len, ret, buflen;
    char *buffer;
    unsigned char char_flags = (unsigned char)flags;

    len = htonl(tok->length);

    /* Send out the whole message in a single write. */
    buflen = 1 + sizeof(OM_uint32) + tok->length;
    buffer = malloc(buflen);
    memcpy(buffer, &char_flags, 1);
    memcpy(buffer + 1, &len, sizeof(OM_uint32));
    memcpy(buffer + 1 + sizeof(OM_uint32), tok->value, tok->length);
    ret = write_all(WRITEFD, buffer, buflen);
    free(buffer);
    if (ret != buflen) {
        write_log(LOG_ERR, "Error while sending token: %d, %d\n", buflen, ret);
        return -1;
    }
    return 0;
}


/*
**  Receive a token from a file descriptor.  Takes pointers into which to
**  store the flags and the token, and returns 0 on success and -1 on failure
**  or if it can't read all of the data.  A log message is recorded on
**  failure.
**
**  recv_token reads the token flags (a single byte, even though they're
**  stored into an integer, then reads the token length (as a network long),
**  allocates memory to hold the data, and then reads the token data from the
**  file descriptor READFD.  It blocks to read the length and data, if
**  necessary.  On a successful return, the token should be freed with
**  gss_release_buffer.
*/
int
recv_token(int *flags, gss_buffer_t tok)
{
    ssize_t ret;
    OM_uint32 len;
    unsigned char char_flags;

    ret = read_all(READFD, &char_flags, 1);
    if (ret < 0) {
        write_log(LOG_ERR, "reading token flags\n");
        return -1;
    } else if (!ret) {
        write_log(LOG_ERR, "reading token flags: 0 bytes read\n");
        return -1;
    } else {
        *flags = char_flags;
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

    if (tok->length > MAXENCRYPT) {
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
    } else if (ret != (ssize_t) tok->length) {
        write_log(LOG_ERR, "sending token data: only %d of %d bytes written\n",
                 ret, tok->length);
        free(tok->value);
        return -1;
    }

    return 0;
}


/*
**  Equivalent to write, but handles EINTR or EAGAIN by retrying and repeats
**  if the write is partial until all the data has been written.
*/
ssize_t
write_all(int fd, const void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    if (size == 0)
        return 0;

    /* Abort the write if we try 100 times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 100)
            break;
        status = write(fd, (const char *) buffer + total, size - total);
        if (status > 0)
            count = 0;
        if (status < 0) {
            if ((errno != EINTR) && (errno != EAGAIN))
                break;
            status = 0;
        }
    }
    return (total < size) ? -1 : (ssize_t) total;
}


/*
**  Equivalent to read, but reads all the available data up to the buffer
**  length, using multiple reads if needed and handling EINTR and EAGAIN.
*/
ssize_t
read_all(int fd, void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    if (size == 0)
        return 0;

    /* Abort the read if we try 100 times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 100)
            break;
        status = read(fd, (char *) buffer + total, size - total);
        if (status > 0)
            count = 0;
        if (status < 0) {
            if ((errno != EINTR) && (errno != EAGAIN))
                break;
            status = 0;
        }
    }
    return (total < size) ? -1 : (ssize_t) total;
}


/*
**  Reads from two streams simultaneously into two different buffers, stopping
**  when both streams reach EOF.  If the buffer fills, truncate at the buffer
**  size but always nul-terminate.
*/
ssize_t
read_two(int readfd1, int readfd2, void *buf1, void *buf2, size_t nbyte1, 
         size_t nbyte2)
{
    void *ptr1, *ptr2;
    ssize_t ret1, ret2;
    char tempbuf[MAXBUFFER];
    int status, max;
    fd_set readfds;

    ptr1 = buf1;
    ptr2 = buf2;
    ret1 = ret2 = 1;

    while (ret1 != 0 || ret2 != 0) {
        FD_ZERO(&readfds);
        if (ret1 != 0)
            FD_SET(readfd1, &readfds);
        if (ret2 != 0)
            FD_SET(readfd2, &readfds);
        max = (readfd1 > readfd2) ? readfd1 : readfd2;
        status = select(max + 1, &readfds, NULL, NULL, NULL);
        if (status == 0)
            continue;
        if (status < 0)
            return status;

        if (FD_ISSET(readfd1, &readfds)) {
            if (nbyte1 != 0) {
                if ((ret1 = read(readfd1, ptr1, nbyte1)) < 0) {
                    if ((errno != EINTR) && (errno != EAGAIN))
                        return (ret1);
                } else {
                    ptr1 = (char *) ptr1 + ret1;
                    nbyte1 -= ret1;
                }
            } else {
                tempbuf[0] = '\0';
                if ((ret1 = read(readfd1, tempbuf, MAXBUFFER)) < 0) {
                    if ((errno != EINTR) && (errno != EAGAIN))
                        return (ret1);
                }
            }
        }

        if (FD_ISSET(readfd2, &readfds)) {
            if (nbyte2 != 0) {
                if ((ret2 = read(readfd2, ptr2, nbyte2)) < 0) {
                    if ((errno != EINTR) && (errno != EAGAIN))
                        return (ret2);
                } else {
                    ptr2 = (char *) ptr2 + ret2;
                    nbyte2 -= ret2;
                }
            } else {
                tempbuf[0] = '\0';
                if ((ret2 = read(readfd2, tempbuf, MAXBUFFER)) < 0) {
                    if ((errno != EINTR) &&  (errno != EAGAIN))
                        return (ret2);
                }
            }
        }
    }

    * ((char *)ptr1) = '\0';
    * ((char *)ptr2) = '\0';
    return ((char *) ptr1 - (char *) buf1 + (char *) ptr2 - (char *) buf2);
}


/*
**  Internal utility function to write a GSSAPI status to the log.  The
**  message is prepended by "GSS-API" error and the provided string.
*/
static void
display_status_1(const char *m, OM_uint32 code, int type)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx;

    msg_ctx = 0;
    while (1) {
        maj_stat = gss_display_status(&min_stat, code,
                                      type, GSS_C_NULL_OID, &msg_ctx, &msg);
        write_log(LOG_ERR, "GSS-API error %s: %s\n", m, (char *) msg.value);
        gss_release_buffer(&min_stat, &msg);
        if (!msg_ctx)
            break;
    }
}


/*
**  Write a GSSAPI status to the log.  Displays both the major and minor
**  status and prepends the provided message to both output lines.
*/
void
display_status(const char *msg, OM_uint32 maj_stat,OM_uint32  min_stat)
{
    display_status_1(msg, maj_stat, GSS_C_GSS_CODE);
    display_status_1(msg, min_stat, GSS_C_MECH_CODE);
}


/*
**  Write a GSSAPI context flag to the log in a human-readable form, preceeded
**  by "context flag: ".
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


/*
**  Print the contents of a raw token in hex, byte by byte, to the log.
*/
void
print_token(gss_buffer_t tok)
{
    unsigned char *p = tok->value;
    char tempbuf[5*MAXBUFFER] = "";
    size_t i, offset;

    for (offset = 0, i = 0; i < tok->length; i++, p++) {
        offset += snprintf(tempbuf + offset, 5*MAXBUFFER - offset, "%02x ",
                           *p);
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
