#!/usr/bin/perl
#
# Test WebAuth Perl bindings and WebLogin for strict, warnings, and syntax.
#
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More;

# Skip tests if Test::Strict is not installed.
eval { require Test::Strict };
if ($@) {
    plan skip_all => 'Test::Strict required to test Perl syntax';
    $Test::Strict::TEST_WARNINGS = 0; # suppress warning
}
Test::Strict->import;

# Test everything in the perl directory.  We also want to check use warnings.
# Ignore the configure template.
$Test::Strict::TEST_WARNINGS = 1;
all_perl_files_ok();
