/*
 * Fake getpwnam for testing purposes.
 *
 * For testing purposes, we want to be able to intercept getpwnam.  We'll try
 * to intercept the C library getpwnam function.
 *
 * We store only one struct passwd data structure statically.  If the user
 * we're looking up matches that, we return it; otherwise, we return NULL.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <pwd.h>

#include <tests/server/acl/fake-getpwnam.h>
#include <tests/tap/basic.h>

/* Stores the static struct passwd returned by getpwnam if the name matches. */
static struct passwd *pwd_info = NULL;


/*
 * Interface specific to this fake library to set the struct passwd that's
 * returned by getpwnam queries if the name matches.  This assumes that all
 * the pointers in the struct are either pointers to memory that won't be
 * freed, such as constant strings or persistent memory allocations, or are
 * NULL.
 */
void
fake_set_passwd(struct passwd *pwd)
{
    struct passwd *new_pwd;

    new_pwd = bmalloc(sizeof(*pwd_info));
    *new_pwd = *pwd;
    free(pwd_info);
    pwd_info = new_pwd;
}


/*
 * Free the fake struct passwd stored in the object.  This does not free any
 * of the contents.
 */
void
fake_free_passwd(void)
{
    free(pwd_info);
}


/*
 * Intercept the C library function and return the fake pwd_info struct if the
 * name matches, otherwise NULL.
 */
struct passwd *
getpwnam(const char *name)
{
    if (pwd_info != NULL && strcmp(pwd_info->pw_name, name) == 0)
        return pwd_info;
    else
        return NULL;
}
