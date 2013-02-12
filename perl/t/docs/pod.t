#!/usr/bin/perl
#
# Check all POD documents for syntax and formatting errors.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More;

# Skip tests if Test::Pod is not installed.
if (!eval { require Test::Pod }) {
    plan skip_all => 'Test::Pod required to test POD syntax';
}
Test::Pod->import;

# Check all POD in the Perl distribution.
all_pod_files_ok();
