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
  | Author: Alacner Zhang <alacner@gmail.com>                            |
  +----------------------------------------------------------------------+
*/

/* $Id: header 226204 2007-01-01 19:32:10Z iliaa $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_globals.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "php_odbx.h"


#ifdef ZEND_ENGINE_2
# include "zend_exceptions.h"
#else
  /* PHP 4 compat */
# define OnUpdateLong   OnUpdateInt
# define E_STRICT               E_NOTICE
#endif


#if HAVE_ODBX

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#define SAFE_STRING(s) ((s)?(s):"")

#include <odbx.h>

/* True global resources - no need for thread safety here */
static int le_result, le_link, le_plink, le_odbx;

ZEND_DECLARE_MODULE_GLOBALS(odbx)

typedef struct _php_odbx_conn {
	odbx_t  *conn;
        int active_result_id;
} php_odbx_conn;

/* {{{ odbx_functions[]
 *
 * Every user visible function must have an entry in odbx_functions[].
 */
zend_function_entry odbx_functions[] = {
	PHP_FE(odbx_init, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ odbx_module_entry
 */
zend_module_entry odbx_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"odbx",
	odbx_functions,
	PHP_MINIT(odbx),
	PHP_MSHUTDOWN(odbx),
	PHP_RINIT(odbx),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(odbx),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(odbx),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_ODBX_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_ODBX
ZEND_GET_MODULE(odbx)
#endif

#define CHECK_LINK(link) { if (link==-1) { php_error_docref(NULL TSRMLS_CC, E_WARNING, "A link to the server could not be established"); RETURN_FALSE; } }

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("odbx.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_odbx_globals, odbx_globals)
    STD_PHP_INI_ENTRY("odbx.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_odbx_globals, odbx_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_odbx_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_odbx_init_globals(zend_odbx_globals *odbx_globals)
{
	odbx_globals->global_value = 0;
	odbx_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ php_odbx_set_default_link
 */
static void php_odbx_set_default_link(int id TSRMLS_DC)
{
        if (ODBX_G(default_link) != -1) {
                zend_list_delete(ODBX_G(default_link));
        }
        ODBX_G(default_link) = id;
        zend_list_addref(id);
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(odbx)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(odbx)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(odbx)
{
	return SUCCESS;
}
/* }}} */


/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(odbx)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(odbx)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "odbx support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


PHP_FUNCTION(odbx_init)
{
	zval *odbx_link = NULL;
	char *backend=NULL, *host=NULL, *port=NULL;
	int backend_len, host_len, port_len;
	char *hashed_details=NULL;
	int hashed_details_length;
	int err;
	php_odbx_conn *odbx=NULL;

        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!s!", &backend, &backend_len, &host, &host_len, &port, &port_len, &odbx_link) == FAILURE) {
                return;
        }

	if (!host) {
		host = "127.0.0.1";
	}

	hashed_details_length = spprintf(&hashed_details, 0, "odbx_%s_%s_%s", SAFE_STRING(backend), SAFE_STRING(host), SAFE_STRING(port));
	/* create the link */
	odbx = (php_odbx_conn *) malloc(sizeof(php_odbx_conn));
	odbx->active_result_id = 0;

	if ( odbx_init( &odbx->conn, backend, host, port ) < 0 ) {
		RETURN_FALSE;
	}

	zend_rsrc_list_entry *index_ptr, new_index_ptr;

	/* add it to the list */
	ZEND_REGISTER_RESOURCE(return_value, odbx, le_link);

	/* add it to the hash */
	new_index_ptr.ptr = (void *) Z_LVAL_P(return_value);
	Z_TYPE(new_index_ptr) = le_index_ptr;
	if (zend_hash_update(&EG(regular_list), hashed_details, hashed_details_length+1,(void *) &new_index_ptr, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
		efree(hashed_details);
		//MYSQL_DO_CONNECT_RETURN_FALSE();
	}
	ODBX_G(num_links)++;

        efree(hashed_details);
        php_odbx_set_default_link(Z_LVAL_P(return_value) TSRMLS_CC);

}
/* }}} */

#endif

/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
