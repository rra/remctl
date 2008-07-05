# Makefile.am -- Automake makefile for remctl.
# $Id$
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2006, 2007, 2008 Board of Trustees, Leland Stanford Jr. University
#
# See LICENSE for licensing terms.

# These two lists of files are needed for Perl builds and for the test suite
# and are not generated or touched by configure.  They're listed here to be
# added to EXTRA_DIST and so that they can be copied over properly for
# builddir != srcdir builds.
PERL_FILES = perl/Remctl.pm perl/Remctl.xs perl/typemap perl/t/api.t \
	perl/t/pod.t perl/t/stanford-netdb.t
TEST_FILES = tests/TESTS tests/client/pod-t.in tests/client/remctl-t \
	tests/data/README tests/data/acl-bad-include \
	tests/data/acl-bad-syntax tests/data/acl-nonexistant \
	tests/data/acl-recursive tests/data/acl-simple \
	tests/data/acl-too-long tests/data/acls/valid \
	tests/data/acls/valid-2 tests/data/cmd-argv tests/data/cmd-env \
	tests/data/cmd-hello tests/data/cmd-status tests/data/conf-simple \
	tests/data/conf-test tests/util/xmalloc-t

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = LICENSE Makefile.w32 autogen client/libremctl.rc client/mapfile \
	client/remctl.rc config.h.w32 configure.bat docs/api/remctl.pod \
	docs/api/remctl_close.pod docs/api/remctl_command.pod \
	docs/api/remctl_error.pod docs/api/remctl_new.pod \
	docs/api/remctl_open.pod docs/api/remctl_output.pod docs/design.html \
	docs/protocol.txt docs/protocol.html docs/protocol.xml \
	docs/remctl.pod docs/remctld.8.in docs/remctld.pod \
	examples/remctl.conf examples/rsh-wrapper examples/xinetd \
	java/.classpath java/.project java/Makefile java/README \
	java/bcsKeytab.conf java/gss_jaas.conf java/j3.conf java/k5.conf \
	java/org/eyrie/eagle/remctl/Remctl.java \
	java/org/eyrie/eagle/remctl/RemctlClient.java \
	java/org/eyrie/eagle/remctl/RemctlServer.java java/t5.java \
	java/t7.java portable/getaddrinfo.h portable/getnameinfo.h \
	portable/getopt.h portable/gssapi.h portable/macros.h \
	portable/socket.h portable/stdbool.h portable/system.h \
	portable/uio.h portable/winsock.c remctl.spec $(PERL_FILES) \
	$(TEST_FILES)

lib_LTLIBRARIES = client/libremctl.la
client_libremctl_la_SOURCES = client/api.c client/client-v1.c \
	client/client-v2.c client/error.c client/internal.h client/open.c
client_libremctl_la_LDFLAGS = -version-info 1:2:0 $(VERSION_LDFLAGS) \
	$(GSSAPI_LDFLAGS)
client_libremctl_la_LIBADD = util/libutil.la $(GSSAPI_LIBS)
include_HEADERS = client/remctl.h

noinst_LTLIBRARIES = portable/libportable.la util/libutil.la
portable_libportable_la_SOURCES = portable/dummy.c
portable_libportable_la_LIBADD = $(LTLIBOBJS)
util_libutil_la_SOURCES = util/concat.c util/fdflag.c util/gss-errors.c \
	util/gss-tokens.c util/messages.c util/network.c util/tokens.c \
	util/util.h util/vector.c util/xmalloc.c util/xwrite.c
util_libutil_la_LDFLAGS = $(GSSAPI_LDFLAGS)
util_libutil_la_LIBADD = portable/libportable.la $(GSSAPI_LIBS)

bin_PROGRAMS = client/remctl
client_remctl_SOURCES = client/remctl.c
client_remctl_LDADD = client/libremctl.la util/libutil.la

sbin_PROGRAMS = server/remctld
server_remctld_SOURCES = server/commands.c server/config.c server/generic.c \
	server/logging.c server/internal.h server/remctld.c \
	server/server-v1.c server/server-v2.c
server_remctld_CPPFLAGS = -DCONFIG_FILE=\"$(sysconfdir)/remctl.conf\"
server_remctld_LDFLAGS = $(GSSAPI_LDFLAGS)
server_remctld_LDADD = util/libutil.la $(GSSAPI_LIBS)

dist_man_MANS = docs/api/remctl.3 docs/api/remctl_close.3 \
	docs/api/remctl_command.3 docs/api/remctl_error.3 \
	docs/api/remctl_new.3 docs/api/remctl_open.3 \
	docs/api/remctl_output.3 docs/remctl.1
man_MANS = docs/remctld.8

$(srcdir)/docs/remctl.1: $(srcdir)/docs/remctl.pod
	pod2man --release=$(VERSION) --center="User Commands" \
	    $(srcdir)/docs/remctl.pod > $@

$(srcdir)/docs/remctld.8.in: $(srcdir)/docs/remctld.pod
	pod2man --release=$(VERSION) --center="System Daemons" \
	    --section=8 $(srcdir)/docs/remctld.pod > $@

docs/remctld.8: $(srcdir)/docs/remctld.8.in
	[ -d docs ] || mkdir docs
	sed 's%\(\\f(CI\)*\@sysconfdir\(\\fI\)*\@%$(sysconfdir)%' \
	    < $(srcdir)/docs/remctld.8.in > $@

$(srcdir)/docs/api/remctl.3: $(srcdir)/docs/api/remctl.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL $(srcdir)/docs/api/remctl.pod > $@

$(srcdir)/docs/api/remctl_close.3: $(srcdir)/docs/api/remctl_close.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL_CLOSE \
	    $(srcdir)/docs/api/remctl_close.pod > $@

$(srcdir)/docs/api/remctl_command.3: $(srcdir)/docs/api/remctl_command.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL_COMMAND \
	    $(srcdir)/docs/api/remctl_command.pod > $@

$(srcdir)/docs/api/remctl_error.3: $(srcdir)/docs/api/remctl_error.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL_ERROR \
	    $(srcdir)/docs/api/remctl_error.pod > $@

$(srcdir)/docs/api/remctl_new.3: $(srcdir)/docs/api/remctl_new.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL_NEW \
	    $(srcdir)/docs/api/remctl_new.pod > $@

$(srcdir)/docs/api/remctl_open.3: $(srcdir)/docs/api/remctl_open.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL_OPEN \
	    $(srcdir)/docs/api/remctl_open.pod > $@

$(srcdir)/docs/api/remctl_output.3: $(srcdir)/docs/api/remctl_output.pod
	pod2man --release=$(VERSION) --center="remctl Library Reference" \
	    --section=3 --name=REMCTL_OUTPUT \
	    $(srcdir)/docs/api/remctl_output.pod > $@

$(srcdir)/docs/protocol.html: $(srcdir)/docs/protocol.xml
	cd $(srcdir)/docs && xml2rfc protocol.xml protocol.html

$(srcdir)/docs/protocol.txt: $(srcdir)/docs/protocol.xml
	cd $(srcdir)/docs && xml2rfc protocol.xml protocol.txt

# Add symlinks for the man pages that document multiple functions.
install-data-hook:
	rm -f $(DESTDIR)$(man3dir)/remctl_result_free.3
	$(LN_S) remctl.3 $(DESTDIR)$(man3dir)/remctl_result_free.3
	rm -f $(DESTDIR)$(man3dir)/remctl_commandv.3
	$(LN_S) remctl_command.3 $(DESTDIR)$(man3dir)/remctl_commandv.3

CLEANFILES = docs/remctld.8
DISTCLEANFILES = perl/Makefile tests/client/pod-t
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.h.in config.h.in~ \
	configure docs/api/*.3 docs/protocol.html docs/protocol.txt \
	docs/remctl.1 docs/remctld.8.in

# Automake should deal with the first part of this automatically, but it
# doesn't.  Probably a bug in the non-recursive support.  Also run make clean
# in the perl directory if needed.
clean-local:
	-rm -rf server/.libs tests/.libs tests/client/.libs tests/data/.libs \
	    tests/portable/.libs tests/server/.libs tests/util/.libs
	set -e; if [ -f "perl/Makefile" ] ; then \
	    cd perl && $(MAKE) clean ; \
	    cd perl && [ ! -f Makefile.old ] || mv Makefile.old Makefile ; \
	fi

# A set of flags for warnings.  Add -O because gcc won't find some warnings
# without optimization turned on, and add -DDEBUG=1 so we'll also compile all
# debugging code and test it.
WARNINGS = -g -O -DDEBUG=1 -Wall -W -Wendif-labels -Wpointer-arith \
	-Wbad-function-cast -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wnested-externs -Werror

warnings:
	$(MAKE) CFLAGS='$(WARNINGS)'
	$(MAKE) CFLAGS='$(WARNINGS)' $(check_PROGRAMS)

# The bits below are for the test suite, not for the main package.
check_PROGRAMS = tests/runtests tests/client/api-t tests/client/large-t \
	tests/client/open-t tests/data/cmd-background tests/data/cmd-closed \
	tests/data/cmd-streaming tests/portable/asprintf-t \
	tests/portable/getaddrinfo-t tests/portable/getnameinfo-t \
	tests/portable/getopt-t tests/portable/inet_aton-t \
	tests/portable/inet_ntoa-t tests/portable/inet_ntop-t \
	tests/portable/setenv-t tests/portable/snprintf-t \
	tests/portable/strlcat-t tests/portable/strlcpy-t \
	tests/server/accept-t tests/server/acl-t tests/server/background-t \
	tests/server/config-t tests/server/continue-t tests/server/env-t \
	tests/server/errors-t tests/server/invalid-t tests/server/streaming-t \
	tests/server/version-t tests/util/concat-t tests/util/fdflag-t \
	tests/util/gss-tokens-t tests/util/messages-t tests/util/network-t \
	tests/util/tokens-t tests/util/vector-t tests/util/xmalloc \
	tests/util/xwrite-t
check_LIBRARIES = tests/libtest.a
tests_libtest_a_SOURCES = tests/libtest.c tests/libtest.h

# Used for server tests.
SERVER_FILES = server/commands.c server/config.c server/generic.c \
	server/logging.c server/server-v1.c server/server-v2.c

# All of the test programs.
tests_client_api_t_LDADD = client/libremctl.la tests/libtest.a util/libutil.la
tests_client_open_t_LDFLAGS = $(GSSAPI_LDFLAGS)
tests_client_open_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la $(GSSAPI_LIBS)
tests_client_large_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_portable_asprintf_t_SOURCES = tests/portable/asprintf-t.c \
	tests/portable/asprintf.c
tests_portable_asprintf_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_getaddrinfo_t_SOURCES = tests/portable/getaddrinfo-t.c \
	tests/portable/getaddrinfo.c
tests_portable_getaddrinfo_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_getnameinfo_t_SOURCES = tests/portable/getnameinfo-t.c \
	tests/portable/getnameinfo.c
tests_portable_getnameinfo_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_getopt_t_SOURCES = tests/portable/getopt-t.c \
	tests/portable/getopt.c
tests_portable_getopt_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_inet_aton_t_SOURCES = tests/portable/inet_aton-t.c \
	tests/portable/inet_aton.c
tests_portable_inet_aton_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_inet_ntoa_t_SOURCES = tests/portable/inet_ntoa-t.c \
	tests/portable/inet_ntoa.c
tests_portable_inet_ntoa_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_inet_ntop_t_SOURCES = tests/portable/inet_ntop-t.c \
	tests/portable/inet_ntop.c
tests_portable_inet_ntop_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_setenv_t_SOURCES = tests/portable/setenv-t.c \
	tests/portable/setenv.c
tests_portable_setenv_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_snprintf_t_SOURCES = tests/portable/snprintf-t.c \
	tests/portable/snprintf.c
tests_portable_snprintf_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_strlcat_t_SOURCES = tests/portable/strlcat-t.c \
	tests/portable/strlcat.c
tests_portable_strlcat_t_LDADD = tests/libtest.a util/libutil.la
tests_portable_strlcpy_t_SOURCES = tests/portable/strlcpy-t.c \
	tests/portable/strlcpy.c
tests_portable_strlcpy_t_LDADD = tests/libtest.a util/libutil.la
tests_server_accept_t_SOURCES = tests/server/accept-t.c $(SERVER_FILES)
tests_server_accept_t_LDFLAGS = $(GSSAPI_LDFLAGS)
tests_server_accept_t_LDADD = tests/libtest.a util/libutil.la $(GSSAPI_LIBS)
tests_server_acl_t_SOURCES = tests/server/acl-t.c $(SERVER_FILES)
tests_server_acl_t_LDADD = tests/libtest.a util/libutil.la
tests_server_background_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_config_t_SOURCES = tests/server/config-t.c $(SERVER_FILES)
tests_server_config_t_LDADD = tests/libtest.a util/libutil.la
tests_server_continue_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_env_t_LDADD = client/libremctl.la tests/libtest.a util/libutil.la
tests_server_errors_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_invalid_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_streaming_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_version_t_LDFLAGS = $(GSSAPI_LDFLAGS)
tests_server_version_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la $(GSSAPI_LIBS)
tests_util_concat_t_LDADD = tests/libtest.a util/libutil.la
tests_util_fdflag_t_LDADD = tests/libtest.a util/libutil.la
tests_util_gss_tokens_t_SOURCES = tests/util/faketoken.c \
	tests/util/gss-tokens.c tests/util/gss-tokens-t.c
tests_util_gss_tokens_t_LDFLAGS = $(GSSAPI_LDFLAGS)
tests_util_gss_tokens_t_LDADD = tests/libtest.a util/libutil.la $(GSSAPI_LIBS)
tests_util_messages_t_LDADD = tests/libtest.a util/libutil.la
tests_util_network_t_LDADD = tests/libtest.a util/libutil.la
tests_util_tokens_t_LDFLAGS = $(GSSAPI_LDFLAGS)
tests_util_tokens_t_LDADD = tests/libtest.a util/libutil.la $(GSSAPI_LIBS)
tests_util_vector_t_LDADD = tests/libtest.a util/libutil.la
tests_util_xmalloc_LDADD = util/libutil.la
tests_util_xwrite_t_SOURCES = tests/util/fakewrite.c tests/util/xwrite.c \
	tests/util/xwrite-t.c
tests_util_xwrite_t_LDADD = tests/libtest.a util/libutil.la

check-local: $(noinst_PROGRAMS)
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then \
	    mkdir tests/data/acls 2>/dev/null || true ; \
	    for f in $(TEST_FILES) ; do \
	        cp "$(srcdir)/$$f" "$(builddir)/$$f" ; \
	    done \
	fi
	cd tests && ./runtests TESTS
	@set -e; if [ -f perl/Makefile ] ; then \
	    echo '' ; \
	    cd perl && LD_LIBRARY_PATH=$(abs_top_builddir)/client/.libs \
	        $(MAKE) test ; \
	fi

# The following section sets up the Perl module build properly.  Most of the
# complexity is here to handle building outside of the source directory, in
# which case we need to copy various files from the source directory and make
# sure they're up to date since Perl can't handle VPATH builds.

perl/Remctl.o: $(srcdir)/perl/Remctl.pm $(srcdir)/perl/Remctl.xs \
		$(srcdir)/perl/typemap
	set -e; if [ x"$(builddir)" != x"$(srcdir)" ] ; then \
	    mkdir perl/t 2>/dev/null || true ; \
	    for f in $(PERL_FILES) ; do \
	        cp "$(srcdir)/$$f" "$(builddir)/$$f" ; \
	    done \
	fi
	cd perl && perl Makefile.PL
	cd perl && $(MAKE) OPTIMIZE="$(CFLAGS) -Wno-write-strings"

# Build and install the Perl bindings if desired.
if BUILD_PERL
all-local: perl/Remctl.o

install-data-local: perl/Remctl.o
	cd perl && $(MAKE) install DESTDIR=$(DESTDIR)
endif
