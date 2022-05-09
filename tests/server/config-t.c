/*
 * Test suite for the server configuration parsing.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2009-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/messages.h>


/*
 * Test for correct handling of a configuration error.  Takes the name of the
 * error configuration file to load and the expected error output.
 */
static void
test_error(const char *file, const char *expected)
{
    struct config *config;
    char *p;

    errors_capture();
    config = server_config_load(file);
    ok(config == NULL, "%s failed", file);

    /* For not found errors, we have to trim off the system error message. */
    p = strstr(errors, "not found: ");
    if (p != NULL) {
        p += strlen("not found");
        *p = '\0';
    }
    is_string(expected, errors, "...with the right error");
    errors_uncapture();
    free(errors);
    errors = NULL;
}


int
main(void)
{
    struct config *config;

    plan(49);
    if (chdir(getenv("C_TAP_SOURCE")) < 0)
        sysbail("can't chdir to C_TAP_SOURCE");

    config = server_config_load("data/conf-test");
    ok(config != NULL, "config loaded");
    if (config == NULL)
        bail("server_config_load returned NULL");
    is_int(4, config->count, "with right count");

    is_string("test", config->rules[0]->command, "command 1");
    is_string("foo", config->rules[0]->subcommand, "subcommand 1");
    is_string("data/cmd-hello", config->rules[0]->program, "program 1");
    ok(config->rules[0]->logmask == NULL, "logmask 1");
    is_string("data/acl-nonexistent", config->rules[0]->acls[0], "acl 1");
    ok(config->rules[0]->acls[1] == NULL, "...and only one acl");

    is_string("test", config->rules[1]->command, "command 2");
    is_string("bar", config->rules[1]->subcommand, "subcommand 2");
    is_string("data/cmd-hello", config->rules[1]->program, "program 2");
    is_int(4, config->rules[1]->logmask[0], "logmask 2");
    is_int(0, config->rules[1]->logmask[1], "...and only one logmask");
    is_string("data/acl-nonexistent", config->rules[1]->acls[0], "acl 2 1");
    is_string("data/acl-no-such-file", config->rules[1]->acls[1], "acl 2 2");
    ok(config->rules[1]->acls[2] == NULL, "...and only two acls");

    is_string("test", config->rules[2]->command, "command 3");
    is_string("baz", config->rules[2]->subcommand, "subcommand 3");
    is_string("data/cmd-hello", config->rules[2]->program, "program 3");
    is_int(4, config->rules[2]->logmask[0], "logmask 3 1");
    is_int(5, config->rules[2]->logmask[1], "logmask 3 2");
    is_int(7, config->rules[2]->logmask[2], "logmask 3 3");
    is_int(0, config->rules[2]->logmask[3], "...and three logmask values");
    is_string("ANYUSER", config->rules[2]->acls[0], "acl 3");
    ok(config->rules[2]->acls[1] == NULL, "...and only one acl");
    is_string("data/cmd-hello", config->rules[2]->summary, "summary 3");
    is_string("data/command-hello", config->rules[2]->help, "help 3");

    is_string("foo", config->rules[3]->command, "command 4");
    is_string("ALL", config->rules[3]->subcommand, "subcommand 4");
    is_string("data/cmd-bar", config->rules[3]->program, "program 4");
    ok(config->rules[3]->logmask == NULL, "logmask 4");
    is_string("data/acl-simple", config->rules[3]->acls[0], "acl 4 1");
    is_string("data/acl-simple", config->rules[3]->acls[1], "acl 4 2");
    is_string("data/acl-simple", config->rules[3]->acls[187], "acl 4 188");
    ok(config->rules[3]->acls[188] == NULL, "...and 188 total ACLs");
    server_config_free(config);

    /* Now test for errors. */
    test_error("data/configs/bad-option-1",
               "data/configs/bad-option-1:1: unknown option unknown=yes\n");
    test_error("data/configs/bad-logmask-1",
               "data/configs/bad-logmask-1:1: invalid logmask parameter"
               " 1foo\n");
    test_error("data/configs/bad-logmask-2",
               "data/configs/bad-logmask-2:1: invalid logmask parameter 0\n");
    test_error("data/configs/bad-logmask-3",
               "data/configs/bad-logmask-3:1: invalid logmask parameter"
               " biteme\n");
    test_error("data/configs/bad-logmask-4",
               "data/configs/bad-logmask-4:1: invalid logmask parameter -1\n");
    test_error("data/configs/bad-include-1",
               "data/configs/bad-include-1:1: included file /no/th/ing not"
               " found\n");
    test_error("data/configs/bad-user-1",
               "data/configs/bad-user-1:1: invalid user value nonexistent\n");

    return 0;
}
