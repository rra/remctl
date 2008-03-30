/* $Id$ */
/* getopt test suite. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2008 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>
#include <portable/getopt.h>

#include <tests/libtest.h>

int test_getopt(int, char **, const char *);
extern int test_optind;
extern int test_opterr;
extern int test_optopt;
extern char *test_optarg;

int
main(void)
{
    const char *test1[] = {
        "foo", "-ab", "-c", "foo", "bar", "-dbar", "-", "-efbaz", "--", "-g"
    };
    const char *test2[] = { "foo", "-a" };
    const char *test3[] = { "foo" };
    const char *test4[] = { "foo", "-a", "foo", "-b" };

    test_init(37);

    /* We currently have no tests for opterr and error reporting. */
    ok_int(1, 1, test_opterr);
    test_opterr = 0;

    /* Basic test with permuting. */
    ok_int(2, 'a', test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_int(3, 'b', test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_int(4, 'c', test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_string(5, "foo", test_optarg);
    ok_int(6, 'd', test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_string(7, "bar", test_optarg);
    ok_int(8, '?', test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_int(9, 'e', test_optopt);
    ok_int(10, 'f', test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_string(11, "baz", test_optarg);
    ok_int(12, -1, test_getopt(10, (char **) test1, "abc:d:f:g"));
    ok_int(13, 7, test_optind);
    ok_string(14, "bar", test1[7]);
    ok_string(15, "-", test1[8]);
    ok_string(16, "-g", test1[9]);

    /* Test for missing argument. */
    test_optind = 1;
    ok_int(17, '?', test_getopt(2, (char **) test2, "a:"));
    ok_int(18, 'a', test_optopt);
    test_optind = 1;
    test_optopt = 0;
    ok_int(19, 'a', test_getopt(2, (char **) test2, "a::"));
    ok(20, test_optarg == NULL);
    ok_int(21, 2, test_optind);
    test_optind = 1;
    test_optopt = 0;
    ok_int(22, ':', test_getopt(2, (char **) test2, ":a:"));
    ok_int(23, 'a', test_optopt);

    /* Test for no arguments. */
    test_optind = 1;
    ok_int(24, -1, test_getopt(1, (char **) test3, "abc"));
    ok_int(25, 1, test_optind);

    /* Test for non-option handling. */
    test_optind = 1;
    ok_int(26, 'a', test_getopt(4, (char **) test4, "+abc"));
    ok_int(27, -1, test_getopt(4, (char **) test4, "+abc"));
    ok_int(28, 2, test_optind);
    ok_string(29, "foo", test4[2]);
    ok_string(30, "-b", test4[3]);
    test_optind = 1;
    ok_int(31, 'a', test_getopt(4, (char **) test4, "-abc"));
    ok_int(32, '\1', test_getopt(4, (char **) test4, "-abc"));
    ok_string(33, "foo", test_optarg);
    ok_int(34, 'b', test_getopt(4, (char **) test4, "-abc"));
    ok(35, test_optarg == NULL);
    ok_int(36, -1, test_getopt(4, (char **) test4, "-abc"));
    ok_int(37, 4, test_optind);

    exit(0);
}
