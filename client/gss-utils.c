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

#include <config.h>
#include <system.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <syslog.h>
#include <time.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include "gss-utils.h"
#include <util/util.h>

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
extern int READFD;
extern int WRITEFD;


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

    debug("sending message of length %d", in_buf.length);
    print_token(&in_buf);

    maj_stat = gss_wrap(&min_stat, context, 1, GSS_C_QOP_DEFAULT,
                        &in_buf, &state, &out_buf);

    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while wrapping message", maj_stat, min_stat);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
    } else if (state == 0) {
        warn("gss_sendmsg: message not encrypted");
        return -1;
    }

    /* Send to server. */
    if (token_send(WRITEFD, &out_buf, flags | TOKEN_SEND_MIC) < 0) {
        close(WRITEFD);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
    }

    gss_release_buffer(&min_stat, &out_buf);

    /* Read signature block into out_buf */
    if (token_recv(READFD, &out_buf, &token_flags, MAXENCRYPT) < 0) {
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
    if (token_recv(READFD, &xmit_buf, token_flags, MAXENCRYPT) < 0)
        return -1;

    maj_stat = gss_unwrap(&min_stat, context, &xmit_buf, &msg_buf,
                          &conf_state, (gss_qop_t *) NULL);
    gss_release_buffer(&min_stat, &xmit_buf);

    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while unwrapping message", maj_stat, min_stat);
        return -1;
    } else if (!conf_state) {
        warn("gss_recvmsg: message not encrypted");
        return -1;
    }

    debug("received message of length %d", msg_buf.length);
    print_token(&msg_buf);

    /* fill in the return value */
    *msg = malloc(msg_buf.length);
    memcpy(*msg, msg_buf.value, msg_buf.length);
    *msglength = msg_buf.length;

    /* Send back a MIC checksum. */
    if (*token_flags & TOKEN_SEND_MIC) {
        maj_stat = gss_get_mic(&min_stat, context, GSS_C_QOP_DEFAULT,
                               &msg_buf, &xmit_buf);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("signing message", maj_stat, min_stat);
            return -1;
        }
        if (token_send(WRITEFD, &xmit_buf, TOKEN_MIC) < 0)
            return -1;
        debug("MIC signature sent");
        gss_release_buffer(&min_stat, &xmit_buf);
    }
    gss_release_buffer(&min_stat, &msg_buf);
    return 0;
}


/*
**  Internal utility function to write a GSSAPI status to the log.  The
**  message is prepended by "GSSAPI error" and the provided string.
*/
static void
display_status_1(const char *m, OM_uint32 code, int type)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx;

    msg_ctx = 0;
    do {
        maj_stat = gss_display_status(&min_stat, code,
                                      type, GSS_C_NULL_OID, &msg_ctx, &msg);
        warn("GSSAPI error %s: %s", m, (char *) msg.value);
        gss_release_buffer(&min_stat, &msg);
    } while (msg_ctx != 0);
}


/*
**  Write a GSSAPI status to the log.  Displays both the major and minor
**  status and prepends the provided message to both output lines.
*/
void
display_status(const char *msg, OM_uint32 maj_stat,OM_uint32  min_stat)
{
    display_status_1(msg, maj_stat, GSS_C_GSS_CODE);
    if (min_stat != 0)
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
        debug("context flag: GSS_C_DELEG_FLAG");
    if (flags & GSS_C_MUTUAL_FLAG)
        debug("context flag: GSS_C_MUTUAL_FLAG");
    if (flags & GSS_C_REPLAY_FLAG)
        debug("context flag: GSS_C_REPLAY_FLAG");
    if (flags & GSS_C_SEQUENCE_FLAG)
        debug("context flag: GSS_C_SEQUENCE_FLAG");
    if (flags & GSS_C_CONF_FLAG)
        debug("context flag: GSS_C_CONF_FLAG");
    if (flags & GSS_C_INTEG_FLAG)
        debug("context flag: GSS_C_INTEG_FLAG");
}


/*
**  Print the contents of a raw token in hex, byte by byte, to the log.
*/
void
print_token(gss_buffer_t tok)
{
    unsigned char *p = tok->value;
    char *buf;
    size_t length, i, offset;

    length = tok->length * 3 + 1;
    buf = xmalloc(length);
    for (offset = 0, i = 0; i < tok->length; i++, p++)
        offset += snprintf(buf + offset, length - offset, "%02x ", *p);
    debug("token: %s", buf);
    free(buf);
}


/*
**  Local variables:
**  mode: c
**  c-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/
