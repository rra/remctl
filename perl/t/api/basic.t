#!/usr/bin/perl
#
# Tests for the Net::Remctl API.  This relies on being run as part of the
# larger remctl build tree and uses the built remctld for testing.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2007, 2008, 2009, 2011, 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

use strict;
use warnings;

use Carp qw(croak);
use Test::More tests => 62;

# Load the module.
BEGIN { use_ok('Net::Remctl') }

# Find a configuration file included as data in the test suite.
#
# $file - Path of file relative to the t/config directory
#
# Returns: Path of the file if found
#  Throws: String exception if the file could not be found.
sub test_file_path {
    my ($file) = @_;
    for my $base (qw(t tests .)) {
        if (-f "$base/$file") {
            return "$base/$file";
        }
    }
    croak("cannot find test file $file");
}

# Returns: The principal to use for authentication.
sub get_principal {
    my $file = test_file_path('config/principal');
    open(my $config, '<', $file) or return;
    my $princ = <$config>;
    close($config) or return;
    chomp($princ);
    return $princ;
}

# Do the bizarre dance to start a test version of remctld.  We use the version
# of remctld that we just built in the enclosing source tree if available.
# Otherwise, we fall back on the system remctld.
#
# Returns: undef
#  Throws: Text exception if remctld cannot be started.
sub start_remctld {
    my $princ = get_principal();

    # If REMCTLD is set in the environment, use that as the binary.
    my $remctld = $ENV{REMCTLD} || 'remctld';

    # In case REMCTLD was not set, add sbin directories to our PATH.
    local $ENV{PATH} = "/usr/local/sbin:/usr/sbin:$ENV{PATH}";

    # Set up a temporary directory for the PID file.
    unlink 't/tmp/pid';
    mkdir 't/tmp';

    # Fork off remctld.
    my $pid = fork;
    if (!defined $pid) {
        die "cannot fork: $!\n";
    } elsif ($pid == 0) {
        close STDERR or warn "can't close STDERR: $!\n";
        exec $remctld, '-m', '-p', '14373',
          (defined($princ) ? ('-s', $princ) : ()),
          '-P', 't/tmp/pid', '-f', 't/data/remctl.conf',
          '-d', '-S', '-F', '-k', 't/config/keytab'
          or die "cannot exec $remctld: $!\n";
    }
}

# Stop the running test remctld and remove the files it left behind.
#
# Returns: undef
sub stop_remctld {
    if (open my $pid_fh, '<', 't/tmp/pid') {
        my $pid = <$pid_fh>;
        close $pid_fh or warn "cannot close PID file: $!\n";
        chomp $pid;
        kill 15, $pid;
        unlink 't/tmp/pid';
        rmdir 't/tmp';
    }
    return;
}

# Obtain tickets, which requires iterating through several different possible
# ways of running kinit.  Warns if unable to get tickets.
#
# Returns: true on success, false on failure
sub run_kinit {
    my $princ = get_principal();
    if (!defined $princ) {
        return;
    }

    # List of commands to try.
    my @commands = (
        [qw(kinit --no-afslog -k -t t/config/keytab), $princ],
        [qw(kinit -k -t t/config/keytab),             $princ],
        [qw(kinit -t t/config/keytab),                $princ],
        [qw(kinit -k -K t/config/keytab),             $princ],
    );

    # Attempt each command in turn.  If any succeed, return true.
    for my $command (@commands) {
        my $status = system "@$command >/dev/null 2>&1 </dev/null";
        if ($status == 0) {
            return 1;
        }
    }

    # If unable to get Kerberos tickets, wait until remctld has started and
    # then stop it.  Then return false.
    warn "Unable to obtain Kerberos tickets\n";
    if (!-f 't/tmp/pid') {
        sleep 1;
    }
    stop_remctld();
    return;
}

# Test setup.
my $okay = (-f 't/config/principal' && -f 't/config/keytab');
local $ENV{KRB5CCNAME} = 't/tmp/krb5cc_test';
if ($okay) {
    start_remctld();
    $okay = run_kinit();
}
SKIP: {
    if (!$okay) {
        skip 'no Kerberos configuration' => 61;
    }

    # Wait for remctld to start.
    if (!-f 't/tmp/pid') {
        sleep 1;
        if (!-f 't/tmp/pid') {
            die "remctld did not start\n";
        }
    }

    # Now we can finally run our tests.  Basic interface, success.
    my $principal = get_principal();
    my $result = remctl('localhost', 14373, $principal, 'test', 'test');
    isa_ok($result, 'Net::Remctl::Result', 'Basic remctl return');
    is($result->status, 0,               '... exit status');
    is($result->stdout, "hello world\n", '... stdout output');
    is($result->stderr, undef,           '... stderr output');
    is($result->error,  undef,           '... error return');

    # The same, but passing the arguments in via an array.
    my @args = ('localhost', 14373, $principal, 'test', 'test');
    $result = remctl(@args);
    isa_ok($result, 'Net::Remctl::Result', 'Basic remctl with array args');
    is($result->status, 0,               '... exit status');
    is($result->stdout, "hello world\n", '... stdout output');
    is($result->stderr, undef,           '... stderr output');
    is($result->error,  undef,           '... error return');

    # Basic interface, failure.
    $result = remctl('localhost', 14373, $principal, 'test', 'bad-command');
    isa_ok($result, 'Net::Remctl::Result', 'Error remctl return');
    is($result->status, 0,                 '... exit status');
    is($result->stdout, undef,             '... stdout output');
    is($result->stderr, undef,             '... stderr output');
    is($result->error,  'Unknown command', '... error return');

    # Test not providing enough arguments to remctl.
    if (eval { $result = remctl() }) {
        ok(0, 'Insufficient arguments to remctl');
    } else {
        ## no critic (RegularExpressions::ProhibitEscapedMetacharacters)
        like(
            $@,
            qr{\A Usage: [ ] Net::Remctl::remctl\(.*}xms,
            'Insufficient arguments to remctl'
        );
    }

    # Complex interface, success.
    my $remctl = Net::Remctl->new;
    isa_ok($remctl, 'Net::Remctl', 'Object');
    is($remctl->error, 'no error', '... no error set');
    ok($remctl->open('localhost', 14373, $principal), 'Connect to server');
    is($remctl->error, 'no error', '... no error set');
    ok($remctl->noop, 'Send NOOP message');
    is($remctl->error, 'no error', '... no error set');
    ok($remctl->command('test', 'test'), 'Send successful command');
    is($remctl->error, 'no error', '... no error set');

    # First output token.
    my $output = $remctl->output;
    isa_ok($output, 'Net::Remctl::Output', 'Output token');
    is($output->type,   'output',        '... of type output');
    is($output->length, 12,              '... and length 12');
    is($output->data,   "hello world\n", '... with the right data');
    is($output->stream, 1,               '... and the right stream');

    # Second output token.
    $output = $remctl->output;
    isa_ok($output, 'Net::Remctl::Output', 'Second output token');
    is($output->type,   'status', '... of type status');
    is($output->status, 0,        '... with status 0');

    # Complex interface, failure.
    ok($remctl->command('test', 'bad-command'), 'Send failing command');
    is($remctl->error, 'no error', '... no error set');
    $output = $remctl->output;
    isa_ok($output, 'Net::Remctl::Output', 'Output token');
    is($output->type,  'error',           '... of type error');
    is($output->data,  'Unknown command', '... with the error message');
    is($output->error, 5,                 '... and the right code');

    # Complex interface with a timeout.
    $remctl = Net::Remctl->new;
    isa_ok($remctl, 'Net::Remctl', 'Object');
    is($remctl->error,          'no error', '... no error set');
    is($remctl->set_timeout(1), 1,          'Set timeout');
    is($remctl->error,          'no error', '... no error set');
    ok($remctl->open('127.0.0.1', 14373, $principal), 'Connect to server');
    is($remctl->error, 'no error', '... no error set');
    ok($remctl->command('test', 'sleep'), 'Send sleep command');
    is($remctl->error, 'no error', '... no error set');
    $output = $remctl->output;
    is($output, undef, 'Output token is undefined');
    is($remctl->error, 'error receiving token: timed out', '... with error');

    # Complex interface with source IP.
    $remctl = Net::Remctl->new;
    isa_ok($remctl, 'Net::Remctl', 'Object');
    is($remctl->error,                      'no error', '... no error set');
    is($remctl->set_source_ip('127.0.0.1'), 1,          'Set source IP');
    is($remctl->error,                      'no error', '... no error set');
    ok($remctl->open('127.0.0.1', 14373, $principal), 'Connect to server');
    is($remctl->error, 'no error', '... no error set');

    # Complex interface with unworkable source IP.
    is($remctl->set_source_ip('::1'), 1,          'Set source IP to ::1');
    is($remctl->error,                'no error', '... no error set');
    ok(!$remctl->open('127.0.0.1', 14373, $principal),
        'Cannot connect to server');
    like(
        $remctl->error,
        qr{ \A (?:cannot[ ] connect [ ] to | unknown [ ] host) [ ] }xms,
        '... with correct error'
    );

    # Get the current ticket cache location and then change KRB5CCNAME.
    # Connecting should then fail since we have no ticket cache.
    my $cache = $ENV{KRB5CCNAME};
    local $ENV{KRB5CCNAME} = './nonexistent-file';
    $remctl = Net::Remctl->new;
    isa_ok($remctl, 'Net::Remctl', 'Object');
    ok(!$remctl->open('localhost', 14373, $principal),
        'Cannot connect without cache');

    # Set the ticket cache, and then open should work.
  SKIP: {
        if (!$remctl->set_ccache($cache)) {
            skip 'gss_krb5_set_ccache not supported' => 1;
        }
        ok($remctl->open('localhost', 14373, $principal), 'Connect now works');
    }

    # Remove our ticket cache; we're done.
    unlink $cache;
}

END {
    stop_remctld();
}
