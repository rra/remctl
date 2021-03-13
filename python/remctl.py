# Python interface to remctl.
#
# This is the high-level interface that most Python programs that use remctl
# should be using.  It's a Python wrapper around the _remctl C module, which
# exposes exactly the libremctl API.
#
# Original implementation by Thomas L. Kula <kula@tproa.net>
# Copyright 2014, 2019 Russ Allbery <eagle@eyrie.org>
# Copyright 2008, 2011-2012
#     The Board of Trustees of the Leland Stanford Junior University
# Copyright 2008 Thomas L. Kula <kula@tproa.net>
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided
# that the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation, and that the name of Thomas L. Kula not be used in
# advertising or publicity pertaining to distribution of the software without
# specific, written prior permission. Thomas L. Kula makes no representations
# about the suitability of this software for any purpose.  It is provided "as
# is" without express or implied warranty.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# There is no SPDX-License-Identifier registered for this license.

"""Interface to remctl.

   This module is an interface to remctl, a client/server
   protocol for running single commands on a remote host
   using Kerberos v5 authentication.
"""

from typing import Iterable, Optional, Text, Tuple, Union

import _remctl

VERSION = "3.17"

RemctlOutput = Tuple[
    str, Optional[bytes], Optional[int], Optional[int], Optional[int]
]

# Exception classes.


class RemctlError(Exception):
    """The underlying remctl library has returned an error."""

    pass


class RemctlProtocolError(RemctlError):
    """A remctl protocol error occurred.

    This exception is only used with the remctl.remctl() simple interface;
    for the full interface, errors are returned as a regular output token.
    """

    pass


class RemctlNotOpenedError(RemctlError):
    """No open connection to a server."""

    pass


# Simple interface.


class RemctlSimpleResult:
    """An object holding the results from the simple interface."""

    def __init__(self):
        # type: () -> None
        self.stdout = None  # type: Optional[bytes]
        self.stderr = None  # type: Optional[bytes]
        self.status = None  # type: Optional[int]


def remctl(
    host,  # type: str
    port=None,  # type: Optional[Union[int, str]]
    principal=None,  # type: Optional[str]
    command=[],  # type: Iterable[Union[Text, bytes]]
):
    # type: (...) -> RemctlSimpleResult
    """Simple interface to remctl.

    Connect to HOST on PORT, using PRINCIPAL as the server principal for
    authentication, and issue COMMAND.  Returns the result as a
    RemctlSimpleResult object, which has three attributes.  stdout holds the
    complete standard output, stderr holds the complete standard error, and
    status holds the exit status.
    """
    if port is None:
        port = 0
    else:
        try:
            port = int(port)
        except ValueError:
            raise TypeError("port must be a number: " + repr(port))
    if (port < 0) or (port > 65535):
        raise ValueError("invalid port number: " + repr(port))
    if isinstance(command, (bytes, str, bool, int, float)):
        raise TypeError("command must be a sequence or iterator")

    # Convert the command to a list of bytes.
    mycommand = [i if isinstance(i, bytes) else i.encode() for i in command]
    if len(mycommand) < 1:
        raise ValueError("command must not be empty")

    # At this point, things should be sane.  Call the low-level interface.
    output = _remctl.remctl(host, port, principal, mycommand)
    if output[0] is not None:
        raise RemctlProtocolError(output[0])
    result = RemctlSimpleResult()
    setattr(result, "stdout", output[1])
    setattr(result, "stderr", output[2])
    setattr(result, "status", output[3])
    return result


# Complex interface.


class Remctl:
    def __init__(
        self,
        host=None,  # type: Optional[str]
        port=None,  # type: Optional[Union[int, str]]
        principal=None,  # type: Optional[str]
    ):
        # type: (...) -> None
        self.r = _remctl.remctl_new()
        self.opened = False

        if host:
            self.open(host, port, principal)

    def set_ccache(self, ccache):
        # type: (str) -> None
        if not _remctl.remctl_set_ccache(self.r, ccache):
            raise RemctlError(self.error())

    def set_source_ip(self, source):
        # type: (str) -> None
        if not _remctl.remctl_set_source_ip(self.r, source):
            raise RemctlError(self.error())

    def set_timeout(self, timeout):
        # type: (int) -> None
        if not _remctl.remctl_set_timeout(self.r, timeout):
            raise RemctlError(self.error())

    def open(self, host, port=None, principal=None):
        # type: (str, Optional[Union[int, str]], Optional[str]) -> None
        if port is None:
            port = 0
        else:
            try:
                port = int(port)
            except ValueError:
                raise TypeError("port must be a number: " + repr(port))
        if (port < 0) or (port > 65535):
            raise ValueError("invalid port number: " + repr(port))

        # At this point, things should be sane.  Call the low-level interface.
        if not _remctl.remctl_open(self.r, host, port, principal):
            raise RemctlError(self.error())
        self.opened = True

    def command(self, comm):
        # type: (Iterable[Union[Text, bytes]]) -> None
        if not self.opened:
            raise RemctlNotOpenedError("no currently open connection")
        if isinstance(comm, (bytes, str, bool, int, float)):
            raise TypeError("command must be a sequence or iterator")

        # Convert the command to a list of strings.
        commlist = [i if isinstance(i, bytes) else i.encode() for i in comm]
        if len(commlist) < 1:
            raise ValueError("command must not be empty")

        # At this point, things should be sane.  Call the low-level interface.
        if not _remctl.remctl_commandv(self.r, commlist):
            raise RemctlError(self.error())

    def output(self):
        # type: () -> RemctlOutput
        if not self.opened:
            raise RemctlNotOpenedError("no currently open connection")
        output = _remctl.remctl_output(self.r)
        if len(output) == 0:
            raise RemctlError(self.error())
        return output

    def noop(self):
        # type: () -> None
        if not self.opened:
            raise RemctlNotOpenedError("no currently open connection")
        if not _remctl.remctl_noop(self.r):
            raise RemctlError(self.error())

    def close(self):
        # type: () -> None
        del self.r
        self.r = None
        self.opened = False

    def error(self):
        # type: () -> str
        if not self.r:
            # We do this instead of throwing an exception so that callers
            # don't have to handle an exception when they are trying to find
            # out why an exception occured.
            return "no currently open connection"
        return _remctl.remctl_error(self.r)
