/*
 * Test suite for the server configuration parsing.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2007, 2009 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <util/util.h>


int
main(void)
{
    struct config *config;

    plan(33);
    if (chdir(getenv("SOURCE")) < 0)
        sysbail("can't chdir to SOURCE");

    config = server_config_load("data/conf-test");
    ok(config != NULL, "config loaded");
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
    is_int(1, config->rules[1]->logmask->count, "logmask 2");
    is_string("4", config->rules[1]->logmask->strings[0], "...and value");
    is_string("data/acl-nonexistent", config->rules[1]->acls[0], "acl 2 1");
    is_string("data/acl-no-such-file", config->rules[1]->acls[1], "acl 2 2");
    ok(config->rules[1]->acls[2] == NULL, "...and only two acls");

    is_string("test", config->rules[2]->command, "command 3");
    is_string("baz", config->rules[2]->subcommand, "subcommand 3");
    is_string("data/cmd-hello", config->rules[2]->program, "program 3");
    is_int(3, config->rules[2]->logmask->count, "logmask 3");
    is_string("4", config->rules[2]->logmask->strings[0], "logmask 3 1");
    is_string("5", config->rules[2]->logmask->strings[1], "logmask 3 2");
    is_string("7", config->rules[2]->logmask->strings[2], "logmask 3 3");
    is_string("ANYUSER", config->rules[2]->acls[0], "acl 3");
    ok(config->rules[2]->acls[1] == NULL, "...and only one acl");

    is_string("foo", config->rules[3]->command, "command 4");
    is_string("ALL", config->rules[3]->subcommand, "subcommand 4");
    is_string("data/cmd-bar", config->rules[3]->program, "program 4");
    ok(config->rules[3]->logmask == NULL, "logmask 4");
    is_string("data/acl-simple", config->rules[3]->acls[0], "acl 4 1");
    is_string("data/acl-simple", config->rules[3]->acls[1], "acl 4 2");
    is_string("data/acl-simple", config->rules[3]->acls[187], "acl 4 188");
    ok(config->rules[3]->acls[188] == NULL, "...and 188 total ACLs");

    return 0;
}
