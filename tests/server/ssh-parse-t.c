/*
 * Test suite for ssh command parsing.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/messages.h>
#include <util/messages.h>


/*
 * Check a struct iovec against a string.
 */
static bool __attribute__((__format__(printf, 3, 4)))
is_iovec_string(const char *wanted, const struct iovec *seen,
                const char *format, ...)
{
    size_t length;
    bool success;
    va_list args;

    length = strlen(wanted);
    if (length != seen->iov_len)
        success = false;
    else
        success = (memcmp(wanted, seen->iov_base, length) == 0);
    if (!success) {
        diag("wanted: %s", wanted);
        diag("  seen: %.*s", (int) seen->iov_len,
             (const char *) seen->iov_base);
    }
    va_start(args, format);
    okv(success, format, args);
    va_end(args);
    return success;
}


int
main(void)
{
    struct iovec **argv;

    plan(29);

    /* Simple argument parse. */
    argv = server_ssh_parse_command("foo bar   baz");
    is_iovec_string("foo", argv[0], "simple word 1");
    is_iovec_string("bar", argv[1], "simple word 2");
    is_iovec_string("baz", argv[2], "simple word 3");
    ok(argv[3] == NULL, "simple length");
    server_free_command(argv);

    /* Extra whitespace. */
    argv = server_ssh_parse_command("   foo\tbar  \t  ");
    is_iovec_string("foo", argv[0], "extra whitespace word 1");
    is_iovec_string("bar", argv[1], "extra whitespace word 2");
    ok(argv[2] == NULL, "extra whitespace length");
    server_free_command(argv);

    /* Double quotes. */
    argv = server_ssh_parse_command("\"one argument\"");
    is_iovec_string("one argument", argv[0], "double quotes word 1");
    ok(argv[1] == NULL, "double quotes length");
    server_free_command(argv);

    /* Single quotes. */
    argv = server_ssh_parse_command("  'one  \"argument'  ");
    is_iovec_string("one  \"argument", argv[0], "single quotes word 1");
    ok(argv[1] == NULL, "single quotes length");
    server_free_command(argv);

    /* Mixed quotes. */
    argv = server_ssh_parse_command("  one'two\" three '\"four '\" ' '");
    is_iovec_string("onetwo\" three four '", argv[0], "mixed quotes word 1");
    is_iovec_string(" ", argv[1], "mixed quotes word 2");
    ok(argv[2] == NULL, "mixed quotes length");
    server_free_command(argv);

    /* Empty arguments. */
    argv = server_ssh_parse_command("  ''  \"\"  ");
    is_iovec_string("", argv[0], "empty arguments word 1");
    is_iovec_string("", argv[1], "empty arguments word 2");
    ok(argv[2] == NULL, "empty arguments length");
    server_free_command(argv);

    /* Backslashes. */
    argv = server_ssh_parse_command("\"foo\\\" bar\" \\'baz");
    is_iovec_string("foo\" bar", argv[0], "backslashes word 1");
    is_iovec_string("'baz", argv[1], "backslashes word 2");
    ok(argv[2] == NULL, "backslashes length");
    server_free_command(argv);

    /* More backslashes. */
    argv = server_ssh_parse_command("  '\\f\\o\\o\\ \\\' bar' \\ foo\\\\");
    is_iovec_string("foo ' bar", argv[0], "more backslashes word 1");
    is_iovec_string(" foo\\", argv[1], "more backslashes word 2");
    ok(argv[2] == NULL, "more backslashes length");
    server_free_command(argv);

    /* Trailing backslash. */
    argv = server_ssh_parse_command("trailing\\");
    is_iovec_string("trailing\\", argv[0], "trailing backslash word 1");
    ok(argv[1] == NULL, "trailing backslash length");
    server_free_command(argv);

    /* Unterminated double quote. */
    errors_capture();
    argv = server_ssh_parse_command("  foo \"bar");
    ok(argv == NULL, "unterminated double quote return");
    is_string(errors, "unterminated \" quote in command\n",
              "unterminated double quote error");
    errors_uncapture();

    /* Unterminated single quote. */
    errors_capture();
    argv = server_ssh_parse_command("' foo \" bar baz  ");
    ok(argv == NULL, "unterminated single quote return");
    is_string(errors, "unterminated ' quote in command\n",
              "unterminated single quote error");
    errors_uncapture();
    free(errors);
    errors = NULL;

    message_handlers_reset();
    return 0;
}
