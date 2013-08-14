# Helper infrastructure for remctl backend programs.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012, 2013
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

use Getopt::Long;
use Text::Wrap qw(wrap);

# Line width for formatting.  Avoid dependency on Readonly.
use constant LINE_WIDTH => 80;

# In the help output, command syntax longer than this will shift the summary
# to the following line and will not affect the column in which the summary
# starts.
use constant MAX_SYNTAX_LENGTH => 48;

# If all syntax lines exceed MAX_SYNTAX_LENGTH, start the summary output in
# this column.
use constant DEFAULT_SUMMARY_COLUMN => 40;

our $VERSION;

# This version matches the version of remctl with which this module was
# released, but with at least two digits for the minor version.
BEGIN {
    $VERSION = '3.05';
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

# Build two parallel arrays of syntax and summary information for help output.
# This is broken out into a separate method so that it can be called
# recursively for nested commands.
#
# $self         - The Net::Remctl::Backend object
# $commands_ref - The commands definition to generate per-command help for
#
# Returns: Array of a pair of references to arrays.  The first is the syntax
#          for each command and the second is the summary.
sub _build_help {
    my ($self, $commands_ref) = @_;
    my (@syntax, @summary);

    # Construct two parallel lists, one of syntax and one of summaries.  Skip
    # commands that are missing a syntax description.  Add in the length of
    # the command.
  COMMAND:
    for my $command (sort keys %{$commands_ref}) {
        my $config = $commands_ref->{$command};

        # If this is a nested command, recurse.  We store the results of the
        # nested command processing for later use since we want the root
        # command to appear first in the help output, before the nested
        # commands.
        my ($more_syntax_ref, $more_summary_ref);
        if ($config->{nested}) {
            my $nest = $config->{nested};
            ($more_syntax_ref, $more_summary_ref) = $self->_build_help($nest);
        }

        # Get the syntax and summary for this command and add it to the
        # arrays if there is any syntax defined.  Avoid trailing whitespace
        # if there is no extra syntax.
        my $syntax  = $config->{syntax};
        my $summary = $config->{summary};
        if (defined($syntax)) {
            if (length($syntax) > 0) {
                push(@syntax, $command . q{ } . $syntax);
            } else {
                push(@syntax, $command);
            }

            # Translate missing summaries into the empty string.
            push(@summary, $summary || q{});
        }

        # Now add any nested data, if there was any.
        if ($more_syntax_ref) {
            push(@syntax, map { $command . q{ } . $_ } @{$more_syntax_ref});
            push(@summary, @{$more_summary_ref});
        }
    }

    # Return the results.
    return (\@syntax, \@summary);
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

    # Generate two parallel lists of syntax and summaries.
    my ($syntax_ref, $summary_ref) = $self->_build_help($self->{commands});

    # Calculate the prefix that will be added to each line of the syntax.
    my $prefix = q{};
    if ($self->{help_banner}) {
        $prefix = q{ } x 2;
    }
    if ($self->{command}) {
        $prefix .= $self->{command} . q{ };
    }

    # Calculate the longest syntax length, except for very long lines that
    # pass the MAX_SYNTAX cutoff.
    my $longest_syntax_len = 0;
    my $max_syntax_len     = MAX_SYNTAX_LENGTH - length($prefix);
    for my $syntax (@{$syntax_ref}) {
        my $length = length($syntax);
        if ($length > $longest_syntax_len && $length <= $max_syntax_len) {
            $longest_syntax_len = $length;
        }
    }

    # The summary starts two blank spaces after the longest syntax block that
    # does not exceed the length cap.  If all the syntax lines are too long,
    # arbitrarily start in column 40.
    my $pad_column;
    if ($longest_syntax_len > 0) {
        $pad_column = length($prefix) + $longest_syntax_len + 2;
    } else {
        $pad_column = DEFAULT_SUMMARY_COLUMN;
    }

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
    for my $i (0 .. $#{$syntax_ref}) {
        my $syntax  = $syntax_ref->[$i];
        my $summary = $summary_ref->[$i];

        # If there is no summary, just add the bare command syntax.
        if (!$summary) {
            $output .= $prefix . $syntax . "\n";
            next;
        }

        # Otherwise, line up the columns and wrap the summary.  If the length
        # goes beyond the pad column minus two (for the blank spaces), move
        # the summary to the next line.
        my $length = length($prefix) + length($syntax);
        if ($length <= $pad_column - 2) {
            my $padding = q{ } x ($pad_column - $length);
            $syntax = $prefix . $syntax . $padding;
            $output .= wrap($syntax, q{ } x $pad_column, $summary . "\n");
        } else {
            my $padding = q{ } x $pad_column;
            $output .= $prefix . $syntax . "\n";
            $output .= wrap($padding, $padding, $summary . "\n");
        }
    }

    # Return the formatted results.
    return $output;
}

# Report a fatal error in running a particular command.  This is the internal
# method used to handle error messages for a particular command.  It will
# append the globally-configured command name, if there is one, and the
# current command.
#
# $self    - The Net::Remctl::Backend object
# $command - The command being run (actually the remctl subcommand)
# $error   - The error message
#
# Returns: undef
#  Throws: Throws the text message constructed from the above
sub _command_die {
    my ($self, $command, $error) = @_;

    # If we have a global command, prepend it to the subcommand.
    if ($self->{command}) {
        $command = $self->{command} . q{ } . $command;
    }

    # Report the error.
    die "$command: $error\n";
}

# Given the command arguments, find the configuration hash for that command.
# This implements proper search through the nested command structure.
#
# $self         - The Net::Remctl::Backend object
# $commands_ref - The command hash to search through
# $command      - The command
# @args         - The remaining arguments
#
# Returns: A list of the full command name, a reference the config hash, and a
#          reference to the remaining arguments.  Only the first will be
#          present if the command could not be found.
sub _parse_command {
    my ($self, $commands_ref, $command, @args) = @_;

    # If the command is not found in the dispatch table, return immediately.
    my $config = $commands_ref->{$command};
    if (!$config) {
        return ($command);
    }

    # If the command is not nested, this is the simple case.
    if (!$config->{nested}) {
        return ($command, $config, \@args);
    }

    # If the command is nested, the value of the nested parameter is another
    # command definition struct.  If we have further arguments, call ourselves
    # recursively to find the correct command.  Otherwise, we have found the
    # command iff there is a code parameter set.
    if (@args) {
        my ($subcommand, $subconfig, $args_ref)
          = $self->_parse_command($config->{nested}, @args);
        $command .= q{ } . $subcommand;
        return ($command, $subconfig, $args_ref);
    } elsif ($config->{code}) {
        return ($command, $config, \@args);
    } else {
        return ($command);
    }
}

# Check the arguments for validity against any check regexes.
#
# $self        - The Net::Remctl::Backend object
# $command     - The command being processed
# $regexes_ref - The array of check regexes for each argument position
# $args_ref    - The array of arguments
#
# Returns: undef
#  Throws: Throws a text error message on validity failure
sub _check_args_regex {
    my ($self, $command, $regexes_ref, $args_ref) = @_;

    # Loop through thte arguments and check against the corresponding regex.
    for my $i (0 .. $#{$args_ref}) {
        my $arg   = $args_ref->[$i];
        my $regex = $regexes_ref->[$i];
        if (defined($regex) && $arg !~ $regex) {
            if ($arg =~ m{ [^[:print:]] }xms) {
                my $n = $i + 1;
                $self->_command_die($command, "invalid data in argument #$n");
            } else {
                $self->_command_die($command, "invalid argument: $arg");
            }
        }
    }
    return;
}

# Parse command-line options and do any required error handling.
#
# $self       - The Net::remctl::Backend object
# $command    - The command being run, for error reporting
# $config_ref - A reference to the options specification
# $args_ref   - The arguments to the command
#
# Returns: A list composed of a reference to a hash of options and values,
#          followed by a reference to the remaining arguments after options
#          have been extracted
#  Throws: A text error message if the options are invalid
sub _parse_options {
    my ($self, $command, $config_ref, $args_ref) = @_;

    # Use the object-oriented syntax to isolate configuration options from the
    # rest of the program.
    my $parser = Getopt::Long::Parser->new;
    $parser->configure(qw(bundling no_ignore_case require_order));

    # Parse the options and capture any errors, turning them into exceptions.
    # The first letter of the Getopt::Long warning message will be capitalized
    # but we want it to be lowercase to follow our error message standard.
    my %options;
    {
        my $error = 'option parsing failed';
        local $SIG{__WARN__} = sub { ($error) = @_ };
        local @ARGV = @{$args_ref};
        if (!$parser->getoptions(\%options, @{$config_ref})) {
            $error =~ s{ \n+ \z }{}xms;
            $error =~ s{ \A (\w) }{ lc($1) }xmse;
            $self->_command_die($command, $error);
        }
        $args_ref = [@ARGV];
    }

    # Success.  Return the options and the remaining arguments.
    return (\%options, $args_ref);
}

# The core of the code, called from the main routine of a backend.  Parse the
# command line and either handle the command directly (for the help command)
# or dispatch it as configured in the object.
#
# If the command and optional arguments aren't given as parameters, expects
# @ARGV to be set to the parameters passed to the backend script.
#
# $self    - The Net::Remctl::Backend object
# $command - The command (remctl subcommand) to run (optional)
# @args    - Additional arguments (optional)
#
# Returns: The return value of the command, which should be an exit status
#  Throws: Text exceptions on syntax errors, unknown commands, etc.
sub run {
    my ($self, @args) = @_;
    if (!@args) {
        @args = @ARGV;
    }

    # Find the command.
    my ($command, $config, $args_ref)
      = $self->_parse_command($self->{commands}, @args);

    # If we can't find it, it's either the help command or we throw an error.
    if (!$config) {
        if ($command eq 'help') {
            print {*STDOUT} $self->help or die "Cannot write to STDOUT: $!\n";
            return 0;
        } else {
            die "Unknown command $command\n";
        }
    }

    # Parse options if any are configured.
    my $options_ref;
    if ($config->{options}) {
        ($options_ref, $args_ref)
          = $self->_parse_options($command, $config->{options}, $args_ref);
    }

    # If configured to read data from standard input, do so, and splice that
    # into the argument list.  Save the index of the stdin argument for later
    # since the error message for invalid arguments changes.
    my $stdin_index;
    my $args_count = scalar(@{$args_ref});
    if (defined($config->{stdin})) {
        my $stdin = do { local $/ = undef; <STDIN> };
        $stdin_index = $config->{stdin};
        if ($stdin_index == -1) {
            $stdin_index = $args_count + 1;
        }
        splice(@{$args_ref}, $stdin_index - 1, 0, $stdin);
    }

    # Check the number of arguments if desired.
    if (defined($config->{args_max}) && $config->{args_max} < $args_count) {
        $self->_command_die($command, 'too many arguments');
    }
    if (defined($config->{args_min}) && $config->{args_min} > $args_count) {
        $self->_command_die($command, 'insufficient arguments');
    }

    # If there are check regexes, apply them to the arguments.
    if ($config->{args_match}) {
        $self->_check_args_regex($command, $config->{args_match}, $args_ref);
    }

    # Add the result of options parsing onto the beginning of the arguments
    # if option parsing was done.
    if ($options_ref) {
        unshift(@{$args_ref}, $options_ref);
    }

    # Run the command.
    return $config->{code}->(@{$args_ref});
}

1;

=for stopwords
remctl remctld backend subcommand subcommands Allbery MERCHANTABILITY
NONINFRINGEMENT sublicense STDERR STDOUT regex regexes CONFIG undef
stdin ARG

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

=item args_match

A reference to an array of regexes that must match the arguments to this
function.  Each element of the array is matched against the corresponding
element in the array of arguments, and if the corresponding regular
expression does not match, the command will be rejected with an error
about an invalid argument.  Set the regular expression to undef to not
check the corresponding argument.

There is currently no way to check all arguments in commands that take any
number of arguments.

=item args_max

The maximum number of arguments.  If there are more than this number of
arguments, run() will die with an error message without running the
command.

=item args_min

The minimum number of arguments.  If there are fewer than this number of
arguments, run() will die with an error message without running the
command.

=item code

A reference to the sub that implements this command.  This sub will be
called with the arguments passed on the command line as its arguments
(possibly preceded by the options hash if the C<options> parameter is set
as described below).  It should return the exit status that should be used
by the backend as a whole: 0 for success and some non-zero value for an
error condition.  This sub should print to STDOUT and STDERR to
communicate back to the remctl client.

=item nested

If set, indicates that this is a nested command.  The value should be a
nested hash of command definitions, the same as the parameter to the
C<commands> argument to new().  When this is set, the first argument to
this command is taken to be a subcommand name, which is looked up in the
hash.  All of the hash parameters are interpreted the same as if it were a
top-level command.

If this command is called without any arguments, behavior varies based on
whether the C<code> parameter is also set alongside the C<nested>
parameter.  If C<code> is set, the command is called normally, with no
arguments.  If C<code> is not set, calling this command without a
subcommand is treated as an unknown command.

=item options

A reference to an array of Getopt::Long option specifications.  If this
setting is present, the arguments passed to run() will be parsed by
Getopt::Long using this option specification first, before any other
option processing (including checking for minimum and maximum numbers of
arguments, checking the validity of arguments, or replacing arguments with
data from standard input).  The result of parsing options will be passed,
as a reference to a hash, as the first argument to the code that
implements this command, with all remaining arguments passed as the
subsequent arguments.

For example, if this is set to C<['help|h', 'version|v']> and the arguments
passed to run() are:

    command -hv foo bar

then the code implementing C<command> will be called with the following
arguments:

    { help => 1, version => 1 }, 'foo', 'bar'

Getopt::Long will always be configured with the options C<bundling>,
C<no_ignore_case>, and C<require_order>.  This means, among other things,
that the first non-option argument will stop option parsing and all
remaining arguments will be passed through verbatim.

If Getopt::Long rejects the options (due to an unknown option or an
invalid argument to an option, for example), run() will die with the error
message from Getopt::Long without running the command.

=item stdin

Specifies that one argument to this function should be read from standard
input.  All of the data on standard input until end of file will be read
into memory, and that data will become the argument number given by the
value of this key is the argument (based at 1).  So if this property is
set to 1, the first argument will be the data from standard input, and any
other arguments will be shifted down accordingly.  The value may be -1, in
which case the data from standard input will become the last argument, no
matter how many arguments there currently are.

Checks for the number of arguments and for the validity of arguments with
regular expression verification are done after reading the data from
standard input and transforming the argument list accordingly.

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

=item run([COMMAND[, ARG ...]])

Parse the command line and perform the appropriate action.  The return
value will be the return value of the command run (if any), which should
be the exit status that the backend script should use.

The command (which is the remctl subcommand) and arguments can be passed
directly to run() as parameters.  If no arguments are passed, run()
expects @ARGV to contain the parameters passed to the backend script.
Either way the first argument will be the subcommand, used to find the
appropriate command to run, and any remaining arguments will be arguments
to that command.  (Note that if the C<options> parameter is set, the first
argument passed to the underlying command will be the options hash.)

If there are errors in the parameters to the command, run() will die with
an appropriate error message.

=back

=head1 DIAGNOSTICS

Since Net::Remctl::Backend is designed to handle command line parsing for
a script and report appropriate errors if there are problems with the
argument, its run() method may die with various errors.  The possible
errors are listed below.  All will be terminated with a newline so the
Perl context information won't be appended.

=over 4

=item %s: insufficient arguments

The given command was configured with a C<args_min> parameter, and the
user passed in fewer arguments than that.

=item %s: invalid argument: %s

The given argument to the given command failed to match a regular
expression that was set with an C<args_match> parameter.

=item %s: too many arguments

The given command was configured with a C<args_max> parameter, and the
user passed in more arguments than that.

=back

=head1 BUGS

There is no way to check all arguments with a regex when the command
supports any number of arguments.

=head1 SEE ALSO

remctld(8)

The current version of this module is available from its web page at
L<http://www.eyrie.org/~eagle/software/remctl/>.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>

=head1 COPYRIGHT AND LICENSE

Copyright 2012, 2013 The Board of Trustees of the Leland Stanford Junior
University

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
