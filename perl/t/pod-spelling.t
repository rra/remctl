#!/usr/bin/perl
#
# Check for spelling errors in POD documentation.
#
# Checks all POD files in a Perl distribution using Test::Spelling.  This test
# is disabled unless RRA_MAINTAINER_TESTS is set, since spelling dictionaries
# vary too much between environments.
#
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More;

# Skip tests unless we're running the test suite in maintainer mode.
if (!$ENV{RRA_MAINTAINER_TESTS}) {
    plan skip_all => 'Spelling tests only run for maintainer';
}

# Skip tests if Test::Spelling is not installed.
if (!eval { require Test::Spelling }) {
    plan skip_all => 'Test::Spelling required to test POD spelling';
}
Test::Spelling->import;

# Check all POD in the Perl distribution.
all_pod_files_spelling_ok();
