#!/usr/bin/perl
#
# Tests for nested commands in Net::Remctl::Backend.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use lib 't/lib';

use Test::More tests => 35;
use Test::Remctl qw(run_wrapper);

# Load the module.
BEGIN { use_ok('Net::Remctl::Backend') }

# Index of the sub name in the list output of caller.
use constant CALLER_SUB => 3;

# We'll use this variable to accumulate call sequences to test that the right
# functions were called by the backend wrapper.  Each element will be a
# reference to an array holding the called function and any arguments.
my @CALLS;

# Helper function for test commands to save arguments in the call stack.
#
# Returns: undef
sub save_args {
    my (@args) = @_;
    my $caller = (caller(1))[CALLER_SUB];
    push(@CALLS, [$caller, @args]);
    return;
}

# Set up a couple of test commands.
sub test_cmd1 { my (@args) = @_; save_args(@args); return 1 }
sub test_cmd2 { my (@args) = @_; save_args(@args); return 2 }

# Set up nested command dispatch.  The first command has a syntax that's
# exactly 48 characters long, once the prefix is added, which tests an edge
# case for help formatting.
my %commands = (
    command => {
        code    => \&test_cmd1,
        summary => 'it is nothing but args!',
        syntax  => 'arg arg arg arg arg arg arg',
    },
    nest => {
        code    => \&test_cmd1,
        summary => 'the nested command with no subcommand',
        syntax  => q{},
        nested  => {
            cmd1 => {
                code   => \&test_cmd1,
                syntax => 'arg1 [arg2]',
            },
            cmd2 => {
                code    => \&test_cmd2,
                summary => 'apply cmd1 to the nest of arg1',
                syntax  => q{},
            },
            nest => {
                nested => {
                    bar => {
                        code     => \&test_cmd1,
                        args_min => 1,
                    },
                    foo => {
                        code    => \&test_cmd2,
                        summary => 'even more nesting!',
                        syntax  => 'arg1',
                    },
                },
            },
        },
    },
);
my %args = (
    command     => 'frobnicate',
    commands    => \%commands,
    help_banner => 'Frobnicate remctl help:',
);
my $backend = Net::Remctl::Backend->new(\%args);
isa_ok($backend, 'Net::Remctl::Backend');

# Try running the nested commands and check the result.
my ($out, $err, $status) = run_wrapper($backend, qw(nest cmd1 foo bar));
is($status, 1,   'cmd1 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 foo bar)]], 'cmd1 called correctly');
@CALLS = ();
($out, $err, $status) = run_wrapper($backend, qw(nest cmd2));
is($status, 2,   'cmd2 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd2)]], 'cmd2 called correctly');
@CALLS = ();

# Ensure there's nothing weird about the regular command.
($out, $err, $status) = run_wrapper($backend, qw(command foo bar));
is($status, 1,   'cmd1 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 foo bar)]], 'cmd1 called correctly');
@CALLS = ();

# Ensure we can call the nested command itself.
($out, $err, $status) = run_wrapper($backend, qw(nest));
is($status, 1,   'nest returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1)]], 'nest called correctly');
@CALLS = ();

# Try the double-nested commands.
($out, $err, $status) = run_wrapper($backend, qw(nest nest bar foo));
is($status, 1,   'nest nest bar returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 foo)]], 'bar called correctly');
@CALLS = ();
($out, $err, $status) = run_wrapper($backend, qw(nest nest foo));
is($status, 2,   'nest nest foo returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd2)]], 'bar called correctly');
@CALLS = ();

# Ensure an unknown nested command returns the right error.
{
    local @ARGV = qw(nest unknown);
    $status = eval { $backend->run };
}
is($status, undef, 'run() of unknown command returns undef');
is($@, "Unknown command nest unknown\n", '...with correct error');

# Ensure an unknown double-nested command returns the right error.
{
    local @ARGV = qw(nest nest unknown);
    $status = eval { $backend->run };
}
is($status, undef, 'run() of unknown command returns undef');
is($@, "Unknown command nest nest unknown\n", '...with correct error');

# Verify argument checking is handled properly in nested commands.
{
    local @ARGV = qw(nest nest bar);
    $status = eval { $backend->run };
}
is($status, undef, 'run() with insufficient arguments returns undef');
is(
    $@,
    "frobnicate nest nest bar: insufficient arguments\n",
    '...with correct error'
);

# Check the help output.
my $expected = <<'END_HELP';
Frobnicate remctl help:
  frobnicate command arg arg arg arg arg arg arg  it is nothing but args!
  frobnicate nest                                 the nested command with no
                                                  subcommand
  frobnicate nest cmd1 arg1 [arg2]
  frobnicate nest cmd2                            apply cmd1 to the nest of
                                                  arg1
  frobnicate nest nest foo arg1                   even more nesting!
END_HELP
is($backend->help, $expected, 'Help output is correct');

# If we remove the code parameter for the nest command, it's now unknown.
delete $commands{nest}{code};
{
    local @ARGV = qw(nest);
    $status = eval { $backend->run };
}
is($status, undef, 'run() of nest command returns undef');
is($@, "Unknown command nest\n", '...with correct error');
