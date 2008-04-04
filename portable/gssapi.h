/* $Id$
 *
 * Portability wrapper around <gssapi.h>.
 *
 * This header tries to encapsulate the differences between the MIT and
 * Heimdal GSS-API implementations and the differences between various
 * versions.
 */

#ifndef PORTABLE_GSSAPI_H
#define PORTABLE_GSSAPI_H 1

#include <config.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* Handle compatibility to older versions of MIT Kerberos. */
#ifndef HAVE_GSS_RFC_OIDS
# define GSS_C_NT_USER_NAME gss_nt_user_name
# define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
#endif

/* Heimdal provides a nice #define for this. */
#if !HAVE_DECL_GSS_KRB5_MECHANISM
# include <gssapi/gssapi_krb5.h>
# define GSS_KRB5_MECHANISM gss_mech_krb5
#endif

#endif /* PORTABLE_GSSAPI_H */
