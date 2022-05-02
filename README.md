# remctl

[![Build
status](https://github.com/rra/remctl/workflows/build/badge.svg)](https://github.com/rra/remctl/actions)
[![Debian
package](https://img.shields.io/debian/v/remctl/unstable)](https://tracker.debian.org/pkg/remctl)

Copyright 2015-2020, 2022 Russ Allbery <eagle@eyrie.org>.  Copyright
2002-2014 The Board of Trustees of the Leland Stanford Junior University.
This software is distributed under a BSD-style license.  Please see the
section [License](#license) below for more information.

## Blurb

remctl is a client/server application that supports remote execution of
specific commands, using Kerberos GSS-API for authentication.
Authorization is controlled by a configuration file and ACL files and can
be set separately for each command, unlike with rsh.  remctl is like a
Kerberos-authenticated simple CGI server, or a combination of Kerberos ssh
and sudo without most of the features and complexity of either.

## Description

remctl is a client/server application that supports remote execution of
specific commands, using Kerberos GSS-API for authentication and
confidentiality.  The commands a given user can execute are controlled by
a configuration file and ACL files and can easily be tightly limited,
unlike with rsh.  The mapping of command to backend program is done by the
configuration file, which allows some additional flexibility compared to
ssh command restrictions and works with Kerberos authentications rather
than being limited to public key authentications.

remctld is very similar to a CGI server that uses a different network
protocol than HTTP, always does strong authentication before executing the
desired command, and guarantees the data is encrypted on the network.
Alternately, you can think of it as a very simple combination of Kerberos
ssh and sudo, without most of the features of both but with simpler
authorization.

There are a lot of different client/server systems that do something
similar: current packages like gRPC, and a wealth of older systems like
rsh, CGI, CERN's arc, and more elaborate systems like MIT's Moira.  remctl
has the advantage over many of these schemes of using GSS-API and being
about as simple as it possibly can be while still being useful.  It
doesn't require any particular programming language, builds self-contained
binaries, and uses as minimal of a protocol as possible.

Both C and Java clients and servers are provided, as well as Perl, PHP,
Python, and Ruby bindings for the C client library.  For more information
about the Java client, see `java/README`.  For more information about the
PHP bindings, see `php/README`.  For more information about the Python
bindings, see `python/README`.  For more information about the Ruby
bindings, see `ruby/README`.

Also included in the remctl package is an alternate way of running the
remctl server: remctl-shell.  This program is designed to be run as either
a shell or a forced command under ssh, using ssh for authentication and
communicating the authentication information to remctl-shell via either
environment variables or command-line arguments via the forced command
configuration.  This version of the server uses simple ssh clients, rather
than using the remctl client program or libraries.

remctl was originally written by Anton Ushakov as a replacement for IBM's
sysctl, a client/server application with Kerberos v4 authentication that
allowed the client to run Tcl code on the server, protected by ACLs.  At
Stanford, we used sysctl extensively, but mostly only to run external
programs, so remctl was developed as a Kerberos v5 equivalent that did
only the portions we needed.

Complete protocol documentation is available in `docs/protocol.html`.
Also present, as `docs/design.html`, is the original design document (now
somewhat out of date).

## Requirements

The remctld server and the standard client are written in C and require a
C compiler and GSS-API libraries to build.  Both will build against either
MIT Kerberos or Heimdal of any reasonable vintage.  remctl will also build
against the Kerberos GSS-API implementation shipped with AIX 5.2 (and
possibly later versions) and the Solaris 10 generic GSS-API library (and
possibly later versions).  The `remctl_set_ccache` implementation is
improved by building with Kerberos libraries and a GSS-API library that
supports `gss_krb5_import_cred`.

The remctld server requires libevent 1.4.x or later.  It was only tested
with libevent 1.4.13-stable and later, but should work with 1.4.4 or
later.  It is now only tested with libevent 2.x, so moving to a later
version of libevent if possible is recommended.

The remctl server will support regex ACLs if the system supports the POSIX
regex API.  The remctl server also optionally supports PCRE regular
expressions in ACLs.  To include that support, the PCRE library (either
PCRE2 or PCRE1) is required.

To build the remctl client for Windows, the Microsoft Windows SDK for
Windows Vista and the MIT Kerberos for Windows SDK are required, along
with a Microsoft Windows build environment (probably Visual Studio).
remctl has only been tested with the 3.2.1 MIT Kerberos for Windows SDK.
To run the resulting binary, MIT Kerberos for Windows must be installed
and configured.  The client was tested on Windows XP and Vista and should
work on Windows 2000 and up; however, the primary maintainer does not use
or test Windows, so it's always possible Windows support has broken.  The
server is not supported on Windows.

To build the Perl bindings for the C client library, you will need Perl
5.8 or later.

To build the PHP bindings for the C client library, you will need PHP 5.x
or later and phpize, plus any other programs that phpize requires.  PHP
5.x support has only been tested on 5.2 and 5.3, and PHP support is now
only tested on PHP 7.x and later.

To build the Python bindings for the C client library, you will need
Python 2.7, or Python 3.1 or later.  You will also need the setuptools and
pytest modules and, for Python 2, the typing module.  Earlier versions may
work back to possibly Python 2.3, but are not tested.

To build the Ruby bindings for the C client library, you will need Ruby
1.8 or later (primarily tested with 2.5 and later).

None of the language bindings have been tested on Windows.

A Java client and Java server are available in the java subdirectory, but
they are not integrated into the normal build or built by default.  There
is a basic Makefile in that directory that may require some tweaking.  It
currently requires the Sun Java JDK (1.4.2, 5, or 6) or OpenJDK 6 or
later.  A considerably better Java client implementation is available on
the `java` branch in the Git repository but has not yet been merged.

To bootstrap from a Git checkout, or if you change the Automake files and
need to regenerate Makefile.in, you will need Automake 1.11 or later.  For
bootstrap or if you change configure.ac or any of the m4 files it includes
and need to regenerate configure or config.h.in, you will need Autoconf
2.64 or later.  Perl is also required to generate manual pages from a
fresh Git checkout.  You will also need pkg-config installed to regenerate
configure and xml2rfc to build the formatted protocol documentation.

## Building and Installation

You can build and install remctl with the standard commands:

```
    ./configure
    make
    make install
```

If you are building from a Git clone, first run `./bootstrap` in the
source directory to generate the build files.  `make install` will
probably have to be done as root.  Building outside of the source
directory is also supported, if you wish, by creating an empty directory
and then running configure with the correct relative path.

Solaris users should look at `examples/remctld.xml`, an SMF manifest for
running the `remctld` daemon.

To also build the Perl bindings for the libremctl client library, pass the
`--enable-perl` option to `configure`.  The Perl module build is handled
by the normal Perl extension build system, and therefore will be built
with compiler flags defined by your Perl installation and installed into
your local Perl module directory regardless of the `--prefix` argument to
`configure`.  To change this, you will need to run `perl Makefile.PL` in
the `perl` subdirectory of the build tree with appropriate options and
rebuild the module after running `make` and before running `make install`.

To also build the remctl PECL extension for PHP, pass the `--enable-php`
option to `configure`.  The PHP PECL module build is handled by the normal
PHP extension build system and therefore will be installed into your local
PHP module directory.  The configure script will look for `phpize` on your
`PATH` by default; if it's in some other directory, set the `PHPIZE`
environment variable to the full path or set it on the configure command
line.  The configure script for the PECL extension will be run during the
build instead of during configure.  This is unfortunately apparently
unavoidable given how the PECL build system works.

To also build the Python bindings for the libremctl client library, pass
the `--enable-python` option to configure.  The Python module build is
handled by the normal Python extension build system, and therefore will be
installed into your local Python module directory regardless of the
`--prefix` argument to `configure`.  To change this, you will need to run
`python setup.py install` by hand in the `python` directory with whatever
options you want to use.

To also build the Ruby bindings for the libremctl client library, pass the
`--enable-ruby` option to configure.  The Ruby module build is handled by
the normal Ruby module build system, and therefore will be installed into
your local Ruby module directory regardless of the `--prefix` argument to
`configure`.  To change this, override the `sitedir` variable on the `make
install` command line, as in:

```
    make install sitedir=/opt/ruby
```

The remctl build system also supports a few other environment variables
that can be set to control aspects of the Perl, Python, and Ruby binding
build systems.  These are primarily only of use when packaging the
software.  For more information, a list of the variables, and their
effects, see the comment at the start of `Makefile.am`.

The Java client and server aren't integrated with the regular build
system.  For information on building and installing them, see
`java/README`.

remctl will use pkg-config if it's available to find the build flags for
libevent.  You can control which pkg-config binary and paths are used with
the normal pkg-config environment variables of `PKG_CONFIG`,
`PKG_CONFIG_PATH`, and `PKG_CONFIG_LIBDIR`, and you can override the
pkg-config results with `LIBEVENT_CFLAGS` and `LIBEVENT_LIBS`.
Alternately, you can bypass pkg-config by passing one or more of
`--with-libevent`, `--with-libevent-include`, and `--with-libevent-lib` to
indicate the install prefix, include directory, or library directory.

remctl will automatically build with PCRE support if PCRE2 or PCRE1 are
found.  As with libevent, remctl will use pkg-config if it's available to
find the build flags for PCRE2.  Use the same variables as documented by
libevent to control which pkg-config is used, and override its results
with `PCRE2_CFLAGS` and `PCRE2_LIBS`.  For PCRE1, the `pcre-config` script
will be used.  You can set `PCRE_CONFIG` to point to a different
pcre-config script, or do similar things as with `PATH_KRB5_CONFIG`
described below.  Alternately, you can bypass pkg-config by passing one or
more of `--with-pcre2`, `--with-pcre2-include`, `--with-pcre2-lib`,
`--with-pcre`, `--with-pcre-include`, or `--with-pcre-lib` to indicate the
install prefix, include directory, or library directory.

remctl will automatically build with GPUT support if the GPUT header and
library are found.  You can pass `--with-gput` to configure to specify the
root directory where GPUT is installed, or set the include and library
directories separately with `--with-gput-include` and `--with-gput-lib`.

Normally, configure will use `krb5-config` to determine the flags to use
to compile with your Kerberos libraries.  To specify a particular
`krb5-config` script to use, either set the `PATH_KRB5_CONFIG` environment
variable or pass it to configure like:

```
    ./configure PATH_KRB5_CONFIG=/path/to/krb5-config
```

If `krb5-config` isn't found, configure will look for the standard
Kerberos libraries in locations already searched by your compiler.  If the
the `krb5-config` script first in your path is not the one corresponding
to the Kerberos libraries you want to use, or if your Kerberos libraries
and includes aren't in a location searched by default by your compiler,
you need to specify a different Kerberos installation root via
`--with-krb5=PATH`.  For example:

```
    ./configure --with-krb5=/usr/pubsw
```

You can also individually set the paths to the include directory and the
library directory with `--with-krb5-include` and `--with-krb5-lib`.  You
may need to do this if Autoconf can't figure out whether to use `lib`,
`lib32`, or `lib64` on your platform.

To not use `krb5-config` and force library probing even if there is a
`krb5-config` script on your path, set `PATH_KRB5_CONFIG` to a nonexistent
path:

```
    ./configure PATH_KRB5_CONFIG=/nonexistent
```

`krb5-config` is not used and library probing is always done if either
`--with-krb5-include` or `--with-krb5-lib` are given.

GSS-API libraries are found the same way: with `krb5-config` by default if
it is found, and a `--with-gssapi=PATH` flag to specify the installation
root.  `PATH_KRB5_CONFIG` is similarly used to find `krb5-config` for the
GSS-API libraries, and `--with-gssapi-include` and `--with-gssapi-lib` can
be used to specify the exact paths, overriding any `krb5-config` results.

Pass `--enable-silent-rules` to configure for a quieter build (similar to
the Linux kernel).  Use `make warnings` instead of `make` to build with
full GCC compiler warnings (requires either GCC or Clang and may require a
relatively current version of the compiler).

You can pass the `--enable-reduced-depends` flag to configure to try to
minimize the shared library dependencies encoded in the binaries.  This
omits from the link line all the libraries included solely because other
libraries depend on them and instead links the programs only against
libraries whose APIs are called directly.  This will only work with shared
libraries and will only work on platforms where shared libraries properly
encode their own dependencies (this includes most modern platforms such as
all Linux).  It is intended primarily for building packages for Linux
distributions to avoid encoding unnecessary shared library dependencies
that make shared library migrations more difficult.  If none of the above
made any sense to you, don't bother with this flag.

## Testing

remctl comes with a comprehensive test suite, but it requires some
configuration in order to test anything other than low-level utility
functions.  For the full test suite, you will need to have a keytab that
can authenticate to a running KDC.  Using a test KDC environment, if you
have one, is recommended.

Follow the instructions in `tests/config/README` to configure the test
suite.

Now, you can run the test suite with:

```
    make check
```

If a test fails, you can run a single test with verbose output via:

```
    tests/runtests -o <name-of-test>
```

Do this instead of running the test program directly since it will ensure
that necessary environment variables are set up.

On particularly slow or loaded systems, you may see intermittant failures
from the `server/streaming` test because it's timing-sensitive.

The test suite will also need to be able to bind to 127.0.0.1 on port
11119 and 14373 to run test network server programs.

To test anonymous authentication, the KDC configured in the test suite
needs to support service tickets for the anonymous identity (not a
standard configuration).  This test will be skipped if the KDC does not
support this.

To test user handling in remctld, you will need the `fakeroot` command
(available in the `fakeroot` package in Debian and Ubuntu).  This test
will be skipped if `fakeroot` isn't available.

To test the Perl bindings, Perl 5.10 or later is required.  (However, the
Perl modules themselves should work with Perl 5.8.)

The following additional Perl modules will be used by the test suite for
the main package and the Perl bindings if installed:

* Test::MinimumVersion
* Test::Perl::Critic
* Test::Pod
* Test::Pod::Coverage
* Test::Spelling
* Test::Strict
* Test::Synopsis

All are available on CPAN.  Those tests will be skipped if the modules are
not available.

To enable tests that don't detect functionality problems but are used to
sanity-check the release, set the environment variable `RELEASE_TESTING`
to a true value.  To enable tests that may be sensitive to the local
environment or that produce a lot of false positives without uncovering
many problems, set the environment variable `AUTHOR_TESTING` to a true
value.

## Building on Windows

(These instructions are not tested by the author and are now dated.
Updated instructions via a pull request, issue, or email are very
welcome.)

First, install the Microsoft Windows SDK for Windows Vista if you have not
already.  This is a free download from Microsoft for users of "Genuine
Microsoft Windows."  The `vcvars32.bat` environment provided by Visual
Studio may work as an alternative, but has not been tested.

Next, install the [MIT Kerberos for Windows
SDK](https://web.mit.edu/kerberos/www/dist/index.html).  remctl has been
tested with version 3.2.1 but should hopefully work with later versions.

Then, follow these steps:

1. Run the `InitEnv.cmd` script included with the Windows SDK with
   parameters `"/xp /release"`.

2. Run the `configure.bat` script, giving it as an argument the location
   of the Kerberos for Windows SDK.  For example, if you installed the KfW
   SDK in `"c:\KfW SDK"`, you should run:

   ```
       configure "c:\KfW SDK"
   ```

3. Run `nmake` to start compiling.  You can ignore the warnings.

If all goes well, you will have `remctl.exe` and `remctl.dll`.  The latter
is a shared library used by the client program.  It exports the same
interface as the UNIX libremctl library.

## Support

The [remctl web page](https://www.eyrie.org/~eagle/software/remctl/) will
always have the current version of this package, the current
documentation, and pointers to any additional resources.

For bug tracking, use the [issue tracker on
GitHub](https://github.com/rra/remctl/issues).  However, please be aware
that I tend to be extremely busy and work projects often take priority.
I'll save your report and get to it as soon as I can, but it may take me a
couple of months.

## Source Repository

remctl is maintained using Git.  You can access the current source on
[GitHub](https://github.com/rra/remctl) or by cloning the repository at:

https://git.eyrie.org/git/kerberos/remctl.git

or [view the repository on the
web](https://git.eyrie.org/?p=kerberos/remctl.git).

The eyrie.org repository is the canonical one, maintained by the author,
but using GitHub is probably more convenient for most purposes.  Pull
requests are gratefully reviewed and normally accepted.

## License

The remctl package as a whole is covered by the following copyright
statement and license:

> Copyright 2015-2020, 2022
>     Russ Allbery <eagle@eyrie.org>
>
> Copyright 2002-2014
>     The Board of Trustees of the Leland Stanford Junior University
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the "Software"),
> to deal in the Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, sublicense,
> and/or sell copies of the Software, and to permit persons to whom the
> Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
> THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
> FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
> DEALINGS IN THE SOFTWARE.

Some files in this distribution are individually released under different
licenses, all of which are compatible with the above general package
license but which may require preservation of additional notices.  All
required notices, and detailed information about the licensing of each
file, are recorded in the LICENSE file.

Files covered by a license with an assigned SPDX License Identifier
include SPDX-License-Identifier tags to enable automated processing of
license information.  See https://spdx.org/licenses/ for more information.

For any copyright range specified by files in this package as YYYY-ZZZZ,
the range specifies every single year in that closed interval.
