/*
 * This file is part of the remctl project.
 *
 * Fake getgrnam_r function only used for testing.
 *
 * This "fake" function enable the caller to pre-feed its
 * answers in the getgrnam_r_responses variable.
 * That allows multiple queries to getgrnam_r() with controlled
 * and predictable responses.
 * That also allow the caller to fake a syscall failure in order
 * to improve the test suite.
 *
 * Written by Remi Ferrand <remi.ferrand@cc.in2p3.fr>
 * Copyright 2014
 *     IN2P3 Computing Centre - CNRS
 *
 * See LICENSE for licensing terms.
 */

#include "getgrnam_r.h"

int call_idx = 0; // Call index that allow
                  // to fake multiple calls to
                  // getgrnam_r

struct faked_getgrnam_call getgrnam_r_responses[PRE_ALLOC_ANSWERS_MAX_IDX];

/**
 * Fake system function and only feed
 * required structure with the one provided
 * in global variable.
 * This is only used for the tests suite.
 */
int getgrnam_r(const char *name, struct group *grp,
                char *buf, size_t buflen, struct group **result) {

    int rc = -1;

    if (call_idx < 0)
        call_idx = 0;

    if (call_idx >= PRE_ALLOC_ANSWERS_MAX_IDX)
        call_idx = 0;

    *grp = *(getgrnam_r_responses[call_idx].getgrnam_grp);
    *result = grp;
    rc = getgrnam_r_responses[call_idx].getgrnam_r_rc;

    call_idx++;

    return rc;
}
