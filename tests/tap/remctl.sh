# Shell function library to start and stop remctld
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
# Copyright 2009, 2012
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

# Start remctld.  Takes the path to remctld, which may be found via configure,
# and the path to the configuration file.
remctld_start () {
    tap_pidfile=`test_tmpdir`/remctld.pid
    rm -f "$tap_pidfile"
    tap_keytab=`test_file_path config/keytab`
    tap_principal=`test_file_path config/principal`
    tap_principal=`cat "$tap_principal" 2>/dev/null`
    if [ -z "$tap_keytab" ] || [ -z "$tap_principal" ] ; then
        return 1
    fi
    if [ -n "$VALGRIND" ] ; then
        ( "$VALGRIND" --log-file=valgrind.%p --leak-check=full "$1" -m \
          -p 14373 -s "$tap_principal" -P "$tap_pidfile" -f "$2" -d -S -F \
          -k "$tap_keytab" &)
        [ -f "$tap_pidfile" ] || sleep 5
    else
        ( "$1" -m -p 14373 -s "$tap_principal" -P "$tap_pidfile" -f "$2" \
          -d -S -F -k "$tap_keytab" &)
    fi
    [ -f "$tap_pidfile" ] || sleep 1
    [ -f "$tap_pidfile" ] || sleep 1
    if [ ! -f "$tap_pidfile" ] ; then
        bail 'remctld did not start'
    fi
}

# Stop remctld and clean up.
remctld_stop () {
    tap_pidfile=`test_tmpdir`/remctld.pid
    if [ -f "$tap_pidfile" ] ; then
        kill -TERM `cat "$tap_pidfile"`
        rm -f "$tap_pidfile"
    fi
}
