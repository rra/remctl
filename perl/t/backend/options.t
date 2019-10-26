#!/usr/bin/perl
#
# Tests for option handling in Net::Remctl::Backend.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT

use strict;
use warnings;

use lib 't/lib';

use Getopt::Long;
use Test::More tests => 13;
use Test::Remctl qw(run_wrapper);

# Load the module.
BEGIN { use_ok('Net::Remctl::Backend') }

# The test function.  Checks the hash passed to the function against the key
# and value pairs given as its remaining arguments and ensures that they
# match.  The results are reported using Test::More.  If the value is given as
# the empty string, that key is expected to not be set in the options.
#
# $options_ref - Reference to the hash of options parsed by Getopt::Long
# $test_name   - Name of test for reporting results
# %expected    - Key and value pairs expected to be set
#
# Returns: undef
sub cmd_options {
    my ($options_ref, $test_name, %expected) = @_;
    is_deeply($options_ref, \%expected, $test_name);
    return;
}

# Set up test with one command, which takes a variety of options, both short
# and long.
my %commands = (
    options => {
        code    => \&cmd_options,
        options => [qw(debug+ help|h input|i=s output=s sort! version|v)],
    },
);
my $backend = Net::Remctl::Backend->new({ commands => \%commands });
isa_ok($backend, 'Net::Remctl::Backend');

# Simple test with no options at all.
$backend->run('options', 'no options');

# Pass a variety of interesting options.
my @flags  = qw(--debug --debug -hv --output=foo --no-sort -i bar);
my %result = (
    debug   => 2,
    help    => 1,
    input   => 'bar',
    output  => 'foo',
    sort    => 0,
    version => 1,
);
$backend->run('options', @flags, 'all options', %result);

# Pass only a single option.
@flags  = qw(--input foo);
%result = (input => 'foo');
$backend->run('options', @flags, 'one option', %result);

# Mix options and non-options.
@flags = qw(-i foo mixed --debug --debug);
$commands{mixed}{code} = sub {
    my ($options_ref, @args) = @_;
    is_deeply($options_ref, { input => 'foo' }, 'mixed options');
    is_deeply([qw(mixed --debug)], [@args], '...and arguments are correct');
};
$commands{mixed}{options} = $commands{options}{options};
$backend->run('mixed', qw(-i foo mixed --debug));

# Handling of an unknown option.
my ($output, $error, $status) = run_wrapper($backend, 'options', '--foo');
is($status, 255,                              'unknown option returns 255');
is($output, q{},                              '...with no output');
is($error,  "options: unknown option: foo\n", '...and correct error');

# Handling of an option with an invalid argument.
$commands{number}{code}    = \&cmd_options;
$commands{number}{options} = ['number=i'];
($output, $error, $status) = run_wrapper($backend, 'number', '--number=foo');
is($status, 255, 'unknown option returns 255');
is($output, q{}, '...with no output');
is(
    $error,
    qq{number: value "foo" invalid for option number (number expected)\n},
    '...and correct error'
);
