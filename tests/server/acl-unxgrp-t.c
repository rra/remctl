/**
  * Test suite for the server ACL unxgrp checking.
  * This file is part of the remctl project.
  *
  * Copyright 2014 - IN2P3 Computing Centre
  * 
  * IN2P3 Computing Centre - written by <remi.ferrand@cc.in2p3.fr>
  * 
  * This file is governed by the CeCILL  license under French law and
  * abiding by the rules of distribution of free software.  You can  use, 
  * modify and/ or redistribute the software under the terms of the CeCILL
  * license as circulated by CEA, CNRS and INRIA at the following URL
  * "http://www.cecill.info". 
  * 
  * As a counterpart to the access to the source code and  rights to copy,
  * modify and redistribute granted by the license, users are provided only
  * with a limited warranty  and the software's author,  the holder of the
  * economic rights,  and the successive licensors  have only  limited
  * liability. 
  * 
  * In this respect, the user's attention is drawn to the risks associated
  * with loading,  using,  modifying and/or developing or reproducing the
  * software by the user in light of its specific status of free software,
  * that may mean  that it is complicated to manipulate,  and  that  also
  * therefore means  that it is reserved for developers  and  experienced
  * professionals having in-depth computer knowledge. Users are therefore
  * encouraged to load and test the software's suitability as regards their
  * requirements in conditions enabling the security of their systems and/or 
  * data to be ensured and,  more generally, to use and operate it in the 
  * same conditions as regards security. 
  * 
  * The fact that you are presently reading this means that you have had
  * knowledge of the CeCILL license and that you accept its terms.
  *
  * See LICENSE for full licensing terms
  */ 

#include <config.h>
#include <portable/system.h>

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

    memset(&getgrnam_r_responses, 0x0, sizeof(getgrnam_r_responses));

    plan(11);
    if (chdir(getenv("SOURCE")) < 0)
        sysbail("can't chdir to SOURCE");

    rule.file = (char *) "TEST";
    rule.acls = (char **) acls;

    acls[0] = "unxgrp:foobargroup";
    acls[1] = NULL;

#ifdef HAVE_GETGRNAM_R

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
    ok(server_config_acl_permit(&rule, "remi/admin@EXAMPLE.ORG"),
    "... with principal with instances but main user in group");

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
    skip_block(9, "UNXGRP support not configured");
#endif

    return 0;
}
