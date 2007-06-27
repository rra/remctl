#!/usr/bin/perl
# $Id$
#
# t/pod.t -- Test POD formatting for Net::Remctl.

eval 'use Test::Pod 1.00';
if ($@) {
    print "1..1\n";
    print "ok 1 # skip - Test::Pod 1.00 required for testing POD\n";
    exit;
}
all_pod_files_ok ();
