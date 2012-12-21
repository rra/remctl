# Helper infrastructure for remctl backend programs.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

package Net::Remctl::Backend;

use 5.006;
use strict;
use warnings;

our $VERSION;

# This version should be increased on any code change to this module.  Always
# use two digits for the minor version with a leading zero if necessary so
# that it will sort properly.
BEGIN {
    $VERSION = '1.00';
}

# Constructor.  Takes all possible parameters as a hash.  See the POD
# documentation for details of the possible parameters and their meanings.
#
# $class  - Class caller requests construction of
# $config - Parameters for the backend behavior
#
# Returns: Newly constructed Net::Remctl::Backend object
sub new {
    my ($class, $config) = @_;
    my $self = { %{$config} };
    bless $self, $class;
    return $self;
}

# The core of the code, called from the main routine of a backend.  Parse the
# command line and either handle the command directly (for the help command)
# or dispatch it as configured in the object.
#
# Expects @ARGV to be set to the parameters passed to the backend script.
#
# $self - The Net::Remctl::Backend object
#
# Returns: The return value of the command, which should be an exit status
#  Throws: Text exceptions on syntax errors, unknown commands, etc.
sub run {
    my ($self) = @_;
    my ($command, @args) = @ARGV;

    # Look up the command in the dispatch table and run it or throw an error.
    if (!$self->{commands}{$command}) {
        die "Unknown command $command\n";
    }
    return $self->{commands}{$command}{code}->(@args);
}

1;

=for stopwords
remctl remctld backend subcommand subcommands Allbery MERCHANTABILITY
NONINFRINGEMENT sublicense STDERR STDOUT

=head1 NAME

Net::Remctl::Backend - Helper infrastructure for remctl backend programs

=head1 SYNOPSIS

    use Net::Remctl::Backend;

    my %commands = (
        cmd1 => { code => \&run_cmd1 },
        cmd2 => { code => \&run_cmd2 },
    );
    my $backend = Net::Remctl::Backend->new({
        commands => \%commands,
    });
    exit $backend->run();

=head1 DESCRIPTION

Net::Remctl::Backend provides a framework for remctl backend commands
(commands run by B<remctld>).  It can be configured with a list of
supported subcommands and handles all the command-line parsing and syntax
checking, dispatching the command to the appropriate sub if it is valid.

=head1 CLASS METHODS

=over 4

=item new(CONFIG)

Create a new backend object with the given configuration.  CONFIG should
be an anonymous hash with one or more of the following keys:

=over 4

=item commands

The value of this key should be an anonymous hash describing all of the
commands that are supported.  See below for the supported keys in the
command configuration.

=back

The commands key, described above, takes a hash of properties for each
subcommand supported by this backend.  The possible keys in that hash are:

=over 4

=item code

A reference to the sub that implements this command.  This sub will be
called with the arguments passed on the command line as its arguments.  It
should return the exit status that should be used by the backend as a
whole: 0 for success and some non-zero value for an error condition.  This
sub should print to STDOUT and STDERR to communicate back to the remctl
client.

=back

=back

=head1 INSTANCE METHODS

=over 4

=item run()

Parse the command line and perform the appropriate action.  The return
value will be the return value of the command run (if any), which should
be the exit status that the backend script should use.

run() expects @ARGV to contain the parameters passed to the backend
script.  The first argument will be the subcommand, used to find the
appropriate command to run, and any remaining arguments will be arguments
to that command.

=back

=head1 SEE ALSO

remctld(8)

The current version of this module is available from its web page at
L<http://www.eyrie.org/~eagle/software/remctl/>.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>

=head1 COPYRIGHT AND LICENSE

Copyright 2012 The Board of Trustees of the Leland Stanford Junior
University.  All rights reserved.

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

=cut
