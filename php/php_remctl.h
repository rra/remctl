#ifndef PHP_REMCTL_H
#define PHP_REMCTL_H			1

#define PHP_REMCTL_VERSION		"0.1"
#define PHP_REMCTL_EXTNAME		"remctl"
#define PHP_REMCTL_RES_NAME		"remctl_resource"

PHP_MINIT_FUNCTION( remctl );
PHP_FUNCTION( remctl );
PHP_FUNCTION( remctl_new );
PHP_FUNCTION( remctl_open );
PHP_FUNCTION( remctl_command );
PHP_FUNCTION( remctl_output );
PHP_FUNCTION( remctl_error );
PHP_FUNCTION( remctl_close );

extern zend_module_entry		remctl_module_entry;

#endif /* REMCTL_H */
