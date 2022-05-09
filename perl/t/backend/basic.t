#!/usr/bin/perl
#
# General tests for Net::Remctl::Backend.
#
# This test uses the PerlIO feature of opening a file descriptor to a scalar.
# This may not work with versions of Perl built without PerlIO, which was more
# common prior to 5.8.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2020 Russ Allbery <eagle@eyrie.org>
# Copyright 2012-2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT

use 5.010;
use strict;
use warnings;

use lib 't/lib';

use Test::More tests => 66;
use Test::Remctl qw(run_wrapper);

# Test loading the module.
BEGIN { use_ok('Net::Remctl::Backend') }

# Index of the sub name in the list output of caller.
use constant CALLER_SUB => 3;

# We'll use this variable to accumulate call sequences to test that the right
# functions were called by the backend wrapper.  Each element will be a
# reference to an array holding the called function and any arguments.
my @CALLS;

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
is($status, 1, 'cmd1 returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Now try running cmd2 and check the result.
($out, $err, $status) = run_wrapper($backend, qw(cmd2 arg1 arg2));
is($status, 2, 'cmd2 returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd2 arg1 arg2)]], 'cmd2 called correctly');
@CALLS = ();

# Set minimum arguments for cmd1 and ensure it fails.
$commands{cmd1}{args_min} = 1;
$backend = Net::Remctl::Backend->new({ commands => \%commands });
($out, $err, $status) = run_wrapper($backend, qw(cmd1));
is($status, 255, 'cmd1 with no args returns 255');
is($out, q{}, '... and no output');
is($err, "cmd1: insufficient arguments\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# But this works fine if we pass in one argument.
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1));
is($status, 1, 'cmd1 with one arg returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1)]], 'cmd1 called correctly');
@CALLS = ();

# Set a maximum number of arguments as well.  Two arguments should be fine.
$commands{cmd1}{args_max} = 2;
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2));
is($status, 1, 'cmd1 with two args returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Three arguments should result in an error.
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2 arg3));
is($status, 255, 'cmd1 with three args returns 255');
is($out, q{}, '... and no output');
is($err, "cmd1: too many arguments\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# Set a global command name in the object, which should change the error.
$backend = Net::Remctl::Backend->new(
    {
        command => 'foo',
        commands => \%commands,
    },
);
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2 arg3));
is($status, 255, 'cmd1 with three args returns 255');
is($out, q{}, '... and no output');
is($err, "foo cmd1: too many arguments\n", '... and correct verbose error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# Add an argument validator for the second argument of cmd1.  This call should
# be fine.
$commands{cmd1}{args_match} = [undef, qr{ \A \w+ \z }xms];
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg2));
is($status, 1, 'cmd1 with argument checking returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 arg2)]], 'cmd1 called correctly');
@CALLS = ();

# Now, pass in an argument that should fail.
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1 arg-2));
is($status, 255, 'cmd1 fails argument checking');
is($out, q{}, '... and no output');
is($err, "foo cmd1: invalid argument: arg-2\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# Change cmd1 to take the first argument on standard input and require two
# arguments to ensure that arguments passed via standard input count.
$commands{cmd1}{stdin} = 1;
$commands{cmd1}{args_min} = 2;
close(STDIN) or BAIL_OUT("Cannot close STDIN: $!");
my $stdin = "some\0data";
open(STDIN, '<', \$stdin) or BAIL_OUT("Cannot redirect STDIN: $!");
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg2));
is($status, 1, 'cmd1 with stdin arg returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(
    \@CALLS,
    [['main::test_cmd1', $stdin, 'arg2']],
    'cmd1 called correctly',
);
@CALLS = ();

# Take the last argument from standard input instead and confirm that argument
# checking still applies to standard input arguments.
seek(STDIN, 0, 0) or BAIL_OUT("Cannot rewind STDIN: $!");
$commands{cmd1}{stdin} = -1;
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1));
is($status, 255, 'cmd1 with final stdin arg fails argument checking');
is($out, q{}, '... and no output');
is($err, "foo cmd1: invalid data in argument #2\n", '... and correct error');
is_deeply(\@CALLS, [], 'no functions called');
@CALLS = ();

# Change the argument to something that would pass.
seek(STDIN, 0, 0) or BAIL_OUT("Cannot rewind STDIN: $!");
$stdin = 'stin';
($out, $err, $status) = run_wrapper($backend, qw(cmd1 arg1));
is($status, 1, 'cmd1 with final stdin arg returns correct status');
is($out, q{}, '... and no output');
is($err, q{}, '... and no errors');
is_deeply(\@CALLS, [[qw(main::test_cmd1 arg1 stin)]], 'cmd1 called correctly');
@CALLS = ();

# Add help information to one of the commands.
$commands{cmd1}{syntax} = 'arg1 [arg2]';
$commands{cmd1}{summary} = 'cmd1 all over the args';
$backend = Net::Remctl::Backend->new({ commands => \%commands });

# Call help.  We should get output only for cmd1.
($out, $err, $status) = run_wrapper($backend, qw(help));
is($status, 0, 'help returns status 0');
is(
    $out,
    "cmd1 arg1 [arg2]  cmd1 all over the args\n",
    '... and correct output',
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
#<<<
$backend = Net::Remctl::Backend->new(
    {
        command     => 'foo',
        commands    => \%commands,
        help_banner => 'Foo manipulation remctl help:',
    },
);
#>>>
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
#<<<
$backend = Net::Remctl::Backend->new(
    {
        command     => 'foo',
        commands    => \%commands,
        help_banner => "Foo manipulation remctl help:\n",
    },
);
#>>>
is($backend->help, $expected, 'Help output w/newline banner is correct');

# Actually run the help command and be sure we get the same thing.
($out, $err, $status) = run_wrapper($backend, qw(help));
is($status, 0, 'help with more configuration returns status 0');
is($out, $expected, '... and correct output');
is($err, q{}, '... and no errors');

# If one of the commands has an extremely long syntax, avoid scrunching the
# summaries against the right margin.
#<<<
$commands{cmd3} = {
    code    => \&test_cmd2,
    syntax  => '<lots of long options beyond 50 columns>',
    summary => 'actually does something very simple',
};
#>>>
$expected = <<'END_HELP';
Foo manipulation remctl help:
  foo cmd1 arg1 [arg2]  cmd1 all over the args
  foo cmd2              does the thing and then the other thing unless the
                        second thing is contradicted by the first thing in
                        which case the third thing is done after the screaming
                        stops
  foo cmd3 <lots of long options beyond 50 columns>
                        actually does something very simple
END_HELP
is($backend->help, $expected, 'Help output w/long syntax is correct');

# Test help summary with a single command with long syntax.
delete $commands{cmd1};
delete $commands{cmd2};
$expected = <<'END_HELP';
Foo manipulation remctl help:
  foo cmd3 <lots of long options beyond 50 columns>
                                        actually does something very simple
END_HELP
is($backend->help, $expected, 'Help output w/single long syntax is correct');

# Run an unknown command and make sure we get the appropriate results.
{
    local @ARGV = qw(unknown);
    $status = eval { $backend->run };
}
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
        '...with correct error',
    );
}
