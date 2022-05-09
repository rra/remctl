/*
 * Define the Kerberos GSS-API mechanism OID.
 *
 * This short bit of code exposes the Kerberos GSS-API mechanism OID has
 * gss_mech_krb5 on platforms that don't have GSS_KRB5_MECHANISM or
 * gss_mech_krb5, such as Solaris 10.
 *
 * On Solaris 10, we could call gss_str_to_oid to convert "kerberos_v5" to an
 * OID or to parse the numeric form of an OID, but this doesn't rely on
 * configuration files and is just as portable in practice.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2011, 2013
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
#include <portable/gssapi.h>

#if !HAVE_DECL_GSS_MECH_KRB5
static const gss_OID_desc gss_mech_krb5_desc = {
    9, "\052\206\110\206\367\022\001\002\002"};
const gss_OID_desc *const gss_mech_krb5 = &gss_mech_krb5_desc;
#endif
