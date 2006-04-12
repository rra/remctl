# Makefile.am -- Top-level Automake makefile for remctl.
# $Id$

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = client/remctl.1 client/remctl.pod docs/design.html \
	docs/protocol.txt docs/protocol.html docs/protocol.xml \
	docs/remctl.conf examples/rsh-wrapper examples/xinetd \
	java/Login.java java/RemctlClient.java java/gss_jaas.conf \
	java/gss_jaas2.conf remctl.spec \
	server/remctld.8.in server/remctld.pod

noinst_LIBRARIES = tests/libtest.a util/libutil.a
util_libutil_a_SOURCES = util/concat.c util/messages.c util/vector.c \
	util/xmalloc.c
util_libutil_a_LIBADD = $(LIBOBJS)

bin_PROGRAMS = client/remctl
client_remctl_SOURCES = client/remctl.c client/gss-utils.c
client_remctl_LDADD = util/libutil.a

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

noinst_PROGRAMS = tests/runtests tests/util/concat-t tests/util/messages-t \
	tests/util/snprintf-t tests/util/strlcat-t tests/util/strlcpy-t \
	tests/util/vector-t tests/util/xmalloc
tests_util_concat_t_LDADD = tests/libtest.a util/libutil.a
tests_util_messages_t_LDADD = tests/libtest.a util/libutil.a
tests_util_snprintf_t_SOURCES = tests/util/snprintf-t.c tests/util/snprintf.c
tests_util_snprintf_t_LDADD = tests/libtest.a util/libutil.a
tests_util_strlcat_t_SOURCES = tests/util/strlcat-t.c tests/util/strlcat.c
tests_util_strlcat_t_LDADD = tests/libtest.a util/libutil.a
tests_util_strlcpy_t_SOURCES = tests/util/strlcpy-t.c tests/util/strlcpy.c
tests_util_strlcpy_t_LDADD = tests/libtest.a util/libutil.a
tests_util_vector_t_LDADD = tests/libtest.a util/libutil.a
tests_util_xmalloc_LDADD = util/libutil.a

check-local: $(noinst_PROGRAMS)
	cd tests && ./runtests TESTS

CLEANFILES = server/remctld.8
