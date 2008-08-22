# Python remctl extension build rules.
#
# Written by Thomas L. Kula <kula@tproa.net>
# Copyright 2008 Thomas L. Kula <kula@tproa.net>
# Copyright 2008 Board of Trustees, Leland Stanford Jr. University
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

VERSION = '0.4'

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
                      sources       = [ '_remctlmodule.c' ],
                      libraries     = [ 'remctl' ],
                      define_macros = [ ('VERSION', '"' + VERSION + '"') ])

setup(name             = 'pyremctl',
      version          = VERSION,
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
