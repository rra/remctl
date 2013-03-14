# Shared code for Net::Remctl tests.
#
# This is a small helper library intended only for use internally by the
# Net::Remctl test suite that contains functions useful in more than one
# test.
#
# This code uses the PerlIO feature of opening a file descriptor to a scalar.
# This may not work with versions of Perl built without PerlIO, which was more
# common prior to 5.8.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2012, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

package Test::Remctl;

use 5.006;
use strict;
use warnings;

use Exporter;

# For Perl 5.006 compatibility.
## no critic (ClassHierarchies::ProhibitExplicitISA)

# Declare variables that should be set in BEGIN for robustness.
our (@EXPORT_OK, @ISA, $VERSION);

# Set $VERSION and everything export-related in a BEGIN block for robustness
# against circular module loading (not that we load any modules, but
# consistency is good).
BEGIN {
    @ISA       = qw(Exporter);
    @EXPORT_OK = qw(run_wrapper);

    # This version is somewhat arbitrary and doesn't track the broader remctl
    # version.  I update it when code in this module changes.
    $VERSION = '1.00';
}

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
    my $status = eval { $backend->run(@args) };
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

1;
__END__

=for stopwords
Allbery ARG

=head1 NAME

Test::Remctl - Helper functions for Net::Remctl tests

=head1 SYNOPSIS

    use Test::Remctl qw(run_wrapper);

    # Some existing Net::Remctl::Backend object.
    my $backend;

    # Call run() while capturing stdout, stderr, and exceptions.
    my ($output, $error, $status) = run_wrapper($backend, 'foo');

=head1 DESCRIPTION

This module accumulates various helper functions for Net::Remctl tests.
It is not expected to be used outside of the Net::Remctl test suite.

=head1 FUNCTIONS

=over 4

=item run_wrapper(BACKEND, COMMAND[, ARG ...])

Calls Net::Remctl::Backend::run() but captures standard output, standard
error, and any exceptions.  Returns the standard output, the standard
error, and the return status as a list.  Any exceptions are caught and
the exception is added to standard error.  If run() exited with an
exception, the exit status will always be set to 255.

=back

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>

=cut
