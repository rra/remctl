/*
 * Test suite for localgroup ACL scheme.
 *
 * Written by Remi Ferrand <remi.ferrand@cc.in2p3.fr>
 * Revisions by Russ Allbery <eagle@eyrie.org>
 * Copyright 2015, 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 * Copyright 2014 IN2P3 Computing Centre - CNRS
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#ifdef HAVE_KRB5
# include <portable/krb5.h>
#endif
#include <portable/system.h>

#include <grp.h>
#include <pwd.h>

#include <server/internal.h>
#include <tests/server/acl/fake-getgrnam.h>
#include <tests/server/acl/fake-getpwnam.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/messages.h>
#include <tests/tap/string.h>

/*
 * Lists of users used to populate the group membership field of various test
 * group structs.
 */
static const char *const empty_members[] = { NULL };
static const char *const goodguys_members[] = { "remi", "eagle", NULL };
static const char *const badguys_members[] = {
    "darth-vader", "darth-maul", "boba-fett", NULL
};

/* Dummy group definitions used as return values from getgrnam_r. */
static const struct group empty = {
    (char *) "empty", NULL, 42, (char **) empty_members
};
static const struct group goodguys = {
    (char *) "goodguys", NULL, 42, (char **) goodguys_members
};
static const struct group badguys = {
    (char *) "badguys", NULL, 42, (char **) badguys_members
};

/*
 * Length of a principal that will be longer than the buffer size we use for
 * the results of krb5_aname_to_localname.
 */
#define VERY_LONG_PRINCIPAL (BUFSIZ + 2)


/*
 * Calls server_config_acl_permit with the given user identity and anonymous
 * set to false and returns the result.  Wrapped in a function so that we can
 * cobble up a client struct.
 */
static bool
acl_permit(const struct rule *rule, const char *user)
{
    struct client client = {
        -1, -1, NULL, NULL, 0, NULL, (char *) user, false, 0, 0, false, false,
        NULL, NULL, NULL
    };
    return server_config_acl_permit(rule, &client);
}


/* If we can't build with localgroup support, test the failure message. */
#if !defined(HAVE_KRB5) || !defined(HAVE_GETGRNAM_R)
int
main(void)
{
    const char *acls[5];
    const struct rule rule = {
        (char *) "TEST", 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, 0,
        NULL, NULL, NULL
    };

    plan(2);

    errors_capture();
    acls[0] = "localgroup:foobargroup";
    acls[1] = NULL;
    ok(!server_config_acl_permit(&rule, "foobaruser@EXAMPLE.ORG"),
       "localgroup ACL check fails");
    is_string("TEST:0: ACL scheme 'localgroup' is not supported\n", errors,
              "...with not supported error");
    free(errors);
    return 0;
}

/* Otherwise, do the regular testing. */
#else /* HAVE_KRB5 && HAVE_GETGRNAM_R */

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
    krb5_context ctx;
    const char *message;
    char *expected;
    char long_principal[VERY_LONG_PRINCIPAL];
    const char *acls[5];
    const struct rule rule = {
        (char *) "TEST", 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, 0,
        NULL, NULL, (char **) acls
    };

    plan(16);

    /* Use a krb5.conf with a default realm of EXAMPLE.ORG. */
    kerberos_generate_conf("EXAMPLE.ORG");

    /* Check behavior with empty groups. */
    fake_queue_group(&empty, 0);
    set_passwd("someone", 0);
    acls[0] = "localgroup:empty";
    acls[1] = NULL;
    ok(!acl_permit(&rule, "someone@EXAMPLE.ORG"), "Empty");

    /* Check behavior when user is expected to be in supplied group. */
    fake_queue_group(&goodguys, 0);
    set_passwd("remi", 0);
    acls[0] = "localgroup:goodguys";
    acls[1] = NULL;
    ok(acl_permit(&rule, "remi@EXAMPLE.ORG"), "User in group");

    /* And when the user is not in the supplied group. */
    fake_queue_group(&goodguys, 0);
    set_passwd("someoneelse", 0);
    ok(!acl_permit(&rule, "someoneelse@EXAMPLE.ORG"), "User not in group");

    /* Check that the user's primary group also counts. */
    fake_queue_group(&goodguys, 0);
    set_passwd("otheruser", 42);
    ok(acl_permit(&rule, "otheruser@EXAMPLE.ORG"),
       "User has group as primary group");

    /* And when the user does not convert to a local user or is complex. */
    fake_queue_group(&goodguys, 0);
    set_passwd("remi", 0);
    errors_capture();
    ok(!acl_permit(&rule, "remi/admin@EXAMPLE.ORG"),
       "User with instance with base user in group");
    is_string(NULL, errors, "...with no error");

    /* Principal name is too long. */
    fake_queue_group(&goodguys, 0);
    memset(long_principal, 'A', sizeof(long_principal));
    long_principal[sizeof(long_principal) - 1] = '\0';
    errors_capture();
    ok(!acl_permit(&rule, long_principal), "Long principal");

    /* Determine the expected error message and check it. */
    if (krb5_init_context(&ctx) != 0)
        bail("cannot create Kerberos context");
    message = krb5_get_error_message(ctx, KRB5_CONFIG_NOTENUFSPACE);
    basprintf(&expected, "conversion of %s to local name failed: %s\n",
              long_principal, message);
    krb5_free_context(ctx);
    is_string(expected, errors, "...with correct error message");
    free(expected);

    /* Unsupported realm. */
    fake_queue_group(&goodguys, 0);
    set_passwd("eagle", 0);
    ok(!acl_permit(&rule, "eagle@ANY.OTHER.REALM.FR"), "Non-local realm");

    /* Check behavior when syscall fails */
    fake_queue_group(&goodguys, EPERM);
    set_passwd("remi", 0);
    errors_capture();
    ok(!acl_permit(&rule, "remi@EXAMPLE.ORG"), "Failing getgrnam_r");
    is_string("TEST:0: retrieving membership of localgroup goodguys failed\n",
              errors, "...with correct error message");

    /* Check that deny group works as expected */
    fake_queue_group(&badguys, 0);
    set_passwd("boba-fett", 0);
    acls[0] = "deny:localgroup:badguys";
    acls[1] = NULL;
    ok(!acl_permit(&rule, "boba-fett@EXAMPLE.ORG"), "Denied user");
    fake_queue_group(&badguys, 0);
    set_passwd("remi", 0);
    ok(!acl_permit(&rule, "remi@EXAMPLE.ORG"),
       "User not in denied group but also not allowed");

    /* Check that both deny and "allow" pragma work together */
    fake_queue_group(&goodguys, 0);
    fake_queue_group(&badguys, 0);
    set_passwd("eagle", 0);
    acls[0] = "localgroup:goodguys";
    acls[1] = "deny:localgroup:badguys";
    acls[2] = NULL;
    ok(acl_permit(&rule, "eagle@EXAMPLE.ORG"),
       "User in allowed group plus a denied group");
    fake_queue_group(&goodguys, 0);
    fake_queue_group(&badguys, 0);
    set_passwd("darth-maul", 0);
    ok(!acl_permit(&rule, "darth-maul@EXAMPLE.ORG"),
       "User in a denied group plus an allowed group");
    fake_queue_group(&goodguys, 0);
    fake_queue_group(&badguys, 0);
    set_passwd("anyoneelse", 0);
    ok(!acl_permit(&rule, "anyoneelse@EXAMPLE.ORG"),
       "User in neither denied nor allowed group");

    /* Clean up. */
    free(errors);
    return 0;
}

#endif /* HAVE_KRB5 && HAVE_GETGRNAM_R */
