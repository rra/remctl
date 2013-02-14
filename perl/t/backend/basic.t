#!/usr/bin/perl
#
# General tests for Net::Remctl::Backend.
#
# This test uses the PerlIO feature of opening a file descriptor to a scalar.
# This may not work with versions of Perl built without PerlIO, which was more
# common prior to 5.8.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Test::More tests => 44;

# Test loading the module.
BEGIN { use_ok('Net::Remctl::Backend') }

# Index of the sub name in the list output of caller.
use constant CALLER_SUB => 3;

# We'll use this variable to accumulate call sequences to test that the right
# functions were called by the backend wrapper.  Each element will be a
# reference to an array holding the called function and any arguments.
my @CALLS;

# Run the backend and capture its output and return status.
#
# $backend - The Net::Remctl::Backend object
# @args    - Arguments to treat like command-line arguments to the backend
#
# Returns: List of the stdout output, the stderr output, and the return status
sub run_wrapper {
    my ($backend, @args) = @_;
    my $output = q{};
    my $error  = q{};

    # Save STDOUT and STDERR and redirect to scalars.
    open(my $oldout, '>&', \*STDOUT) or BAIL_OUT("Cannot save STDOUT: $!");
    open(my $olderr, '>&', \*STDERR) or BAIL_OUT("Cannot save STDERR: $!");
    close(STDOUT) or BAIL_OUT("Cannot close STDOUT: $!");
    close(STDERR) or BAIL_OUT("Cannot close STDERR: $!");
    open(STDOUT, '>', \$output) or BAIL_OUT("Cannot redirect STDOUT: $!");
    open(STDERR, '>', \$error)  or BAIL_OUT("Cannot redirect STDERR: $!");

    # Run the backend.
    local @ARGV = @args;
    my $status = eval { $backend->run };
    if ($@) {
        print {*STDERR} $@ or BAIL_OUT("Cannot write to STDERR: $!");
        $status = 255;
    }

    # Restore STDOUT and STDERR.
    open(STDOUT, '>&', $oldout) or BAIL_OUT("Cannot restore STDOUT: $!");
    open(STDERR, '>&', $olderr) or BAIL_OUT("Cannot restore STDERR: $!");
    close($oldout) or BAIL_OUT("Cannot close duplicate STDOUT: $!");
    close($olderr) or BAIL_OUT("Cannot close duplicate STDERR: $!");

    # Return the results.
    return ($output, $error, $status);
}

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

# Set up test for simple command dispatch.
my %commands = (
    cmd1 => { code => \&test_cmd1 },
    cmd2 => { code => \&test_cmd2 },
);
my $backend = Net::Remctl::Backend->new({ commands => \%commands });
isa_ok($backend, 'Net::Remctl::Backend');

# Try running cmd1 and check the result.
my ($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2));
is($status, 1,   'cmd1 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Now try running cmd2 and check the result.
($out, $err, $status) = run_wrapper($backend, qw(cmd2 arg1 arg2));
is($status, 2,   'cmd2 returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd2 arg1 arg2)]], 'cmd2 called correctly');
@CALLS = ();

# Set minimum arguments for cmd1 and ensure it fails.
$commands{cmd1}{min_args} = 1;
$backend = Net::Remctl::Backend->new({ commands => \%commands });
($out, $err, $status) = run_wrapper($backend, qw(cmd1));
is($status, 255,                              'cmd1 with no args returns 255');
is($out,    q{},                              '... and no output');
is($err,    "cmd1: insufficient arguments\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# But this works fine if we pass in one argument.
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1));
is($status, 1,   'cmd1 with one arg returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1)]], 'cmd1 called correctly');
@CALLS = ();

# Set a maximum number of arguments as well.  Two arguments should be fine.
$commands{cmd1}{max_args} = 2;
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2));
is($status, 1,   'cmd1 with two args returns correct status');
is($out,    q{}, '... and no output');
is($err,    q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Three arguments should result in an error.
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2 arg3));
is($status, 255,                          'cmd1 with three args returns 255');
is($out,    q{},                          '... and no output');
is($err,    "cmd1: too many arguments\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# Set a global command name in the object, which should change the error.
$backend = Net::Remctl::Backend->new(
    {
        command  => 'foo',
        commands => \%commands,
    }
);
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2 arg3));
is($status, 255, 'cmd1 with three args returns 255');
is($out,    q{}, '... and no output');
is($err, "foo cmd1: too many arguments\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# Add help information to one of the commands.
$commands{cmd1}{syntax}  = 'arg1 [arg2]';
$commands{cmd1}{summary} = 'cmd1 all over the args';
$backend = Net::Remctl::Backend->new({ commands => \%commands });

# Call help.  We should get output only for cmd1.
($out, $err, $status) = run_wrapper($backend, qw(help));
is($status, 0, 'help returns status 0');
is(
    $out,
    "cmd1 arg1 [arg2]  cmd1 all over the args\n",
    '... and correct output'
);
is($err, q{}, '... and no errors');

# Add only syntax information to the second command.  We should get help
# output for both, but only a summary for the first command.
$commands{cmd2}{syntax} = q{};
my $expected = <<'END_HELP';
cmd1 arg1 [arg2]  cmd1 all over the args
cmd2
END_HELP
is($backend->help, $expected, 'Help output without summary is correct');

# Add long help information to the second command.
$commands{cmd2}{syntax} = q{};
$commands{cmd2}{summary}
  = 'does the thing and then the other thing unless the second thing is'
  . ' contradicted by the first thing in which case the third thing is done'
  . ' after the screaming stops';
$backend = Net::Remctl::Backend->new({ commands => \%commands });

# Test the help call directly this time.  We should get back the formatted
# help string.
$expected = <<'END_HELP';
cmd1 arg1 [arg2]  cmd1 all over the args
cmd2              does the thing and then the other thing unless the second
                  thing is contradicted by the first thing in which case the
                  third thing is done after the screaming stops
END_HELP
is($backend->help, $expected, 'Wrapped help output is correct');

# Now, add a help banner and set the command to get a more typical help
# configuration example.
$backend = Net::Remctl::Backend->new(
    {
        command     => 'foo',
        commands    => \%commands,
        help_banner => 'Foo manipulation remctl help:',
    }
);
$expected = <<'END_HELP';
Foo manipulation remctl help:
  foo cmd1 arg1 [arg2]  cmd1 all over the args
  foo cmd2              does the thing and then the other thing unless the
                        second thing is contradicted by the first thing in
                        which case the third thing is done after the screaming
                        stops
END_HELP
is($backend->help, $expected, 'Help output w/command and banner is correct');

# Add a newline to the end of the help banner and make sure we get the same
# thing, not a double newline.
$backend = Net::Remctl::Backend->new(
    {
        command     => 'foo',
        commands    => \%commands,
        help_banner => "Foo manipulation remctl help:\n",
    }
);
is($backend->help, $expected, 'Help output w/newline banner is correct');

# Actually run the help command and be sure we get the same thing.
($out, $err, $status) = run_wrapper($backend, qw(help));
is($status, 0,         'help with more configuration returns status 0');
is($out,    $expected, '... and correct output');
is($err,    q{},       '... and no errors');

# Run an unknown command and make sure we get the appropriate results.
local @ARGV = qw(unknown);
$status = eval { $backend->run };
is($status, undef, 'run() of unknown command returns undef');
is($@, "Unknown command unknown\n", '...with correct error');

# Close STDOUT and then try to run help.  Suppress warnings as well to get rid
# of the warning about printing to a closed file handle.
{
    local $SIG{__WARN__} = sub { };

    # Save standard output and close it.
    open(my $oldout, '>&', \*STDOUT) or BAIL_OUT("Cannot save STDOUT: $!");
    close(STDOUT) or BAIL_OUT("Cannot close STDOUT: $!");

    # Run the backend.
    local @ARGV = qw(help);
    $status = eval { $backend->run };

    # Restore stdout so that Test::More can use it.
    open(STDOUT, '>&', $oldout) or BAIL_OUT("Cannot restore STDOUT: $!");
    close($oldout) or BAIL_OUT("Cannot close duplicate STDOUT: $!");

    # Check whether the right thing happened.
    is($status, undef, 'run() of help with no STDOUT returns undef');
    like(
        $@,
        qr{ \A Cannot [ ] write [ ] to [ ] STDOUT: [ ] }xms,
        '...with correct error'
    );
}
