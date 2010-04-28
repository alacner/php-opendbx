/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header 226204 2007-01-01 19:32:10Z iliaa $ */

#ifndef PHP_ODBX_H
#define PHP_ODBX_H

extern zend_module_entry odbx_module_entry;
#define phpext_odbx_ptr &odbx_module_entry

#ifdef PHP_WIN32
#define PHP_ODBX_API __declspec(dllexport)
#else
#define PHP_ODBX_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif


PHP_MINIT_FUNCTION(odbx);
PHP_MSHUTDOWN_FUNCTION(odbx);
PHP_RINIT_FUNCTION(odbx);
PHP_RSHUTDOWN_FUNCTION(odbx);
PHP_MINFO_FUNCTION(odbx);

PHP_FUNCTION(odbx_init);
PHP_FUNCTION(odbx_finish);
PHP_FUNCTION(odbx_bind);
PHP_FUNCTION(odbx_error);
PHP_FUNCTION(odbx_error_type);
PHP_FUNCTION(odbx_unbind);
PHP_FUNCTION(odbx_capabilities);
PHP_FUNCTION(odbx_escape);
PHP_FUNCTION(odbx_set_option);
PHP_FUNCTION(odbx_get_option);
PHP_FUNCTION(odbx_query);
PHP_FUNCTION(odbx_column_count);
PHP_FUNCTION(odbx_column_name);
PHP_FUNCTION(odbx_column_type);
PHP_FUNCTION(odbx_field_length);
PHP_FUNCTION(odbx_field_value);
PHP_FUNCTION(odbx_rows_affected);
//PHP_FUNCTION(odbx_);

#define PHP_ODBX_VERSION "1.0.0"

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
*/

ZEND_BEGIN_MODULE_GLOBALS(odbx)


        long default_link; /* default link when connection is omitted */
        long num_links,num_persistent;
        long max_links,max_persistent;
	long allow_persistent;
        //int le_lofp,le_string;

	char *default_host;
	//char *global_string;
ZEND_END_MODULE_GLOBALS(odbx)

/* In every utility function you add that needs to use variables 
   in php_odbx_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as ODBX_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define ODBX_G(v) TSRMG(odbx_globals_id, zend_odbx_globals *, v)
#else
#define ODBX_G(v) (odbx_globals.v)
#endif

#endif	/* PHP_ODBX_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
