--TEST--
Check setting a timeout for operations
--CREDIT--
Russ Allbery
# Copyright 2012
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT
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
    if (!remctl_set_timeout($r, 1)) {
        echo "remctl_set_timeout failed\n";
        exit(2);
    }
    if (!remctl_open($r, "127.0.0.1", 14373, $principal)) {
        echo "remctl_open failed\n";
        exit(2);
    }
    $args = array("test", "sleep");
    if (!remctl_command($r, $args)) {
        echo "remctl_command failed\n";
        exit(2);
    }
    echo "Sent sleep command\n";
    $output = remctl_output($r);
    if (!$output) {
        echo "output is correctly empty\n";
    }
    $error = remctl_error($r);
    echo "Error: $error\n";
    remctl_close($r);
?>
--EXPECT--
Created object
Sent sleep command
output is correctly empty
Error: error receiving token: timed out
