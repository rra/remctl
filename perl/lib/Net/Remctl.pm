# Net::Remctl -- Perl bindings for the remctl client library.
#
# This is the Perl boostrap file for the Net::Remctl module, nearly all of
# which is implemented in XS.  For the actual source, see Remctl.xs.  This
# file contains the bootstrap and export code and the documentation.
#
# SPDX-License-Identifier: MIT

package Net::Remctl;

use 5.006;
use strict;
use warnings;

use Exporter;
use DynaLoader;

# Yes, I shouldn't have exported something by default, but I did, and now
# removing it would break backward compatibility.  So we live with it.
## no critic (Modules::ProhibitAutomaticExportation)
use vars qw($VERSION @EXPORT @ISA);

BEGIN {
    $VERSION = '3.16';
}

# use base qw(Exporter) requires Perl 5.8 and we still support Perl 5.6.
## no critic (ClassHierarchies::ProhibitExplicitISA)
BEGIN {
    @ISA    = qw(Exporter DynaLoader);
    @EXPORT = qw(remctl);
}

bootstrap Net::Remctl;
1;

=for stopwords
remctl libremctl GSS-API HOSTNAME remctld IPv4 IPv6 NUL CNAME DNS Allbery
MERCHANTABILITY sublicense NONINFRINGEMENT libdefaults CCACHE IP DNS-based
unencoded lookups canonicalization IANA-registered unannotated versioning

=head1 NAME

Net::Remctl - Perl bindings for remctl (Kerberos remote command execution)

=head1 SYNOPSIS

    # Simplified form.
    use Net::Remctl;
    my $result = remctl("hostname", undef, undef, "test", "echo", "Hi");
    if ($result->error) {
        die "test echo failed with error ", $result->error, "\n";
    } else {
        warn $result->stderr;
        print $result->stdout;
        exit $result->status;
    }

    # Full interface.
    use Net::Remctl ();
    my $remctl = Net::Remctl->new;
    $remctl->open("hostname")
        or die "Cannot connect to hostname: ", $remctl->error, "\n";
    $remctl->command("test", "echo", "Hi there")
        or die "Cannot send command: ", $remctl->error, "\n";
    my $output;
    do {
        $output = $remctl->output;
        if ($output->type eq 'output') {
            if ($output->stream == 1) {
                print $output->data;
            } elsif ($output->stream == 2) {
                warn $output->data;
            }
        } elsif ($output->type eq 'error') {
            warn $output->error, "\n";
        } elsif ($output->type eq 'status') {
            exit $output->status;
        } else {
            die "Unknown output token from library: ", $output->type, "\n";
        }
    } while ($output->type eq 'output');
    $remctl->noop or die "Cannot send NOOP: ", $remctl->error, "\n";

=head1 DESCRIPTION

Net::Remctl provides Perl bindings to the libremctl client library.  remctl
is a protocol for remote command execution using GSS-API authentication.
The specific allowable commands must be listed in a configuration file on
the remote system and the remote system can map the remctl command names to
any local command without exposing that mapping to the client.  This module
implements a remctl client.

=head2 Simplified Interface

If you want to run a single command on a remote system and get back the
output and exit status, you can use the exported remctl() function:

=over 4

=item remctl(HOSTNAME, PORT, PRINCIPAL, COMMAND, [ARGS, ...])

Runs a command on the remote system and returns a Net::Remctl::Result
object (see below).  HOSTNAME is the remote host to contact.  PORT is the
port of the remote B<remctld> server and may be 0 to tell the library to
use the default (first try 4373, the registered remctl port, and fall back
to the legacy 4444 port if that fails).  PRINCIPAL is the principal of the
server to use for authentication; pass in the empty string to use the
default of host/HOSTNAME, with the realm determined by domain-realm
mapping.  The remaining arguments are the remctl command and arguments
passed to the remote server.

As far as the module is concerned, undef may be passed as PORT and
PRINCIPAL and is the same as 0 and the empty string respectively.
However, Perl will warn about passing undef explicitly as a function
argument.

The return value is a Net::Remctl::Result object which supports the
following methods:

=over 4

=item error()

Returns the error message from either the remote host or from the local
client library (if, for instance, contacting the remote host failed).
Returns undef if there was no error.  Checking whether error() returns undef
is the supported way of determining whether the remctl() call succeeded.

=item stdout()

Returns the command's standard output or undef if there was none.

=item stderr()

Returns the command's standard error or undef if there was none.

=item status()

Returns the command's exit status.

=back

Each call to remctl() will open a new connection to the remote host and
close it after retrieving the results of the command.  To maintain a
persistent connection, use the full interface described below.

=back

=head2 Full Interface

The full remctl library interface requires that the user do more
bookkeeping, but it provides more flexibility and allows one to issue
multiple commands on the same persistent connection (provided that the
remote server supports protocol version two; if not, the library will
transparently fall back to opening a new connection for each command).

To use the full interface, first create a Net::Remctl object with new()
and then connect() to a remote server.  Then, issue a command() and call
output() to retrieve output tokens (as Net::Remctl::Output objects) until
a status token is received.  Destroying the Net::Remctl object will close
the connection.

Methods below are annotated with the version at which they were added.
Note that this is the version of the remctl distribution, which only
matches the Net::Remctl module version in 3.05 and later.  Earlier
versions of the module used an independent versioning system that is not
documented here, and versions earlier than 3.05 are therefore not suitable
for use with C<use>.

The supported object methods are:

=over 4

=item new()

[2.8] Create a new Net::Remctl object.  This doesn't attempt to connect to
a host and hence will only fail (by throwing an exception) if the library
cannot allocate memory.

=item error()

[2.8] Retrieves the error message from the last failing operation and
returns it as a string.

=item set_ccache(CCACHE)

[3.00] Sets the GSS-API credential cache for outgoing connections to
CCACHE, which is normally the path to a Kerberos ticket cache but may have
other valid forms depending on the underlying Kerberos implementation in
use by GSS-API.  This method will affect all subsequent open() calls on at
least the same object, but will have no effect on connections that are
already open.  Returns true on success and false on failure.

If the remctl client library was built against a Kerberos library and the
GSS-API library supported gss_krb5_import_cred, this call affects only
this Net::Remctl object.  Otherwise, this will affect not only all
subsequent open() calls for the same object, but all subsequent remctl
connections of any kind from the same process, and even other GSS-API
connections from the same process unrelated to remctl.

Not all GSS-API implementations support setting the credential cache.  If
this is not supported, false will be returned.

=item set_source_ip(SOURCE)

[3.00] Sets the source IP for outgoing connections to SOURCE, which can be
either an IPv4 or an IPv6 address (if IPv6 is supported).  It must be an
IP address, not a host name.  This will affect all subsequent open() calls
on the same object, but will have no effect on connections that are
already open.  Returns true on success and false on failure.

=item set_timeout(TIMEOUT)

[3.01] Sets the timeout for connections and commands to TIMEOUT, which
should be an integer number of seconds.  TIMEOUT may be 0 to clear a
timeout that was previously set.  All subsequent operations on this
Net::Remctl object will be subject to this timeout, including open() if
called prior to calling open().  Returns true on success and false on
failure.  Failure is only possible if TIMEOUT is malformed.

The timeout is a timeout on network activity from the server, not on a
complete operation.  So, for example, a timeout of ten seconds just
requires that the server send some data every ten seconds.  If the server
sends only tiny amounts of data at a time, the complete operation could
take much longer than ten seconds without triggering the timeout.

=item open(HOSTNAME[, PORT[, PRINCIPAL]])

[2.8] Connect to HOSTNAME on port PORT using PRINCIPAL as the remote
server's principal for authentication.  If PORT is omitted or 0, use the
default (first try 4373, the registered remctl port, and fall back to the
legacy 4444 port if that fails).  If PRINCIPAL is omitted or the empty
string, use the default of host/HOSTNAME, with the realm determined by
domain-realm mapping.  Returns true on success, false on failure.  On
failure, call error() to get the failure message.

As far as the module is concerned, undef may be passed as PORT and
PRINCIPAL and is the same as 0 and the empty string respectively.
However, Perl will warn about passing undef explicitly as a function
argument.

=item command(COMMAND[, ARGS, ...])

[2.8] Send the command and arguments to the remote host.  The command and
the arguments may, under the remctl protocol, contain any character, but
be aware that most remctl servers will reject commands or arguments
containing ASCII 0 (NUL), so currently this cannot be used for upload of
arbitrary unencoded binary data.  Returns true on success (meaning success
in sending the command, and implying nothing about the result of the
command), false on failure.  On failure, call error() to get the failure
message.

=item output()

[2.8] Returns the next output token from the remote host.  The token is
returned as a Net::Remctl::Output object, which supports the following
methods:

=over 4

=item type()

Returns the type of the output token, which will be one of C<output>,
C<error>, C<status>, or C<done>.  A command will result in either one
C<error> token or zero or more C<output> tokens followed by a C<status>
token.  After either a C<error> or C<status> token is seen, another command
can be issued.  If the caller tries to retrieve another output token when it
has already consumed all of them for that command, the library will return a
C<done> token.

=item data()

Returns the contents of the token.  This method only makes sense for
C<output> and C<error> tokens; otherwise, it will return undef.  Note that
the returned value may contain any character, including ASCII 0 (NUL).

=item length()

Returns the length of the data in the token.  As with data(), this method
only makes sense for the C<output> and C<error> tokens.  It will return 0 if
there is no data or if the data is zero-length.

=item stream()

For an C<output> token, returns the stream with which the data is
associated.  Currently, only two stream values will be used: 1, meaning
standard output; and 2, meaning standard error.  The value is undefined for
all other output token types.

=item status()

For a C<status> token, returns the exit status of the remote command.  The
value is undefined for all other token types.

=item error()

For an C<error> token, returns the remctl error code for the protocol
error.  The text message will be returned by data().  The value is undefined
for all other token types.

=back

=item noop()

[3.00] Send a NOOP message to the server and read the reply.  This is
primarily used to keep a connection to a remctl server alive, such as
through a firewall with a session timeout, while waiting to issue further
commands.  Returns true on success, false on failure.  On failure, call
error() to get the failure message.

The NOOP message requires protocol version 3 support in the server, so the
caller should be prepared for this function to fail, indicating that the
connection could not be kept alive and possibly that it was closed by the
server.  In this case, the client will need to explicitly reopen the
connection with open().

=back

Note that, due to internal implementation details in the library, the
Net::Remctl::Output object returned by output() will be invalidated by the
next call to command() or output() or by destroying the producing
Net::Remctl object.  Therefore, any data in the output token should be
processed and stored if needed before making any further Net::Remctl method
calls on the same object.

=head1 COMPATIBILITY

The main object methods are annotated above with the version in which that
interface was added.  All unannotated methods have been present since the
first release of the module.

Support for the gss_krb5_import_cred method of isolating the changed
ticket cache to only this remctl client object was added in version 3.5.

The default port was changed to the IANA-registered port of 4373 in
version 2.11.

This module was first released with version 2.8.

=head1 CAVEATS

If the I<principal> argument to remctl() or remctl_open() is NULL, most
GSS-API libraries will canonicalize the I<host> using DNS before deriving
the principal name from it.  This means that when connecting to a remctl
server via a CNAME, remctl() and remctl_open() will normally authenticate
using a principal based on the canonical name of the host instead of the
specified I<host> parameter.  This behavior may cause problems if two
consecutive DNS lookups of I<host> may return two different results, such
as with some DNS-based load-balancing systems.

The canonicalization behavior is controlled by the GSS-API library; with
the MIT Kerberos GSS-API library, canonicalization can be disabled by
setting C<rdns> to false in the [libdefaults] section of F<krb5.conf>.  It
can also be disabled by passing an explicit Kerberos principal name via
the I<principal> argument, which will then be used without changes.  If
canonicalization is desired, the caller may wish to canonicalize I<host>
before calling remctl() or remctl_open() to avoid problems with multiple
DNS calls returning different results.

The default behavior, when the port is not specified, of trying 4373 and
falling back to 4444 will be removed in a future version of this module in
favor of using the C<remctl> service in F</etc/services> if set and then
falling back on only 4373.  4444 was the poorly-chosen original remctl
port and should be phased out.

=head1 NOTES

The remctl port number, 4373, was derived by tracing the diagonals of a
QWERTY keyboard up from the letters C<remc> to the number row.

=head1 AUTHOR

Russ Allbery <eagle@eyrie.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2007-2008, 2011-2014 The Board of Trustees of the Leland Stanford
Junior University

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

=head1 SEE ALSO

remctl(1), remctld(8)

The current version of this module is available from its web page at
L<https://www.eyrie.org/~eagle/software/remctl/>.

=cut

# Local Variables:
# copyright-at-end-flag: t
# End:
