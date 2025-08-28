#!/usr/bin/perl
#
# Test that all methods are documented in POD.
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2019, 2021, 2024 Russ Allbery <eagle@eyrie.org>
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

use 5.012;
use warnings;

use lib 't/lib';

use Test::RRA qw(skip_unless_automated use_prereq);
use Test::RRA::Config qw(@POD_COVERAGE_EXCLUDE);

use Test::More;

# Skip for normal user installs since this doesn't affect functionality.
skip_unless_automated('POD coverage tests');

# Load prerequisite modules.
use_prereq('Test::Pod::Coverage');

# Test everything found in the distribution.
all_pod_coverage_ok({ also_private => [@POD_COVERAGE_EXCLUDE] });
