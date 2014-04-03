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
#include <portable/system.h>
#include <portable/krb5.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/messages.h>

#include <sys/types.h>
#include "getgrnam_r.h"

#define STACK_GETGRNAM_RESP(grp, rc) \
do { \
    struct faked_getgrnam_call s; \
    memset(&s, 0x0, sizeof(s)); \
    s.getgrnam_grp = grp; \
    s.getgrnam_r_rc = rc; \
    memcpy(&getgrnam_r_responses[call_idx], &s, sizeof(s)); \
    call_idx++; \
} while(0)

#define RESET_GETGRNAM_CALL_IDX(v) \
do { \
    call_idx = v; \
} while(0)

#define VERY_LONG_PRINCIPAL 512

/**
 * Dummy group definitions used to override return value of getgrnam
 */
struct group emptygrp = { "emptygrp", NULL, 42, (char *[]) { NULL } };
struct group goodguys = { "goodguys", NULL, 42, (char *[]) { "remi", "eagle", NULL } };
struct group badguys = { "badguys", NULL, 42, (char *[]) { "darth-vader", "darth-maul", "boba-fett", NULL } };

int
main(void)
{
    struct rule rule = {
        NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL
    };
    const char *acls[5];
    int i = 0;

    char long_principal[VERY_LONG_PRINCIPAL];
    char expected_error[VERY_LONG_PRINCIPAL*2];

    memset(&getgrnam_r_responses, 0x0, sizeof(getgrnam_r_responses));

    plan(14);

    if (chdir(getenv("SOURCE")) < 0)
        sysbail("can't chdir to SOURCE");

    rule.file = (char *) "TEST";
    rule.acls = (char **) acls;

    acls[0] = "unxgrp:foobargroup";
    acls[1] = NULL;

#ifdef HAVE_REMCTL_UNXGRP_ACL

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
    acls[0] = "unxgrp:emptygrp";
    acls[1] = NULL;

    ok(!server_config_acl_permit(&rule, "someone@EXAMPLE.ORG"),
    "... with empty group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check behavior when user is expected to be in supplied group ... */
    STACK_GETGRNAM_RESP(&goodguys, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "unxgrp:goodguys";
    acls[1] = NULL;
    ok(server_config_acl_permit(&rule, "remi@EXAMPLE.ORG"), 
    "... with user within group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* ... and when it's not ... */
    ok(!server_config_acl_permit(&rule, "someoneelse@EXAMPLE.ORG"),
    "... with user not in group");

    RESET_GETGRNAM_CALL_IDX(0);

    /* ... and when principal is complex */
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

    memset(&expected_error, 0x0, sizeof(expected_error));
    sprintf(expected_error, "TEST:0: converting krb5 principal %s to localname failed with"
    " status %d (Insufficient space to return complete information)\n", long_principal, KRB5_CONFIG_NOTENUFSPACE);
    expected_error[1023] = '\0';

    is_string(expected_error, errors, "... match error message with principal too long");
    errors_uncapture();

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check when user comes from a not supported REALM */
    ok(!server_config_acl_permit(&rule, "eagle@ANY.OTHER.REALM.FR"),
    "... with user from not supported REALM");

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check behavior when syscall fails */
    STACK_GETGRNAM_RESP(&goodguys, 2);
    RESET_GETGRNAM_CALL_IDX(0);
    errors_capture();
    ok(!server_config_acl_permit(&rule, "remi@EXAMPLE.ORG"), 
    "... with getgrnam_r failing");
    is_string("TEST:0: resolving unix group 'goodguys' failed with status 2\n", errors,
              "... with getgrnam_r error handling");
    errors_uncapture();

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check that deny group works as expected */
    STACK_GETGRNAM_RESP(&badguys, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "deny:unxgrp:badguys";
    acls[1] = NULL;

    ok(!server_config_acl_permit(&rule, "boba-fett@EXAMPLE.ORG"),
    "... with denied user in group");

    RESET_GETGRNAM_CALL_IDX(0);

    ok(!server_config_acl_permit(&rule, "remi@EXAMPLE.ORG"),
    "... with user not in denied group but not allowed");

    RESET_GETGRNAM_CALL_IDX(0);

    /* Check that both deny and "allow" pragma work together */
    STACK_GETGRNAM_RESP(&goodguys, 0);
    STACK_GETGRNAM_RESP(&badguys, 0);
    RESET_GETGRNAM_CALL_IDX(0);
    acls[0] = "unxgrp:goodguys";
    acls[1] = "deny:unxgrp:badguys";
    acls[2] = NULL;

    ok(server_config_acl_permit(&rule, "eagle@EXAMPLE.ORG"),
    "... with user within group plus a deny pragma");

    RESET_GETGRNAM_CALL_IDX(0);

    ok(!server_config_acl_permit(&rule, "darth-maul@EXAMPLE.ORG"),
    "... with user in denied group plus a allow group pragma");
    
    RESET_GETGRNAM_CALL_IDX(0);

    ok(!server_config_acl_permit(&rule, "anyoneelse@EXAMPLE.ORG"),
    "... with user neither in allowed or denied group");

    RESET_GETGRNAM_CALL_IDX(0);

#else
    errors_capture();
    ok(!server_config_acl_permit(&rule, "foobaruser@EXAMPLE.ORG"),
       "UNXGRP");
    is_string("TEST:0: ACL scheme 'unxgrp' is not supported\n", errors,
    "...with not supported error");
    errors_uncapture();
    skip_block(12, "UNXGRP support not configured");
#endif

    return 0;
}
