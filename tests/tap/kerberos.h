/*
 * Utility functions for tests that use Kerberos.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2011, 2012
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

#ifndef TAP_KERBEROS_H
#define TAP_KERBEROS_H 1

#include <config.h>
#include <tests/tap/macros.h>

#ifdef HAVE_KERBEROS
# include <portable/krb5.h>
#endif

/* Holds the information parsed from the Kerberos test configuration. */
struct kerberos_config {
    char *keytab;               /* Path to the keytab. */
    char *principal;            /* Principal whose keys are in the keytab. */
    char *cache;                /* Path to the Kerberos ticket cache. */
    char *userprinc;            /* The fully-qualified principal. */
    char *username;             /* The local (non-realm) part of principal. */
    char *realm;                /* The realm part of the principal. */
    char *password;             /* The password. */
};

/*
 * Whether to skip all tests (by calling skip_all) in kerberos_setup if
 * certain configuration information isn't available.
 */
enum kerberos_needs {
    TAP_KRB_NEEDS_NONE,
    TAP_KRB_NEEDS_KEYTAB,
    TAP_KRB_NEEDS_PASSWORD,
    TAP_KRB_NEEDS_BOTH
};

BEGIN_DECLS

/*
 * Set up Kerberos, returning the test configuration information.  This
 * obtains Kerberos tickets from config/keytab, if one is present, and stores
 * them in a Kerberos ticket cache, sets KRB5_KTNAME and KRB5CCNAME.  It also
 * loads the principal and password from config/password, if it exists, and
 * stores the principal, password, username, and realm in the returned struct.
 *
 * If there is no config/keytab file, KRB5_KTNAME and KRB5CCNAME won't be set
 * and the keytab field will be NULL.  If there is no config/password file,
 * the principal field will be NULL.  If the files exist but loading them
 * fails, or authentication fails, kerberos_setup calls bail.
 *
 * kerberos_cleanup will be set up to run from an atexit handler.  This means
 * that any child processes that should not remove the Kerberos ticket cache
 * should call _exit instead of exit.  The principal will be automatically
 * freed when kerberos_cleanup is called or if kerberos_setup is called again.
 * The caller doesn't need to worry about it.
 */
struct kerberos_config *kerberos_setup(enum kerberos_needs)
    __attribute__((__malloc__));
void kerberos_cleanup(void);

/*
 * Generate a krb5.conf file for testing and set KRB5_CONFIG to point to it.
 * The [appdefaults] section will be stripped out and the default realm will
 * be set to the realm specified, if not NULL.  This will use config/krb5.conf
 * in preference, so users can configure the tests by creating that file if
 * the system file isn't suitable.
 *
 * Depends on data/generate-krb5-conf being present in the test suite.
 *
 * kerberos_cleanup_conf will clean up after this function, but usually
 * doesn't need to be called directly since it's registered as an atexit
 * handler.
 */
void kerberos_generate_conf(const char *realm);
void kerberos_cleanup_conf(void);

/* Thes interfaces are only available with native Kerberos support. */
#ifdef HAVE_KERBEROS

/* Bail out with an error, appending the Kerberos error message. */
void bail_krb5(krb5_context, krb5_error_code, const char *format, ...)
    __attribute__((__noreturn__, __nonnull__, __format__(printf, 3, 4)));

/* Report a diagnostic with Kerberos error to stderr prefixed with #. */
void diag_krb5(krb5_context, krb5_error_code, const char *format, ...)
    __attribute__((__nonnull__, __format__(printf, 3, 4)));

/*
 * Given a Kerberos context and the path to a keytab, retrieve the principal
 * for the first entry in the keytab and return it.  Calls bail on failure.
 * The returned principal should be freed with krb5_free_principal.
 */
krb5_principal kerberos_keytab_principal(krb5_context, const char *path)
    __attribute__((__nonnull__));

#endif /* HAVE_KERBEROS */

END_DECLS

#endif /* !TAP_MESSAGES_H */
