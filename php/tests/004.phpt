--TEST--
Check setting source IP
--CREDIT--
Russ Allbery
# Copyright 2011
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.
--ENV--
KRB5CCNAME=remctl-test.cache
LD_LIBRARY_PATH=../client/.libs
--SKIPIF--
<?php
    if (!file_exists("remctl-test.pid"))
        echo "skip remctld not running";
?>
--FILE--
<?php
    $fh = fopen("remctl-test.princ", "r");
    $principal = rtrim(fread($fh, filesize("remctl-test.princ")));
    $r = remctl_new();
    if ($r == null) {
        echo "remctl_new failed\n";
        exit(2);
    }
    echo "Created object\n";
    if (!remctl_set_source_ip($r, "127.0.0.1")) {
        echo "remctl_set_source_ip failed\n";
        exit(2);
    }
    if (!remctl_open($r, "127.0.0.1", 14373, $principal)) {
        echo "remctl_open failed\n";
        exit(2);
    }
    echo "Opened connection with source 127.0.0.1\n";
    if (!remctl_set_source_ip($r, "::1")) {
        echo "remctl_set_source_ip failed\n";
        exit(2);
    }
    if (remctl_open($r, "127.0.0.1", 14373, $principal)) {
        echo "remctl_open unexpectedly succeeded\n";
        exit(2);
    }
    echo "Open failed with source ::1\n";
    remctl_close($r);
?>
--EXPECT--
Created object
Opened connection with source 127.0.0.1
Open failed with source ::1
