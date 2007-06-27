#!/usr/bin/perl -w
# $Id$
#
# t/api.t -- Tests for the Net::Remctl API.

BEGIN { our $total = 5 }
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
    open (PRINC, 'data/test.principal')
        or die "cannot open data/test.principal: $!\n";
    my $princ = <PRINC>;
    close PRINC;
    chomp $princ;
    return $princ;
}

# Do the bizarre dance to start a test version of remctld.
sub start_remctld {
    unlink ('data/pid');
    $ENV{KRB5_KTNAME} = 'data/test.keytab';
    my $princ = get_principal;
    my $pid = fork;
    if (not defined $pid) {
        die "cannot fork: $!\n";
    } elsif ($pid == 0) {
        exec ('../server/remctld', '-m', '-p', '14444', '-s', $princ,
              '-P', 'data/pid', '-f', 'data/conf-simple', '-d', '-S')
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
    my $princ = get_principal;
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
start_remctld;
my $okay = run_kinit;
SKIP: {
    skip "no Kerberos configuration", $total unless $okay;

    sleep 1 unless -f 'data/pid';
    die "remctld did not start" unless -f 'data/pid';

    # Now we can finally run our tests.
    my $principal = get_principal;
    my $result = remctl ('localhost', 14444, $principal, 'test', 'test');
    ok (defined $result, 'Basic remctl returns result');
    is ($result->status, 0, '  ... exit status');
    is ($result->stdout, "hello world\n", '  ... stdout output');
    is ($result->stderr, undef, '  ... stderr output');
    is ($result->error, undef, '  ... error return');
}

END {
    stop_remctld;
}
