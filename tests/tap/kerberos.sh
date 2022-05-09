# Shell function library to initialize Kerberos credentials
#
# Note that while many of the functions in this library could benefit from
# using "local" to avoid possibly hammering global variables, Solaris /bin/sh
# doesn't support local and this library aspires to be portable to Solaris
# Bourne shell.  Instead, all private variables are prefixed with "tap_".
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2016 Russ Allbery <eagle@eyrie.org>
# Copyright 2009-2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# SPDX-License-Identifier: MIT

. "${C_TAP_SOURCE}/tap/libtap.sh"

# Set up Kerberos, including the ticket cache environment variable.  Bail out
# if not successful, return 0 if successful, and return 1 if Kerberos is not
# configured.  Sets the global principal variable to the principal to use.
kerberos_setup () {
    tap_keytab=`test_file_path config/keytab`
    principal=`test_file_path config/principal`
    principal=`cat "$principal" 2>/dev/null`
    if [ -z "$tap_keytab" ] || [ -z "$principal" ] ; then
        return 1
    fi
    KRB5CCNAME=`test_tmpdir`/krb5cc_test; export KRB5CCNAME
    kinit --no-afslog -k -t "$tap_keytab" "$principal" >/dev/null </dev/null
    status=$?
    if [ $status != 0 ] ; then
        kinit -k -t "$tap_keytab" "$principal" >/dev/null </dev/null
        status=$?
    fi
    if [ $status != 0 ] ; then
        kinit -t "$tap_keytab" "$principal" >/dev/null </dev/null
        status=$?
    fi
    if [ $status != 0 ] ; then
        kinit -k -K "$tap_keytab" "$principal" >/dev/null </dev/null
        status=$?
    fi
    if [ $status != 0 ] ; then
        bail "Can't get Kerberos tickets"
    fi
    return 0
}

# Clean up at the end of a test.  Currently only removes the ticket cache.
kerberos_cleanup () {
    tap_tmp=`test_tmpdir`
    rm -f "$tap_tmp"/krb5cc_test
}

# List the contents of a keytab with enctypes and keys.  This adjusts for the
# difference between MIT Kerberos (which uses klist) and Heimdal (which uses
# ktutil).  Be careful to try klist first, since the ktutil on MIT Kerberos
# may just hang.  Takes the keytab to list and the file into which to save the
# output, and strips off the header containing the file name.
ktutil_list () {
    tap_tmp=`test_tmpdir`
    if klist -keK "$1" > "$tap_tmp"/ktutil-tmp 2>/dev/null ; then
        :
    else
        ktutil -k "$1" list --keys > "$tap_tmp"/ktutil-tmp </dev/null \
            2>/dev/null
    fi
    sed -e '/Keytab name:/d' -e "/^[^ ]*:/d" "$tap_tmp"/ktutil-tmp > "$2"
    rm -f "$tap_tmp"/ktutil-tmp
}
