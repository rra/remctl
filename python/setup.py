# Python remctl extension build rules.
#
# Written by Thomas L. Kula <kula@tproa.net>
# Copyright 2008 Thomas L. Kula <kula@tproa.net>
# Copyright 2008 Board of Trustees, Leland Stanford Jr. University
#
# See LICENSE for licensing terms.

# Keep the wrapping of the description to 65 columns and make sure there is no
# trailing newline so that PKG-INFO looks right.
"""Python bindings for remctl remote command execution

remctl is a client/server application that supports remote
execution of specific commands, using Kerberos v5 GSS-API for
authentication.  Authorization is controlled by a configuration
file and ACL files and can be set separately for each command,
unlike with rsh.  remctl is like a Kerberos-authenticated simple
CGI server, or a combination of Kerberos rsh and sudo without
most of the features and complexity of either.

This module provides Python bindings to the remctl client
library."""

from distutils.core import setup, Extension

doclines = __doc__.split("\n")
classifiers = """\
Development Status :: 4 - Beta
Intended Audience :: Developers
License :: OSI Approved :: MIT License
Operating System :: OS Independent
Programming Language :: C
Programming Language :: Python
Topic :: Security
Topic :: Software Development :: Libraries :: Python Modules
"""
extension = Extension('_remctl',
                      sources   = [ '_remctlmodule.c' ],
                      libraries = [ 'remctl' ])

setup(name             = 'pyremctl',
      version          = '0.4',
      author           = 'Thomas L. Kula',
      author_email     = 'kula@tproa.net',
      url              = 'http://www.eyrie.org/~eagle/software/remctl/',
      description      = doclines[0],
      long_description = "\n".join(doclines[2:]),
      license          = 'MIT',
      classifiers      = filter(None, classifiers.split("\n")),
      platforms        = 'any',
      keywords         = [ 'remctl', 'kerberos', 'remote', 'command' ],

      ext_modules      = [ extension ],
      py_modules       = ['remctl'])
