/*
 * Test suite for the server configuration parsing.
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
    struct config *config;

    test_init(33);

    if (access("../data/conf-test", F_OK) == 0)
        chdir("..");
    else if (access("tests/data/conf-test", F_OK) == 0)
        chdir("tests");
    if (access("data/conf-test", F_OK) != 0)
        sysdie("cannot find data/conf-test");

    config = server_config_load("data/conf-test");
    ok(1, config != NULL);
    ok_int(2, 4, config->count);

    ok_string(3, "test", config->rules[0]->type);
    ok_string(4, "foo", config->rules[0]->service);
    ok_string(5, "data/cmd-hello", config->rules[0]->program);
    ok(6, config->rules[0]->logmask == NULL);
    ok_string(7, "data/acl-nonexistent", config->rules[0]->acls[0]);
    ok(8, config->rules[0]->acls[1] == NULL);

    ok_string(9, "test", config->rules[1]->type);
    ok_string(10, "bar", config->rules[1]->service);
    ok_string(11, "data/cmd-hello", config->rules[1]->program);
    ok_int(12, 1, config->rules[1]->logmask->count);
    ok_string(13, "4", config->rules[1]->logmask->strings[0]);
    ok_string(14, "data/acl-nonexistent", config->rules[1]->acls[0]);
    ok_string(15, "data/acl-no-such-file", config->rules[1]->acls[1]);
    ok(16, config->rules[1]->acls[2] == NULL);

    ok_string(17, "test", config->rules[2]->type);
    ok_string(18, "baz", config->rules[2]->service);
    ok_string(19, "data/cmd-hello", config->rules[2]->program);
    ok_int(20, 3, config->rules[2]->logmask->count);
    ok_string(21, "4", config->rules[2]->logmask->strings[0]);
    ok_string(22, "5", config->rules[2]->logmask->strings[1]);
    ok_string(23, "7", config->rules[2]->logmask->strings[2]);
    ok_string(24, "ANYUSER", config->rules[2]->acls[0]);
    ok(25, config->rules[2]->acls[1] == NULL);

    ok_string(26, "foo", config->rules[3]->type);
    ok_string(27, "ALL", config->rules[3]->service);
    ok_string(28, "data/cmd-bar", config->rules[3]->program);
    ok(29, config->rules[3]->logmask == NULL);
    ok_string(30, "data/acl-simple", config->rules[3]->acls[0]);
    ok_string(31, "data/acl-simple", config->rules[3]->acls[1]);
    ok_string(32, "data/acl-simple", config->rules[3]->acls[187]);
    ok(33, config->rules[3]->acls[188] == NULL);

    return 0;
}
