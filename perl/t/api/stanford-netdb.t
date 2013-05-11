#!/usr/bin/perl
#
# Test default parameters against Stanford's NetDB service.
#
# This test can only be run by someone local to Stanford with appropriate
# access to the NetDB role server and will be skipped in all other
# environments.  We need to use a known service running on the standard ports
# in order to test undefined values passed to Net::Remctl functions.
#
# Written by Russ Allbery
# Copyright 2008, 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use IO::Handle;
use IPC::Open3 qw(open3);
use Test::More;

# Regular expression to match klist output for a stanford.edu principal.
# Copes with both MIT Kerberos klist and Heimdal klist.
my $KLIST_REGEX = qr{
    \n \s*
    (Default [ ])? [Pp]rincipal: [ ]
    \S+\@stanford[.]edu
    \s* \n
}ixms;

# Skip tests unless we're running the test suite in maintainer mode.
if (!$ENV{RRA_MAINTAINER_TESTS}) {
    plan skip_all => 'Stanford-specific test only run for maintainer';
}

# Skip tests unless we have a stanford.edu realm ticket.
my $klist_out = IO::Handle->new;
my $klist_err = IO::Handle->new;
my $klist;
my $pid = open3(\*STDIN, $klist_out, $klist_err, 'klist');
{
    local $/ = undef;
    $klist = <$klist_out>;
}
waitpid $pid, 0;
close $klist_out or warn "cannot close klist output: $!\n";
close $klist_err or warn "cannot close klist errors: $!\n";
if ($klist !~ $KLIST_REGEX) {
    plan skip_all => 'stanford.edu Kerberos tickets required';
}

# Okay, we can proceed.
plan tests => 7;

# The server to query, the node to ask about, and the user on that node.
my $NETDB = 'netdb-node-roles-rc.stanford.edu';
my $HOST  = 'windlord.stanford.edu';
my $USER  = 'rra';

# Load the module.
require_ok('Net::Remctl');

# Create a remctl object.
my $remctl = Net::Remctl->new();
isa_ok($remctl, 'Net::Remctl', 'Object creation');

# We want to test behavior in the presence of explicitly undefined values,
# so suppress the warnings.
## no critic (TestingAndDebugging::ProhibitNoWarnings)
{
    no warnings 'uninitialized';
    ok($remctl->open($NETDB, undef, undef), 'open with explicit undef');
    undef $remctl;
    $remctl = Net::Remctl->new;
    my $port      = undef;
    my $principal = undef;
    ok($remctl->open($NETDB, $port, $principal), 'open with implicit undef');
}

# Test sending the command.
ok($remctl->command('netdb', 'node-roles', $USER, $HOST), 'Sending command');
my $roles;
my $okay = 1;
while (defined(my $output = $remctl->output)) {
    if ($output->type eq 'output') {
        if ($output->stream == 1) {
            $roles .= $output->data;
        } elsif ($output->stream == 2) {
            print {*STDERR} $output->data
              or die "Cannot display standard error\n";
            $okay = 0;
        }
    } elsif ($output->type eq 'error') {
        warn $output->error, "\n";
        $okay = 0;
        last;
    } elsif ($output->type eq 'status') {
        if ($output->status != 0) {
            $okay = 0;
        }
        last;
    } else {
        die 'Unknown output token from library: ', $output->type, "\n";
    }
}
ok($okay, 'Reading output');
is($roles, "admin\nuser\n", 'Saw correct output');
