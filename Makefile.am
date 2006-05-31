# Makefile.am -- Top-level Automake makefile for remctl.
# $Id$

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = client/remctl.1 client/remctl.pod docs/design.html \
	docs/protocol.txt docs/protocol.html docs/protocol.xml \
	docs/remctl.conf examples/rsh-wrapper examples/xinetd \
	java/Login.java java/RemctlClient.java java/gss_jaas.conf \
	java/gss_jaas2.conf remctl.spec \
	server/remctld.8.in server/remctld.pod \
	tests/TESTS tests/data/README tests/data/acl-nonexistant \
	tests/data/cmd-hello tests/data/cmd-status tests/data/simple.conf \
	tests/client/remctl-t tests/util/xmalloc-t

lib_LIBRARIES = client/libremctl.a
client_libremctl_a_SOURCES = client/api.c client/client-v1.c client/error.c \
	client/open.c

noinst_LIBRARIES = util/libutil.a
util_libutil_a_SOURCES = util/concat.c util/gss-tokens.c util/messages.c \
	util/tokens.c util/vector.c util/xmalloc.c util/xwrite.c
util_libutil_a_LIBADD = $(LIBOBJS)

bin_PROGRAMS = client/remctl
client_remctl_SOURCES = client/remctl.c
client_remctl_LDADD = client/libremctl.a util/libutil.a

sbin_PROGRAMS = server/remctld
server_remctld_SOURCES = server/remctld.c server/gss-utils.c
server_remctld_LDADD = util/libutil.a
server_remctld_CPPFLAGS = -DCONFIG_FILE=\"$(sysconfdir)/remctl.conf\"

man_MANS = client/remctl.1 server/remctld.8

$(srcdir)/client/remctl.1: $(srcdir)/client/remctl.pod
	pod2man --release=$(VERSION) --center="User Commands" $< > $@

$(srcdir)/server/remctld.8.in: $(srcdir)/server/remctld.pod
	pod2man --release=$(VERSION) --center="System Daemons" \
	    --section=8 $< > $@

server/remctld.8: $(srcdir)/server/remctld.8.in
	sed 's%@sysconfdir\@%$(sysconfdir)%' < $< > $@

CLEANFILES = server/remctld.8
MAINTAINERCLEANFILES = config.h.in config.h.in~ configure

# The bits below are for the test suite, not for the main package.
check_PROGRAMS = tests/runtests tests/client/api-t tests/util/asprintf-t \
	tests/util/concat-t tests/util/gss-tokens-t tests/util/messages-t \
	tests/util/snprintf-t tests/util/strlcat-t tests/util/strlcpy-t \
	tests/util/tokens-t tests/util/vector-t tests/util/xmalloc \
	tests/util/xwrite-t
check_LIBRARIES= tests/libtest.a
tests_client_api_t_LDADD = client/libremctl.a tests/libtest.a util/libutil.a
tests_util_asprintf_t_SOURCES = tests/util/asprintf-t.c tests/util/asprintf.c
tests_util_asprintf_t_LDADD = tests/libtest.a util/libutil.a
tests_util_concat_t_LDADD = tests/libtest.a util/libutil.a
tests_util_gss_tokens_t_SOURCES = tests/util/faketoken.c \
	tests/util/gss-tokens.c tests/util/gss-tokens-t.c
tests_util_gss_tokens_t_LDADD = tests/libtest.a util/libutil.a
tests_util_messages_t_LDADD = tests/libtest.a util/libutil.a
tests_util_snprintf_t_SOURCES = tests/util/snprintf-t.c tests/util/snprintf.c
tests_util_snprintf_t_LDADD = tests/libtest.a util/libutil.a
tests_util_strlcat_t_SOURCES = tests/util/strlcat-t.c tests/util/strlcat.c
tests_util_strlcat_t_LDADD = tests/libtest.a util/libutil.a
tests_util_strlcpy_t_SOURCES = tests/util/strlcpy-t.c tests/util/strlcpy.c
tests_util_strlcpy_t_LDADD = tests/libtest.a util/libutil.a
tests_util_tokens_t_LDADD = tests/libtest.a util/libutil.a
tests_util_vector_t_LDADD = tests/libtest.a util/libutil.a
tests_util_xmalloc_LDADD = util/libutil.a
tests_util_xwrite_t_SOURCES = tests/util/fakewrite.c tests/util/xwrite.c \
	tests/util/xwrite-t.c
tests_util_xwrite_t_LDADD = tests/libtest.a util/libutil.a

check-local: $(noinst_PROGRAMS)
	cd tests && ./runtests TESTS

