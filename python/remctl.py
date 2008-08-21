# Python interface to remctl.
#
# This is the high-level interface that most Python programs that use remctl
# should be using.  It's a Python wrapper around the _remctl C module, which
# exposes exactly the libremctl API.
#
# Written by Thomas L. Kula <kula@tproa.net>
# Copyright 2008 Thomas L. Kula <kula@tproa.net>
# Copyright 2008 Board of Trustees, Leland Stanford Jr. University
#
# See LICENSE for licensing terms.

"""Interface to remctl.

   This module is an interface to remctl, a client/server
   protocol for running single commands on a remote host
   using Kerberos v5 authentication.
"""

VERSION = 0.4

import _remctl

REMCTL_OUT_OUTPUT = _remctl.REMCTL_OUT_OUTPUT
REMCTL_OUT_STATUS = _remctl.REMCTL_OUT_STATUS
REMCTL_OUT_ERROR  = _remctl.REMCTL_OUT_ERROR
REMCTL_OUT_DONE   = _remctl.REMCTL_OUT_DONE

# Exception classes.

class RemctlGenericError(Exception):
    """Base error class remctl exceptions."""
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return str(self.value)

class RemctlArgError(RemctlGenericError):
    """Invalid arguments supplied."""
    pass

class RemctlProtocolError(RemctlGenericError):
    """A remctl protocol error occurred."""
    pass

class RemctlError(RemctlGenericError):
    """The underlying remctl library has returned an error."""
    pass

class RemctlNotOpened(RemctlGenericError):
    """No open connection to a server."""
    pass

# Simple interface.

class RemctlSimpleResult:
    """An object holding the results from the simple interface."""
    def __init__(self):
        self.stdout = None
        self.stderr = None
        self.status = None

def remctl(host, port, principal, command):
    """Simple interface to remctl.

    Connect to HOST on PORT, using PRINCIPAL as the server principal for
    authentication, and issue COMMAND.  Returns the result as a
    RemctlSimpleResult object, which has three attributes.  stdout holds the
    complete standard output, stderr holds the complete standard error, and
    status holds the exit status.
    """
    try:
        myport = int(port)
    except:
        raise RemctlArgError, 'port must be a number'
    if (myport < 0) or (myport > 65535):
        raise RemctlArgError, 'invalid port number'
    if len(command) < 1:
        raise RemctlArgError, 'command must not be empty'

    # Convert the command to a list of strings.
    mycommand = []
    for item in command:
        mycommand.append(str(item))

    # At this point, things should be sane.  Call the low-level interface.
    output = _remctl.remctl(host, port, principal, mycommand)
    if output[0] != None:
        raise RemctlProtocolError, output[0]
    result = RemctlSimpleResult()
    setattr(result, 'stdout', output[1])
    setattr(result, 'stderr', output[2])
    setattr(result, 'status', output[3])
    return result

# Complex interface.

class Remctl:
    def __init__(self, host = None, port = None, principal = None):
        self.r = _remctl.remctl_new()
        self.host = host
        self.port = port
        self.principal = principal
        self.opened = False

        if host != None:
            self.open(host, port, principal)

    def open(self, host, port = None, principal = None):
        self.host = host
        if port == None:
            self.port = 0
        else:
            try:
                self.port = int(port)
            except ValueError:
                raise RemctlArgError, 'port must be a number'
        if (self.port < 0) or (self.port > 65535):
            raise RemctlArgError, 'invalid port number'
        self.principal = principal

        # At ths point, things should be sane.  Call the low-level interface.
        if not _remctl.remctl_open(self.r, self.host, self.port,
                                   self.principal):
            raise RemctlError, 'error opening connection'
        self.opened = True

    def command(self, comm):
        self.commlist = []
        if self.opened == False:
            raise RemctlNotOpened, 'no currently open connection'
        if isinstance(comm, list) == False:
            raise RemctlArgError, 'you must supply a list of commands'
        if len(comm) < 1:
            raise RemctlArgError, 'command must not be empty'

        # Convert the command to a list of strings.
        for item in comm:
            self.commlist.append(str(item))
        if not _remctl.remctl_commandv(self.r, self.commlist):
            raise RemctlError, 'error sending command'

    def output(self):
        if self.opened == False:
            raise RemctlNotOpened, 'no currently open connection'
        return _remctl.remctl_output(self.r)

    def close(self):
        del(self.r)
        self.r = None
        self.opened = False

    def error(self):
        if self.r == None:
            # We do this instead of throwing an exception so that callers
            # don't have to handle an exception when they are trying to find
            # out why an exception occured.
            return 'no currently open connection'
        return _remctl.remctl_error(self.r)
