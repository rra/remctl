# Helper functions for Perl test programs in Automake distributions.
#
# This module provides a collection of helper functions used by test programs
# written in Perl and included in C source distributions that use Automake.
# They embed knowledge of how I lay out my source trees and test suites with
# Autoconf and Automake.  They may be usable by others, but doing so will
# require closely following the conventions implemented by the rra-c-util
# utility collection.
#
# All the functions here assume that BUILD and SOURCE are set in the
# environment.  This is normally done via the C TAP Harness runtests wrapper.
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

package Test::RRA::Automake;

use 5.006;
use strict;
use warnings;

# For Perl 5.006 compatibility.
## no critic (ClassHierarchies::ProhibitExplicitISA)

use Exporter;
use File::Spec;
use Test::More;
use Test::RRA::Config qw($LIBRARY_PATH);

# Used below for use lib calls.
my ($PERL_BLIB_ARCH, $PERL_BLIB_LIB);

# Determine the path to the build tree of any embedded Perl module package in
# this source package.  We do this in a BEGIN block because we're going to use
# the results in a use lib command below.
BEGIN {
    $PERL_BLIB_ARCH = File::Spec->catdir(qw(perl blib arch));
    $PERL_BLIB_LIB  = File::Spec->catdir(qw(perl blib lib));

    # If BUILD is set, we can come up with better values.
    if (defined($ENV{BUILD})) {
        my ($vol, $dirs) = File::Spec->splitpath($ENV{BUILD}, 1);
        my @dirs = File::Spec->splitdir($dirs);
        pop(@dirs);
        $PERL_BLIB_ARCH = File::Spec->catdir(@dirs, qw(perl blib arch));
        $PERL_BLIB_LIB  = File::Spec->catdir(@dirs, qw(perl blib lib));
    }
}

# Prefer the modules built as part of our source package.  Otherwise, we may
# not find Perl modules while testing, or find the wrong versions.
use lib $PERL_BLIB_ARCH;
use lib $PERL_BLIB_LIB;

# Declare variables that should be set in BEGIN for robustness.
our (@EXPORT_OK, @ISA, $VERSION);

# Set $VERSION and everything export-related in a BEGIN block for robustness
# against circular module loading (not that we load any modules, but
# consistency is good).
BEGIN {
    @ISA       = qw(Exporter);
    @EXPORT_OK = qw(automake_setup perl_dirs test_file_path);

    # This version should match the corresponding rra-c-util release, but with
    # two digits for the minor version, including a leading zero if necessary,
    # so that it will sort properly.
    $VERSION = '4.08';
}

# Perl directories to skip globally for perl_dirs.  We ignore the perl
# directory if it exists since, in my packages, it is treated as a Perl module
# distribution and has its own standalone test suite.
my @GLOBAL_SKIP = qw(.git perl);

# Perform initial test setup for running a Perl test in an Automake package.
# This verifies that BUILD and SOURCE are set and then changes directory to
# the SOURCE directory by default.  Sets LD_LIBRARY_PATH if the $LIBRARY_PATH
# configuration option is set.  Calls BAIL_OUT if BUILD or SOURCE are missing
# or if anything else fails.
#
# $args_ref - Reference to a hash of arguments to configure behavior:
#   chdir_build - If set to a true value, changes to BUILD instead of SOURCE
#
# Returns: undef
sub automake_setup {
    my ($args_ref) = @_;

    # Bail if BUILD or SOURCE are not set.
    if (!$ENV{BUILD}) {
        BAIL_OUT('BUILD not defined (run under runtests)');
    }
    if (!$ENV{SOURCE}) {
        BAIL_OUT('SOURCE not defined (run under runtests)');
    }

    # BUILD or SOURCE will be the test directory.  Change to the parent.
    my $start = $args_ref->{chdir_build} ? $ENV{BUILD} : $ENV{SOURCE};
    my ($vol, $dirs) = File::Spec->splitpath($start, 1);
    my @dirs = File::Spec->splitdir($dirs);
    pop(@dirs);
    if ($dirs[-1] eq File::Spec->updir) {
        pop(@dirs);
        pop(@dirs);
    }
    my $root = File::Spec->catpath($vol, File::Spec->catdir(@dirs), q{});
    chdir($root) or BAIL_OUT("cannot chdir to $root: $!");

    # If BUILD is a subdirectory of SOURCE, add it to the global ignore list.
    my ($buildvol, $builddirs) = File::Spec->splitpath($ENV{BUILD}, 1);
    my @builddirs = File::Spec->splitdir($builddirs);
    pop(@builddirs);
    if ($buildvol eq $vol && @builddirs == @dirs + 1) {
        while (@dirs && $builddirs[0] eq $dirs[0]) {
            shift(@builddirs);
            shift(@dirs);
        }
        if (@builddirs == 1) {
            push(@GLOBAL_SKIP, $builddirs[0]);
        }
    }

    # Set LD_LIBRARY_PATH if the $LIBRARY_PATH configuration option is set.
    ## no critic (Variables::RequireLocalizedPunctuationVars)
    if (defined($LIBRARY_PATH)) {
        @builddirs = File::Spec->splitdir($builddirs);
        pop(@builddirs);
        my $libdir = File::Spec->catdir(@builddirs, $LIBRARY_PATH);
        my $path = File::Spec->catpath($buildvol, $libdir, q{});
        if (-d "$path/.libs") {
            $path .= '/.libs';
        }
        if ($ENV{LD_LIBRARY_PATH}) {
            $ENV{LD_LIBRARY_PATH} .= ":$path";
        } else {
            $ENV{LD_LIBRARY_PATH} = $path;
        }
    }
    return;
}

# Returns a list of directories that may contain Perl scripts and that should
# be passed to Perl test infrastructure that expects a list of directories to
# recursively check.  The list will be all eligible top-level directories in
# the package except for the tests directory, which is broken out to one
# additional level.  Calls BAIL_OUT on any problems
#
# $args_ref - Reference to a hash of arguments to configure behavior:
#   skip - A reference to an array of directories to skip
#
# Returns: List of directories possibly containing Perl scripts to test
sub perl_dirs {
    my ($args_ref) = @_;

    # Add the global skip list.
    my @skip = $args_ref->{skip} ? @{ $args_ref->{skip} } : ();
    push(@skip, @GLOBAL_SKIP);

    # Separate directories to skip under tests from top-level directories.
    my @skip_tests = grep { m{ \A tests/ }xms } @skip;
    @skip = grep { !m{ \A tests }xms } @skip;
    for my $skip_dir (@skip_tests) {
        $skip_dir =~ s{ \A tests/ }{}xms;
    }

    # Convert the skip lists into hashes for convenience.
    my %skip = map { $_ => 1 } @skip, 'tests';
    my %skip_tests = map { $_ => 1 } @skip_tests;

    # Build the list of top-level directories to test.
    opendir(my $rootdir, q{.}) or BAIL_OUT("cannot open .: $!");
    my @dirs = grep { -d $_ && !$skip{$_} } readdir($rootdir);
    closedir($rootdir);
    @dirs = File::Spec->no_upwards(@dirs);

    # Add the list of subdirectories of the tests directory.
    if (-d 'tests') {
        opendir(my $testsdir, q{tests}) or BAIL_OUT("cannot open tests: $!");

        # Skip if found in %skip_tests or if not a directory.
        my $is_skipped = sub {
            my ($dir) = @_;
            return 1 if $skip_tests{$dir};
            $dir = File::Spec->catdir('tests', $dir);
            return -d $dir ? 0 : 1;
        };

        # Build the filtered list of subdirectories of tests.
        my @test_dirs = grep { !$is_skipped->($_) } readdir($testsdir);
        closedir($testsdir);
        @test_dirs = File::Spec->no_upwards(@test_dirs);

        # Add the tests directory to the start of the directory name.
        push(@dirs, map { File::Spec->catdir('tests', $_) } @test_dirs);
    }
    return @dirs;
}

# Find a configuration file for the test suite.  Searches relative to BUILD
# first and then SOURCE and returns whichever is found first.  Calls BAIL_OUT
# if the file could not be found.
#
# $file - Partial path to the file
#
# Returns: Full path to the file
sub test_file_path {
    my ($file) = @_;
  BASE:
    for my $base ($ENV{BUILD}, $ENV{SOURCE}) {
        next if !defined($base);
        if (-f "$base/$file") {
            return "$base/$file";
        }
    }
    BAIL_OUT("cannot find $file");
    return;
}

1;
__END__

=for stopwords
Allbery Automake Automake-aware Automake-based rra-c-util ARGS
subdirectories sublicense MERCHANTABILITY NONINFRINGEMENT

=head1 NAME

Test::RRA::Automake - Automake-aware support functions for Perl tests

=head1 SYNOPSIS

    use Test::RRA::Automake qw(automake_setup perl_dirs test_file_path);
    automake_setup({ chdir_build => 1 });

    # Paths to directories that may contain Perl scripts.
    my @dirs = perl_dirs({ skip => [qw(lib)] });

    # Configuration for Kerberos tests.
    my $keytab = test_file_path('config/keytab');

=head1 DESCRIPTION

This module collects utility functions that are useful for test scripts
written in Perl and included in a C Automake-based package.  They assume
the layout of a package that uses rra-c-util and C TAP Harness for the
test structure.

Loading this module will also add the directories C<perl/blib/arch> and
C<perl/blib/lib> to the Perl library search path, relative to BUILD if
that environment variable is set.  This is harmless for C Automake
projects that don't contain an embedded Perl module, and for those
projects that do, this will allow subsequent C<use> calls to find modules
that are built as part of the package build process.

The automake_setup() function should be called before calling any other
functions provided by this module.

=head1 FUNCTIONS

None of these functions are imported by default.  The ones used by a
script should be explicitly imported.  On failure, all of these functions
call BAIL_OUT (from Test::More).

=over 4

=item automake_setup([ARGS])

Verifies that the BUILD and SOURCE environment variables are set and
then changes directory to the top of the source tree (which is one
directory up from the SOURCE path, since SOURCE points to the top of
the tests directory).

If ARGS is given, it should be a reference to a hash of configuration
options.  Only one option is supported: C<chdir_build>.  If it is set
to a true value, automake_setup() changes directories to the top of
the build tree instead.

=item perl_dirs([ARGS])

Returns a list of directories that may contain Perl scripts that should be
tested by test scripts that test all Perl in the source tree (such as
syntax or coding style checks).  The paths will be simple directory names
relative to the current directory or two-part directory names under the
F<tests> directory.  (Directories under F<tests> are broken out separately
since it's common to want to apply different policies to different
subdirectories of F<tests>.)

If ARGS is given, it should be a reference to a hash of configuration
options.  Only one option is supported: C<skip>, whose value should be a
reference to an array of additional top-level directories or directories
starting with C<tests/> that should be skipped.

=item test_file_path(FILE)

Given FILE, which should be a relative path, locates that file relative to
the test directory in either the source or build tree.  FILE will be
checked for relative to the environment variable BUILD first, and then
relative to SOURCE.  test_file_path() returns the full path to FILE or
calls BAIL_OUT if FILE could not be found.

=back

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>

=head1 COPYRIGHT AND LICENSE

Copyright 2013 The Board of Trustees of the Leland Stanford Junior
University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=head1 SEE ALSO

Test::More(3), Test::RRA(3), Test::RRA::Config(3)

The C TAP Harness test driver and libraries for TAP-based C testing are
available from L<http://www.eyrie.org/~eagle/software/c-tap-harness/>.

This module is maintained in the rra-c-util package.  The current version
is available from L<http://www.eyrie.org/~eagle/software/rra-c-util/>.

=cut
