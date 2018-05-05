/*
 * Testing interface to the fake getgrnam_r replacement.
 *
 * Written by Remi Ferrand <remi.ferrand@cc.in2p3.fr>
 * Copyright 2014 IN2P3 Computing Centre - CNRS
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */
 
#ifndef TESTS_SERVER_ACL_FAKE_GETGRNAM_H
#define TESTS_SERVER_ACL_FAKE_GETGRNAM_H 1

#include <config.h>
#include <portable/macros.h>

/* Forward declarations to avoid additional includes. */
struct group;

BEGIN_DECLS

/*
 * Queue up a new return from getgrnam_r with the group struct and the return
 * status.  These will be returned in the order in which they're queued,
 * provided that the requested group name matches the gr_name struct field.
 * If not, NULL will be returned and the queue will not be advanced.
 */
void fake_queue_group(const struct group *, int);

#endif /* !TESTS_SERVER_ACL_FAKE_GETGRNAM_H */
