#!/usr/bin/perl
#
# General tests for Net::Remctl::Backend.
#
# This test uses the PerlIO feature of opening a file descriptor to a scalar.
# This may not work with versions of Perl built without PerlIO, which was more
# common prior to 5.8.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More tests => 10;

# Test loading the module.
BEGIN { use_ok('Net::Remctl::Backend') }

# Index of the sub name in the list output of caller.  I don't want to require
# people to have the Readonly module to run the test suite.
## no critic (ValuesAndExpressions::ProhibitConstantPragma)
use constant CALLER_SUB => 3;

# We'll use this variable to accumulate call sequences to test that the right
# functions were called by the backend wrapper.  Each element will be a
# reference to an array holding the called function and any arguments.
my @CALLS;

# Run the backend and capture its output and return status.
#
# $backend - The Net::Remctl::Backend object
# @args    - Arguments to treat like command-line arguments to the backend
#
# Returns: List of the stdout output, the stderr output, and the return status
sub run_wrapper {
    my ($backend, @args) = @_;
    my $output = q{};
    my $error  = q{};

    # Save STDOUT and STDERR and redirect to scalars.
    open my $oldout, '>&', \*STDOUT or BAIL_OUT("Cannot save STDOUT: $!");
    open my $olderr, '>&', \*STDERR or BAIL_OUT("Cannot save STDERR: $!");
    close STDOUT or BAIL_OUT("Cannot close STDOUT: $!");
    close STDERR or BAIL_OUT("Cannot close STDERR: $!");
    open STDOUT, '>', \$output or BAIL_OUT("Cannot redirect STDOUT: $!");
    open STDERR, '>', \$error  or BAIL_OUT("Cannot redirect STDERR: $!");

    # Run the backend.
    local @ARGV = @args;
    my $status = $backend->run();

    # Restore STDOUT and STDERR.
    open STDOUT, '>&', $oldout or BAIL_OUT("Cannot restore STDOUT: $!");
    open STDERR, '>&', $olderr or BAIL_OUT("Cannot restore STDERR: $!");
    close $oldout or BAIL_OUT("Cannot close duplicate STDOUT: $!");
    close $olderr or BAIL_OUT("Cannot close duplicate STDERR: $!");

    # Return the results.
    return ($output, $error, $status);
}

# Helper function for test commands to save arguments in the call stack.
#
# Returns: undef
sub save_args {
    my (@args) = @_;
    my $caller = (caller 1)[CALLER_SUB];
    push @CALLS, [$caller, @args];
    return;
}

# Set up a couple of test commands.
sub test_cmd1 { my (@args) = @_; save_args(@args); return 1 }
sub test_cmd2 { my (@args) = @_; save_args(@args); return 2 }

# Set up test for simple command dispatch.
my %commands = (
    cmd1 => { code => \&test_cmd1 },
    cmd2 => { code => \&test_cmd2 },
);
my $backend = Net::Remctl::Backend->new({ commands => \%commands });
isa_ok($backend, 'Net::Remctl::Backend');

# Try running cmd1 and check the result.
my ($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2));
is($status, 1,   'cmd1 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Now try running cmd2 and check the result.
($out, $err, $status) = run_wrapper($backend, qw(cmd2 arg1 arg2));
is($status, 2,   'cmd2 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd2 arg1 arg2)]], 'cmd2 called correctly');
@CALLS = ();
