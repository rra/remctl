--TEST--
Check for remctl module
--CREDIT--
Russ Allbery
# Copyright 2008, 2010
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT
--ENV--
LD_LIBRARY_PATH=../client/.libs
--FILE--
<?php if (extension_loaded("remctl")) echo "remctl module is available"; ?>
--EXPECT--
remctl module is available
