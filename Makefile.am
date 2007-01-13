# Makefile.am -- Top-level Automake makefile for remctl.
# $Id$
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2006, 2007 Board of Trustees, Leland Stanford Jr. University
# See README for licensing terms.

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = autogen client/remctl.1 client/remctl.pod docs/design.html \
	docs/protocol.txt docs/protocol.html docs/protocol.xml \
	examples/remctl.conf examples/rsh-wrapper examples/xinetd \
	java/Login.java java/RemctlClient.java java/gss_jaas.conf \
	java/gss_jaas2.conf remctl.spec server/remctld.8.in \
	server/remctld.pod system.h tests/TESTS tests/data/README \
	tests/data/acl-nonexistant tests/data/cmd-hello tests/data/cmd-env \
	tests/data/cmd-status tests/data/simple.conf tests/client/remctl-t \
	tests/util/xmalloc-t

lib_LTLIBRARIES = client/libremctl.la
client_libremctl_la_SOURCES = client/api.c client/client-v1.c \
	client/client-v2.c client/error.c client/internal.h client/open.c
client_libremctl_la_LDFLAGS = -version-info 1:0:0
client_libremctl_la_LIBADD = util/libutil.la
include_HEADERS = client/remctl.h

noinst_LTLIBRARIES = util/libutil.la
util_libutil_la_SOURCES = util/concat.c util/gss-tokens.c util/messages.c \
	util/network.c util/tokens.c util/util.h util/vector.c \
	util/xmalloc.c util/xwrite.c
util_libutil_la_LIBADD = $(LTLIBOBJS)

bin_PROGRAMS = client/remctl
client_remctl_SOURCES = client/remctl.c
client_remctl_LDADD = client/libremctl.la util/libutil.la

sbin_PROGRAMS = server/remctld
server_remctld_SOURCES = server/commands.c server/config.c server/generic.c \
	server/logging.c server/internal.h server/remctld.c \
	server/server-v1.c server/server-v2.c
server_remctld_LDADD = util/libutil.la
server_remctld_CPPFLAGS = -DCONFIG_FILE=\"$(sysconfdir)/remctl.conf\"

man_MANS = client/remctl.1 server/remctld.8

$(srcdir)/client/remctl.1: $(srcdir)/client/remctl.pod
	pod2man --release=$(VERSION) --center="User Commands" \
	    $(srcdir)/client/remctl.pod > $@

$(srcdir)/server/remctld.8.in: $(srcdir)/server/remctld.pod
	pod2man --release=$(VERSION) --center="System Daemons" \
	    --section=8 $(srcdir)/server/remctld.pod > $@

server/remctld.8: $(srcdir)/server/remctld.8.in
	sed 's%\(\\f(CI\)*\@sysconfdir\(\\fI\)*\@%$(sysconfdir)%' \
	    < $(srcdir)/server/remctld.8.in > $@

$(srcdir)/docs/protocol.html: $(srcdir)/docs/protocol.xml
	cd $(srcdir)/docs && xml2rfc protocol.xml protocol.html

$(srcdir)/docs/protocol.txt: $(srcdir)/docs/protocol.xml
	cd $(srcdir)/docs && xml2rfc protocol.xml protocol.txt

# We shouldn't have to explicitly list $(LIBOBJS) and $(LTLIBOBJS) here.
# This is a bug in the non-recursive support.
MOSTLYCLEANFILES = $(LIBOBJS) $(LTLIBOBJS)
CLEANFILES = server/remctld.8
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.h.in config.h.in~ \
	configure client/remctl.1 docs/protocol.html docs/protocol.txt \
	server/remctld.8.in

# Automake should deal with this automatically, but it doesn't.  Probably a
# bug in the non-recursive support.
clean-local:
	-rm -rf server/.libs tests/.libs tests/client/.libs tests/data/.libs \
	    tests/server/.libs tests/util/.libs

# More bugs in the non-recursive support.
distclean-local:
	-rm -rf .deps

# A set of flags for warnings.  Add -O because gcc won't find some warnings
# without optimization turned on, and add -DDEBUG=1 so we'll also compile all
# debugging code and test it.
WARNINGS = -g -O -DDEBUG=1 -Wall -W -Wendif-labels -Wpointer-arith \
	-Wbad-function-cast -Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wnested-externs -Werror

warnings:
	$(MAKE) CFLAGS='$(WARNINGS)'
	$(MAKE) CFLAGS='$(WARNINGS)' $(check_PROGRAMS)

# The bits below are for the test suite, not for the main package.
check_PROGRAMS = tests/runtests tests/client/api-t tests/client/open-t \
	tests/data/cmd-streaming tests/server/accept-t \
	tests/server/continue-t tests/server/env-t tests/server/errors-t \
	tests/server/streaming-t tests/server/version-t \
	tests/util/asprintf-t tests/util/concat-t tests/util/getaddrinfo-t \
	tests/util/getnameinfo-t tests/util/gss-tokens-t \
	tests/util/inet_aton-t tests/util/inet_ntoa-t tests/util/inet_ntop-t \
	tests/util/messages-t tests/util/network-t tests/util/setenv-t \
	tests/util/snprintf-t tests/util/strlcat-t tests/util/strlcpy-t \
	tests/util/tokens-t tests/util/vector-t tests/util/xmalloc \
	tests/util/xwrite-t
check_LIBRARIES= tests/libtest.a
tests_libtest_a_SOURCES = tests/libtest.c tests/libtest.h

# Used for server tests.
SERVER_FILES = server/commands.c server/config.c server/generic.c \
	server/logging.c server/server-v1.c server/server-v2.c

# All of the test programs.
tests_client_api_t_LDADD = client/libremctl.la tests/libtest.a util/libutil.la
tests_client_open_t_LDADD = client/libremctl.la tests/libtest.a util/libutil.la
tests_server_accept_t_SOURCES = tests/server/accept-t.c $(SERVER_FILES)
tests_server_accept_t_LDADD = tests/libtest.a util/libutil.la
tests_server_continue_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_env_t_LDADD = client/libremctl.la tests/libtest.a util/libutil.la
tests_server_errors_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_streaming_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_server_version_t_LDADD = client/libremctl.la tests/libtest.a \
	util/libutil.la
tests_util_asprintf_t_SOURCES = tests/util/asprintf-t.c tests/util/asprintf.c
tests_util_asprintf_t_LDADD = tests/libtest.a util/libutil.la
tests_util_concat_t_LDADD = tests/libtest.a util/libutil.la
tests_util_getaddrinfo_t_SOURCES = tests/util/getaddrinfo-t.c \
	tests/util/getaddrinfo.c
tests_util_getaddrinfo_t_LDADD = tests/libtest.a util/libutil.la
tests_util_getnameinfo_t_SOURCES = tests/util/getnameinfo-t.c \
	tests/util/getnameinfo.c
tests_util_getnameinfo_t_LDADD = tests/libtest.a util/libutil.la
tests_util_gss_tokens_t_SOURCES = tests/util/faketoken.c \
	tests/util/gss-tokens.c tests/util/gss-tokens-t.c
tests_util_gss_tokens_t_LDADD = tests/libtest.a util/libutil.la
tests_util_inet_aton_t_SOURCES = tests/util/inet_aton-t.c \
	tests/util/inet_aton.c
tests_util_inet_aton_t_LDADD = tests/libtest.a util/libutil.la
tests_util_inet_ntoa_t_SOURCES = tests/util/inet_ntoa-t.c \
	tests/util/inet_ntoa.c
tests_util_inet_ntoa_t_LDADD = tests/libtest.a util/libutil.la
tests_util_inet_ntop_t_SOURCES = tests/util/inet_ntop-t.c \
	tests/util/inet_ntop.c
tests_util_inet_ntop_t_LDADD = tests/libtest.a util/libutil.la
tests_util_messages_t_LDADD = tests/libtest.a util/libutil.la
tests_util_network_t_LDADD = tests/libtest.a util/libutil.la
tests_util_setenv_t_SOURCES = tests/util/setenv-t.c tests/util/setenv.c
tests_util_setenv_t_LDADD = tests/libtest.a util/libutil.la
tests_util_snprintf_t_SOURCES = tests/util/snprintf-t.c tests/util/snprintf.c
tests_util_snprintf_t_LDADD = tests/libtest.a util/libutil.la
tests_util_strlcat_t_SOURCES = tests/util/strlcat-t.c tests/util/strlcat.c
tests_util_strlcat_t_LDADD = tests/libtest.a util/libutil.la
tests_util_strlcpy_t_SOURCES = tests/util/strlcpy-t.c tests/util/strlcpy.c
tests_util_strlcpy_t_LDADD = tests/libtest.a util/libutil.la
tests_util_tokens_t_LDADD = tests/libtest.a util/libutil.la
tests_util_vector_t_LDADD = tests/libtest.a util/libutil.la
tests_util_xmalloc_LDADD = util/libutil.la
tests_util_xwrite_t_SOURCES = tests/util/fakewrite.c tests/util/xwrite.c \
	tests/util/xwrite-t.c
tests_util_xwrite_t_LDADD = tests/libtest.a util/libutil.la

check-local: $(noinst_PROGRAMS)
	cd tests && ./runtests TESTS

