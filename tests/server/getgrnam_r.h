/*
 * This file is part of the remctl project.
 *
 * Header file for fake getgrnam_r function only used for testing.
 *
 * Written by Remi Ferrand <remi.ferrand@cc.in2p3.fr>
 * Copyright 2014
 *     IN2P3 Computing Centre - CNRS
 *
 * See LICENSE for licensing terms.
 */
 
#ifndef TESTS_SERVER_FAKE_GETGRNAM_R_H
#define TESTS_SERVER_FAKE_GETGRNAM_R_H

#include <stdio.h>
#include <sys/types.h>
#include <grp.h>

#define PRE_ALLOC_ANSWERS_MAX_IDX 5

struct faked_getgrnam_call {
    struct group *getgrnam_grp; // Group struct returned
    int getgrnam_r_rc; // syscall return code
};

/* Call index iterator
 * incremented at every getgrnam_r call
 */
extern int call_idx;

/* Stack of pre-made answers
 * The current answer is decided based on *call_idx* value
 */
extern struct faked_getgrnam_call getgrnam_r_responses[PRE_ALLOC_ANSWERS_MAX_IDX];

int getgrnam_r(const char *name, struct group *grp,
                char *buf, size_t buflen, struct group **result);

#endif
