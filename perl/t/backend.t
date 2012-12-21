#!/usr/bin/perl
#
# General tests for Net::Remctl::Backend.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More tests => 6;

# Test loading the module.
BEGIN { use_ok('Net::Remctl::Backend') }

# We'll use this variable to accumulate call sequences to test that the right
# functions were called by the backend wrapper.  Each element will be a
# reference to an array holding the called function and any arguments.
my @CALLS;

# Set up a couple of test commands.
sub test_cmd1 { my (@args) = @_; push @CALLS, ['cmd1', @args]; return 1 }
sub test_cmd2 { my (@args) = @_; push @CALLS, ['cmd2', @args]; return 2 }

# Set up test for simple command dispatch.
my %commands = (
    cmd1 => { code => \&test_cmd1 },
    cmd2 => { code => \&test_cmd2 },
);
my $backend = Net::Remctl::Backend->new({ commands => \%commands });
isa_ok($backend, 'Net::Remctl::Backend');

# Try running cmd1 and check the result.
local @ARGV = qw(cmd1 arg1 arg2);
is($backend->run(), 1, 'cmd1 returns correct status');
is_deeply(\@CALLS, [[qw(cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Now try running cmd2 and check the result.
local @ARGV = qw(cmd2 arg1 arg2);
is($backend->run(), 2, 'cmd2 returns correct status');
is_deeply(\@CALLS, [[qw(cmd2 arg1 arg2)]], 'cmd2 called correctly');
