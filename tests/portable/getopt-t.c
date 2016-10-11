/*
 * getopt test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2009
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/getopt.h>

#include <tests/tap/basic.h>

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

    plan(37);

    /* We currently have no tests for opterr and error reporting. */
    is_int(1, test_opterr, "default opterr value");
    test_opterr = 0;

    /* Basic test with permuting. */
    is_int('a', test_getopt(10, (char **) test1, "abc:d:f:g"), "-a");
    is_int('b', test_getopt(10, (char **) test1, "abc:d:f:g"), "-b");
    is_int('c', test_getopt(10, (char **) test1, "abc:d:f:g"), "-c");
    is_string("foo", test_optarg, "-c foo");
    is_int('d', test_getopt(10, (char **) test1, "abc:d:f:g"), "-d");
    is_string("bar", test_optarg, "-dbar");
    is_int('?', test_getopt(10, (char **) test1, "abc:d:f:g"), "- option");
    is_int('e', test_optopt, "-e");
    is_int('f', test_getopt(10, (char **) test1, "abc:d:f:g"), "-f");
    is_string("baz", test_optarg, "-fbaz");
    is_int(-1, test_getopt(10, (char **) test1, "abc:d:f:g"),
           "end of options");
    is_int(7, test_optind, "optind value");
    is_string("bar", test1[7], "bar is first non-argument");
    is_string("-", test1[8], "then -");
    is_string("-g", test1[9], "then -g");

    /* Test for missing argument. */
    test_optind = 1;
    is_int('?', test_getopt(2, (char **) test2, "a:"), "-a without arg");
    is_int('a', test_optopt, "optopt set properly");
    test_optind = 1;
    test_optopt = 0;
    is_int('a', test_getopt(2, (char **) test2, "a::"), "-a w/optional arg");
    ok(test_optarg == NULL, "no optarg");
    is_int(2, test_optind, "correct optind");
    test_optind = 1;
    test_optopt = 0;
    is_int(':', test_getopt(2, (char **) test2, ":a:"),
           ": starting option string");
    is_int('a', test_optopt, "...with correct optopt");

    /* Test for no arguments. */
    test_optind = 1;
    is_int(-1, test_getopt(1, (char **) test3, "abc"), "no arguments");
    is_int(1, test_optind, "...and optind set properly");

    /* Test for non-option handling. */
    test_optind = 1;
    is_int('a', test_getopt(4, (char **) test4, "+abc"), "-a with +");
    is_int(-1, test_getopt(4, (char **) test4, "+abc"), "end of arguments");
    is_int(2, test_optind, "and optind is correct");
    is_string("foo", test4[2], "foo is first non-option");
    is_string("-b", test4[3], "-b is second non-option");
    test_optind = 1;
    is_int('a', test_getopt(4, (char **) test4, "-abc"), "-a with -");
    is_int('\1', test_getopt(4, (char **) test4, "-abc"), "foo with -");
    is_string("foo", test_optarg, "...and optarg is correct");
    is_int('b', test_getopt(4, (char **) test4, "-abc"), "-b with -");
    ok(test_optarg == NULL, "...and optarg is not set");
    is_int(-1, test_getopt(4, (char **) test4, "-abc"),
           "and now end of arguments");
    is_int(4, test_optind, "...and optind is set correctly");

    exit(0);
}
