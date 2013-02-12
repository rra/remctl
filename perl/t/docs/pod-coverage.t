#!/usr/bin/perl
#
# Test that all methods are documented in POD.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More;

# Skip tests if Test::Pod::Coverage is not installed.
if (!eval { require Test::Pod::Coverage }) {
    plan skip_all => 'Test::Pod::Coverage required to test POD coverage';
}
Test::Pod::Coverage->import;

# Test everything found in the distribution.
all_pod_coverage_ok();
