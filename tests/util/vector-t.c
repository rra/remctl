/*
 * vector test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2001-2002, 2004-2006, 2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2008-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include <config.h>
#include <portable/system.h>

#include <sys/wait.h>

#include <tests/tap/basic.h>
#include <tests/tap/string.h>
#include <util/vector.h>
#include <util/xmalloc.h>


int
main(void)
{
    struct vector *vector;
    struct cvector *cvector;
    char *command, *string;
    char *p;
    pid_t child;
    char empty[] = "";
    static const char cstring[] = "This is a\ttest.  ";
    static const char nulls1[] = "This\0is\0a\0test.";
    static const char nulls2[] = "This is a\t\0es\0.  ";
    static const char tabs[] = "test\t\ting\t";

    /* Set up the plan. */
    plan(119);

    /* Be sure that freeing NULL doesn't cause a NULL pointer dereference. */
    vector_free(NULL);
    cvector_free(NULL);

    /* Test basic add and resize functionality for vectors. */
    vector = vector_new();
    ok(vector != NULL, "vector_new returns non-NULL");
    if (vector == NULL)
        bail("vector_new returned NULL");
    vector_add(vector, cstring);
    is_int(1, vector->count, "vector_add increases count");
    ok(vector->strings[0] != cstring, "...and allocated new memory");
    vector_resize(vector, 4);
    is_int(4, vector->allocated, "vector_resize works");
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    is_int(4, vector->allocated, "...and no reallocation when adding strings");
    is_int(4, vector->count, "...and the count matches");
    is_string(cstring, vector->strings[0], "added the right string");
    is_string(cstring, vector->strings[1], "added the right string");
    is_string(cstring, vector->strings[2], "added the right string");
    is_string(cstring, vector->strings[3], "added the right string");

    /* Verify that adding the same string creates new copies. */
    ok(vector->strings[1] != vector->strings[2], "each pointer is different");
    ok(vector->strings[2] != vector->strings[3], "each pointer is different");
    ok(vector->strings[3] != vector->strings[0], "each pointer is different");
    ok(vector->strings[0] != cstring, "each pointer is different");

    /* Test vector_clear. */
    vector_clear(vector);
    is_int(0, vector->count, "vector_clear works");
    is_int(4, vector->allocated, "...but doesn't free the allocation");

    /* Test that resizing the vector shrinks it and frees the string. */
    string = xstrdup(cstring);
    vector_add(vector, cstring);
    vector_add(vector, string);
    is_int(2, vector->count, "added two strings to the vector");
    ok(vector->strings[1] != string, "...and the pointers are different");
    vector_resize(vector, 1);
    is_int(1, vector->count, "vector_resize shrinks the vector");
    ok(vector->strings[0] != cstring, "...and the pointer is different");
    free(string);

    /* Test vector_addn. */
    vector_addn(vector, cstring, 4);
    is_int(2, vector->count, "vector_addn increments count");
    is_string("This", vector->strings[1], "...and adds the right data");
    vector_free(vector);

    /* Test basic add and resize functionality for cvectors. */
    cvector = cvector_new();
    ok(cvector != NULL, "cvector_new returns non-NULL");
    if (cvector == NULL)
        bail("cvector_new returned NULL");
    cvector_add(cvector, cstring);
    is_int(1, cvector->count, "cvector_add adds a string");
    ok(cvector->strings[0] == cstring, "...and keeps the same pointer");
    cvector_resize(cvector, 4);
    is_int(4, cvector->allocated, "cvector_resize works");
    cvector_add(cvector, cstring);
    cvector_add(cvector, cstring);
    cvector_add(cvector, cstring);
    is_int(4, cvector->allocated, "...and subsequent adds don't resize");
    is_int(4, cvector->count, "...and the count is right");
    ok(cvector->strings[1] == cvector->strings[2], "all pointers match");
    ok(cvector->strings[2] == cvector->strings[3], "all pointers match");
    ok(cvector->strings[3] == cvector->strings[0], "all pointers match");
    ok(cvector->strings[0] == cstring, "all pointers match");

    /* Test cvector_clear. */
    cvector_clear(cvector);
    is_int(0, cvector->count, "cvector_clear works");
    is_int(4, cvector->allocated, "...but doesn't free the allocation");

    /* Test that resizing the vector shrinks it. */
    string = xstrdup(cstring);
    cvector_add(cvector, cstring);
    cvector_add(cvector, string);
    is_int(2, cvector->count, "added two strings to the vector");
    ok(cvector->strings[1] == string, "...and the pointers match");
    cvector_resize(cvector, 1);
    is_int(1, cvector->count, "cvector_resize shrinks the vector");
    ok(cvector->strings[0] == cstring, "...and the pointers match");
    cvector_free(cvector);
    free(string);

    /* Test vector_split_space. */
    vector = vector_split_space("This is a\ttest.  ", NULL);
    is_int(4, vector->count, "vector_split_space returns right count");
    is_int(4, vector->allocated, "...and allocation");
    is_string("This", vector->strings[0], "...first string");
    is_string("is", vector->strings[1], "...second string");
    is_string("a", vector->strings[2], "...third string");
    is_string("test.", vector->strings[3], "...fourth string");

    /* Test that vector_add increases the size of the vector by one. */
    vector_add(vector, cstring);
    is_string(cstring, vector->strings[4], "...and can add another");
    ok(vector->strings[4] != cstring, "allocates a new pointer");
    is_int(5, vector->allocated, "allocation goes up by one");

    /* Test vector_split with vector reuse. */
    vector = vector_split(cstring, 't', vector);
    is_int(3, vector->count, "resplitting returns the right count");
    is_int(5, vector->allocated, "...but doesn't change allocation");
    is_string("This is a\t", vector->strings[0], "...first string");
    is_string("es", vector->strings[1], "...second string");
    is_string(".  ", vector->strings[2], "...third string");
    ok(vector->strings[0] != cstring, "...and allocated new string");

    /* Test vector_join. */
    p = vector_join(vector, "fe");
    is_string("This is a\tfeesfe.  ", p, "vector_join works");
    free(p);
    vector_free(vector);

    /* Test cvector_split_space. */
    string = xstrdup(cstring);
    cvector = cvector_split_space(string, NULL);
    is_int(4, cvector->count, "cvector_split_space returns right count");
    is_int(4, cvector->allocated, "...and allocation");
    is_string("This", cvector->strings[0], "...first string");
    is_string("is", cvector->strings[1], "...second string");
    is_string("a", cvector->strings[2], "...third string");
    is_string("test.", cvector->strings[3], "...fourth string");
    ok(memcmp(string, nulls1, 16) == 0, "original string modified in place");

    /* Test that cvector_add increases the size of the vector by one. */
    cvector_add(cvector, cstring);
    ok(cvector->strings[4] == cstring, "cvector_add then works");
    is_int(5, cvector->allocated, "...and allocation increases by one");
    free(string);

    /* Test cvector_split with vector reuse. */
    string = xstrdup(cstring);
    cvector = cvector_split(string, 't', cvector);
    is_int(3, cvector->count, "cvector_split into same cvector works");
    is_int(5, cvector->allocated, "...and doesn't lower allocation");
    is_string("This is a\t", cvector->strings[0], "...first string");
    is_string("es", cvector->strings[1], "...second string");
    is_string(".  ", cvector->strings[2], "...third string");
    ok(cvector->strings[0] == string, "no new memory is allocated");
    ok(memcmp(string, nulls2, 18) == 0, "...and string is modified in place");

    /* Test cvector_join. */
    p = cvector_join(cvector, "oo");
    is_string("This is a\tooesoo.  ", p, "cvector_join works");
    free(p);
    cvector_free(cvector);
    free(string);

    /* Test vector_split and cvector_split on empty string. */
    vector = vector_split("", ' ', NULL);
    is_int(1, vector->count, "vector_split on empty string");
    is_string("", vector->strings[0], "...returns only empty string");
    vector_free(vector);
    cvector = cvector_split(empty, ' ', NULL);
    is_int(1, cvector->count, "cvector_split on empty string");
    is_string("", cvector->strings[0], "...returns only empty string");
    cvector_free(cvector);

    /* Test vector_split_space and cvector_split_space on empty string. */
    vector = vector_split_space("", NULL);
    is_int(0, vector->count, "vector_split_space on empty string");
    p = vector_join(vector, "mumble");
    is_string("", p, "vector_join with an empty vector works");
    free(p);
    vector_free(vector);
    cvector = cvector_split_space(empty, NULL);
    is_int(0, cvector->count, "cvector_split_space on empty string");
    p = cvector_join(cvector, "mumble");
    is_string("", p, "cvector_join with an empty vector works");
    free(p);
    cvector_free(cvector);

    /* Test vector_split again and then join with an empty string. */
    vector = vector_split(tabs, '\t', NULL);
    is_int(4, vector->count, "vector_split on tab string");
    is_string("test", vector->strings[0], "...first string");
    is_string("", vector->strings[1], "...second string");
    is_string("ing", vector->strings[2], "...third string");
    is_string("", vector->strings[3], "...fourth string");
    p = vector_join(vector, "");
    is_string("testing", p, "vector_join with an empty string works");
    free(p);
    vector_free(vector);

    /* Test cvector_split again and then join with an empty string. */
    string = xstrdup(tabs);
    cvector = cvector_split(string, '\t', NULL);
    is_int(4, cvector->count, "cvector_split on tab string");
    is_string("test", cvector->strings[0], "...first string");
    is_string("", cvector->strings[1], "...second string");
    is_string("ing", cvector->strings[2], "...third string");
    is_string("", cvector->strings[3], "...fourth string");
    p = cvector_join(cvector, "");
    is_string("testing", p, "cvector_join with an empty string works");
    free(p);
    cvector_free(cvector);
    free(string);

    /* Test that newline is not space for (c)vector_split_space. */
    vector = vector_split_space("foo\nbar", NULL);
    is_int(1, vector->count, "newline is not space for vector_split_space");
    is_string("foo\nbar", vector->strings[0], "...first string");
    vector_free(vector);
    string = xstrdup("foo\nbar");
    cvector = cvector_split_space(string, NULL);
    is_int(1, cvector->count, "newline is not space for cvector_split_space");
    is_string("foo\nbar", cvector->strings[0], "...first string");
    cvector_free(cvector);
    free(string);

    /* Test (c)vector_split_space with leading and trailing delimiters. */
    vector = vector_split_space(" \t foo\t", NULL);
    is_int(1, vector->count, "extra whitespace in vector_split_space");
    is_string("foo", vector->strings[0], "...first string");
    vector_free(vector);
    string = xstrdup(" \t foo\t");
    cvector = cvector_split_space(string, NULL);
    is_int(1, cvector->count, "extra whitespace in cvector_split_space");
    is_string("foo", cvector->strings[0], "...first string");
    cvector_free(cvector);
    free(string);

    /* Test (c)vector_split_space on a string that's all delimiters. */
    vector = vector_split_space(" \t ", NULL);
    is_int(0, vector->count, "vector_split_space on all whitespace string");
    vector_free(vector);
    string = xstrdup(" \t ");
    cvector = cvector_split_space(string, NULL);
    is_int(0, cvector->count, "cvector_split_space on all whitespace string");
    cvector_free(cvector);
    free(string);

    /* Test vector_split_multi. */
    vector = vector_split_multi("foo, bar, baz", ", ", NULL);
    is_int(3, vector->count, "vector_split_multi returns right count");
    is_string("foo", vector->strings[0], "...first string");
    is_string("bar", vector->strings[1], "...second string");
    is_string("baz", vector->strings[2], "...third string");
    vector_free(vector);
    vector = vector_split_multi("", ", ", NULL);
    is_int(0, vector->count, "vector_split_multi reuse with empty string");
    vector = vector_split_multi(",,,  foo,   ", ", ", vector);
    is_int(1, vector->count, "vector_split_multi with extra separators");
    is_string("foo", vector->strings[0], "...first string");
    vector = vector_split_multi(", ,  ", ", ", vector);
    is_int(0, vector->count, "vector_split_multi with only separators");
    vector_free(vector);

    /* Test cvector_split_multi. */
    string = xstrdup("foo, bar, baz");
    cvector = cvector_split_multi(string, ", ", NULL);
    is_int(3, cvector->count, "cvector_split_multi returns right count");
    is_string("foo", cvector->strings[0], "...first string");
    is_string("bar", cvector->strings[1], "...second string");
    is_string("baz", cvector->strings[2], "...third string");
    free(string);
    cvector_free(cvector);
    cvector = cvector_split_multi(empty, ", ", NULL);
    is_int(0, cvector->count, "cvector_split_multi reuse with empty string");
    string = xstrdup(",,,  foo,   ");
    cvector = cvector_split_multi(string, ", ", cvector);
    is_int(1, cvector->count, "cvector_split_multi with extra separators");
    is_string("foo", cvector->strings[0], "...first string");
    free(string);
    string = xstrdup(", ,  ");
    cvector = cvector_split_multi(string, ", ", cvector);
    is_int(0, cvector->count, "cvector_split_multi with only separators");
    cvector_free(cvector);
    free(string);

    /*
     * Test vector_exec.  We mess with testnum here since the child outputs
     * the okay message.
     */
    vector = vector_new();
    vector_add(vector, "/bin/sh");
    vector_add(vector, "-c");
    basprintf(&command, "echo ok %lu - vector_exec", testnum++);
    vector_add(vector, command);
    child = fork();
    if (child < 0)
        sysbail("unable to fork");
    else if (child == 0)
        if (vector_exec("/bin/sh", vector) < 0)
            sysdiag("unable to exec /bin/sh");
    waitpid(child, NULL, 0);
    vector_free(vector);
    free(command);

    /* Test cvector_exec the same way. */
    cvector = cvector_new();
    cvector_add(cvector, "/bin/sh");
    cvector_add(cvector, "-c");
    basprintf(&command, "echo ok %lu - cvector_exec", testnum++);
    cvector_add(cvector, command);
    child = fork();
    if (child < 0)
        sysbail("unable to fork");
    else if (child == 0)
        if (cvector_exec("/bin/sh", cvector) < 0)
            sysdiag("unable to exec /bin/sh");
    waitpid(child, NULL, 0);
    cvector_free(cvector);
    free(command);

    /* All done. */
    return 0;
}
