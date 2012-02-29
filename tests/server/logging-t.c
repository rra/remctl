/*
 * Test suite for server command logging.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/messages.h>


int
main(void)
{
    struct confline confline = {
        NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 0, NULL
    };
    struct iovec **command;
    int i;

    plan(8);

    /* Command without subcommand. */
    command = bcalloc(5, sizeof(struct iovec *));
    command[0] = bmalloc(sizeof(struct iovec));
    command[0]->iov_base = bstrdup("foo");
    command[0]->iov_len = strlen("foo");
    command[1] = NULL;
    errors_capture();
    server_log_command(command, &confline, "test@EXAMPLE.ORG");
    is_string("COMMAND from test@EXAMPLE.ORG: foo\n", errors,
              "command without subcommand logging");

    /* Filtering of non-printable characters. */
    command[1] = bmalloc(sizeof(struct iovec));
    command[1]->iov_base = bstrdup("f\1o\33o\37o\177");
    command[1]->iov_len = strlen("f\1o\33o\37o\177");
    command[2] = NULL;
    errors_capture();
    server_log_command(command, &confline, "test");
    is_string("COMMAND from test: foo f.o.o.o.\n", errors,
              "logging of unprintable characters");

    /* Simple command. */
    for (i = 2; i < 5; i++)
        command[i] = bmalloc(sizeof(struct iovec));
    free(command[1]->iov_base);
    command[1]->iov_base = bstrdup("bar");
    command[1]->iov_len = strlen("bar");
    command[2]->iov_base = bstrdup("arg1");
    command[2]->iov_len = strlen("arg1");
    command[3]->iov_base = bstrdup("arg2");
    command[3]->iov_len = strlen("arg2");
    command[4] = NULL;
    errors_capture();
    server_log_command(command, &confline, "test@EXAMPLE.ORG");
    is_string("COMMAND from test@EXAMPLE.ORG: foo bar arg1 arg2\n", errors,
              "simple command logging");

    /* Command with stdin numeric argument. */
    confline.stdin_arg = 2;
    errors_capture();
    server_log_command(command, &confline, "test");
    is_string("COMMAND from test: foo bar **DATA** arg2\n", errors,
              "stdin argument");

    /* Command with stdin set to "last". */
    confline.stdin_arg = -1;
    errors_capture();
    server_log_command(command, &confline, "test");
    is_string("COMMAND from test: foo bar arg1 **DATA**\n", errors,
              "stdin last argument");

    /* Logmask of a single argument. */
    confline.stdin_arg = 0;
    confline.logmask = bmalloc(2 * sizeof(unsigned int));
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
    confline.logmask = bmalloc(4 * sizeof(unsigned int));
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
