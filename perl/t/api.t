#!/usr/bin/perl -w
# $Id$
#
# t/api.t -- Tests for the Net::Remctl API.

BEGIN { our $total = 30 }
use Test::More tests => $total;

use Net::Remctl;

# Find and change into the right directory for running tests.
sub change_directory {
    if (-f "tests/data/conf-simple") {
        chdir 'tests' or die "cannot cd to tests: $!\n";
    } elsif (-f "../tests/data/conf-simple") {
        chdir '../tests' or die "cannot cd to ../tests: $!\n";
    } elsif (-f "../../tests/data/conf-simple") {
        chdir '../../tests' or die "cannot cd to ../../tests: $!\n";
    }
    unless (-f "data/conf-simple") {
        die "cannot find test data directory\n";
    }
}

# Returns the principal to use for authentication.
sub get_principal {
    open (PRINC, 'data/test.principal') or return;
    my $princ = <PRINC>;
    close PRINC;
    chomp $princ;
    return $princ;
}

# Do the bizarre dance to start a test version of remctld.
sub start_remctld {
    unlink ('data/pid');
    my $princ = get_principal;
    my $pid = fork;
    if (not defined $pid) {
        die "cannot fork: $!\n";
    } elsif ($pid == 0) {
        exec ('../server/remctld', '-m', '-p', '14444',
              (defined ($princ) ? ('-s', $princ) : ()),
              '-P', 'data/pid', '-f', 'data/conf-simple', '-d', '-S', '-F',
              '-k', 'data/test.keytab')
            or die "cannot exec ../server/remctld: $!\n";
    }
}

# Stop the running test remctld.
sub stop_remctld {
    if (open (PID, 'data/pid')) {
        my $pid = <PID>;
        chomp $pid;
        kill (1, $pid);
        unlink ('data/pid');
    }
}

# Obtain tickets, which requires iterating through several different possible
# ways of running kinit.
sub run_kinit {
    $ENV{KRB5CCNAME} = 'data/test.cache';
    my $princ = get_principal;
    return unless $princ;
    my @commands = ([ qw(kinit -k -t data/test.keytab), $princ ],
                    [ qw(kinit -t data/test.keytab), $princ ],
                    [ qw(kinit -k -K data/test.keytab), $princ ]);
    my $status;
    for (@commands) {
        $status = system "@$_ > /dev/null < /dev/null";
        if ($status == 0) {
            return 1;
        }
    }
    warn "Unable to obtain Kerberos tickets\n";
    unless (-f 'data/pid') {
        sleep 1;
    }
    stop_remctld;
    return;
}

# Test setup.
change_directory;
my $okay = (-f 'data/test.principal' && -f 'data/test.keytab');
if ($okay) {
    start_remctld;
    $okay = run_kinit;
}
SKIP: {
    skip "no Kerberos configuration", $total unless $okay;

    sleep 1 unless -f 'data/pid';
    die "remctld did not start" unless -f 'data/pid';

    # Now we can finally run our tests.  Basic interface, success.
    my $principal = get_principal;
    my $result = remctl ('localhost', 14444, $principal, 'test', 'test');
    isa_ok ($result, 'Net::Remctl::Result', 'Basic remctl return');
    is ($result->status, 0, '... exit status');
    is ($result->stdout, "hello world\n", '... stdout output');
    is ($result->stderr, undef, '... stderr output');
    is ($result->error, undef, '... error return');

    # Basic interface, failure.
    $result = remctl ('localhost', 14444, $principal, 'test', 'bad-command');
    isa_ok ($result, 'Net::Remctl::Result', 'Error remctl return');
    is ($result->status, 0, '... exit status');
    is ($result->stdout, undef, '... stdout output');
    is ($result->stderr, undef, '... stderr output');
    is ($result->error, 'Unknown command', '... error return');

    # Complex interface, success.
    my $remctl = Net::Remctl->new;
    isa_ok ($remctl, 'Net::Remctl', 'Object');
    is ($remctl->error, 'No error', '... no error set');
    ok ($remctl->open ('localhost', 14444, $principal), 'Connect to server');
    is ($remctl->error, 'No error', '... no error set');
    ok ($remctl->command ('test', 'test'), 'Send successful command');
    is ($remctl->error, 'No error', '... no error set');
    my $output = $remctl->output;
    isa_ok ($output, 'Net::Remctl::Output', 'Output token');
    is ($output->type, 'output', '... of type output');
    is ($output->length, 12, '... and length 12');
    is ($output->data, "hello world\n", '... with the right data');
    is ($output->stream, 1, '... and the right stream');
    $output = $remctl->output;
    isa_ok ($output, 'Net::Remctl::Output', 'Second output token');
    is ($output->type, 'status', '... of type status');
    is ($output->status, 0, '... with status 0');

    # Complex interface, failure.
    ok ($remctl->command ('test', 'bad-command'), 'Send failing command');
    is ($remctl->error, 'No error', '... no error set');
    $output = $remctl->output;
    isa_ok ($output, 'Net::Remctl::Output', 'Output token');
    is ($output->type, 'error', '... of type error');
    is ($output->data, 'Unknown command', '... with the error message');
    is ($output->error, 5, '... and the right code');
}

END {
    stop_remctld;
}
