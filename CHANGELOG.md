# remctl change log

Beginning with version 4.0.0, remctl is versioned with [semver](https://semver.org/). Versions older than 4.0.0 use a relaxed semver-like versioning scheme.

Find changes for the upcoming release in the project's [changelog.d directory](https://github.com/rra/rra-c-util/tree/main/changelog.d/).

<!-- scriv-insert-here -->

<a id='changelog-3.18'></a>

## 3.18.0 (2022-05-20)

### Backwards-incompatible changes

- Drop support for Perl 5.8. The Perl libraries and the remctl test suite now require Perl 5.10 or later.

### New features

- Add support for PCRE2 for pcre ACLs and use it by preference over PCRE1 if it is found. UTF-8 regular expressions are not enabled by default, but can be enabled by adding `(*UTF)` to the beginning of the regular expression (a standard PCRE2 feature not specific to remctl).
- Mark remctl client library functions that allocate memory with their corresponding deallocation functions so that GCC 11 and later can diagnose memory deallocation bugs.

### Bug fixes

- Remove remaining references to pytest-runner in the Python bindings. Thanks, Ken Dreyer.
- Switch the Ruby bindings tests to Minitest from Test::Unit. Thanks, Ken Dreyer.
- Fix `IN6_ARE_ADDR_EQUAL` Autoconf probe on macOS.
- Fix compiler flag probes with Clang.

### Other changes

- Document that pcre and regex ACL expressions are not automatically anchored at the start and end of the principal name, so they should be explicitly anchored in the configuration unless one intends to allow partial matches.
- Document sending SIGHUP to remctld when running in stand-alone mode to ask it to re-read its configuration file, and document that SIGTERM will cause it to exit. (Fixes #30)

<a id='changelog-3.17'></a>

## 3.17 (2020-12-13)

### New features

- Port the PHP extention to PHP 8. This required declaring the arguments to the functions (which should have been done with PHP 7) and removing some obsolete constructs.

### Bug fixes

- Make the Python `install_requires` dependency on typing conditional on Python versions earlier than 3.5 so that setuptools won't attempt to download typing when it's part of the standard library. Thanks to Gianfranco Costamagna and Matthias Klose for the bug report.
- Fix the Python module build to more reliably test the newly-built module and to enable verbose testing.
- Fix non-Kerberos network tests on hosts with no IPv4 addresses. In this case, the network tests for binding all configured addresses will bind only to IPv6, which broke some prior assumptions in the test suite. Thanks to Niko Tyni for the bug report. Note that the tests that require a Kerberos setup will still fail in this scenario, since they assume remctld will bind to 127.0.0.1 by default.
- Include `string.h` when probing for getaddrinfo properties.
- Fix support for configuring the test suite with a `krb5.conf` file.
- Fix tests when the system `krb5.conf` file does not set `default_realm`.

### Other changes

- Stop providing a replacement for a broken snprintf and assume the libc version works correctly. This portability code has proven difficult to maintain, and was only relevant for ancient proprietary UNIX versions that have been obsolete for many years.

<a id='changelog-3.16'></a>

## 3.16 (2019-10-26)

### Backwards-incompatible changes

- Modernize the Python bindings to remove obsolete syntax, which may mean that versions of Python back to Python 2.3 are no longer supported. The bindings are only tested with Python 2.7.
- Deprecate passing in anything other than an iterable of `str` or `bytes` to the Python bindings as the command to run. Support for using objects that can be converted to `str` in commands will be removed in a future release.

### New features

- Add support for Python 3 to the Python bindings. They have been tested only with Python 2.7 and Python 3.7, but should work with any version of Python 3 later than Python 3.1.
- Update the Python bindings documentation to use proper Python types and to document how `str` and `bytes` values are handled.
- Add `-t` flag to the remctl client to specify the network timeout. Thanks, Remi Ferrand.
- Add GCC attributes to the declarations of the libremctl client functions, which will allow some minor optimization improvements and better compiler errors about NULL pointers.

### Bug fixes

- Fix NULL pointer dereference in the client library if allocation of memory fails, caught by cppcheck.
- More correctly handle user-supplied CFLAGS in combination with make warnings when building the PHP bindings. Add the warning flags to `AM_CFLAGS` instead of `CFLAGS` and pass user-supplied `CFLAGS` through to configure (but not the warning flags). Thanks, Ken Dreyer.
- Fix Kerberos library probing with `--enable-reduced-depends` and correctly suppress probing for Kerberos library features when no Kerberos library is present.

<a id='changelog-3.15'></a>

## 3.15 (2018-12-13)

### Bug fixes

- Fix a bug where output could have been truncated for a command run by the server that was accepting an argument on standard input, if it exited before reading all of the input data. Incorrect server logic disabled reads from the child process on write failure, so could have missed the last buffer of output from the child. This was only seen under valgrind testing, not reported as a bug, so it's not clear how widespread of a problem this was.
- Validate that command argument count, the length of command arguments, and the length of blocks of output from the server fit into the data type used in the wire protocol.
- Check the port argument to remctl and remctld to ensure that it is a valid port number.
- Define `UINT32_MAX` for systems that don't have it.

### Other changes

- Rework the `check-valgrind` target to use the new C TAP Harness valgrind support and automatically check the valgrind log files for errors at the end of the test suite. This catches the bad free that caused the security issue in 3.14.
- Add `SPDX-License-Identifier` headers to all substantial source files.
- Avoid spurious test failures from the network library.

<a id='changelog-3.14'></a>

## 3.14 (2018-03-31)

### Bug fixes

- SECURITY: Fix use-after-free and double-free when handling the `sudo` option in the remctld and remctl-shell servers. For remctl-shell, this will occasionally produce a spurious non-zero exit status for a command that succeeded. For remctld, the normal consequence is a server process crash after running a command with the `sudo` option, but it may be possible (albeit difficult) for a streaming client to abuse this bug to execute an arbitrary command on the server or corrupt server memory. Thanks, Santosh Ananthakrishnan. (CVE-2018-0493)

<a id='changelog-3.13'></a>

## 3.13 (2016-10-10)

### New features

- remctl-shell now also supports being run as a forced command from `authorized_keys` (or other methods). This may be preferrable to using it as a shell since it doesn't require setting non-standard sshd options.
- The `summary` configuration option is now allowed for commands with subcommands other than `ALL`. When generating a help summary (done in response to the command `help` with no arguments), command lines with a subcommand and a `summary` option will be run with two arguments: the value of the `summary` option and then the subcommand. This allows proper generation of command summaries even for users who only have access to a few subcommands of a command. Patch from Remi Ferrand.
- The build system now supports new `REMCTL_PROGRAM_CFLAGS` and `REMCTL_PROGRAM_LDFLAGS` variables that can be set at build time to pass in additional arguments when compiling and linking programs (like remctl and remctld) but not libraries and, more importantly, language bindings. This can be used in distribution builds to pass in `-fPIE` for additional binary hardening. (`CFLAGS` and `LDFLAGS` cannot be used since `-fPIE` breaks the builds of the dynamic modules for langauges like Perl.)

<a id='changelog-3.12'></a>

## 3.12 (2016-07-29)

### New features

- Add a new server implementation, remctl-shell. This does not use the remctl protocol; instead, it is meant to be run via ssh by being configured as the shell of a dedicated user. It interprets a command it was given as a remctl command, using the same configuration and authorization checking as the normal remctl server. This can be useful to introduce remctl into an environment that has ssh public key authentication instead of Kerberos. remctl-shell has some significant limitations inherited from ssh and requires some setup to use. See its manual page for more information.
- Add a new configuration option, `sudo`, which tells remctld and remctl-shell to run the command as a different user using sudo. The path to the sudo binary is determined when remctld is compiled. Normally, it's more convenient to use the existing `user` option, but that relies on remctld running as root. If running the daemon as a non-root user, or when running remctl-shell as a non-root user, this option may work better.

<a id='changelog-3.11'></a>

## 3.11 (2016-05-07)

### New features

- The PHP bindings have been ported to PHP 7, based on work by Nish Aravamudan. The PHP 7 API is sufficiently different that this was done by forking the PHP code and creating a new version for PHP 7 and later, chosing which extension to compile based on the discovered version of PHP. Currently, there is no functionality difference, but the PHP 5 extension should be considered frozen and may not get any new features. It will eventually be removed in a future version of remctl when PHP 7 is sufficiently widespread.

### Bug fixes

- Fix numerous portability issues to various versions of Heimdal, thanks to multiple patches from Jeffrey Hutzelman.
- Multiple fixes and improvements to the RPM spec file from Jeffrey Hutzelman: systemd support, SLES support, add the missing libevent-devel dependency, fix the version, and fix an invalid date.
- Make the `util/network/server-t` test more robust against missing IPv6.

### Other changes

- Rename the script to bootstrap from a Git checkout to `bootstrap`, matching the emerging consensus in the Autoconf world.

<a id='changelog-3.10'></a>

## 3.10 (2015-11-27)

### Backwards-incompatible changes

- Anonymous authentication (such as via anonymous PKINIT) no longer satisfies `ANYUSER` ACLs. It's unlikely that existing installations would have encountered anonymous authentication, since obtaining service tickets with anonymous PKINIT is disabled by default.
- Simplify the Python `RemctlError` exception class. The code in the exception class just duplicated the behavior of the parent `Exception` class and was unnecessary, and it interfered with pickling the exception. This means that `RemctlError` exceptions, and any derived from `RemctlError`, will no longer have a `value` attribute. To get this information, use the string value of the exception object, or call the `error()` method on the remctl object. Thanks to Andrew Deason for the report.

### New features

- Two new remctld ACLs are supported: `anyuser:auth` and `anyuser:anonymous`. The first is equivalent to `ANYUSER`, and indeed `ANYUSER` is now treated as a backwards-compatibility alias for `anyuser:auth`. This permits any authenticated user in either the local realm or any realm with which there is cross-realm trust. The new `anyuser:anonymous` ACL permits absolutely any user, even unauthenticated users, allowing anyone with network access to the server to run the command. (Note, however, that actually running commands anonymously requires anonymous PKINIT and anonymous service tickets be enabled for the local Kerberos realm. These are not common configurations, particularly the second.)
- The remctld server now sets the `REMOTE_EXPIRES` environment variable to the time (in seconds since UNIX epoch) when the authenticated session used to run a command will expire. This will generally be the expiration time of the Kerberos ticket used to authenticate to the server.

### Bug fixes

- Previous versions always passed the flags to disable certain warnings to the language binding builds, even if warnings weren't otherwise enabled. As of remctl 3.9, that included a warning flag not supported by old versions of gcc, breaking builds on RHEL 5. Instead, only pass the warning suppression flags when building with warnings (via `make warnings`), which is not the default and is only supported with recent versions of gcc. Thanks to Ken Dreyer for the report.
- For the `localgroup` ACL scheme, dynamically resize the buffer passed to `getgrnam_r` if the call fails due to `ERANGE`. Users in large numbers of local groups may require more space than the buffer size returned by the `sysconf` call. Patch from Hugh Cole-Baker.
- Fix test suite portability to systems with older versions of Kerberos that didn't have `krb5_get_init_creds_opt_alloc`, such as the included Kerberos in Solaris 10.
- Fix Perl test suite framework for new Automake relative paths.
- Avoid `$()` in the probe for systemd support for Solaris portability.
- Improve portability to Kerberos included in Solaris 10.
- Fix hidden visibility of some utility functions.
- Improve portability of socket error codes to Windows.

### Other changes

- Prefer libsystemd to libsystemd-daemon if it is available.

<a id='changelog-3.9'></a>

## 3.9 (2014-07-02)

### New features

- Add a new server ACL type, `localgroup`, which converts the principal to a local username with `krb5_aname_to_localname` and then checks whether it is a member of a given local group. Based on work by Remi Ferrand.

### Bug fixes

- Fix incorrect handling of interruptions of network writes by signals in the server. Previous versions of remctld did not correctly handle `EINTR` returns from select, read, and write and might abort the connection instead of retrying the system call.
- Reset the `SIGPIPE` signal handler before running a command. The server sets `SIGPIPE` to `SIG_IGN`, which meant that, since ignored signals are inherited across an exec, the child process would inherit possibly surprising `SIGPIPE` behavior. Reset the handler to `SIG_DFL` so that commands get default `SIGPIPE` handling.

### Other changes

- Add version and compatibility information to all manual pages. Command-line and configuration options, ACL methods, environment variables, client library APIs, and other major features are now annotated with the version of remctl in which they were added.

<a id='changelog-3.8'></a>

## 3.8 (2014-01-28)

### New features

- The remctld server now uses libevent for the event loop that processes output from a command. This is primarily an internal change to improve maintainability, but it does have some noticable if minor benefits: primarily, no need to poll for child process exit every five seconds, and therefore faster responsiveness and less resource usage in each remctld process. libevent 1.4.4 or later is now required to build remctl.
- Rather than capping the data returned by the server in one `MESSAGE_OUTPUT` token at the rather arbitrary length of 65,000 octets, send up to the maximum amount of data permitted by the protocol. This also slightly increases the maximum length of the output returned under the version one protocol.

### Bug fixes

- Fix a minor memory leak in the server when processing help commands.
- Fix a GSS-API context leak in the remctl client when failing to send a protocol version one token.
- Use a temporary file and atomic rename when writing the server PID file to avoid racing with a process monitor that tries to read the PID out of the file before it's written.

<a id='changelog-3.7'></a>

## 3.7 (2014-01-06)

### New features

- Add support for systemd. If built on a system with systemd installed, remctl will install (but not enable) systemd units to start remctld via socket activation. remctld will also notify systemd when its initialization is complete if started by systemd with service notification enabled.
- Add support for upstart's expect stop daemon synchronization method. When starting remctld in stand-alone mode with upstart, pass the new `-Z` option to remctld, and it will raise `SIGSTOP` when ready to accept connections, signaling to upstart that the daemon has fully started.

### Bug fixes

- Fix a client memory leak when `remctl_set_ccache` is used with a Kerberos library that supports `gss_krb5_import_cred`. The credential was never freed, leaking memory with each remctl client call, and a Kerberos ticket cache struct could also be leaked in some situations.
- Fix Net::Remctl::Backend argument count validation when one of the arguments is coming from standard input. The count of arguments was previously not updated properly after splicing in the extra argument.
- Work around a bug in the Module::Build version that comes with RHEL 5 in passing compiler and linker flags to the Perl module build.
- Net::Remctl and related classes now check that the class argument is not undef and croak if it is, rather than dereferencing a NULL pointer. Caught by clang --analyze.
- Don't attempt to use Kerberos if no Kerberos error APIs were found.

<a id='changelog-3.6'></a>

## 3.6 (2013-08-14)

### Bug fixes

- If the client specifies a timeout, restart the wait for a nonblocking connect when interrupted by a signal. This can mean that a connect can take longer than the timeout if interrupted; hopefully both timeouts and catching signals are rare enough that this won't pose a serious issue.
- The help output from Net::Remctl::Backend now checks for commands whose syntax is excessively long and does not let them influence the formatting of the summary. This keeps commands with a long syntax from forcing all the summary output into a skinny column against the right margin and allows proper help output for commands with a syntax longer than 80 columns.
- Fix compilation problems with Kerberos libraries that don't have `gss_krb5_import_cred`, including Mac OS X and older Red Hat. Patch from Ken Dreyer.
- Fix problems with PCRE detection on platforms that have the library but not `pcre-config` or the `pcre.h` header file, such as Mac OS X.

<a id='changelog-3.5'></a>

## 3.5 (2013-06-28)

### Backwards-incompatible changes

- The version numbers of the Net::Remctl and Net::Remctl::Backend Perl modules now match the versions of the remctl package, but with at least two digits for the minor version so that, for example, 3.9 (which becomes 3.09) and 3.10 will sort properly as numbers. This means that, from Perl's perspective, the version numbers have gone backwards in this release relative to earlier 3.0 releases. This is a one-time adjustment to a more reliable versioning scheme.

### Bug fixes

- Fix a long-standing race condition in remctld (introduced in remctl 2.7) that could truncate large backend output if the backend program exits immediately after sending that output. On systems with pipe buffers larger than 64KB, remctld could discard some buffered output after determining that the child had exited. remctld now polls for and continues to process output from the child until no more is immediately available, even after the child has exited.
- If a Kerberos library and `gss_krb5_import_cred` are available at build time, libremctl now uses them to implement `remctl_set_ccache` to avoid affecting global program GSS-API state. If those requirements are met, `remctl_set_ccache` will only affect the remctl context on which it's called.

<a id='changelog-3.4'></a>

## 3.4 (2013-03-26)

### Backwards-incompatible changes

- The Net::Remctl Perl bindings now require Perl 5.8 or later (instead of 5.006 in previous versions) and are now built with Module::Build instead of ExtUtils::MakeMaker. This should be transparent to anyone not working with the source code, since Perl 5.8 was released in 2002, but Module::Build and ExtUtils::CBuilder are now required to build Net::Remctl. They are included in Perl 5.10 or later and can be installed separately for older versions of Perl.

### New features

- Add new C APIs for establishing a remctl connection given a `sockaddr`, a list of `struct addrinfo`, or an already-open socket. Patch from Jeffrey Hutzelman.
- The Perl bindings now include a new module, Net::Remctl::Backend, which handles the setup, dispatch, and help output for the recommended style for remctl backend scripts written in Perl. See its documentation for more information.

### Bug fixes

- Following Perl Best Practices, remove prototypes from all Net::Remctl functions. The confusion caused by changing context away from how Perl normally works is not worth any diagnostic value.
- Return an error if an empty command is passed into `remctl_command` rather than attempting to malloc zero bytes.
- Fix probing for Heimdal's libroken to work with older versions.

<a id='changelog-3.3'></a>

## 3.3 (2012-09-25)

### Bug fixes

- Fix a file descriptor leak when checking ACL files. This would cause long-running remctld processes to eventually run out of available file descriptors.
- Fix some memory leaks when reloading the remctld configuration and several memory leaks when closing or reusing client connections in libremctl.
- Don't create the remctld PID file until the network socket is bound and listening. This helps init scripts starting the daemon to know when startup is complete and the service is available.
- Remove prototypes from the Perl `remctl()` function. With prototypes, the connection and command information could not be provided via an array, since the prototype forces it into scalar context.
- Fix build dependencies for language bindings to work with parallel builds and pass `CPPFLAGS` down to the language binding build systems.

<a id='changelog-3.2'></a>

## 3.2 (2012-06-19)

### New features

- Add new `summary` option to the remctld configuration. If remctld receives a command of `help` with no arguments and no command by that name has been defined, the server will look through the configuration for any command with a `summary` option set, a subcommand of `ALL`, and which the user would have been allowed to run. If any such commands are found, the server will run each with the subcommand specified by the `summary` option, sending the results to the user. This allows display of a command summary to the user based on which commands that user is authorized to run. Written by Jon Robertson.
- Add new `help` option to the remctld configuration. If remctld receives a command of `help` with either one or two arguments and no command by that name has been defined, it takes the arguments to the command as a command and subcommand and checks for an entry in the configuration file that matches. If such an entry is found, the `help` option is set for that command, and the user is authorized to run it, remctld runs the command, passing the value of the `help` option as the subcommand and the arguments to `help` as additional arguments. This permits a standard interface to get additional help for a particular remctl command. Written by Jon Robertson.

### Bug fixes

- remctld now always closes the client connection after low-level errors reading or sending tokens. Previously, it would attempt to continue after some socket or GSS-API errors, which may have caused hanging remctld processes in some circumstances.
- Fix remctld segfault when the configuration does not define any commands. Thanks to Andrew Mortensen for the report.
- Fix GSS-API header probes when configure was told to build with a specific GSS-API library in a non-default path. Previously, configure still used the compiler to probe for the correct header names, which could pick up incorrect headers from the default include path. Thanks to Jeffrey Hutzelman for the suggested solution.
- Solaris can return `ECONNRESET` instead of `EPIPE` on write when the other end of the network connection closes it. Handle that error properly in the remctld server. Patch from Jeffrey Hutzelman.
- Fix multiple portability issues in the test suite on Solaris and old versions of Heimdal. Thanks to Jeffrey Hutzelman for the series of patches.

<a id='changelog-3.1'></a>

## 3.1 (2012-02-29)

### New features

- Add new `remctl_set_timeout` function to the remctl library API and the Perl, PHP, Python, and Ruby bindings. Call this function any time after `remctl_new` to set a network timeout in seconds for all subsequent operations. The client must then receive a reply from the server in no more than that number of seconds or will abort whatever action is in progress with a timeout error. The timeout also applies to the initial connection if `remctl_set_timeout` is called before `remctl_open`.
- The remctld server now supports an additional configuration option, `user`, which sets the user as which to run a command. If this option is set for a command configuration, remctld will run the command as that user (including their primary and supplemental groups). The user may be specified as either a username or a UID. Patch from Andrew Mortensen.
- The remctld server now imposes a one-hour timeout between messages from the client rather than a one-hour limit on the entire session, allowing clients to continue to send commands for as long as they stay connected and not idle.

### Bug fixes

- The PHP bindings no longer output a PHP warning if `remctl_output` fails. This was inconsistent with the other API calls (`remctl_open` and `remctl_command` can also fail but didn't result in warnings), may be expected and handled by the caller, and made testing difficult.
- Fix error reporting for non-blocking connect.

### Other changes

- The internal `_remctl.remctl_output` function in the Python bindings now returns an empty tuple on error instead of a bool. This change will not affect callers that only use the recommended public remctl interface.

<a id='changelog-3.0'></a>

## 3.0 (2011-10-31)

### New features

- New protocol version 3, which introduces a new `NOOP` message. When the client sends this message, the server replies with a `NOOP` message. This can be used to keep a persistent remctl connection alive despite network session timeouts. Add new `remctl_noop` function to the remctl library API and the Perl, PHP, Python, and Ruby bindings to send the `NOOP` message and read the response.
- Be more explicit in the protocol about handling of continuation commands. Do not allow any messages from the client after a continued command except the continuation of that command or a `QUIT` message. Explicitly document that a `QUIT` message abandons the partial command. Add the new `ERROR_UNEXPECTED_MESSAGE` error code, used when the client sends incorrect messages during a command continuation.
- The server no longer closes the connection after version or error replies. The connection will now stay open until `MESSAGE_COMMAND` is sent without keepalive or `MESSAGE_QUIT` is sent.
- Add new `remctl_set_source_ip` function to the remctl library API and the Perl, PHP, Python, and Ruby bindings. Call this function after `remctl_new` and before `remctl_open` to set the source IP address that will be used for subequent client connections to a remctl server. For the Ruby bindings, this is implemented as the `source_ip` class variable rather than a separate method.
- Add new `-b` option to the remctl command-line client to specify the source IP for client connections.
- Add new `remctl_set_ccache` function to the remctl library API and the Perl, PHP, Python, and Ruby bindings. Call this function after `remctl_new` and before `remctl_open` to set the Kerberos credential cache that will be used for client authentication, overriding `KRB5CCNAME`. Be aware that this will normally change the default credential cache for all other GSS-API operations in this context or thread, not just for that remctl object, due to GSS-API limitations. For the Ruby bindings, this is implemented as the `ccache` class variable rather than a separate method.

### Bug fixes

- In the client, only check the negotiated GSS-API context flags after the context has been fully established. Current versions of Heimdal, including the system Kerberos libraries in Mac OS X Lion, only declare mutual authentication once the context negotiation is complete.
- Close a client memory leak caused by the GSS-API context not being freed by the client in `remctl_close`.
- When calling `remctl_open` on an existing struct remctl object, send `QUIT` to the server if a connection is already open.
- remctld can be configured to pass the subcommand on standard input, but the documentation said this was not allowed. Fix the documentation to match the implementation.

### Other changes

- Use `PATH_KRB5_CONFIG` as the environment variable to set the path to `krb5-config` rather than `KRB5_CONFIG` when running configure, since the latter is used by the Kerberos libraries to specify an alternative path to `krb5.conf`.

<a id='changelog-2.18'></a>

## 2.18 (2011-05-31)

### Bug fixes

- Fix uninitialized variable in the remctld standalone server code that could cause all remote connections to fail and add a more complete test suite for remote address handling.

<a id='changelog-2.17'></a>

## 2.17 (2011-05-31)

### New features

- The remctld server now supports a `-b` command-line option specifying which local addresses to which to bind. This option may be given multiple times to bind to multiple local addresses.
- When run as a standalone daemon, remctld now binds to both IPv4 and IPv6 addresses rather than only IPv4.
- The remctl client library also installs a `pkg-config` configuration file for the use of software that wants to link against it. Thanks to Tollef Fog Heen for the assistance in writing it.
- Symbol versioning is now enabled on any system using GNU ld, rather than only Linux and related platforms, and a Libtool symbol list is used as a fallback to prevent leaking symbols with other linkers where possible.

### Bug fixes

- Fix construction of the return object for the Python bindings to the simple remctl interface. Patch from Andrew Mortensen.
- Remove reference to the defunct `messages-die.c` source file in the Windows build system.
- Fix broken GCC attribute markers causing problems with compilation on Windows (and likely any non-GCC compiler).
- Set the PHP extension test suite to be noninteractive so that the user is not prompted to send results to the PHP QA group.
- Skip `portable/getaddrinfo` test on systems where invalid hostnames still resolve.
- Check for `krb5-config` in `/usr/kerberos/bin` as well as `PATH`.

<a id='changelog-2.16'></a>

## 2.16 (2010-05-02)

### New features

- Add Ruby bindings contributed by Anthony M. Martinez, enabled with `--enable-ruby` at configure time. These bindings are tested with Ruby 1.8 and may not work with older versions. See `ruby/README` for more information.
- remctld now includes support for a PCRE (Perl-compatible regular expressions) ACL type if the PCRE library is found at configure time. A PCRE ACL matches any user whose identity matches the given Perl-compatible regular expression. Based on work contributed by Anton Lundin.
- remctld now includes support for a POSIX regex ACL type if the system supports the POSIX regex API. A regex ACL matches any user whose identity matches the given POSIX extended regular expression. Based on work contributed by Anton Lundin.
- remctld now sets the environment variable `REMCTL_COMMAND` to the command (not subcommand or arguments) that causes a program to be run. Thanks, Thomas L. Kula.
- `remctld -h` now reports the list of supported ACL methods for that build of remctld.
- Add an example SMF manifest for the remctld daemon in `examples/remctld.xml`. Contributed by Peter Eriksson.

### Bug fixes

- Fix PHP test suite to work with PHP 5.3, which no longer passes environment variables down to the running test program.
- Stop passing GCC-specific warning suppression flags into the language binding build systems unless the compiler used to build remctl is GCC. This still isn't quite right, since the language bindings may use a different compiler than the main remctl build, but it should be closer than the previous behavior of using GCC flags unconditionally.

<a id='changelog-2.15'></a>

## 2.15 (2009-11-29)

### New features

- Allow subcommand to be omitted on the remctl command line, which sends a command without a subcommand. This makes available on the command line functionality that was already available via the library API.
- Add the special keyword `EMPTY` for the subcommand field in the remctld configuration file, specifying that this line should only match commands with no subcommands.
- Allow use of `ALL` in the command field in the remctld configuration file as well as the subcommand field, matching all commands.

### Bug fixes

- Fix read of uninitialized memory caused by moving one character beyond the beginning of the buffer when parsing blank lines in ACL files.
- Use a `socket_type` typedef rather than `int` directly to store the file descriptors of sockets and, on Windows, typedef that to `SOCKET` instead of `int`. Update the function signatures of the network utility functions appropriately. Compare `socket_type` variables against an `INVALID_SOCKET` define instead of -1. Fixes portability issues to 64-bit Windows. Thanks, Jeffrey Altman.
- For the Windows build, get the current version number from `configure.ac` rather than `configure` so that the Windows build scripts work from a Git checkout. Link with the correct GSS-API library for 64-bit Windows builds. Correct or suppress multiple warnings. Thanks, Jeffrey Altman.
- Fall back on manual library probing if `krb5-config` doesn't work.
- Don't try to use a non-executable `krb5-config` for GSS-API probes.
- Prefer `KRB5_CONFIG` over a path constructed from `--with-gssapi`.

### Other changes

- Enable Automake silent rules. For a quieter build, pass the `--enable-silent-rules` option to configure or build with `make V=0`.

<a id='changelog-2.14'></a>

## 2.14 (2009-05-22)

### New features

- The remctld configuration file may now specify that one argument to a command is passed on standard input instead of on the command line using the `stdin` option. This option allows passing data to commands that's too long to fit into a command-line argument or that contains nul characters.
- The protocol now permits commands with no arguments. remctld currently doesn't support them, but now returns `ERROR_UNKNOWN_COMMAND` instead of `ERROR_BAD_COMMAND` when receiving one.
- Support building against Solaris 10's native generic GSS-API libraries. Thanks, Peter Eriksson.

### Bug fixes

- remctld logging of commands or arguments now replaces unprintable characters (characters between ASCII 0 and 31 and ASCII 127) with periods rather than assuming syslog will cope with them correctly.
- Declare `message_fatal_cleanup` extern in `util.h`. Fixes compilation problems on Mac OS X and probably elsewhere.
- Diagnose and explicitly reject on the server nul characters in command arguments that don't support them rather than truncating the argument silently.
- Plug several memory leaks in the remctld server. (These would have little practical effect unless a client stayed connected and issued multiple commands.)
- Better check `logmask` options when parsing the server configuration file and report errors instead of silently ignoring them. Masking the command is also no longer supported (it previously worked by accident).
- Correctly set `-L` options with `--with-gssapi-lib`, not `-I`.

### Other changes

- Use command and subcommand as the names for the first two parameters to the remctl client and the first two strings in a remctl command instead of the unintuitive "type" and "service" terminology borrowed from sysctl. This only changes documentation and some internal variable names; no external APIs should be affected.
- Add documentation on extending remctl in `docs/extending`.
- Add initial protocol version three draft in `docs/protocol-v3`.

<a id='changelog-2.13'></a>

## 2.13 (2008-11-14)

### New features

- Add support for ACL methods in the remctld server. The supported schemes in this release are `file` and `princ`, which together provide the same functionality as earlier releases, plus `deny` to explicitly reject a user who matches another ACL and support for the CMU GPUT authorization system. There is now a framework in place for adding new ACL methods in the future. This work was contributed by Jeffrey Hutzelman.
- When processing the include of a directory for configuration files or ACL files, limit the files read to those whose names contain only characters in `[a-zA-Z0-9_-]`. This replaces the previous exclusion of files containing periods and also excludes Emacs backup and temporary files. Thanks, Timothy G. Abbott.
- Add a PHP remctl PECL module from Andrew Mortensen, enabled with `--enable-php` at configure time. These bindings are only tested with PHP 5.
- Add Python bindings from Thomas L. Kula, enabled with `--enable-python` at configure time. These bindings are tested with Python 2.5 but should work with versions back to 2.3.
- Add an ant build configuration for the Java remctl implementation. It also has the capability to generate a distribution of just the Java implementation using a file layout more similar to an Apache Jakarta project than the layout of the `java` subdirectory.
- Add `--with-gssapi-include` and `--with-gssapi-lib` options to set the include and library paths separately if needed.

### Bug fixes

- Include all `*.class` files in the JAR file built by `java/Makefile`, making the resulting JAR actually useful. Thanks, Marcus Watts.
- Several Windows fixes from Matthew Loar, plus really include `portable/winsock.c` in the distribution. This version should now build and run on Windows.
- With `--with-gssapi`, attempt to determine if the library directory is `lib32` or `lib64` instead of `lib` and set `LDFLAGS` accordingly. Based on an idea from the CMU Autoconf macros.
- Restore GSS-API portability checks for old versions of MIT Kerberos accidentally dropped in the previous release.
- Provide a proper bool type when built with Sun Studio 12 on Solaris 10. Thanks, Jeffrey Hutzelman.
- Sanity-check the results of `krb5-config` before proceeding and error out in configure if they don't work.
- Fix Autoconf syntax error when probing for libkrb5support. Thanks, Mike Garrison.
- Create the docs directory in the build tree if it's missing, fixing a build failure when builddir != srcdir. Thanks, Jeffrey Hutzelman.
- In standalone mode, close the main server socket immediately in the child handler processes. Since the socket was already marked close on exec, this probably only matters for consistent test suite results, ensuring that the port is released immediately, but it's more correct.

<a id='changelog-2.12'></a>

## 2.12 (2008-04-04)

### Backwards-incompatible changes

- If no server principal is specified on the remctl command line or in the `remctl()` or `remctl_open()` C or Perl library interfaces, remctl now uses a host-based service name for the server instead of a Kerberos principal of host/server. The practical effect of this is that domain-realm mapping rules will be applied rather than assuming the server's principal is in the local domain and, for the C and Perl library interfaces, server name canonicalization will be done if configured in the GSS-API library. Users of the C or Perl library interfaces will find that remctl now authenticates to a principal for the host after a forward and reverse DNS lookup instead of the host specified in the API call with most GSS-API libraries. To disable this canonicalization behavior, see your GSS-API library documentation; setting `rdns` in `[libdefaults]` to false works for MIT Kerberos. The remctl command-line client continues to canonicalize its host argument always prior to any network connection or GSS-API calls.

### New features

- Include the Windows port of the client done by Matthew Loar. See `README` for information on requirements and compilation. Only the client shared library and command-line utility are supported or built currently. I cannot easily test this code and probably broke it when integrating the patch; please report any problems so that they can be fixed in subsequent releases.
- The configure option to specify the path to the GSS-API libraries is now `--with-gssapi` instead of `--with-kerberos` and the GSS-API probes should be more robust.

### Bug fixes

- Fix a place in libremctl where the library would call exit rather than returning an error on memory allocation failure.
- When running the server in standalone mode, set the network file descriptors close-on-exec so that they're not inherited by commands run by remctl. Also close the low-numbered file descriptors before running a command to catch the replay cache file, which isn't marked close-on-exec in older versions of MIT Kerberos.
- When passing a variable set to undef into `remctl_open` in the Perl API, the principal was converted to the empty string. Adjust Net::Remctl to recognize the empty string as an unspecified principal.
- Delete the man page symlinks before recreating them so that reinstalls work. Thanks, Nicholas Riley.
- Belatedly bump the libtool versioning for libremctl for the port number change in the previous release. (This is primarily for documentation purposes and doesn't change the library SONAME.)

### Other changes

- Add documentation of hostname canonicalization and the choice of authentication principals to the remctl client, `remctl()` and `remctl_open()` C API, and Net::Remctl Perl API documentation.
- Standardize on lowercase first characters in library error strings.

<a id='changelog-2.11'></a>

## 2.11 (2007-11-09)

### Backwards-incompatible changes

- remctl now has an official port registered with IANA (4373), replacing the original, poorly-chosen port of 4444. The previous port conflicts with the krb524 service. The remctld server and example configuration files have been changed to bind to port 4373 by default if no port is specified. The client will attempt to connect to port 4373 first if no port is specified and then fall back to trying 4444. All sites running remctl are encouraged to upgrade their clients and then migrate their servers to the new port. Support for the old port without explicit configuration will be phased out in a future release.

### New features

- Port to the Kerberos GSS-API implementation shipped with AIX 5.2. Thanks to Sandor Sklar for bug reports and testing.

### Bug fixes

- Stop using `stdout` and `stderr` as structure members, fixing compilation problems on AIX, NetBSD, and other platforms.
- Fix (non-exploitable) segfaults in remctld when sent a command with a type and no service (not permitted by the command-line client but possible with the library API). Thanks to Marcus Watts for the analysis.

### Other changes

- Improve the configuration file documentation in the remctld man page. Document the first-match properties.

<a id='changelog-2.10'></a>

## 2.10 (2007-08-26)

### New features

- Include a rewritten Java client and a Java server implementation, both by Marcus Watts. The rewritten Java client supports protocol version two and works with Sun Java 1.4.2, 5, and 6.
- The remctl client now also requests sequence protection, but the client and server do not insist on it or on replay protection since Heimdal 0.6 doesn't support replay protection. This has been documented in the protocol specification as well.
- remctld when running in stand-alone mode now removes the PID file (if any) and exits cleanly after receiving `SIGINT` or `SIGTERM`. Based on a patch by Marcus Watts.
- remctld when running in stand-alone mode now re-reads its configuration file file after receiving a `SIGHUP`.
- The libremctl client library now uses symbol versioning on Linux.

### Bug fixes

- Fix a (non-exploitable) remctld crash when the client sent more command arguments than it claimed it was going to send. Thanks, Marcus Watts. Also add a test with a variety of malformed command tokens in an effort to keep bugs like this from going unnoticed in the future.
- Don't self-destruct after an hour in stand-alone mode, fixing a bug introduced in 2.8.
- Allow port and principal to be omitted in calls to Net::Remctl::open, matching the documentation. Thanks, Marcus Watts.
- Include a dummy symbol in libportable so that it always contains at least one object. Fixes compilation problems on Mac OS X 10.4 and Solaris 10.
- Fix builds outside the source directory by creating the docs directory properly, based on a patch by Marcus Watts. Also fix `make clean` and the POD tests when run outside the source directory.
- Check for the MIT Kerberos GSS-API library first in reduced dependency mode for improved reproducibility of the Debian build.

### Other changes

- Change the Net::Remctl documentation for `remctl()` to suggest 0 and the empty string as default values for port and principal, since this avoids Perl warnings.

<a id='changelog-2.9'></a>

## 2.9 (2007-06-29)

### New features

- Added complete C API documentation (as section 3 manual pages) for the libremctl library.

### Bug fixes

- Fix remctl client library crashes due to an uninitialized variable when the network connection fails.
- Fix several inaccuracies in the Net::Remctl API documentation. Thanks, Alf Wachsmann.
- Pass DESTDIR to the Perl module installation as well. Thanks, Darren Patterson.

<a id='changelog-2.8'></a>

## 2.8 (2007-06-27)

### New features

- Add a Net::Remctl Perl module, optionally compiled (and enabled with the `--enable-perl` configure flag), that provides native Perl bindings to the libremctl client library.
- When running in stand-alone mode, remctld now forks a new child for each incoming connection and can therefore handle multiple simultaneous connections. This makes stand-alone mode useful for more than just testing. Also, remctld now backgrounds itself by default in stand-alone mode; disable this with the `-F` flag. Based on a patch by Andrew Mortensen.
- Add a new `-k` flag to remctld to tell it to use a non-default keytab. Thanks, Andrew Mortensen.

### Bug fixes

- Fix various null pointer dereferences in the simplified remctl client library call when the server returns an error.
- Default to port 4444 in the library if a port of 0 is passed in, and (following the documentation) default to `host/<hostname>` if a NULL principal is passed in.
- remctld now exits properly when it can't parse its configuration file rather than proceeding with a null configuration.
- Fix problems with the parameter types for GSS-API memory freeing functions in some error cases.

<a id='changelog-2.7'></a>

## 2.7 (2007-03-25)

### Bug fixes

- In remctld, consider the command complete once the child process exits. Do not wait for its standard output and error to be closed, since the child process may have spawned a long-running daemon that doesn't clean up its file descriptors properly.
- When the command-line remctl client canonicalizes the name of the server host to get the right principal, it then needs to connect to the canonical hostname. Otherwise, DNS schemes that return a different answer each time one asks for a given host may cause remctl to connect to a different host than the canonical name used for the principal, resulting in authentication failure.
- Fixed a subtle bookkeeping error when sending commands larger than the maximum token size that would have resulted in malformed tokens for boundary cases of argument lengths.
- Fixed memory and file descriptor leaks in remctld that only become apparent when the server runs many commands before exiting.
- Various minor fixes so that `make warnings` and `make check` work on a Solaris 8 system without IPv6 configured.

<a id='changelog-2.6'></a>

## 2.6 (2007-02-03)

### Bug fixes

- SECURITY: If an ACL listed for a command didn't exist, the authorization check was treated as a success instead of a failure. This had, embarassingly, apparently been broken since at least 2.0.

<a id='changelog-2.5'></a>

## 2.5 (2007-02-03)

### New features

- Automatically use a continued `MESSAGE_COMMAND` if the total command length is larger than 64KB (minus token overhead). The remctl client library can now send arbitrarily large commands, at some cost in memory consumption on the client and server. The server is still limited by the OS-imposed maximum length of a command line.
- When the server runs a command, open `/dev/null` for standard input rather than leaving standard input closed. Some programs don't cope with a closed standard input.
- Use the same limit (1MB) on token size everywhere. Enforce the protocol limit on unencrypted data size (64KB) in both the server and when sending messages in the client.

### Bug fixes

- Audited memory handling of buffers sent to and read from the network and closed several memory leaks.
- Correctly handle a zero-length argument at the end of a command in the server. Previously, that argument was ignored.
- Check that the expected argument count matches the count of arguments seen in the server and that all of the client data was consumed when parsing arguments.
- Add a newline to the end of error messages when converting to protocol version one replies. The old remctl client didn't add a newline.

### Other changes

- Document the limits on token size and unencrypted data size in the protocol specification. Improve the protocol documentation for the continue status for `MESSAGE_COMMAND`. Use octet instead of byte uniformly.

<a id='changelog-2.4'></a>

## 2.4 (2007-01-17)

### New features

- IPv6 support is now automatically enabled on systems that support it. The remctl code uniformly uses the new IPv6-aware host and address functions, using replacements on systems that don't provide them in libc. Thanks to Jonathan Kollasch for the initial patch.
- Add error messages to the protocol specification for sending too many arguments in a command and sending too much data with a command. Return the more specific error message if the number of command arguments exceed the current hard-coded limit rather than just reporting a bad command token.

### Bug fixes

- When sending tokens, correctly check for network errors rather than ignoring them due to a miswritten test.
- In the remctl command-line client, print a newline after protocol error messages from the server.
- Don't use `$<` in non-pattern rules (again), fixing a build error on some systems with non-GNU make (although since the generated man pages are part of the distribution, only those modifying the POD source would have seen this error).

<a id='changelog-2.3'></a>

## 2.3 (2006-12-06)

### New features

- Increase the maximum number of arguments the server will accept for a command to 4096 from 64. This is an arbitrary limit to protect against memory-consumption denial-of-service attacks.
- Add the `-S` flag to remctld, which tells it to log to standard output and standard error rather than syslog. Use this flag in the test suite so that make check doesn't spew into a system's syslog.

### Bug fixes

- Require Automake 1.10 and Autoconf 2.60 and use `AC_CONFIG_LIBOBJ_DIR` to locate replacements for missing system functions. This means that an Automake patch is no longer required for bootstrapping and remctl will now work with stock Autoconf and Automake.

### Other changes

- Document the exit status of the remctl client.

<a id='changelog-2.2'></a>

## 2.2 (2006-09-08)

### Bug fixes

- Add appropriate casts when passing `size_t` variables to printf on 64-bit systems.
- Include `sys/socket.h` in appropriate places for `socklen_t` on Solaris.
- Work around strange GCC 4.1 behavior on AMD64 that creates a const temporary variable in the macro expansion of the W* wait macros on glibc systems, causing the build of runtests to fail. For some reason this apparently only affects AMD64.

<a id='changelog-2.1'></a>

## 2.1 (2006-08-22)

### Backwards-incompatible changes

- Stop setting `SCPRINCIPAL` in the environment. This was for backward compatibility with sysctl and it's highly unlikely that anyone still cares (not to mention that the value was qualified with the realm and therefore didn't match sysctld's setting anyway).

### New features

- Set `REMOTE_USER` in the environment for commands run by remctld, using the same value as `REMUSER`. This makes it easier to use programs that also run as CGI scripts. Also set `REMOTE_ADDR` to the IP address of the remote host and set `REMOTE_HOST` to the hostname if available.
- Support `make check` with builddir != srcdir builds. Thanks to Ralf Wildenhues for the help in identifying the issues.

### Bug fixes

- Properly nul-terminate error replies when using the simplified remctl client API.

<a id='changelog-2.0'></a>

## 2.0 (2006-08-09)

### New features

- Implement a new version 2 protocol, with automatic down-negotiation to the old protocol for backward compatibility. The new protocol is more binary-safe for command arguments, supports streaming output from the server, allows distinguishing between stdout output and stderr output, has no arbitrary limits on output size, and supports persistant connections.
- Document the details of the remctl protocol, both the old version 1 protocol and the new version 2 protocol, in hopefully sufficient detail for anyone else to implement it.
- Add the `-P` flag to remctld to write its PID to a file when invoked in stand-alone mode.
- Add an automated test suite.

### Bug fixes

- Don't consider inclusion of empty directories in a configuration file an error.
- Don't use `$<` in non-pattern rules, fixing a build error on some systems with non-GNU make.

### Other changes

- Completely rewrite the build system to use Automake, a supporting utility library, separate subdirectories for different parts of the source tree, and a wrapper include file for system headers.

<a id='changelog-1.12'></a>

## 1.12 (2006-01-01)

### Bug fixes

- Initialize memory properly when parsing the server configuration file.
- Library probes with `--enable-static` cannot use `krb5-config`, since we can't distinguish between the Kerberos libraries that should be static and the system library dependencies that must not be made static.

<a id='changelog-1.11'></a>

## 1.11 (2005-12-22)

### New features

- Support include directives in remctld ACL files with the same syntax and semantics as include directives in configuration files.
- Stop option parsing at the first non-option on Linux (this is the standard behavior of getopt on other platforms). Otherwise, calling remote programs that take options is annoying.
- Use `krb5-config` where available to get Kerberos libraries and compiler flags unless `--enable-reduced-depends` is used.
- Initial port to Heimdal. remctl now compiles but isn't able to talk to a server built with MIT Kerberos, so further porting is still needed.

### Bug fixes

- Fix builds and installs where builddir != srcdir.
- Remove some debugging code for displaying the GSS-API OID as a string that isn't supported by the Heimdal API and is of questionable usefulness regardless.

<a id='changelog-1.10'></a>

## 1.10 (2005-12-01)

### Backwards-incompatible changes

- Move the `-v` option to remctl and remctld to `-d` (debug), since the verbose output or logging is only really useful when debugging.

### New features

- Add `-h` (show usage) and `-v` (show version) options to both remctl and remctld and add real option parsing (so combining multiple options in one switch should now work).
- Overhaul error and status reporting in remctl and remctld. Among other advantages, this should eliminate any lingering format string worries and get rid of the trailing newlines in syslog messages from remctld, as well as regularize the text of the error messages and the priority of syslog messages.

<a id='changelog-1.9'></a>

## 1.9 (2005-05-10)

### Bug fixes

- Fix serious bug with inclusion of configuration directories. When reading any file after the first, remctl would use random bits of memory as the file name.

<a id='changelog-1.8'></a>

## 1.8 (2005-05-04)

### Backwards-incompatible changes

- Change the default `remctl.conf` location to be relative to sysconfdir (`<prefix>/etc` by default) instead of the current directory.

### New features

- Support `include <file>` in the configuration file. Also support including a directory, which includes every file in that directory that doesn't have a period in the name.
- Support continuation lines (using backslash) in the configuration file, and clean up the parser to be more flexible about whitespace on otherwise empty lines or comment lines.
- remctld now only logs the initial connection authentication and the argument count if `-v` was given, reducing to one the number of syslog messages per command.

### Other changes

- Improve the remctld man page, documenting all of the supported options including stand-alone mode.

<a id='changelog-1.7'></a>

## 1.7 (2005-02-22)

### Bug fixes

- Close extra file descriptors before spawning a child process in remctl. The only file descriptors open should be standard output and standard error. This will fix problems with using remctld to start long-running daemons; before, remctld would never realize that the child process had exited.
- Use select to wait for child output in remctld rather than busy-waiting so as not to burn CPU cycles when the child takes a while to produce output.

### Other changes

- Document the `-p` option for the client.

<a id='changelog-1.6'></a>

## 1.6 (2004-05-18)

### Bug fixes

- Fix format string vulnerabilities when logging the remote command.

<a id='changelog-1.5'></a>

## 1.5 (2004-03-04)

### Bug fixes

- Fix a bug in remctld where it would segfault when trying to check the ACLs for a command not present in the configuration file.
- Portability fix to return the exit status of the command in network byte order.

<a id='changelog-1.4'></a>

## 1.4 (2003-11-12)

### New features

- Add support for a `logmask=n` option in the configuration file that masks those arguments in the logging output (used when some of the options for that command contain private information).
- Add optimizations in the GSS code to do fewer network writes.
- Significant improvements to the Java client.
- Some minor cleanups to logging, installation, and the configure script.

<a id='changelog-1.3'></a>

## 1.3 (2003-07-21)

### New features

- Adjust logging priorities and include some additional information in the log of the command.

### Bug fixes

- Exit with non-zero status if the remote command failed rather than always exiting with zero status if the network exchange worked successfully.

### Other changes

- Improved the `README` and added a `make dist` target to the makefile.

<a id='changelog-1.2'></a>

## 1.2 (2003-04-04)

### New features

- Set the `REMUSER` environment variable to the remote authenticated user (and continue setting `SCPRINCIPAL` as well for backward compatibility).

### Bug fixes

- Read from both standard out and standard error of the spawned command in turn to better prevent deadlock.

<a id='changelog-1.1'></a>

## 1.1 (2003-02-28)

### New features

- Additional fleshing out of the Java client.

### Bug fixes

- Add an snprintf implementation for systems that don't have it and use it for log messages.
- Lots of code cleanup and style fixes.

<a id='changelog-1.0'></a>

## 1.0 (2002-11-22)

Initial release.
