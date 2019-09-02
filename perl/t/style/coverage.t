#!/usr/bin/perl
#
# Test Perl code for test coverage.
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2013-2014
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
#
# SPDX-License-Identifier: MIT

use 5.006;
use strict;
use warnings;

use lib 't/lib';

use Test::RRA qw(skip_unless_author use_prereq);
use Test::RRA::Config qw($COVERAGE_LEVEL @COVERAGE_SKIP_TESTS);

use File::Spec;
use Test::More;

# Skip code coverage unless author tests are enabled since it takes a long
# time, is sensitive to versions of various libraries, and does not detect
# functionality problems.
skip_unless_author('Coverage tests');

# Load prerequisite modules.
use_prereq('Devel::Cover');
use_prereq('Test::Strict');

# Build a list of test directories to use for coverage.
my %ignore = map { $_ => 1 } qw(data docs style), @COVERAGE_SKIP_TESTS;
opendir(my $testdir, 't') or BAIL_OUT("cannot open t: $!");
my @t_dirs = readdir($testdir) or BAIL_OUT("cannot read t: $!");
closedir($testdir) or BAIL_OUT("cannot close t: $!");

# Filter out ignored and system directories.
@t_dirs = grep { !$ignore{$_} } File::Spec->no_upwards(@t_dirs);

# Prepend the t directory name to the directories.
@t_dirs = map { File::Spec->catfile('t', $_) } @t_dirs;

# Disable POD coverage; that's handled separately and is confused by
# autoloading.
$Test::Strict::DEVEL_COVER_OPTIONS
  = '-coverage,statement,branch,condition,subroutine';

# Do the coverage analysis.
all_cover_ok($COVERAGE_LEVEL, @t_dirs);

# Hack to suppress "used only once" warnings.
END { $Test::Strict::DEVEL_COVER_OPTIONS = q{} }
