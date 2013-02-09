#!/usr/bin/perl
#
# Check that too-new features of Perl are not being used.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More;

# Currently, Perl 5.8 or newer is required.
my $MINIMUM_VERSION = '5.008';

# Skip tests if Test::MinimumVersion is not installed.
if (!eval { require Test::MinimumVersion }) {
    plan skip_all => 'Test::MinimumVersion required to test version limits';
}
Test::MinimumVersion->import;

# Check all files in the Perl distribution.
all_minimum_version_ok($MINIMUM_VERSION);
