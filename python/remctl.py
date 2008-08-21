"""Interface to remctl.

    This module is an interface to remctl, a client/server
    protocol for running single commands on a remote host
    using Kerberos v5 authentication.
"""

# Author: Thomas L. Kula <kula@tproa.net>

VERSION=0.4
RCS='$Id: remctl.py,v 1.4 2008/03/08 01:09:39 kula Exp $'

import _remctl

REMCTL_OUT_OUTPUT   = _remctl.REMCTL_OUT_OUTPUT
REMCTL_OUT_STATUS   = _remctl.REMCTL_OUT_STATUS
REMCTL_OUT_ERROR    = _remctl.REMCTL_OUT_ERROR
REMCTL_OUT_DONE     = _remctl.REMCTL_OUT_DONE

# Error classes

class Error(Exception):
    """Base error class for this module."""
    def __init__(self, value):
	self.value = value
    def __str__(self):
	return str(self.value)

class RemctlArgError(Error):
    """Invalid arguments supplied."""
    pass

class RemctlProtocolError(Error):
    """An error in the remctl protocol happened."""
    pass

class RemctlError(Error):
    """The underlying remctl library has returned an error."""
    pass

class RemctlNotOpened(Error):
    """No open connection."""
    pass

# Simple interface

class RemctlSimpleResult:
    """An object holding the results from the simple interface"""
    def __init__(self):
	self.stdout = None
	self.stderr = None
	self.status = None

def remctl( host = None, port = 4373, principal = None, command = [] ):
    """Simple interface to remctl"""

    if host == None:
	raise RemctlArgError, 'No host supplied'

    try:
	myport = int( port )
    except:
	raise RemctlArgError, "port must be a number"

    if (( myport < 0 ) or ( myport > 65535 )):
	raise RemctlArgError, "invalid port number"

    if principal == None:
	principal = 'host/' + host

    if len( command ) < 2: 
	raise RemctlArgError, "must have two or more arguments to command"

    mycommand = []
    for item in command:
	mycommand.append( str( item ))

    # At this point, everything should be sane. Call the low-level
    # interface

    myresult = _remctl.remctl( host, port, principal, mycommand )

    if myresult[ 0 ] != None:
	raise RemctlProtocolError, myresult[ 0 ]

    result = RemctlSimpleResult()

    setattr( result, 'stdout', myresult[ 1 ])
    setattr( result, 'stderr', myresult[ 2 ])
    setattr( result, 'status', myresult[ 3 ])


    return result

# Complex interface

class Remctl:
    def __init__( self, host=None, port=None, principal=None ):
	self.remobj = _remctl.remctlnew()
	self.host = host
	self.port = port
	self.principal = principal
	self.opened = False

	if host != None:
	    self.open( host=host, port=port, principal=principal)
	
    def open( self, host=None, port=None, principal=None ):
	if host == None:
	    raise RemctlArgError, "no host supplied"
	self.host = host

	if port == None:
	    self.port = 4373
	else:
            try:
                self.port = int( port )
            except ValueError:
                raise RemctlArgError, "port must be a number"

	if (( self.port < 0 ) or ( self.port > 65535 )):
	    raise RemctlArgError, "invalid port number"

	self.principal = principal
	if self.principal == None:
	    self.principal = 'host/' + self.host

	# At ths point, things should be sane, call the low-level
	# interface

	if _remctl.remctlopen( self.remobj, self.host, self.port, self.principal ) != 1:
	    raise RemctlError, "error opening connection"

	self.opened = True

    def command( self, comm ):
	self.commlist = []

	if self.opened == False:
	    raise RemctlNotOpened, "no currently open connection"

	if isinstance( comm, list ) == False:
	    raise RemctlArgError, "you must supply a list of commands"

	if len( comm ) < 2:
	    raise RemctlArgError, "you must supply at least two strings in your command"

	for item in comm:
	    self.commlist.append( str( item ))

	if _remctl.remctlcommandv( self.remobj, self.commlist ) == False:
	    raise RemctlError, "error sending command"

    def output( self ):
	if self.opened == False:
	    raise RemctlNotOpened, "no currently open connection"

	return _remctl.remctloutput( self.remobj )

    def close( self ):
	del( self.remobj )
	self.remobj = None
	self.opened = False

    def error( self ):
	if self.remobj == None:
	    # We do this instead of throwing an exception so that callers
	    # don't have to handle an exception when they are trying to
	    # find out why an exception occured
	    return 'pyremctl: no currently opened connection'

	return _remctl.remctlerror( self.remobj )


