#!/usr/bin/perl
#
# Test Perl code for strict, warnings, and syntax.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More;

# Skip tests if Test::Strict is not installed.
if (!eval { require Test::Strict }) {
    plan skip_all => 'Test::Strict required to test Perl syntax';
    $Test::Strict::TEST_WARNINGS = 0;    # suppress warning
}
Test::Strict->import;

# Test everything in the perl directory.  We also want to check use warnings.
# Ignore the configure template.
$Test::Strict::TEST_WARNINGS = 1;
all_perl_files_ok();
