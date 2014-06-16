/*
 * This file is part of the remctl project.
 *
 * Test suite for unxgrp ACL scheme.
 *
 * Written by Remi Ferrand <remi.ferrand@cc.in2p3.fr>
 * Copyright 2014
 *     IN2P3 Computing Centre - CNRS
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/system.h>

#include <pwd.h>

#include <server/internal.h>
#include <tests/server/acl/fake-getgrnam.h>
#include <tests/server/acl/fake-getpwnam.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/messages.h>
#include <tests/tap/string.h>

#define STACK_GETGRNAM_RESP(grp, rc) \
do { \
    struct faked_getgrnam_call s; \
    memset(&s, 0x0, sizeof(s)); \
    s.getgrnam_grp = grp;               \
    s.getgrnam_r_rc = rc; \
    memcpy(&getgrnam_r_responses[call_idx], &s, sizeof(s)); \
    call_idx++; \
} while(0)

#define RESET_GETGRNAM_CALL_IDX(v) \
do { \
    call_idx = v; \
} while(0)

#define VERY_LONG_PRINCIPAL (BUFSIZ + 2)

/**
 * Dummy group definitions used to override return value of getgrnam
 */
struct group emptygrp = { (char *) "emptygrp", NULL, 42, (char *[]) { NULL } };
struct group goodguys = { (char *) "goodguys", NULL, 42, (char *[]) { (char *) "remi", (char *) "eagle", NULL } };
struct group badguys = { (char *) "badguys", NULL, 42, (char *[]) { (char *) "darth-vader", (char *) "darth-maul", (char *) "boba-fett", NULL } };


/*
 * Set a fake getpwnam response for the given user.  This will have no useful
 * data other than the GID, which will be set to match the given value.
 */
static void
set_passwd(const char *user, gid_t gid)
{
    struct passwd pw;

    memset(&pw, 0, sizeof(pw));
    pw.pw_name = (char *) user;
    pw.pw_uid  = 1000;
    pw.pw_gid  = gid;
    fake_set_passwd(&pw);
}


int
main(void)
{
    struct rule rule = {
        NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL
    };
    const char *acls[5];
    int i = 0;

    char long_principal[VERY_LONG_PRINCIPAL];
    char *expected;
    krb5_context ctx;
    const char *message;

    memset(&getgrnam_r_responses, 0x0, sizeof(getgrnam_r_responses));

    plan(14);

    /* Use a krb5.conf with a default realm of EXAMPLE.ORG. */
    kerberos_generate_conf("EXAMPLE.ORG");

    if (chdir(getenv("SOURCE")) < 0)
        sysbail("can't chdir to SOURCE");

    rule.file = (char *) "TEST";
    rule.acls = (char **) acls;

    acls[0] = "localgroup:foobargroup";
    acls[1] = NULL;

#if defined(HAVE_KRB5) && defined(HAVE_GETGRNAM_R)

    /*
     * Note(remi):
     * Those tests requires a krb5 configuration with realm EXAMPLE.ORG
     * defined.
     *
     * If not, krb5_aname_to_localname will refuse to resolve a principal
     * from an unknown REALM to a local user name (at least with Heimdal libs).
     */

    /* Check behavior with empty groups */
    STACK_GETGRNAM_RESP(&emptygrp, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "localgroup:emptygrp";
    acls[1] = NULL;

    ok(!server_config_acl_permit(&rule, "someone@EXAMPLE.ORG"),
    "... with empty group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check behavior when user is expected to be in supplied group ... */
    STACK_GETGRNAM_RESP(&goodguys, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "localgroup:goodguys";
    acls[1] = NULL;
    set_passwd("remi", 0);
    ok(server_config_acl_permit(&rule, "remi@EXAMPLE.ORG"),
    "... with user within group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* ... and when it's not ... */
    set_passwd("someoneelse", 0);
    ok(!server_config_acl_permit(&rule, "someoneelse@EXAMPLE.ORG"),
    "... with user not in group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* ... and when principal is complex */
    set_passwd("remi", 0);
    ok(!server_config_acl_permit(&rule, "remi/admin@EXAMPLE.ORG"),
    "... with principal with instances but main user in group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* and when principal name is too long */
    for (i = 0; i < VERY_LONG_PRINCIPAL - 1; i++)
        long_principal[i] = 'A';
    long_principal[VERY_LONG_PRINCIPAL-1] = '\0';

    errors_capture();
    ok(!server_config_acl_permit(&rule, long_principal),
    "... with long_principal very very long");

    /* Determine the expected error message. */
    if (krb5_init_context(&ctx) != 0)
        bail("cannot create Kerberos context");
    message = krb5_get_error_message(ctx, KRB5_CONFIG_NOTENUFSPACE);
    basprintf(&expected, "conversion of %s to local name failed: %s\n",
              long_principal, message);
    is_string(expected, errors, "... match error message with principal too long");
    errors_uncapture();
    free(expected);

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check when user comes from a not supported REALM */
    set_passwd("eagle", 0);
    ok(!server_config_acl_permit(&rule, "eagle@ANY.OTHER.REALM.FR"),
    "... with user from not supported REALM");

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check behavior when syscall fails */
    STACK_GETGRNAM_RESP(&goodguys, EPERM);
    RESET_GETGRNAM_CALL_IDX(0);
    errors_capture();
    set_passwd("remi", 0);
    ok(!server_config_acl_permit(&rule, "remi@EXAMPLE.ORG"),
    "... with getgrnam_r failing");
    is_string("TEST:0: retrieving membership of localgroup goodguys failed\n",
              errors, "... with getgrnam_r error handling");
    errors_uncapture();

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check that deny group works as expected */
    STACK_GETGRNAM_RESP(&badguys, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "deny:localgroup:badguys";
    acls[1] = NULL;

    set_passwd("boba-fett", 0);
    ok(!server_config_acl_permit(&rule, "boba-fett@EXAMPLE.ORG"),
    "... with denied user in group");

    RESET_GETGRNAM_CALL_IDX(0);

    set_passwd("remi", 0);
    ok(!server_config_acl_permit(&rule, "remi@EXAMPLE.ORG"),
    "... with user not in denied group but not allowed");

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check that both deny and "allow" pragma work together */
    STACK_GETGRNAM_RESP(&goodguys, 0);
    STACK_GETGRNAM_RESP(&badguys, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "localgroup:goodguys";
    acls[1] = "deny:localgroup:badguys";
    acls[2] = NULL;

    set_passwd("eagle", 0);
    ok(server_config_acl_permit(&rule, "eagle@EXAMPLE.ORG"),
    "... with user within group plus a deny pragma");

    RESET_GETGRNAM_CALL_IDX(0);

    set_passwd("darth-maul", 0);
    ok(!server_config_acl_permit(&rule, "darth-maul@EXAMPLE.ORG"),
    "... with user in denied group plus a allow group pragma");

    RESET_GETGRNAM_CALL_IDX(0);

    set_passwd("anyoneelse", 0);
    ok(!server_config_acl_permit(&rule, "anyoneelse@EXAMPLE.ORG"),
    "... with user neither in allowed or denied group");

    RESET_GETGRNAM_CALL_IDX(0);

#else
    errors_capture();
    ok(!server_config_acl_permit(&rule, "foobaruser@EXAMPLE.ORG"),
       "localgroup ACL check fails");
    is_string("TEST:0: ACL scheme 'localgroup' is not supported\n", errors,
    "...with not supported error");
    errors_uncapture();
    skip_block(12, "localgroup ACL support not supported");
#endif

    return 0;
}
