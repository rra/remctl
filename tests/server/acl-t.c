/*
 * Test suite for the server ACL checking.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2007, 2008 Board of Trustees, Leland Stanford Jr. University
 * Copyright 2008 Jeffrey Hutzelman
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
    struct confline confline = { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL };
    const char *acls[5];

    test_init(50);

    if (access("../data/acl-simple", F_OK) == 0)
        chdir("..");
    else if (access("tests/data/acl-simple", F_OK) == 0)
        chdir("tests");
    if (access("data/acl-simple", F_OK) != 0)
        sysdie("cannot find data/acl-simple");

    confline.file = (char *) "TEST";
    confline.acls = (char **) acls;
    acls[0] = "data/acl-simple";
    acls[1] = NULL;
    acls[2] = NULL;
    acls[3] = NULL;
    acls[4] = NULL;

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
    ok_string(19, "TEST:0: included file data/acl-no-such-file not found\n",
              errors);
    errors_capture();
    ok(20, !server_config_acl_permit(&confline, "test2@EXAMPLE.COM"));
    ok_string(21, "TEST:0: included file data/acl-no-such-file not found\n",
              errors);
    acls[0] = "data/acl-bad-syntax";
    errors_capture();
    ok(22, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(23, "data/acl-bad-syntax:2: parse error\n", errors);
    errors_uncapture();

    /* Check that file: works at the top level. */
    acls[0] = "file:data/acl-simple";
    acls[1] = NULL;
    ok(24, server_config_acl_permit(&confline, "rra@example.org"));
    ok(25, !server_config_acl_permit(&confline, "rra@EXAMPLE.ORG"));

    /* Check that include syntax works. */
    ok(26, server_config_acl_permit(&confline, "incfile@EXAMPLE.ORG"));
    ok(27, server_config_acl_permit(&confline, "incfdir@EXAMPLE.ORG"));
    ok(28, server_config_acl_permit(&confline, "explicit@EXAMPLE.COM"));
    ok(29, server_config_acl_permit(&confline, "direct@EXAMPLE.COM"));
    ok(30, server_config_acl_permit(&confline, "good@EXAMPLE.ORG"));
    ok(31, !server_config_acl_permit(&confline, "evil@EXAMPLE.ORG"));

    /* Check that princ: works at the top level. */
    acls[0] = "princ:direct@EXAMPLE.NET";
    ok(32, server_config_acl_permit(&confline, "direct@EXAMPLE.NET"));
    ok(33, !server_config_acl_permit(&confline, "wrong@EXAMPLE.NET"));

    /* Check that deny: works at the top level. */
    acls[0] = "deny:princ:evil@EXAMPLE.NET";
    acls[1] = "princ:good@EXAMPLE.NET";
    acls[2] = "princ:evil@EXAMPLE.NET";
    acls[3] = NULL;
    ok(34, server_config_acl_permit(&confline, "good@EXAMPLE.NET"));
    ok(35, !server_config_acl_permit(&confline, "evil@EXAMPLE.NET"));

    /* And make sure deny interacts correctly with files. */
    acls[0] = "data/acl-simple";
    acls[1] = "princ:evil@EXAMPLE.NET";
    acls[2] = NULL;
    ok(36, !server_config_acl_permit(&confline, "evil@EXAMPLE.NET"));
    acls[0] = "deny:princ:rra@example.org";
    acls[1] = "data/acl-simple";
    ok(37, !server_config_acl_permit(&confline, "rra@example.org"));

    /*
     * Ensure deny never affirmatively grants access, so deny:deny: matches
     * nothing.
     */
    acls[0] = "deny:deny:princ:rra@example.org";
    acls[1] = "data/acl-simple";
    ok(38, server_config_acl_permit(&confline, "rra@example.org"));
    ok(39, server_config_acl_permit(&confline, "rra@EXAMPLE.COM"));

    /*
     * Denying a file denies anything that would match the file, and nothing
     * that wouldn't, including due to an embedded deny.
     */
    acls[0] = "deny:file:data/acl-simple";
    acls[1] = "princ:explicit@EXAMPLE.COM";
    acls[2] = "princ:evil@EXAMPLE.ORG";
    acls[3] = "princ:evil@EXAMPLE.NET";
    acls[4] = NULL;
    ok(40, !server_config_acl_permit(&confline, "explicit@EXAMPLE.COM"));
    ok(41, server_config_acl_permit(&confline, "evil@EXAMPLE.ORG"));
    ok(42, server_config_acl_permit(&confline, "evil@EXAMPLE.NET"));

    /* Check for an invalid ACL scheme. */
    acls[0] = "ihateyou:verymuch";
    acls[1] = "data/acls/valid";
    errors_capture();
    ok(43, !server_config_acl_permit(&confline, "test@EXAMPLE.COM"));
    ok_string(44, "TEST:0: invalid ACL scheme 'ihateyou'\n", errors);
    errors_uncapture();

    /*
     * Check for GPUT ACLs and also make sure they behave sanely when GPUT
     * support is not compiled.
     */
    server_config_set_gput_file((char *) "data/gput");
    acls[0] = "gput:test";
    acls[1] = NULL;
#ifdef HAVE_GPUT
    ok(45, server_config_acl_permit(&confline, "priv@EXAMPLE.ORG"));
    ok(46, !server_config_acl_permit(&confline, "nonpriv@EXAMPLE.ORG"));
    ok(47, !server_config_acl_permit(&confline, "priv@EXAMPLE.NET"));
    acls[0] = "gput:test[%@EXAMPLE.NET]";
    ok(48, server_config_acl_permit(&confline, "priv@EXAMPLE.NET"));
    ok(49, !server_config_acl_permit(&confline, "nonpriv@EXAMPLE.NET"));
    ok(50, !server_config_acl_permit(&confline, "priv@EXAMPLE.ORG"));
#else
    errors_capture();
    ok(45, !server_config_acl_permit(&confline, "priv@EXAMPLE.ORG"));
    ok_string(46, "TEST:0: ACL scheme 'gput' is not supported\n", errors);
    errors_uncapture();
    skip_block(47, 4, "GPUT support not configured");
#endif

    return 0;
}
