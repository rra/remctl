--TEST--
Check for remctl module
--ENV--
LD_LIBRARY_PATH=../client/.libs
--FILE--
<?php if (extension_loaded("remctl")) echo "remctl module is available"; ?>
--EXPECT--
remctl module is available
