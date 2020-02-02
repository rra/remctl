/*
 * Test suite for anonymous authentication.
 *
 * Copyright 2015-2016, 2018 Russ Allbery <eagle@eyrie.org>
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/system.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>
#include <util/protocol.h>


/*
 * Initialize an internal anonymous ticket cache with a random name, make sure
 * that we can get a service ticket for the provided principal, and return the
 * name of the Kerberos ticket cache on success and NULL on failure.  Internal
 * Kerberos errors resort in an abort instead.
 *
 * Some older versions of Heimdal not only can't do PKINIT, but also crash
 * when krb5_get_init_creds_password is called with no password or prompter.
 * Heimdal versions which avoid the crash have krb5_init_creds_set_password,
 * as do all versions of MIT Kerberos which support PKINIT.  So, disable the
 * anonymous tests if that function is not present.
 */
#ifdef HAVE_KRB5
#    if !defined(HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ANONYMOUS) \
        || !defined(HAVE_KRB5_INIT_CREDS_SET_PASSWORD)

static char *
cache_init_anonymous(krb5_context ctx UNUSED, const char *principal UNUSED)
{
    return NULL;
}

#    else /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ANONYMOUS */

static char *
cache_init_anonymous(krb5_context ctx, const char *principal)
{
    krb5_error_code retval;
    krb5_principal princ = NULL;
    krb5_principal test_server = NULL;
    krb5_ccache ccache;
    char *realm;
    char *name = NULL;
    krb5_creds creds, in_creds;
    krb5_creds *out_creds = NULL;
    bool creds_valid = false;
    krb5_get_init_creds_opt *opts = NULL;

    /* Initialize credential structs. */
    memset(&creds, 0, sizeof(creds));
    memset(&in_creds, 0, sizeof(in_creds));

    /* Construct the anonymous principal name. */
    retval = krb5_get_default_realm(ctx, &realm);
    if (retval < 0) {
        diag_krb5(ctx, retval, "cannot find realm for anonymous PKINIT");
        return NULL;
    }
    retval = krb5_build_principal_ext(
        ctx, &princ, (unsigned int) strlen(realm), realm,
        strlen(KRB5_WELLKNOWN_NAME), KRB5_WELLKNOWN_NAME,
        strlen(KRB5_ANON_NAME), KRB5_ANON_NAME, NULL);
    if (retval != 0)
        bail_krb5(ctx, retval, "cannot create anonymous principal");
    krb5_free_default_realm(ctx, realm);

    /*
     * Set up the credential cache the anonymous credentials.  We use a
     * memory cache whose name is based on the pointer value of our Kerberos
     * context, since that should be unique among threads.
     */
    basprintf(&name, "MEMORY:%p", (void *) ctx);
    retval = krb5_cc_resolve(ctx, name, &ccache);
    if (retval != 0)
        bail_krb5(ctx, retval, "cannot create memory ticket cache %s", name);

    /* Obtain the credentials. */
    retval = krb5_get_init_creds_opt_alloc(ctx, &opts);
    if (retval != 0)
        bail_krb5(ctx, retval, "cannot initialize credential options");
    krb5_get_init_creds_opt_set_anonymous(opts, 1);
    krb5_get_init_creds_opt_set_tkt_life(opts, 60);
#        ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_OUT_CCACHE
    krb5_get_init_creds_opt_set_out_ccache(ctx, opts, ccache);
#        endif
    retval = krb5_get_init_creds_password(ctx, &creds, princ, NULL, NULL, NULL,
                                          0, NULL, opts);
    if (retval != 0) {
        diag_krb5(ctx, retval, "cannot perform anonymous PKINIT");
        goto done;
    }
    creds_valid = true;

    /*
     * If set_out_ccache wasn't available, we have to manually set up the
     * ticket cache.  Use the principal from the acquired credentials when
     * initializing the ticket cache, since the realm will not match the realm
     * of our input principal.
     */
#        ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_OUT_CCACHE
    retval = krb5_cc_initialize(ctx, ccache, creds.client);
    if (retval != 0)
        bail_krb5(ctx, retval, "cannot initialize ticket cache");
    retval = krb5_cc_store_cred(ctx, ccache, &creds);
    if (retval != 0)
        bail_krb5(ctx, retval, "cannot store credentials");
#        endif /* !HAVE_KRB5_GET_INIT_CREDS_OPT_SET_OUT_CCACHE */

    /*
     * Now, test to see if we can get credentials for a service.  Usually,
     * anonymous users aren't allowed to get service tickets, so this will
     * often fail and we'll have to skip the test.
     */
    retval = krb5_parse_name(ctx, principal, &test_server);
    if (retval != 0)
        bail_krb5(ctx, retval, "cannot parse principal %s", principal);
    in_creds.client = creds.client;
    in_creds.server = test_server;
    retval = krb5_get_credentials(ctx, 0, ccache, &in_creds, &out_creds);

done:
    if (retval != 0) {
        if (ccache != NULL)
            krb5_cc_destroy(ctx, ccache);
        free(name);
        name = NULL;
    }
    if (princ != NULL)
        krb5_free_principal(ctx, princ);
    if (test_server != NULL)
        krb5_free_principal(ctx, test_server);
    if (opts != NULL)
        krb5_get_init_creds_opt_free(ctx, opts);
    if (creds_valid)
        krb5_free_cred_contents(ctx, &creds);
    if (out_creds != NULL)
        krb5_free_creds(ctx, out_creds);
    return name;
}
#    endif     /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ANONYMOUS */
#endif         /* HAVE_KRB5 */


/*
 * Run the given command and return the error code from the server, or
 * ERROR_INTERNAL if the command unexpectedly succeeded or we didn't get an
 * error code.
 */
static int
test_error(struct remctl *r, const char *arg)
{
    struct remctl_output *output;
    const char *command[] = {"test", NULL, NULL};

    command[1] = arg;
    if (arg == NULL)
        arg = "(null)";
    if (!remctl_command(r, command)) {
        diag("remctl error %s", remctl_error(r));
        return ERROR_INTERNAL;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            diag("test %s returned output: %.*s", arg, (int) output->length,
                 output->data);
            break;
        case REMCTL_OUT_STATUS:
            diag("test %s returned status %d", arg, output->status);
            return ERROR_INTERNAL;
        case REMCTL_OUT_ERROR:
            return output->error;
        case REMCTL_OUT_DONE:
            diag("unexpected done token");
            return ERROR_INTERNAL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return ERROR_INTERNAL;
}


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    int status;
#ifdef HAVE_KRB5
    krb5_context ctx;
    char *cache_name;
    char *krb5ccname = NULL;
#endif

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);

    /* Check to see if we can obtain anonymous credentials. */
#ifdef HAVE_KRB5
    if (krb5_init_context(&ctx) != 0)
        bail("cannot initialize Kerberos");
    cache_name = cache_init_anonymous(ctx, config->principal);
    if (cache_name == NULL)
        skip_all("cannot obtain anonymous credentials");
    basprintf(&krb5ccname, "KRB5CCNAME=%s", cache_name);
    free(cache_name);
#endif

    /*
     * We won't get here unless anonymous PKINIT and obtaining service
     * tickets with anonymous principals is supported.
     */
    plan(1);

    /* Start remctld and then switch to anonymous credentials. */
    remctld_start(config, "data/conf-simple", NULL);
    if (putenv(krb5ccname) < 0)
        sysbail("cannot set KRB5CCNAME");

    /* Test that we get permission denied from an ANYUSER command. */
    r = remctl_new();
    if (!remctl_open(r, "localhost", 14373, config->principal))
        bail("cannot contact remctld");
    status = test_error(r, "test");
    is_int(ERROR_ACCESS, status, "access denied");
    remctl_close(r);

    /* Clean up. */
    if (krb5ccname != NULL) {
        putenv((char *) "KRB5CCNAME=");
        free(krb5ccname);
    }
    return 0;
}
