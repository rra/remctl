#!/usr/bin/env python
#
# Python remctl extension build rules.
#
# Original implementation by Thomas L. Kula <kula@tproa.net>
# Copyright 2019 Russ Allbery <eagle@eyrie.org>
# Copyright 2008
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

import os

from setuptools import Extension, setup

try:
    from typing import List
except Exception:
    pass

VERSION = "3.16"

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


def parse_flags(prefix, flags):
    # type: (str, str) -> List[str]
    """Parse a string of compiler or linker flags for items of interest.

    Helper function to parse a string of compiler flags for ones of interest
    and strip the flag off, returning the list that the distutils Extension
    interface expects.
    """
    result = []
    filtered = [opt for opt in flags.split() if opt.startswith(prefix)]
    for opt in filtered:
        result.append(opt[len(prefix) :])
    return result


# When built as part of the remctl distribution, the top-level build
# configuration will set REMCTL_PYTHON_LIBS to any additional flags that
# should be used in the link.  Extract those flags and pass them into the
# extension configuration.  When built stand-alone, use the defaults and
# assume we don't need special contortions to link with libremctl.
library_dirs = parse_flags("-L", os.environ.get("REMCTL_PYTHON_LIBS", ""))
libraries = parse_flags("-l", os.environ.get("REMCTL_PYTHON_LIBS", ""))

extension = Extension(
    "_remctl",
    sources=["_remctlmodule.c"],
    define_macros=[("VERSION", '"' + VERSION + '"')],
    libraries=["remctl"] + libraries,
    library_dirs=library_dirs,
)

kwargs = {
    "name": "pyremctl",
    "version": VERSION,
    "author": "Thomas L. Kula",
    "author_email": "kula@tproa.net",
    "url": "https://www.eyrie.org/~eagle/software/remctl/",
    "description": doclines[0],
    "long_description": "\n".join(doclines[2:]),
    "license": "MIT",
    "install_requires": ["typing"],
    "setup_requires": ["pytest-runner"],
    "tests_require": ["pytest"],
    "classifiers": filter(None, classifiers.split("\n")),
    "platforms": "any",
    "keywords": ["remctl", "kerberos", "remote", "command"],
    "ext_modules": [extension],
    "py_modules": ["remctl"],
}

setup(**kwargs)
