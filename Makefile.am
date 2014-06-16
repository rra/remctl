# Automake makefile for remctl.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.

# These variables exist only for the use of the Debian packaging and similar
# situations and aren't normally set.  We want to honor them if they're set
# in the environment, as well as via make arguments.
#
# REMCTL_PERL_FLAGS are additional flags to pass to Makefile.PL when
# building the Makefile.
#
# REMCTL_PYTHON_INSTALL adds additional flags to the setup.py install
# command.  REMCTL_PYTHON_VERSIONS, if set, is a space-separated list of
# Python interpretors with which to build the Python extension.  The
# extension will be built, tested, and installed with each one.  If not set,
# it will only be built with python (without any version extension).
#
# REMCTL_RUBY controls which Ruby interpretor is used to do the build.
# REMCTL_RUBY_FLAGS are additional flags to pass to extconf.rb when building
# the Makefile.
REMCTL_PERL_FLAGS      ?=
REMCTL_PYTHON_INSTALL  ?=
REMCTL_PYTHON_VERSIONS ?= python
REMCTL_RUBY            ?= ruby
REMCTL_RUBY_FLAGS      ?=

# These three lists of files are needed for Perl builds, Python builds, and
# for the test suite and are not generated or touched by configure.  They're
# listed here to be added to EXTRA_DIST and so that they can be copied over
# properly for builddir != srcdir builds.
PERL_FILES = perl/Build.PL perl/MANIFEST perl/MANIFEST.SKIP		   \
	perl/lib/Net/Remctl.pm perl/lib/Net/Remctl.xs			   \
	perl/lib/Net/Remctl/Backend.pm perl/t/api/basic.t		   \
	perl/t/api/stanford-netdb.t perl/t/backend/basic.t		   \
	perl/t/backend/nested.t perl/t/backend/options.t		   \
	perl/t/data/perl.conf perl/t/data/perlcriticrc			   \
	perl/t/data/perltidyrc perl/t/data/remctl.conf			   \
	perl/t/docs/pod-coverage.t perl/t/docs/pod-spelling.t		   \
	perl/t/docs/pod.t perl/t/docs/synopsis.t perl/t/lib/Test/Remctl.pm \
	perl/t/style/coverage.t perl/t/style/critic.t			   \
	perl/t/style/minimum-version.t perl/t/style/strict.t perl/typemap
PHP_FILES = php/README php/php_remctl.c php/test-wrapper php/tests/001.phpt \
	php/tests/002.phpt php/tests/003.phpt php/tests/004.phpt	    \
	php/tests/005.phpt php/tests/006.phpt
PYTHON_FILES = python/MANIFEST.in python/README python/_remctlmodule.c
RUBY_FILES = ruby/README ruby/remctl.c

# Directories that have to be created in builddir != srcdir builds before
# copying PERL_FILES over.
PERL_DIRECTORIES = perl perl/lib perl/lib/Net perl/lib/Net/Remctl perl/t \
	perl/t/api perl/t/backend perl/t/data perl/t/docs perl/t/lib	 \
	perl/t/lib/Test perl/t/style

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = .gitignore LICENSE Makefile.w32 autogen client/libremctl.pc.in \
	client/libremctl.map client/libremctl.rc client/libremctl.sym	    \
	client/remctl.rc config.h.w32 configure.cmd docs/api/remctl.pod	    \
	docs/api/remctl_close.pod docs/api/remctl_command.pod		    \
	docs/api/remctl_error.pod docs/api/remctl_new.pod		    \
	docs/api/remctl_noop.pod docs/api/remctl_open.pod		    \
	docs/api/remctl_output.pod docs/api/remctl_set_ccache.pod	    \
	docs/api/remctl_set_source_ip.pod docs/api/remctl_set_timeout.pod   \
	docs/design.html docs/extending docs/protocol-v4 docs/protocol.txt  \
	docs/protocol.html docs/protocol.xml docs/remctl.pod		    \
	docs/remctld.8.in docs/remctld.pod examples/remctl.conf		    \
	examples/remctld.xml examples/rsh-wrapper examples/xinetd	    \
	java/.classpath java/.project java/Makefile java/README		    \
	java/bcsKeytab.conf java/gss_jaas.conf java/j3.conf java/k5.conf    \
	java/org/eyrie/eagle/remctl/Remctl.java				    \
	java/org/eyrie/eagle/remctl/RemctlClient.java			    \
	java/org/eyrie/eagle/remctl/RemctlServer.java java/t5.java	    \
	java/t7.java php/remctl.ini portable/winsock.c remctl.spec	    \
	server/README systemd/remctld.service.in systemd/remctld.socket	    \
	tests/HOWTO tests/TESTS tests/client/remctl-t tests/config/README   \
	tests/data/acl-bad-include tests/data/acl-bad-syntax		    \
	tests/data/acl-nonexistant tests/data/acl-recursive		    \
	tests/data/acl-simple tests/data/acl-too-long			    \
	tests/data/acl-valid-3 tests/data/acls/VALID_09			    \
	tests/data/acls/val\#id tests/data/acls/val.id			    \
	tests/data/acls/valid tests/data/acls/valid-2			    \
	tests/data/acls/val~id tests/data/acls2/valid-4 tests/data/cmd-argv \
	tests/data/cmd-env tests/data/cmd-hello tests/data/cmd-help	    \
	tests/data/cmd-sleep tests/data/cmd-status			    \
	tests/data/conf-nosummary tests/data/conf-simple		    \
	tests/data/conf-test tests/data/configs/bad-logmask-1		    \
	tests/data/configs/bad-include-1 tests/data/configs/bad-logmask-2   \
	tests/data/configs/bad-logmask-3 tests/data/configs/bad-logmask-4   \
	tests/data/configs/bad-option-1 tests/data/configs/bad-user-1	    \
	tests/data/perl.conf tests/data/gput tests/data/valgrind.supp	    \
	tests/docs/pod-spelling-t tests/docs/pod-t			    \
	tests/perl/module-version-t tests/tap/kerberos.sh		    \
	tests/tap/libtap.sh tests/tap/perl/Test/RRA.pm			    \
	tests/tap/perl/Test/RRA/Automake.pm				    \
	tests/tap/perl/Test/RRA/Config.pm tests/tap/remctl.sh		    \
	tests/server/misc-t tests/util/xmalloc-t $(PERL_FILES) $(PHP_FILES) \
	$(PYTHON_FILES) $(RUBY_FILES)

# Override systemdsystemunitdir when running distcheck so that we don't try
# to install files in the system directory.  It would be nice to also enable
# all the language binding integrations here, but they don't play with
# installcheck or changing prefix (but not DESTDIR) during install.
DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

# Set this globally, since we have too many header files that include the
# GSS-API headers even if the code itself doesn't call GSS-API functions.
AM_CPPFLAGS = $(GSSAPI_CPPFLAGS)

if HAVE_LD_VERSION_SCRIPT
    VERSION_LDFLAGS = -Wl,--version-script=${srcdir}/client/libremctl.map
else
    VERSION_LDFLAGS = -export-symbols ${srcdir}/client/libremctl.sym
endif

# Supporting convenience libraries used by other targets.
noinst_LTLIBRARIES = portable/libportable.la util/libutil.la
portable_libportable_la_SOURCES = portable/dummy.c portable/event.h	\
	portable/getaddrinfo.h portable/getnameinfo.h portable/getopt.h	\
	portable/gssapi.h portable/krb5.h portable/macros.h		\
	portable/sd-daemon.h portable/socket.h portable/stdbool.h	\
	portable/system.h portable/uio.h
portable_libportable_la_LIBADD = $(LTLIBOBJS)
util_libutil_la_SOURCES = util/fdflag.c util/fdflag.h util/gss-errors.c	    \
	util/gss-errors.h util/gss-tokens.c util/gss-tokens.h util/macros.h \
	util/messages.c util/messages.h util/network.c util/network.h	    \
	util/protocol.h util/tokens.c util/tokens.h util/vector.c	    \
	util/vector.h util/xmalloc.c util/xmalloc.h util/xwrite.c	    \
	util/xwrite.h
util_libutil_la_LDFLAGS = $(GSSAPI_LDFLAGS)
util_libutil_la_LIBADD = $(GSSAPI_LIBS)

# If built with Kerberos support, add messages-krb5.
if HAVE_KRB5
    util_libutil_la_SOURCES += util/messages-krb5.c util/messages-krb5.h
    util_libutil_la_CPPFLAGS = $(KRB5_CPPFLAGS)
    util_libutil_la_LDFLAGS += $(KRB5_LDFLAGS)
    util_libutil_la_LIBADD += $(KRB5_LIBS)
endif

# The remctl client library.
lib_LTLIBRARIES = client/libremctl.la
client_libremctl_la_SOURCES = client/api.c client/client-v1.c \
	client/client-v2.c client/error.c client/internal.h client/open.c
client_libremctl_la_LDFLAGS = -version-info 2:0:1 $(VERSION_LDFLAGS) \
	$(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS)
client_libremctl_la_LIBADD = util/libutil.la portable/libportable.la \
	$(GSSAPI_LIBS) $(KRB5_LIBS)
include_HEADERS = client/remctl.h

# pkg-config configuration for the library.
pkgconfigdir = $(libdir)/pkgconfig
nodist_pkgconfig_DATA = client/libremctl.pc

# Substitute configured paths into the pkg-config configuration file.
client/libremctl.pc: $(srcdir)/client/libremctl.pc.in
	sed -e 's![@]prefix[@]!$(prefix)!g'			\
	    -e 's![@]exec_prefix[@]!$(exec_prefix)!g'		\
	    -e 's![@]includedir[@]!$(includedir)!g'		\
	    -e 's![@]libdir[@]!$(libdir)!g'			\
	    -e 's![@]PACKAGE_VERSION[@]!$(PACKAGE_VERSION)!g'	\
	    -e 's![@]GSSAPI_LDFLAGS[@]!$(GSSAPI_LDFLAGS)!g'	\
	    -e 's![@]GSSAPI_LIBS[@]!$(GSSAPI_LIBS)!g'		\
	    $(srcdir)/client/libremctl.pc.in > $@

# The remctl command-line client.
bin_PROGRAMS = client/remctl
client_remctl_SOURCES = client/remctl.c
client_remctl_LDADD = client/libremctl.la util/libutil.la \
	portable/libportable.la

# The remctld server.  We include portable/event-extra.c directly in the
# sources for the server instead of including it in libportable to avoid
# introducing a libevent dependency in libremctl, since apparently the
# linker isn't smart enough to figure out that the event functions are
# hidden and never called and optimize them out.
sbin_PROGRAMS = server/remctld
server_remctld_SOURCES = portable/event-extra.c server/commands.c	    \
	server/config.c server/generic.c server/logging.c server/internal.h \
	server/process.c server/remctld.c server/server-v1.c		    \
	server/server-v2.c
server_remctld_CPPFLAGS = -DCONFIG_FILE=\"$(sysconfdir)/remctl.conf\"	\
	$(GSSAPI_CPPFLAGS) $(KRB5_CPPFLAGS) $(GPUT_CPPFLAGS)		\
	$(PCRE_CPPFLAGS) $(LIBEVENT_CPPFLAGS) $(SYSTEMD_DAEMON_CFLAGS)
server_remctld_LDFLAGS = $(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS) $(GPUT_LDFLAGS) \
	$(PCRE_LDFLAGS) $(LIBEVENT_LDFLAGS)
server_remctld_LDADD = util/libutil.la portable/libportable.la	\
	$(GSSAPI_LIBS) $(KRB5_LIBS) $(GPUT_LIBS) $(PCRE_LIBS)	\
	$(LIBEVENT_LIBS) $(SYSTEMD_DAEMON_LIBS)

# Install the systemd unit file if systemd support was detected.
if HAVE_SYSTEMD
    systemdsystemunit_DATA = systemd/remctld.service systemd/remctld.socket
endif

# Substitute the installation paths into the systemd unit file.
systemd/remctld.service: $(srcdir)/systemd/remctld.service.in
	mkdir -p systemd
	sed -e 's![@]sbindir[@]!$(sbindir)!g' \
	    $(srcdir)/systemd/remctld.service.in > $@

# Documentation.
dist_man_MANS = docs/api/remctl.3 docs/api/remctl_close.3		    \
	docs/api/remctl_command.3 docs/api/remctl_error.3		    \
	docs/api/remctl_new.3 docs/api/remctl_noop.3 docs/api/remctl_open.3 \
	docs/api/remctl_output.3 docs/api/remctl_set_ccache.3		    \
	docs/api/remctl_set_source_ip.3 docs/api/remctl_set_timeout.3	    \
	docs/remctl.1
man_MANS = docs/remctld.8

# Substitute the system configuration path into the manual page.
docs/remctld.8: $(srcdir)/docs/remctld.8.in
	[ -d docs ] || mkdir docs
	sed 's%\(\\f(CI\)*\@sysconfdir\(\\fI\)*\@%$(sysconfdir)%' \
	    < $(srcdir)/docs/remctld.8.in > $@

# Add symlinks for the man pages that document multiple functions.
install-data-hook:
	rm -f $(DESTDIR)$(man3dir)/remctl_result_free.3
	$(LN_S) remctl.3 $(DESTDIR)$(man3dir)/remctl_result_free.3
	rm -f $(DESTDIR)$(man3dir)/remctl_commandv.3
	$(LN_S) remctl_command.3 $(DESTDIR)$(man3dir)/remctl_commandv.3
	rm -f $(DESTDIR)$(man3dir)/remctl_open_addrinfo.3
	$(LN_S) remctl_open.3 $(DESTDIR)$(man3dir)/remctl_open_addrinfo.3
	rm -f $(DESTDIR)$(man3dir)/remctl_open_fd.3
	$(LN_S) remctl_open.3 $(DESTDIR)$(man3dir)/remctl_open_fd.3
	rm -f $(DESTDIR)$(man3dir)/remctl_open_sockaddr.3
	$(LN_S) remctl_open.3 $(DESTDIR)$(man3dir)/remctl_open_sockaddr.3

CLEANFILES = client/libremctl.pc docs/remctld.8 perl/t/lib/Test/RRA.pm	\
	perl/t/lib/Test/RRA/Automake.pm perl/t/lib/Test/RRA/Config.pm	\
	stamp-python systemd/remctld.service
DISTCLEANFILES = perl/Makefile python/MANIFEST
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 build-aux/compile		   \
	build-aux/config.guess build-aux/config.sub build-aux/depcomp	   \
	build-aux/install-sh build-aux/ltmain.sh build-aux/missing	   \
	config.h.in config.h.in~ configure docs/api/*.3 docs/protocol.html \
	docs/protocol.txt docs/remctl.1 docs/remctld.8.in		   \
	m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4	   \
	m4/lt~obsolete.m4

# Also clean the perl, php, and ruby directories if needed and clean up
# after Python.
clean-local:
	set -e; if [ -f "perl/Build" ] ; then		\
	    cd perl && ./Build realclean ;		\
	fi
	set -e; if [ -f "php/Makefile" ] ; then	\
	    cd php && $(MAKE) clean ;		\
	fi
	set -e; if [ -f "ruby/Makefile" ] ; then	\
	    cd ruby && $(MAKE) distclean ;		\
	fi
	rm -rf perl/t/config perl/t/lib/Test/RRA php/tmp-php.ini \
	    python/build python/dist python/*.pyc

# Also clean the Perl, PHP, and Python directories on make distclean if
# needed.
#
# phpize --clean is stupid and requires that config.m4 exist, but it was
# already deleted by the regular distclean target.  Hack around that.
distclean-local: clean-local
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then		  \
	    rm -rf php ;						  \
	    for f in $(PERL_FILES) ; do					  \
		rm -f "$(builddir)/$$f" ;				  \
	    done ;							  \
	    for f in $(PYTHON_FILES) ; do				  \
		rm -f "$(builddir)/$$f" ;				  \
	    done							  \
	else								  \
	    if [ -n "$(PHPIZE)" ] ; then				  \
		cd php && touch config.m4 && $(PHPIZE) --clean && cd .. ; \
	    fi ;							  \
	    rm -f php/config.h.in~ php/config.m4 ;			  \
	fi
	set -e; if [ -f "ruby/Makefile" ] ; then	\
	    cd ruby && $(MAKE) distclean ;		\
	fi

# A set of flags for warnings.  Add -O because gcc won't find some warnings
# without optimization turned on.  Desirable warnings that can't be turned
# on due to other problems:
#
#     -Wconversion	http://bugs.debian.org/488884 (htons warnings)
#
# Last checked against gcc 4.8.2 (2014-04-12).  -D_FORTIFY_SOURCE=2 enables
# warn_unused_result attribute markings on glibc functions on Linux, which
# catches a few more issues.
WARNINGS = -g -O -fstrict-overflow -fstrict-aliasing -D_FORTIFY_SOURCE=2    \
	-Wall -Wextra -Wendif-labels -Wformat=2 -Winit-self -Wswitch-enum   \
	-Wstrict-overflow=5 -Wfloat-equal -Wdeclaration-after-statement	    \
	-Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-align	    \
	-Wwrite-strings -Wjump-misses-init -Wlogical-op -Wstrict-prototypes \
	-Wold-style-definition -Wmissing-prototypes -Wnormalized=nfc	    \
	-Wpacked -Wredundant-decls -Wnested-externs -Winline -Wvla -Werror

# Suppress some warnings for Perl, PHP, Python, and Ruby code because the
# auto-generated code doesn't pass them.
if GCC
    PERL_WARNINGS = -Wno-write-strings -Wno-nested-externs		\
	-Wno-strict-prototypes -Wno-missing-prototypes			\
	-Wno-redundant-decls -Wno-shadow -Wno-old-style-definition
    PHP_WARNINGS = -Wno-strict-prototypes -Wno-write-strings		\
	-Wno-missing-prototypes -Wno-unused-parameter -Wno-sign-compare	\
	-Wno-redundant-decls -Wno-old-style-definition -Wno-float-equal \
	-Wno-strict-overflow
    PYTHON_WARNINGS = -Wno-unused-parameter -Wno-strict-aliasing
    RUBY_WARNINGS = -Wno-strict-prototypes -Wno-redundant-decls
else
    PERL_WARNINGS =
    PHP_WARNINGS =
    PYTHON_WARNINGS =
    RUBY_WARNINGS =
endif

warnings:
	$(MAKE) V=0 CFLAGS='$(WARNINGS)' KRB5_CPPFLAGS='$(KRB5_CPPFLAGS_GCC)'
	$(MAKE) V=0 CFLAGS='$(WARNINGS)' \
	    KRB5_CPPFLAGS='$(KRB5_CPPFLAGS_GCC)' $(check_PROGRAMS)

# The bits below are for the test suite, not for the main package.
check_PROGRAMS = tests/runtests tests/client/api-t tests/client/ccache-t   \
	tests/client/large-t tests/client/open-t tests/client/source-ip-t  \
	tests/client/timeout-t tests/data/cmd-background		   \
	tests/data/cmd-closed tests/data/cmd-large-output		   \
	tests/data/cmd-sigpipe tests/data/cmd-stdin			   \
	tests/data/cmd-streaming tests/data/cmd-user			   \
	tests/portable/asprintf-t tests/portable/daemon-t		   \
	tests/portable/getaddrinfo-t tests/portable/getnameinfo-t	   \
	tests/portable/getopt-t tests/portable/inet_aton-t		   \
	tests/portable/inet_ntoa-t tests/portable/inet_ntop-t		   \
	tests/portable/mkstemp-t tests/portable/setenv-t		   \
	tests/portable/snprintf-t tests/portable/strlcat-t		   \
	tests/portable/strlcpy-t tests/server/accept-t tests/server/acl-t  \
	tests/server/acl/localgroup-t tests/server/bind-t		   \
	tests/server/config-t tests/server/continue-t tests/server/empty-t \
	tests/server/env-t tests/server/errors-t tests/server/help-t	   \
	tests/server/invalid-t tests/server/logging-t tests/server/noop-t  \
	tests/server/stdin-t tests/server/streaming-t			   \
	tests/server/summary-t tests/server/user-t tests/server/version-t  \
	tests/util/fdflag-t tests/util/gss-tokens-t			   \
	tests/util/messages-krb5-t tests/util/messages-t		   \
	tests/util/network/addr-ipv4-t tests/util/network/addr-ipv6-t	   \
	tests/util/network/client-t tests/util/network/server-t		   \
	tests/util/tokens-t tests/util/vector-t tests/util/xmalloc	   \
	tests/util/xwrite-t
check_LIBRARIES = tests/tap/libtap.a
tests_runtests_CPPFLAGS = -DSOURCE='"$(abs_top_srcdir)/tests"' \
	-DBUILD='"$(abs_top_builddir)/tests"'
tests_tap_libtap_a_CPPFLAGS = -I$(abs_top_srcdir)/tests		\
	-DPATH_FAKEROOT='"$(PATH_FAKEROOT)"'			\
	-DPATH_REMCTLD='"$(abs_top_builddir)/server/remctld"'	\
	$(KRB5_CPPFLAGS)
tests_tap_libtap_a_SOURCES = tests/tap/basic.c tests/tap/basic.h	\
	tests/tap/kerberos.c tests/tap/kerberos.h tests/tap/messages.c	\
	tests/tap/messages.h tests/tap/macros.h tests/tap/process.c	\
	tests/tap/process.h tests/tap/remctl.c tests/tap/remctl.h	\
	tests/tap/string.c tests/tap/string.h

# Used for server tests.
SERVER_FILES = portable/event-extra.c server/commands.c server/config.c	\
	server/generic.c server/logging.c server/process.c		\
	server/server-v1.c server/server-v2.c

# All of the test programs.
tests_client_api_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_client_api_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_client_ccache_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_client_ccache_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_client_large_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_client_large_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_client_open_t_LDFLAGS = $(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS)
tests_client_open_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(GSSAPI_LIBS) $(KRB5_LIBS)
tests_client_source_ip_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_client_source_ip_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_client_timeout_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_client_timeout_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_data_cmd_background_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_data_cmd_large_output_LDADD = util/libutil.la portable/libportable.la
tests_data_cmd_sigpipe_LDADD = portable/libportable.la
tests_data_cmd_stdin_LDADD = util/libutil.la portable/libportable.la
tests_portable_asprintf_t_SOURCES = tests/portable/asprintf-t.c \
	tests/portable/asprintf.c
tests_portable_asprintf_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_daemon_t_SOURCES = tests/portable/daemon-t.c \
	tests/portable/daemon.c
tests_portable_daemon_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_getaddrinfo_t_SOURCES = tests/portable/getaddrinfo-t.c \
	tests/portable/getaddrinfo.c
tests_portable_getaddrinfo_t_LDADD = tests/tap/libtap.a \
	portable/libportable.la
tests_portable_getnameinfo_t_SOURCES = tests/portable/getnameinfo-t.c \
	tests/portable/getnameinfo.c
tests_portable_getnameinfo_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_portable_getopt_t_SOURCES = tests/portable/getopt-t.c \
	tests/portable/getopt.c
tests_portable_getopt_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_inet_aton_t_SOURCES = tests/portable/inet_aton-t.c \
	tests/portable/inet_aton.c
tests_portable_inet_aton_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_inet_ntoa_t_SOURCES = tests/portable/inet_ntoa-t.c \
	tests/portable/inet_ntoa.c
tests_portable_inet_ntoa_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_inet_ntop_t_SOURCES = tests/portable/inet_ntop-t.c \
	tests/portable/inet_ntop.c
tests_portable_inet_ntop_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_mkstemp_t_SOURCES = tests/portable/mkstemp-t.c \
	tests/portable/mkstemp.c
tests_portable_mkstemp_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_setenv_t_SOURCES = tests/portable/setenv-t.c \
	tests/portable/setenv.c
tests_portable_setenv_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_snprintf_t_SOURCES = tests/portable/snprintf-t.c \
	tests/portable/snprintf.c
tests_portable_snprintf_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_strlcat_t_SOURCES = tests/portable/strlcat-t.c \
	tests/portable/strlcat.c
tests_portable_strlcat_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_portable_strlcpy_t_SOURCES = tests/portable/strlcpy-t.c \
	tests/portable/strlcpy.c
tests_portable_strlcpy_t_LDADD = tests/tap/libtap.a portable/libportable.la
tests_server_accept_t_SOURCES = tests/server/accept-t.c $(SERVER_FILES)
tests_server_accept_t_LDFLAGS = $(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS) \
	$(GPUT_LDFLAGS) $(PCRE_LDFLAGS) $(LIBEVENT_LDFLAGS)
tests_server_accept_t_LDADD = tests/tap/libtap.a util/libutil.la	 \
	portable/libportable.la $(GSSAPI_LIBS) $(KRB5_LIBS) $(GPUT_LIBS) \
	$(PCRE_LIBS) $(LIBEVENT_LIBS)
tests_server_acl_t_SOURCES = tests/server/acl-t.c $(SERVER_FILES)
tests_server_acl_t_LDFLAGS = $(GPUT_LDFLAGS) $(PCRE_LDFLAGS) \
	$(LIBEVENT_LDFLAGS)
tests_server_acl_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la $(GPUT_LIBS) $(PCRE_LIBS) $(LIBEVENT_LIBS)
tests_server_acl_localgroup_t_SOURCES = tests/server/acl/localgroup-t.c	  \
	$(SERVER_FILES) tests/server/acl/fake-getgrnam.c		  \
	tests/server/acl/fake-getgrnam.h tests/server/acl/fake-getpwnam.c \
	tests/server/acl/fake-getpwnam.h
tests_server_acl_localgroup_t_LDFLAGS = $(GPUT_LDFLAGS) $(PCRE_LDFLAGS) \
	$(LIBEVENT_LDFLAGS)
tests_server_acl_localgroup_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la $(GPUT_LIBS) $(PCRE_LIBS) $(LIBEVENT_LIBS)
tests_server_bind_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_bind_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_config_t_SOURCES = tests/server/config-t.c $(SERVER_FILES)
tests_server_config_t_LDFLAGS = $(GPUT_LDFLAGS) $(PCRE_LDFLAGS) \
	$(LIBEVENT_LDFLAGS)
tests_server_config_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la $(GPUT_LIBS) $(PCRE_LIBS) $(LIBEVENT_LIBS)
tests_server_continue_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_continue_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_empty_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_empty_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_env_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_env_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_errors_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_errors_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_help_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_help_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_invalid_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_invalid_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_logging_t_SOURCES = tests/server/logging-t.c $(SERVER_FILES)
tests_server_logging_t_LDFLAGS = $(GPUT_LDFLAGS) $(PCRE_LDFLAGS) \
	$(LIBEVENT_LDFLAGS)
tests_server_logging_t_LDADD = tests/tap/libtap.a util/libutil.la	 \
	portable/libportable.la $(GSSAPI_LIBS) $(GPUT_LIBS) $(PCRE_LIBS) \
	$(LIBEVENT_LIBS)
tests_server_noop_t_LDFLAGS = $(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS) \
	$(PCRE_LDFLAGS) $(LIBEVENT_LDFLAGS)
tests_server_noop_t_LDADD = client/libremctl.la tests/tap/libtap.a	    \
	util/libutil.la portable/libportable.la $(GSSAPI_LIBS) $(KRB5_LIBS) \
	$(PCRE_LIBS) $(LIBEVENT_LIBS)
tests_server_stdin_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_stdin_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_streaming_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_streaming_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_summary_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_summary_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_user_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_server_user_t_LDADD = client/libremctl.la tests/tap/libtap.a \
	util/libutil.la portable/libportable.la $(KRB5_LIBS)
tests_server_version_t_LDFLAGS = $(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS) \
	$(PCRE_LDFLAGS)
tests_server_version_t_LDADD = client/libremctl.la tests/tap/libtap.a	    \
	util/libutil.la portable/libportable.la $(GSSAPI_LIBS) $(KRB5_LIBS) \
	$(PCRE_LIBS)
tests_util_fdflag_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_gss_tokens_t_SOURCES = tests/util/faketoken.c \
	tests/util/gss-tokens.c tests/util/gss-tokens-t.c
tests_util_gss_tokens_t_LDFLAGS = $(GSSAPI_LDFLAGS) $(KRB5_LDFLAGS)
tests_util_gss_tokens_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la $(GSSAPI_LIBS) $(KRB5_LIBS)
tests_util_messages_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_messages_krb5_t_CPPFLAGS = $(KRB5_CPPFLAGS)
tests_util_messages_krb5_t_LDFLAGS = $(KRB5_LDFLAGS)
tests_util_messages_krb5_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la $(KRB5_LIBS)
tests_util_network_addr_ipv4_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_network_addr_ipv6_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_network_client_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_network_server_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_tokens_t_LDFLAGS = $(GSSAPI_LDFLAGS)
tests_util_tokens_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la $(GSSAPI_LIBS)
tests_util_vector_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la
tests_util_xmalloc_LDADD = util/libutil.la portable/libportable.la
tests_util_xwrite_t_SOURCES = tests/util/fakewrite.c tests/util/xwrite.c \
	tests/util/xwrite-t.c
tests_util_xwrite_t_LDADD = tests/tap/libtap.a util/libutil.la \
	portable/libportable.la

TEST_RPATH = $(abs_top_builddir)/client/.libs

check-local: $(check_PROGRAMS)
	cd tests && ./runtests -l '$(abs_top_srcdir)/tests/TESTS'
	@set -e; if [ -f perl/Build ] ; then			\
	    echo '' ;						\
	    [ -d perl/t/config ] || mkdir perl/t/config ;	\
	    if [ -f tests/config/keytab ] ; then		\
		cp tests/config/keytab perl/t/config ;		\
	    fi ;						\
	    if [ -f tests/config/principal ] ; then		\
		cp tests/config/principal perl/t/config ;	\
	    fi ;						\
	    cd perl && LD_LIBRARY_PATH=$(TEST_RPATH)		\
		REMCTLD="$(abs_top_builddir)/server/remctld"	\
		./Build test ;					\
	fi
	@set -e; if [ -f php/Makefile ] ; then				    \
	    echo '' ;							    \
	    cd php && NO_INTERACTION=1 ./test-wrapper "$(abs_top_builddir)" \
		"$(abs_top_srcdir)" ;					    \
	fi
	@set -e; if [ -f python/setup.py ] ; then		\
	    echo '' ;						\
	    echo 'Testing Python extension' ;			\
	    for python in $(REMCTL_PYTHON_VERSIONS) ; do	\
		v=`echo "$$python" | sed 's/^[^0-9]*//'` ;	\
		cd python ;					\
		LD_LIBRARY_PATH=$(TEST_RPATH)			\
		    PYTHONPATH="`ls -d build/lib.*$$v`"		\
		    "$$python" test_remctl.py ;			\
		cd .. ;						\
	    done ;						\
	fi
	@set -e; if [ -f ruby/Makefile ] ; then		\
	    echo '' ;					\
	    cd ruby && LD_LIBRARY_PATH=$(TEST_RPATH)	\
		$(REMCTL_RUBY) -I. test_remctl.rb ;	\
	fi

# Used by maintainers to run the main test suite under valgrind.  Suppress
# the xmalloc and pod-spelling tests because the former won't work properly
# under valgrind (due to increased memory usage) and the latter is pointless
# to run under valgrind.
check-valgrind: $(check_PROGRAMS)
	rm -rf $(abs_top_builddir)/tmp-valgrind
	mkdir $(abs_top_builddir)/tmp-valgrind
	env RRA_MAINTAINER_TESTS= valgrind --leak-check=full		   \
	    --show-reachable=yes --track-fds=yes --trace-children=yes	   \
	    --log-file=$(abs_top_builddir)/tmp-valgrind/log.%p		   \
	    --suppressions=$(abs_top_srcdir)/tests/data/valgrind.supp	   \
	    --trace-children-skip="/bin/sh,*/cat,*/cut,*/expr,*/getopt,*/kinit,*/ls,*/mkdir,*/rm,*/rmdir,*/sed,*/sleep,*/wc,*/data/cmd-*,*/docs/pod*-t,*/perl/*-t" \
	    tests/runtests -l '$(abs_top_srcdir)/tests/TESTS'

# Used for hooking in the build of optional language bindings.
BINDINGS =
BINDINGS_INSTALL =

# The following section sets up the Perl module build properly.  Most of the
# complexity is here to handle building outside of the source directory, in
# which case we need to copy various files from the source directory and make
# sure they're up to date since Perl can't handle VPATH builds.

# CFLAGS and LDFLAGS to pass to the Perl build.
PERL_CFLAGS = -I$(abs_top_builddir) -I$(abs_top_srcdir) \
	-I$(abs_top_srcdir)/client $(CPPFLAGS)
PERL_LDFLAGS_FULL = -L$(abs_top_builddir)/client/.libs $(PERL_LDFLAGS) \
	$(LDFLAGS)

perl/lib/Net/Remctl.o: $(srcdir)/perl/lib/Net/Remctl.pm			 \
		$(srcdir)/perl/lib/Net/Remctl.xs			 \
		$(srcdir)/perl/lib/Net/Remctl/Backend.pm		 \
		$(srcdir)/perl/typemap client/libremctl.la perl/Build.PL
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then		\
	    for d in $(PERL_DIRECTORIES) ; do				\
		[ -d "$(builddir)/$$d" ] || mkdir "$(builddir)/$$d" ;	\
	    done ;							\
	    for f in $(PERL_FILES) ; do					\
	        cp "$(srcdir)/$$f" "$(builddir)/$$f" ;			\
	    done ;							\
	fi
	cp -R $(srcdir)/tests/tap/perl/* perl/t/lib/
	rm -f perl/lib/Net/Remctl.o
	cd perl && perl Build.PL					\
	    --config lddlflags='$(PERL_LDFLAGS_FULL)'			\
	    --config optimize='$(CFLAGS) $(PERL_WARNINGS)'		\
	    --extra-compiler-flags '$(PERL_CFLAGS)'			\
	    --extra-linker-flags '-lremctl $(DEPEND_LIBS) $(LIBS)'	\
	    $(REMCTL_PERL_FLAGS)
	cd perl && ./Build

install-data-local-perl: perl/lib/Net/Remctl.o
	cd perl && ./Build install --destdir '$(DESTDIR)'

# Build and install the Perl bindings if desired.
if BUILD_PERL
BINDINGS += perl/lib/Net/Remctl.o
BINDINGS_INSTALL += install-data-local-perl
endif

# The following section sets up the PECL extension for PHP.

php/modules/remctl.so: php/config.m4 php/php_remctl.h \
		$(srcdir)/php/php_remctl.c client/libremctl.la
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then	\
	    mkdir php/tests 2>/dev/null || true ;		\
	    for f in $(PHP_FILES) ; do				\
	        cp "$(srcdir)/$$f" "$(builddir)/$$f" ;		\
	    done						\
	fi
	cd php && $(PHPIZE) --clean && $(PHPIZE)
	cd php && ./configure CPPFLAGS="$(CPPFLAGS)" \
	    CFLAGS="$(CFLAGS) $(PHP_WARNINGS)" LDFLAGS="$(LDFLAGS)"
	cd php && $(MAKE) CFLAGS="$(CFLAGS) $(PHP_WARNINGS)"

# PHP's build system uses INSTALL_ROOT where everyone else uses DESTDIR.
install-data-local-php: php/modules/remctl.so
	cd php && $(MAKE) install INSTALL_ROOT=$(DESTDIR)

# Build and install the PHP bindings if desired.
if BUILD_PHP
BINDINGS += php/modules/remctl.so
BINDINGS_INSTALL += install-data-local-php
endif

# The following section sets up the Python extension build properly.  Most of
# the complexity is here to handle building outside of the source directory,
# in which case we need to copy various files from the source directory and
# make sure they're up to date.

stamp-python: $(srcdir)/python/_remctlmodule.c python/remctl.py	\
		client/libremctl.la
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then	\
	    for f in $(PYTHON_FILES) ; do			\
		cp "$(srcdir)/$$f" "$(builddir)/$$f" ;		\
	    done						\
	fi
	set -e; for python in $(REMCTL_PYTHON_VERSIONS) ; do		   \
	    cd python && CFLAGS="$(CPPFLAGS) $(CFLAGS) $(PYTHON_WARNINGS)" \
		"$$python" setup.py build && cd .. ;			   \
	done
	touch stamp-python


# If RPM_BUILD_ROOT is set, we're being built by RPM.  Use that as DESTDIR.
install-data-local-python: stamp-python
	set -e; for python in $(REMCTL_PYTHON_VERSIONS) ; do		\
	    cd python ;							\
	    if [ -n "$(DESTDIR)" ] ; then				\
		"$$python" setup.py install $(REMCTL_PYTHON_INSTALL)	\
		    --root $(DESTDIR) ;					\
	    elif [ -n "$(RPM_BUILD_ROOT)" ] ; then			\
		"$$python" setup.py install $(REMCTL_PYTHON_INSTALL)	\
		    --root $(RPM_BUILD_ROOT) ;				\
	    else							\
		"$$python" setup.py install $(REMCTL_PYTHON_INSTALL) ;	\
	    fi ;							\
	    cd .. ;							\
	done

# Build and install the Python bindings if desired.
if BUILD_PYTHON
BINDINGS += stamp-python
BINDINGS_INSTALL += install-data-local-python
endif

# The following section sets up the Ruby bindings for remctl.

# CFLAGS to pass to the Ruby build.
RUBY_CFLAGS = -I$(abs_top_builddir) -I$(abs_top_srcdir) \
	-I$(abs_top_srcdir)/client $(CPPFLAGS) $(CFLAGS) $(RUBY_WARNINGS)

# Always enable verbose builds, since we want them for automated builds and
# the two lines aren't important to suppress for people who want quiet
# builds.  (PHP and Perl are way noisier.)
ruby/remctl.so: ruby/extconf.rb $(srcdir)/ruby/remctl.c client/libremctl.la
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then	\
	    for f in $(RUBY_FILES) ; do			\
		cp "$(srcdir)/$$f" "$(builddir)/$$f" ;		\
	    done						\
	fi
	cd ruby && $(REMCTL_RUBY) extconf.rb $(REMCTL_RUBY_FLAGS)
	cd ruby && $(MAKE) V=1 CFLAGS="$(RUBY_CFLAGS)"

install-data-local-ruby: ruby/remctl.so
	cd ruby && $(MAKE) install DESTDIR=$(DESTDIR)

# Build and install the Ruby bindings if desired.
if BUILD_RUBY
BINDINGS += ruby/remctl.so
BINDINGS_INSTALL += install-data-local-ruby
endif

all-local: $(BINDINGS)
install-data-local: $(BINDINGS_INSTALL)
