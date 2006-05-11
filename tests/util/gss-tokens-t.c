/* $Id$ */
/* gss-tokens test suite. */

#include <config.h>
#include <system.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* Handle compatibility to older versions of MIT Kerberos. */
#ifndef HAVE_GSS_RFC_OIDS
# define GSS_C_NT_USER_NAME gss_nt_user_name
#endif

/* Heimdal provides a nice #define for this. */
#if !HAVE_DECL_GSS_KRB5_MECHANISM
# include <gssapi/gssapi_krb5.h>
# define GSS_KRB5_MECHANISM gss_mech_krb5
#endif

#include <tests/libtest.h>
#include <util/util.h>

extern char send_buffer[2048];
extern char recv_buffer[2048];
extern size_t send_length;
extern size_t recv_length;
extern int send_flags;
extern int recv_flags;

int
main(void)
{
    char *principal;
    gss_buffer_desc name_buf, server_tok, client_tok, *token_ptr;
    gss_name_t server_name, client_name;
    gss_cred_id_t server_creds;
    gss_ctx_id_t server_ctx, client_ctx;
    OM_uint32 c_stat, c_min_stat, s_stat, s_min_stat, ret_flags;
    gss_OID doid;
    int status, flags;

    test_init(26);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 8, "Kerberos tests not configured");
        return 0;
    }
    putenv("KRB5_KTNAME=data/test.keytab");

    /* We have to set up a context first in order to do this test, which is
       rather annoying. */
    name_buf.value = principal;
    name_buf.length = strlen(principal) + 1;
    s_stat = gss_import_name(&s_min_stat, &name_buf, GSS_C_NT_USER_NAME,
                             &server_name);
    if (s_stat != GSS_S_COMPLETE)
        die("cannot import name");
    s_stat = gss_acquire_cred(&s_min_stat, server_name, 0, GSS_C_NULL_OID_SET,
                              GSS_C_ACCEPT, &server_creds, NULL, NULL);
    if (s_stat != GSS_S_COMPLETE)
        die("cannot acquire creds");
    server_ctx = GSS_C_NO_CONTEXT;
    client_ctx = GSS_C_NO_CONTEXT;
    token_ptr = GSS_C_NO_BUFFER;
    do {
        c_stat = gss_init_sec_context(&c_min_stat, GSS_C_NO_CREDENTIAL,
                                      &client_ctx, server_name,
                                      (const gss_OID) GSS_KRB5_MECHANISM,
                                      GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG,
                                      0, NULL, token_ptr, NULL, &client_tok,
                                      &ret_flags, NULL);
        if (token_ptr != GSS_C_NO_BUFFER)
            gss_release_buffer(&c_min_stat, &server_tok);
        if (client_tok.length == 0)
            break;
        s_stat = gss_accept_sec_context(&s_min_stat, &server_ctx,
                                        server_creds, &client_tok,
                                        GSS_C_NO_CHANNEL_BINDINGS,
                                        &client_name, &doid, &server_tok,
                                        &ret_flags, NULL, NULL);
        gss_release_buffer(&c_min_stat, &client_tok);
        if (server_tok.length == 0)
            break;
        token_ptr = &server_tok;
    } while (c_stat == GSS_S_CONTINUE_NEEDED
             || s_stat == GSS_S_CONTINUE_NEEDED);
    if (c_stat != GSS_S_COMPLETE || s_stat != GSS_S_COMPLETE)
        die("cannot establish context");

    /* Okay, we should now be able to send and receive a token. */
    server_tok.value = "hello";
    server_tok.length = 5;
    status = token_send_priv(0, server_ctx, 3, &server_tok, &s_stat,
                             &s_min_stat);
    ok_int(1, TOKEN_OK, status);
    ok_int(2, 3, send_flags);
    ok(3, send_length > 5);
    server_tok.value = send_buffer;
    server_tok.length = send_length;
    c_stat = gss_unwrap(&c_min_stat, client_ctx, &server_tok, &client_tok,
                        NULL, NULL);
    ok_int(4, GSS_S_COMPLETE, c_stat);
    ok_int(5, 5, client_tok.length);
    ok(6, memcmp(client_tok.value, "hello", 5) == 0);
    gss_release_buffer(&c_min_stat, &client_tok);
    client_tok.length = 0;
    client_tok.value = NULL;
    server_tok.value = "hello";
    server_tok.length = 5;
    status = token_send_priv(0, server_ctx, 3, &server_tok, &s_stat,
                             &s_min_stat);
    ok_int(7, TOKEN_OK, status);
    memcpy(recv_buffer, send_buffer, send_length);
    recv_length = send_length;
    recv_flags = send_flags;
    status = token_recv_priv(0, client_ctx, &flags, &client_tok, 64, &s_stat,
                             &c_min_stat);
    ok_int(8, TOKEN_OK, status);
    ok_int(9, 5, client_tok.length);
    ok(10, memcmp(client_tok.value, "hello", 5) == 0);
    ok_int(11, 3, flags);
    gss_release_buffer(&c_min_stat, &client_tok);

    /* Test receiving too large of a token. */
    status = token_recv_priv(0, client_ctx, &flags, &client_tok, 4, &s_stat,
                             &s_min_stat);
    ok_int(12, TOKEN_FAIL_LARGE, status);

    /* Test receiving a corrupt token. */
    recv_length = 4;
    status = token_recv_priv(0, client_ctx, &flags, &client_tok, 64, &s_stat,
                             &s_min_stat);
    ok_int(13, TOKEN_FAIL_GSSAPI, status);

    /* Now, fake up a token to make sure that token_recv_priv is doing the
       right thing. */
    recv_flags = 5;
    client_tok.value = "hello";
    client_tok.length = 5;
    c_stat = gss_wrap(&c_min_stat, client_ctx, 1, GSS_C_QOP_DEFAULT,
                      &client_tok, NULL, &server_tok);
    ok_int(14, GSS_S_COMPLETE, c_stat);
    recv_length = server_tok.length;
    memcpy(recv_buffer, server_tok.value, server_tok.length);
    gss_release_buffer(&c_min_stat, &server_tok);
    status = token_recv_priv(0, server_ctx, &flags, &server_tok, 64, &s_stat,
                             &s_min_stat);
    ok_int(15, TOKEN_OK, status);
    ok_int(16, 5, flags);
    ok_int(17, 5, server_tok.length);
    ok(18, memcmp(server_tok.value, "hello", 5) == 0);
    gss_release_buffer(&c_min_stat, &server_tok);

    /* Test the stupid protocol v1 MIC stuff. */
    server_tok.value = "hello";
    server_tok.length = 5;
    c_stat = gss_get_mic(&c_min_stat, client_ctx, GSS_C_QOP_DEFAULT,
                         &server_tok, &client_tok);
    ok_int(19, GSS_S_COMPLETE, c_stat);
    memcpy(recv_buffer, client_tok.value, client_tok.length);
    recv_length = client_tok.length;
    recv_flags = TOKEN_MIC;
    status = token_send_priv(0, server_ctx, TOKEN_DATA | TOKEN_SEND_MIC,
                             &server_tok, &s_stat, &s_min_stat);
    ok_int(20, TOKEN_OK, status);
    memcpy(recv_buffer, send_buffer, send_length);
    recv_length = send_length;
    recv_flags = send_flags;
    status = token_recv_priv(0, client_ctx, &flags, &client_tok, 64, &c_stat,
                             &c_min_stat);
    ok_int(21, TOKEN_OK, status);
    ok_int(22, TOKEN_DATA, flags);
    ok_int(23, 5, client_tok.length);
    ok(24, memcmp(client_tok.value, "hello", 5) == 0);
    ok_int(25, TOKEN_MIC, send_flags);
    server_tok.value = send_buffer;
    server_tok.length = send_length;
    s_stat = gss_verify_mic(&s_min_stat, server_ctx, &client_tok, &server_tok,
                            NULL);
    ok_int(26, GSS_S_COMPLETE, s_stat);

    return 0;
}
