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

use Text::Wrap qw(wrap);

# Tab and line width for formatting.  Avoid dependency on Readonly.
use constant TAB_WIDTH  => 8;
use constant LINE_WIDTH => 80;

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

# Return the summary help for all of our configured commands.  This is used by
# run() to get the string to display, but can also be called separately to get
# the formatted help summary if desired.
#
# $self - The Net::Remctl::Backend object
#
# Returns: Formatted summary help as a string
sub help {
    my ($self) = @_;

    # Construct two parallel lists, one of syntax and one of summaries.  Skip
    # commands that are missing a syntax description.  Add in the length of
    # the command.
    my (@syntax, @summary);
    for my $command (sort keys %{ $self->{commands} }) {
        next if !defined $self->{commands}{$command}{syntax};
        push @syntax, $command . q{ } . $self->{commands}{$command}{syntax};
        my $summary = $self->{commands}{$command}{summary} || q{};
        push @summary, $summary;
    }

    # Calculate the maximum syntax length.  Add in the length of the command.
    my $max_syntax_len = 0;
    for my $syntax (@syntax) {
        if (length($syntax) > $max_syntax_len) {
            $max_syntax_len = length $syntax;
        }
    }

    # Padding is constructed as follows: add two to the maximum length to
    # account for two blank spaces at the start of the line if there is a
    # help_prefix set.  Add the length of the command and a space if command
    # is set.  Then pad to two blanck spaces after the longest syntax block.
    my $prefix = q{};
    if ($self->{help_banner}) {
        $prefix = q{ } x 2;
    }
    if ($self->{command}) {
        $prefix .= $self->{command} . q{ };
    }
    my $pad_column = $max_syntax_len + 2 + length $prefix;

    # Add the help banner if one is set.  Add a newline if there isn't one.
    my $output = q{};
    if ($self->{help_banner}) {
        $output = $self->{help_banner};
        if ($self->{help_banner} !~ m{ \n \z }xms) {
            $output .= "\n";
        }
    }

    # Now, we can format each line of the help output with Text::Wrap.
    local $Text::Wrap::columns  = LINE_WIDTH;
    local $Text::Wrap::unexpand = 0;
    for my $i (0 .. $#syntax) {
        if (!$summary[$i]) {
            $output .= $prefix . $syntax[$i] . "\n";
            next;
        }
        my $length  = length($prefix) + length $syntax[$i];
        my $padding = q{ } x ($pad_column - $length);
        my $syntax  = $prefix . $syntax[$i] . $padding;
        $output .= wrap($syntax, q{ } x $pad_column, $summary[$i] . "\n");
    }

    # Return the formatted results.
    return $output;
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

    # Look up the command in the dispatch table and run it, handle the help
    # command, or throw an error.  Allow the caller to define a help command
    # to override ours.
    if (!$self->{commands}{$command}) {
        if ($command eq 'help') {
            print {*STDOUT} $self->help or die "Cannot write to STDOUT: $!\n";
            return 0;
        }
        else {
            die "Unknown command $command\n";
        }
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

=item command

If set, defines the base remctl command implemented by this backend.  The
primary use of this string is in usage and help output.  If set, it will
be added to the beginning of each command syntax description so that the
help output will match the remctl command that the user actually runs.

=item commands

The value of this key should be an anonymous hash describing all of the
commands that are supported.  See below for the supported keys in the
command configuration.

=item help_banner

If set, the value will be displayed as the first line of help output.
Recommended best practice is to use a string of the form:

    <service> remctl help:

where <service> is something like C<Event handling> or C<User database> or
whatever this set of commands generally does or manipulates.

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

=item syntax

The syntax of this subcommand.  This should be short, since it needs to
fit on the same line as the summary of what this subcommand does.  Both
the command and subcommand should be omitted; the former will be set by
the I<command> parameter to the new() constructor for
Net::Remctl::Backend, and the latter will come from the command itself.  A
typical example will look like:

    syntax => '<object>'

which will result in help output (assuming I<command> is set to C<object>
and this parameter is set on the C<delete> command) that looks like:

    object delete <object>

Use abbreviations heavily to keep this string short so that the help
output will remain readable.

Set this key to the empty string to indicate that this subcommand takes
no arguments or flags.

If this key is omitted, the subcommand will be omitted from help output.

=item summary

The summary of what this subcommand does, as text.  Ideally, this should
fit on the same line with the syntax after the help output has been laid
out in columns.  If it is too long to fit, it will be wrapped, with each
subsequent line indented to the column where the summaries start.

If this key is omitted, the subcommand will still be shown in help
output, provided that it has a syntax key, but without any trailing
summary.

=back

=back

=head1 INSTANCE METHODS

=over 4

=item help()

Returns the formatted help summary for the commands supported by this
backend.  This is the same as would be printed to standard output in
response to the command C<help> with no arguments.  The output will
consist of the syntax and summary attributes for each command that has a
syntax attribute defined, as described above under the command
specification.  It will be wrapped to 80 columns.

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
