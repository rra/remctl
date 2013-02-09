#!/usr/bin/perl
#
# Check for perlcritic errors in included Perl code.
#
# Checks all Perl code in blib/lib, t, and Makefile.PL for problems uncovered
# by perlcritic.  This test is disabled unless RRA_MAINTAINER_TESTS is set,
# since coding style will not interfere with functionality.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use 5.006;
use strict;
use warnings;

use Carp qw(croak);
use File::Spec;
use Test::More;

# Find a configuration file included as data in the test suite.
#
# $file - Path of file relative to the t directory
#
# Returns: Path of the file if found
#  Throws: String exception if the file could not be found.
sub test_file_path {
    my ($file) = @_;
    for my $base (qw{t tests .}) {
        if (-f "$base/$file") {
            return "$base/$file";
        }
    }
    croak "cannot find test file $file";
}

# Skip tests unless we're running the test suite in maintainer mode.
if (!$ENV{RRA_MAINTAINER_TESTS}) {
    plan skip_all => 'Coding style tests only run for maintainer';
}

# Skip tests if Test::Perl::Critic is not installed.
if (!eval { require Test::Perl::Critic }) {
    plan skip_all => 'Test::Perl::Critic required to test coding style';
}
if (!eval { require Perl::Critic::Utils }) {
    plan skip_all => 'Perl::Critic::Utils required to test coding style';
}

# Force the embedded Perl::Tidy check to use the correct configuration.
local $ENV{PERLTIDY} = test_file_path('data/perltidyrc');

# Import the configuration file and run Perl::Critic.
my $profile = test_file_path('data/perlcriticrc');
Test::Perl::Critic->import(-profile => $profile);

# By default, Test::Perl::Critic only checks blib.  We also want to check t.
my @files = Perl::Critic::Utils::all_perl_files('blib');
if (!@files) {
    @files = Perl::Critic::Utils::all_perl_files('lib');
}
push @files, Perl::Critic::Utils::all_perl_files('t');
push @files, 'Build.PL';
@files = grep { !m{ [.](?:in|tdy) }xms } @files;
plan tests => scalar @files;

# Run the actual tests.
for my $file (@files) {
    critic_ok($file);
}
