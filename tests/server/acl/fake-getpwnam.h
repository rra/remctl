/*
 * Testing interface to the fake getpwnam replacement.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#ifndef TESTS_SERVER_ACL_FAKE_GETPWNAM_H
#define TESTS_SERVER_ACL_FAKE_GETPWNAM_H 1

#include <config.h>
#include <portable/macros.h>

/* Forward declarations to avoid additional includes. */
struct passwd;

BEGIN_DECLS

/*
 * Sets the struct passwd returned by getpwnam calls.  The last struct passed
 * to this function will be returned provided the pw_name matches.
 */
void fake_set_passwd(struct passwd *pwd);

/* Frees the memory allocated by fake_set_passwd. */
void fake_free_passwd(void);

END_DECLS

#endif /* !TESTS_SERVER_ACL_FAKE_GETPWNAM_H */
