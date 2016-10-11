--TEST--
Check setting credential cache
--CREDIT--
Russ Allbery
# Copyright 2011
#     The Board of Trustees of the Leland Stanford Junior University
#
# See LICENSE for licensing terms.
--ENV--
KRB5CCNAME=nonexistent-file
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
    if (remctl_open($r, "127.0.0.1", 14373, $principal)) {
        echo "remctl_open succeeded unexpectedly\n";
        exit(2);
    }
    echo "remctl_open failed without credential cache\n";
    if (remctl_set_ccache($r, "remctl-test.cache")) {
        if (!remctl_open($r, "127.0.0.1", 14373, $principal)) {
            echo "remctl_open failed\n";
            exit (2);
        }
    }
    echo "Success or expected failure\n";
    remctl_close($r);
?>
--EXPECT--
Created object
remctl_open failed without credential cache
Success or expected failure
