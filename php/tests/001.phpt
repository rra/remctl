--TEST--
Check for remctl module
--SKIPIF--
<?php if (!extension_load("remctl")) print "skip"; ?>
--FILE--
<?php
 echo "remctl module is available";
?>
--EXPECT--
remctl module is available
