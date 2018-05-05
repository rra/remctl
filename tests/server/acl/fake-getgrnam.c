/*
 * Fake getgrnam_r function only used for testing.
 *
 * This "fake" function enable the caller to pre-feed answers into a queue of
 * pending answers.  This allows multiple queries to getgrnam_r by a program
 * under test, with controlled and predictable responses.  This also allow the
 * caller to fake a syscall failure to exercise failure behavior.
 *
 * Written by Remi Ferrand <remi.ferrand@cc.in2p3.fr>
 * Copyright 2014 IN2P3 Computing Centre - CNRS
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <grp.h>

#include <util/macros.h>
#include <tests/server/acl/fake-getgrnam.h>
#include <tests/tap/basic.h>

/*
 * The queue of fake responses.  This is a linked list of responses to return,
 * each of which will be internally discarded after being returned.  The
 * response is a struct group whose contents will be copied (shallowly) to the
 * caller and the return status to return from getgrnam_r.
 */
struct fake_getgrnam_return {
    struct group gr;
    int status;
    struct fake_getgrnam_return *next;
};

/* The head of the queue of responses. */
static struct fake_getgrnam_return *return_queue = NULL;


/*
 * Interface specific to this fake library to queue a getgrnam_r return.  This
 * assumes that all the pointers in the struct are either pointers to memory
 * that won't be freed, such as constant strings or persistent memory
 * allocations, or are NULL.
 */
void
fake_queue_group(const struct group *gr, int status)
{
    struct fake_getgrnam_return *fake, *last;

    fake = bcalloc(1, sizeof(struct fake_getgrnam_return));
    fake->gr = *gr;
    fake->status = status;
    if (return_queue == NULL)
        return_queue = fake;
    else {
        for (last = return_queue; last->next != NULL; last = last->next)
            ;
        last->next = fake;
    }
}


/*
 * The fake getgrnam_r function.  Intercept the C library function and, if the
 * name matches, return the top response on the queue and advance the queue.
 * If the name does not match, return NULL.  The client-supplied buffer is
 * just ignored since we don't need to store other data.
 */
int
getgrnam_r(const char *name, struct group *grp, char *buf UNUSED,
           size_t buflen UNUSED, struct group **result)
{
    struct fake_getgrnam_return *old;
    int status;

    /* If the group name doesn't match, return success with NULL result. */
    if (return_queue == NULL || strcmp(name, return_queue->gr.gr_name) != 0) {
        *result = NULL;
        return 0;
    }

    /* Pull the result off the queue. */
    *grp = return_queue->gr;
    *result = grp;
    status = return_queue->status;
    old = return_queue;
    return_queue = return_queue->next;
    free(old);

    /* Return the desired status. */
    return status;
}
