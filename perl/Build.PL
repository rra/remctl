#!/usr/bin/perl
#
# Build script for the Net::Remctl distribution.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2007, 2012-2013, 2020
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT

use 5.008;
use strict;
use warnings;

use Module::Build;

# Create a command script that will be run by remctld during the test suite.
# These scripts will be stored in the t/data directory.
#
# $build - Module::Build object
# $name  - Name of command
# $code  - Perl code, as a string, to put into the script
#
# Returns: undef
#  Throws: Text exceptions on I/O failures
sub create_command {
    my ($build, $name, $code) = @_;
    my $path = File::Spec->catfile(qw(t data), $name);

    # Create the file.
    open(my $command, q{>}, $path)
      or die "Cannot create $path: $!\n";
    print {$command} "#!perl\nuse 5.006;\nuse strict;\nuse warnings;\n"
      or die "Cannot write to $path: $!\n";
    print {$command} $code, "\n"
      or die "Cannot write to $path: $!\n";
    close($command)
      or die "Cannot write to $path: $!\n";

    # Fix the shebang line.
    $build->fix_shebang_line($path);

    # Set permissions, which seems to be un-done by fix_shebang_ling.
    chmod(0755, $path)
      or die "Cannot set permissions on $path: $!\n";
    return;
}

# Basic package configuration.
my $build = Module::Build->new(
    module_name          => 'Net::Remctl',
    dist_author          => 'Russ Allbery <eagle@eyrie.org>',
    license              => 'mit',
    recursive_test_files => 1,
    add_to_cleanup       => [qw(cover_db t/data/cmd-hello t/data/cmd-sleep)],

    # XS configuration.  For in-tree builds, we override this to add the full
    # list of dependency libraries, which will work on more systems.
    extra_linker_flags => [qw(-lremctl)],

    # Other package relationships.
    configure_requires => { 'Module::Build' => 0.28 },
    requires           => { perl            => '5.008' },
);

# Work around a bug in the version of Module::Build that shipped with RHEL 5.
if ($Module::Build::VERSION <= 0.2807) {
    for my $param (qw(extra_compiler_flags extra_linker_flags)) {
        my $value = $build->$param;
        if (ref($value) eq 'ARRAY' && @{$value} == 1) {
            $build->$param($build->split_like_shell($value->[0]));
        }
    }
}

# Generate the build script.
$build->create_build_script;

# Generate the commands that will be run by remctld during the test suite.
## no critic (ValuesAndExpressions::RequireInterpolationOfMetachars)
create_command($build, 'cmd-hello', 'print "hello world\n" or die "fail\n"');
create_command($build, 'cmd-sleep', 'sleep 3;');
