/* $Id$
 *
 * Test suite for the server ACL checking.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2007 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <server/internal.h>
#include <tests/libtest.h>
#include <util/util.h>


int
main(void)
{
    struct confline confline = { NULL, NULL, NULL, NULL, NULL, NULL };
    const char *acls[3];

    test_init(23);

    if (access("../data/acl-simple", F_OK) == 0)
        chdir("..");
    else if (access("tests/data/acl-simple", F_OK) == 0)
        chdir("tests");
    if (access("data/acl-simple", F_OK) != 0)
        sysdie("cannot find data/acl-simple");

    confline.acls = (char **) acls;
    acls[0] = "data/acl-simple";
    acls[1] = NULL;
    acls[2] = NULL;

    ok(1, server_config_acl_permit(&confline, "rra@example.org"));
    ok(2, server_config_acl_permit(&confline, "rra@EXAMPLE.COM"));
    ok(3, server_config_acl_permit(&confline, "cindy@EXAMPLE.COM"));
    ok(4, server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok(5, server_config_acl_permit(&confline, "test2@EXAMPLE.COM"));

    ok(6, !server_config_acl_permit(&confline, "rra@EXAMPLE.ORG"));
    ok(7, !server_config_acl_permit(&confline, "rra@example.com"));
    ok(8, !server_config_acl_permit(&confline, "paul@EXAMPLE.COM"));
    ok(9, !server_config_acl_permit(&confline, "peter@EXAMPLE.COM"));

    /* Okay, now capture and check the errors. */
    acls[0] = "data/acl-bad-include";
    acls[1] = "data/acls/valid";
    errors_capture();
    ok(10, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(11, "data/acl-bad-include:1: included file"
              " data/acl-nosuchfile not found\n", errors);
    acls[0] = "data/acl-recursive";
    errors_capture();
    ok(12, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(13, "data/acl-recursive:3: data/acl-recursive recursively"
              " included\n", errors);
    acls[0] = "data/acls/valid-2";
    acls[1] = "data/acl-too-long";
    errors_capture();
    ok(14, server_config_acl_permit(&confline, "test2@EXAMPLE.COM"));
    ok(15, errors == NULL);
    ok(16, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(17, "data/acl-too-long:1: ACL file line too long\n", errors);
    acls[0] = "data/acl-no-such-file";
    acls[1] = "data/acls/valid";
    errors_capture();
    ok(18, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(19, "cannot open ACL file data/acl-no-such-file\n", errors);
    errors_capture();
    ok(20, !server_config_acl_permit(&confline, "test2@EXAMPLE.COM"));
    ok_string(21, "cannot open ACL file data/acl-no-such-file\n", errors);
    acls[0] = "data/acl-bad-syntax";
    errors_capture();
    ok(22, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(23, "data/acl-bad-syntax:2: parse error\n", errors);
    errors_uncapture();

    return 0;
}
