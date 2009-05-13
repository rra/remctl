/*
 * Test suite for server command logging.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2009 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/messages.h>
#include <util/util.h>


int
main(void)
{
    struct confline confline = { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL };
    struct iovec **command;
    int i;

    plan(5);

    /* Command without subcommand. */
    command = xcalloc(5, sizeof(struct iovec *));
    command[0] = xmalloc(sizeof(struct iovec));
    command[0]->iov_base = xstrdup("foo");
    command[0]->iov_len = strlen("foo");
    command[1] = NULL;
    errors_capture();
    server_log_command(command, &confline, "test@EXAMPLE.ORG");
    is_string("COMMAND from test@EXAMPLE.ORG: foo\n", errors,
              "command without subcommand logging");

    /* Simple command. */
    for (i = 1; i < 5; i++)
        command[i] = xmalloc(sizeof(struct iovec));
    command[1]->iov_base = xstrdup("bar");
    command[1]->iov_len = strlen("bar");
    command[2]->iov_base = xstrdup("arg1");
    command[2]->iov_len = strlen("arg1");
    command[3]->iov_base = xstrdup("arg2");
    command[3]->iov_len = strlen("arg2");
    command[4] = NULL;
    errors_capture();
    server_log_command(command, &confline, "test@EXAMPLE.ORG");
    is_string("COMMAND from test@EXAMPLE.ORG: foo bar arg1 arg2\n", errors,
              "simple command logging");

    /* Logmask of a single argument. */
    confline.logmask = xmalloc(2 * sizeof(unsigned int));
    confline.logmask[0] = 2;
    confline.logmask[1] = 0;
    errors_capture();
    server_log_command(command, &confline, "test");
    is_string("COMMAND from test: foo bar **MASKED** arg2\n", errors,
              "one masked parameter");

    /* Logmask that doesn't apply to this command. */
    confline.logmask[0] = 4;
    errors_capture();
    server_log_command(command, &confline, "test@EXAMPLE.ORG");
    is_string("COMMAND from test@EXAMPLE.ORG: foo bar arg1 arg2\n", errors,
              "mask that doesn't apply");

    /* Logmask of the subcommand and multiple masks. */
    free(confline.logmask);
    confline.logmask = xmalloc(4 * sizeof(unsigned int));
    confline.logmask[0] = 4;
    confline.logmask[1] = 1;
    confline.logmask[2] = 3;
    confline.logmask[3] = 0;
    errors_capture();
    server_log_command(command, &confline, "test");
    is_string("COMMAND from test: foo **MASKED** arg1 **MASKED**\n", errors,
              "two masked parameters");

    return 0;
}
