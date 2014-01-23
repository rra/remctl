/*
 * Test suite for the server ACL checking.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2008, 2009, 2010, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2008 Carnegie Mellon University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/messages.h>


int
main(void)
{
    struct rule rule = {
        NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 0, NULL, NULL, NULL
    };
    const char *acls[5];

    plan(70);
    if (chdir(getenv("SOURCE")) < 0)
        sysbail("can't chdir to SOURCE");

    rule.file = (char *) "TEST";
    rule.acls = (char **) acls;
    acls[0] = "data/acl-simple";
    acls[1] = NULL;
    acls[2] = NULL;
    acls[3] = NULL;
    acls[4] = NULL;

    ok(server_config_acl_permit(&rule, "rra@example.org"), "simple 1");
    ok(server_config_acl_permit(&rule, "rra@EXAMPLE.COM"), "simple 2");
    ok(server_config_acl_permit(&rule, "cindy@EXAMPLE.COM"), "simple 3");
    ok(server_config_acl_permit(&rule, "test@EXAMPLE.COM"), "simple 4");
    ok(server_config_acl_permit(&rule, "test2@EXAMPLE.COM"), "simple 5");

    ok(!server_config_acl_permit(&rule, "rra@EXAMPLE.ORG"), "no 1");
    ok(!server_config_acl_permit(&rule, "rra@example.com"), "no 2");
    ok(!server_config_acl_permit(&rule, "paul@EXAMPLE.COM"), "no 3");
    ok(!server_config_acl_permit(&rule, "peter@EXAMPLE.COM"), "no 4");

    /* Okay, now capture and check the errors. */
    acls[0] = "data/acl-bad-include";
    acls[1] = "data/acls/valid";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "included file not found");
    is_string("data/acl-bad-include:1: included file data/acl-nosuchfile"
              " not found\n", errors, "...and correct error message");
    acls[0] = "data/acl-recursive";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "recursive ACL inclusion");
    is_string("data/acl-recursive:3: data/acl-recursive recursively"
              " included\n", errors, "...and correct error message");
    acls[0] = "data/acls/valid-2";
    acls[1] = "data/acl-too-long";
    errors_capture();
    ok(server_config_acl_permit(&rule, "test2@EXAMPLE.COM"),
       "granted access based on first ACL file");
    ok(errors == NULL, "...with no errors");
    ok(!server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "...but failed when we hit second file with long line");
    is_string("data/acl-too-long:1: ACL file line too long\n", errors,
              "...with correct error message");
    acls[0] = "data/acl-no-such-file";
    acls[1] = "data/acls/valid";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "no such ACL file");
    is_string("TEST:0: included file data/acl-no-such-file not found\n",
              errors, "...with correct error message");
    errors_capture();
    ok(!server_config_acl_permit(&rule, "test2@EXAMPLE.COM"),
       "...even with a principal in an ACL file");
    is_string("TEST:0: included file data/acl-no-such-file not found\n",
              errors, "...still with right error message");
    acls[0] = "data/acl-bad-syntax";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "incorrect syntax");
    is_string("data/acl-bad-syntax:2: parse error\n", errors,
              "...with correct error message");
    errors_uncapture();

    /* Check that file: works at the top level. */
    acls[0] = "file:data/acl-simple";
    acls[1] = NULL;
    ok(server_config_acl_permit(&rule, "rra@example.org"),
       "file: success");
    ok(!server_config_acl_permit(&rule, "rra@EXAMPLE.ORG"),
       "file: failure");

    /* Check that include syntax works. */
    ok(server_config_acl_permit(&rule, "incfile@EXAMPLE.ORG"),
       "include 1");
    ok(server_config_acl_permit(&rule, "incfdir@EXAMPLE.ORG"),
       "include 2");
    ok(server_config_acl_permit(&rule, "explicit@EXAMPLE.COM"),
       "include 3");
    ok(server_config_acl_permit(&rule, "direct@EXAMPLE.COM"),
       "include 4");
    ok(server_config_acl_permit(&rule, "good@EXAMPLE.ORG"),
       "include 5");
    ok(!server_config_acl_permit(&rule, "evil@EXAMPLE.ORG"),
       "include failure");

    /* Check that princ: works at the top level. */
    acls[0] = "princ:direct@EXAMPLE.NET";
    ok(server_config_acl_permit(&rule, "direct@EXAMPLE.NET"),
       "princ: success");
    ok(!server_config_acl_permit(&rule, "wrong@EXAMPLE.NET"),
       "princ: failure");

    /* Check that deny: works at the top level. */
    acls[0] = "deny:princ:evil@EXAMPLE.NET";
    acls[1] = "princ:good@EXAMPLE.NET";
    acls[2] = "princ:evil@EXAMPLE.NET";
    acls[3] = NULL;
    ok(server_config_acl_permit(&rule, "good@EXAMPLE.NET"),
       "deny: success");
    ok(!server_config_acl_permit(&rule, "evil@EXAMPLE.NET"),
       "deny: failure");

    /* And make sure deny interacts correctly with files. */
    acls[0] = "data/acl-simple";
    acls[1] = "princ:evil@EXAMPLE.NET";
    acls[2] = NULL;
    ok(!server_config_acl_permit(&rule, "evil@EXAMPLE.NET"),
       "deny in file beats later princ");
    acls[0] = "deny:princ:rra@example.org";
    acls[1] = "data/acl-simple";
    ok(!server_config_acl_permit(&rule, "rra@example.org"),
       "deny overrides later file");

    /*
     * Ensure deny never affirmatively grants access, so deny:deny: matches
     * nothing.
     */
    acls[0] = "deny:deny:princ:rra@example.org";
    acls[1] = "data/acl-simple";
    ok(server_config_acl_permit(&rule, "rra@example.org"),
       "deny:deny does nothing");
    ok(server_config_acl_permit(&rule, "rra@EXAMPLE.COM"),
       "deny:deny doesn't break anything");

    /*
     * Denying a file denies anything that would match the file, and nothing
     * that wouldn't, including due to an embedded deny.
     */
    acls[0] = "deny:file:data/acl-simple";
    acls[1] = "princ:explicit@EXAMPLE.COM";
    acls[2] = "princ:evil@EXAMPLE.ORG";
    acls[3] = "princ:evil@EXAMPLE.NET";
    acls[4] = NULL;
    ok(!server_config_acl_permit(&rule, "explicit@EXAMPLE.COM"),
       "deny of a file works");
    ok(server_config_acl_permit(&rule, "evil@EXAMPLE.ORG"),
       "...and doesn't break anything");
    ok(server_config_acl_permit(&rule, "evil@EXAMPLE.NET"),
       "...and deny inside a denied file is ignored");

    /* Check for an invalid ACL scheme. */
    acls[0] = "ihateyou:verymuch";
    acls[1] = "data/acls/valid";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "invalid ACL scheme");
    is_string("TEST:0: invalid ACL scheme 'ihateyou'\n", errors,
              "...with correct error");
    errors_uncapture();

    /*
     * Check GPUT ACLs, or make sure they behave sanely when GPUT support is
     * not compiled.
     */
    server_config_set_gput_file((char *) "data/gput");
    acls[0] = "gput:test";
    acls[1] = NULL;
#ifdef HAVE_GPUT
    ok(server_config_acl_permit(&rule, "priv@EXAMPLE.ORG"), "GPUT 1");
    ok(!server_config_acl_permit(&rule, "nonpriv@EXAMPLE.ORG"), "GPUT 2");
    ok(!server_config_acl_permit(&rule, "priv@EXAMPLE.NET"), "GPUT 3");
    acls[0] = "gput:test[%@EXAMPLE.NET]";
    ok(server_config_acl_permit(&rule, "priv@EXAMPLE.NET"),
       "GPUT with transform 1");
    ok(!server_config_acl_permit(&rule, "nonpriv@EXAMPLE.NET"),
       "GPUT with transform 2");
    ok(!server_config_acl_permit(&rule, "priv@EXAMPLE.ORG"),
       "GPUT with transform 3");
#else
    errors_capture();
    ok(!server_config_acl_permit(&rule, "priv@EXAMPLE.ORG"), "GPUT");
    is_string("TEST:0: ACL scheme 'gput' is not supported\n", errors,
              "...with not supported error");
    errors_uncapture();
    skip_block(4, "GPUT support not configured");
#endif

    /*
     * Check PCRE ACLs, or make sure they behave as they should when not
     * supported.
     */
    acls[0] = "deny:pcre:host/foo.+\\.org@EXAMPLE\\.ORG";
    acls[1] = "pcre:host/.+\\.org@EXAMPLE\\.ORG";
    acls[2] = NULL;
#ifdef HAVE_PCRE
    ok(server_config_acl_permit(&rule, "host/bar.org@EXAMPLE.ORG"),
       "PCRE 1");
    ok(!server_config_acl_permit(&rule, "host/foobar.org@EXAMPLE.ORG"),
       "PCRE 2");
    ok(!server_config_acl_permit(&rule, "host/baz.org@EXAMPLE.NET"),
       "PCRE 3");
    ok(!server_config_acl_permit(&rule, "host/.org@EXAMPLE.ORG"),
       "PCRE 4 (plus operator)");
    ok(!server_config_acl_permit(&rule, "host/seaorg@EXAMPLE.ORG"),
       "PCRE 5 (escaped period)");
    acls[1] = "pcre:+host/.*";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "host/bar.org@EXAMPLE.ORG"),
       "PCRE invalid regex");
    is_string("TEST:0: compilation of regex '+host/.*' failed around 0\n",
              errors, "...with invalid regex error");
    errors_uncapture();
#else
    errors_capture();
    ok(!server_config_acl_permit(&rule, "host/foobar.org@EXAMPLE.ORG"),
       "PCRE");
    is_string("TEST:0: ACL scheme 'pcre' is not supported\n", errors,
              "...with not supported error");
    errors_uncapture();
    skip_block(5, "PCRE support not configured");
#endif

    /*
     * Check POSIX regex ACLs, or make sure they behave as they should when
     * not supported.
     */
    acls[0] = "deny:regex:host/foo.*\\.org@EXAMPLE\\.ORG";
    acls[1] = "regex:host/.*\\.org@EXAMPLE\\.ORG";
    acls[2] = NULL;
#ifdef HAVE_REGCOMP
    ok(server_config_acl_permit(&rule, "host/bar.org@EXAMPLE.ORG"),
       "regex 1");
    ok(!server_config_acl_permit(&rule, "host/foobar.org@EXAMPLE.ORG"),
       "regex 2");
    ok(!server_config_acl_permit(&rule, "host/baz.org@EXAMPLE.NET"),
       "regex 3");
    ok(server_config_acl_permit(&rule, "host/.org@EXAMPLE.ORG"),
       "regex 4");
    ok(!server_config_acl_permit(&rule, "host/seaorg@EXAMPLE.ORG"),
       "regex 5 (escaped period)");
    acls[1] = "regex:*host/.*";
    errors_capture();
    ok(!server_config_acl_permit(&rule, "host/bar.org@EXAMPLE.ORG"),
       "regex invalid regex");
    ok(strncmp(errors, "TEST:0: compilation of regex '*host/.*' failed:",
               strlen("TEST:0: compilation of regex '*host/.*' failed:")) == 0,
       "...with invalid regex error");
    errors_uncapture();
#else
    errors_capture();
    ok(!server_config_acl_permit(&rule, "host/foobar.org@EXAMPLE.ORG"),
       "regex");
    is_string("TEST:0: ACL scheme 'regex' is not supported\n", errors,
              "...with not supported error");
    errors_uncapture();
    skip_block(5, "regex support not available");
#endif

    /* Test for valid characters in ACL files. */
    acls[0] = "file:data/acls";
    acls[1] = NULL;
    ok(server_config_acl_permit(&rule, "upcase@EXAMPLE.ORG"),
       "valid chars 1");
    ok(server_config_acl_permit(&rule, "test@EXAMPLE.COM"),
       "valid chars 2");
    ok(server_config_acl_permit(&rule, "test2@EXAMPLE.COM"),
       "valid chars 3");
    ok(!server_config_acl_permit(&rule, "hash@EXAMPLE.ORG"),
       "invalid chars 1");
    ok(!server_config_acl_permit(&rule, "period@EXAMPLE.ORG"),
       "invalid chars 2");
    ok(!server_config_acl_permit(&rule, "tilde@EXAMPLE.ORG"),
       "invalid chars 3");

    return 0;
}
