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
#include "ext/standard/php_smart_str.h"
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

#define CHECK_DEFAULT_LINK(x) if ((x) == -1) { php_error_docref(NULL TSRMLS_CC, E_WARNING, "No OpenDBX link opened yet"); }

#define ODBX_DO_INIT_RETURN_FALSE()         \
	smart_str_free(&str); \
        RETURN_FALSE;


#include <odbx.h>

/* True global resources - no need for thread safety here */
static int le_link, le_plink, le_result, le_lofp, le_string;

ZEND_DECLARE_MODULE_GLOBALS(odbx)
static PHP_GINIT_FUNCTION(odbx);

/* {{{ odbx_functions[]
 *
 * Every user visible function must have an entry in odbx_functions[].
 */
zend_function_entry odbx_functions[] = {
	PHP_FE(odbx_init, NULL)
	PHP_FE(odbx_finish, NULL)
	PHP_FE(odbx_bind, NULL)
	PHP_FE(odbx_error, NULL)
	PHP_FE(odbx_error_type, NULL)
	PHP_FE(odbx_unbind, NULL)
	PHP_FE(odbx_capabilities, NULL)
	PHP_FE(odbx_escape, NULL)
	PHP_FE(odbx_set_option, NULL)
	PHP_FE(odbx_get_option, NULL)
	PHP_FE(odbx_query, NULL)
	PHP_FE(odbx_row_fetch, NULL)
	PHP_FE(odbx_column_count, NULL)
	PHP_FE(odbx_column_name, NULL)
	PHP_FE(odbx_column_type, NULL)
	PHP_FE(odbx_field_length, NULL)
	PHP_FE(odbx_field_value, NULL)
	PHP_FE(odbx_rows_affected, NULL)
	//PHP_FE(odbx_, NULL)
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
Remove comments and fill if you need to have entries in php.ini
 */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN( "odbx.allow_persistent",      "1",  PHP_INI_SYSTEM, OnUpdateBool, allow_persistent,      zend_odbx_globals, odbx_globals)

	STD_PHP_INI_ENTRY_EX("odbx.max_persistent",       "-1",  PHP_INI_SYSTEM, OnUpdateLong, max_persistent,        zend_odbx_globals, odbx_globals, display_link_numbers)
	STD_PHP_INI_ENTRY_EX("odbx.max_links",            "-1",  PHP_INI_SYSTEM, OnUpdateLong, max_links,             zend_odbx_globals, odbx_globals, display_link_numbers)
	STD_PHP_INI_ENTRY("odbx.default_host", "127.0.0.1", PHP_INI_SYSTEM, OnUpdateString, default_host, zend_odbx_globals, odbx_globals)
    //STD_PHP_INI_ENTRY("odbx.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_odbx_globals, odbx_globals)
    //STD_PHP_INI_ENTRY("odbx.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_odbx_globals, odbx_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_GINIT_FUNCTION
 */
static PHP_GINIT_FUNCTION(odbx)
{
        memset(odbx_globals, 0, sizeof(zend_odbx_globals));
        /* Initilize notice message hash at MINIT only */
	odbx_globals->num_persistent = 0;
}
/* }}} */

/* {{{ php_odbx_set_default_link
 */
static void php_odbx_set_default_link(int id TSRMLS_DC)
{
        zend_list_addref(id);

        if (ODBX_G(default_link) != -1) {
                zend_list_delete(ODBX_G(default_link));
        }

        ODBX_G(default_link) = id;
}
/* }}} */
#if 0
/* {{{ _free_odbx_result
 * This wrapper is required since odbx_result_finish() returns an integer, and
 * thus, cannot be used directly
 */
static void _free_odbx_result(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
        odbx_result_t *odbx_result = (odbx_result_t*)rsrc->ptr;

        odbx_result_finish(odbx_result);
        ODBX_G(result_allocated)--;
}
/* }}} */
#endif

/* {{{ _close_pgsql_link
 */
static void _close_odbx_link(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
        odbx_t *link = ( odbx_t *)rsrc->ptr;
        odbx_result_t *res;

	odbx_finish(link);
        ODBX_G(num_links)--;
}
/* }}} */

/* {{{ _close_odbx_plink
 */
static void _close_odbx_plink(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
        odbx_t *link = ( odbx_t *)rsrc->ptr;
        odbx_result_t *res;

	odbx_finish(link);

        ODBX_G(num_links)--;
        ODBX_G(num_persistent)--;
}
/* }}} */

/* {{{ _free_result
 */
static void _free_odbx_result(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
        odbx_result_t *res = (odbx_result_t *)rsrc->ptr;

	odbx_result_finish(res);
}
/* }}} */


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(odbx)
{
	/* If you have INI entries, uncomment these lines 
	*/
	REGISTER_INI_ENTRIES();
        le_link = zend_register_list_destructors_ex(_close_odbx_link, NULL, "odbx link", module_number);
	le_plink = zend_register_list_destructors_ex(NULL, _close_odbx_plink, "odbx link persistent", module_number);
        le_result = zend_register_list_destructors_ex(_free_odbx_result, NULL, "odbx result", module_number);
        Z_TYPE(odbx_module_entry) = type;


	/*
	 *  Extended capabilities supported by the backends
	 *  0x0000-0x00ff: Well known capabilities
	*/
        REGISTER_LONG_CONSTANT("ODBX_CAP_BASIC", ODBX_CAP_BASIC, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_CAP_LO", ODBX_CAP_LO, CONST_CS | CONST_PERSISTENT);
	/*
	 * ODBX bind type
	*/
        REGISTER_LONG_CONSTANT("ODBX_BIND_SIMPLE", ODBX_BIND_SIMPLE, CONST_CS | CONST_PERSISTENT);
	/*
	 *  ODBX error types
	*/
        REGISTER_LONG_CONSTANT("ODBX_ERR_SUCCESS", ODBX_ERR_SUCCESS, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_BACKEND", ODBX_ERR_BACKEND, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_NOCAP", ODBX_ERR_NOCAP, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_PARAM", ODBX_ERR_PARAM, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_NOMEM", ODBX_ERR_NOMEM, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_SIZE", ODBX_ERR_SIZE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_NOTEXIST", ODBX_ERR_NOTEXIST, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_NOOP", ODBX_ERR_NOOP, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_OPTION", ODBX_ERR_OPTION, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_OPTRO", ODBX_ERR_OPTRO, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_OPTWR", ODBX_ERR_OPTWR, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_RESULT", ODBX_ERR_RESULT, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_NOTSUP", ODBX_ERR_NOTSUP, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ERR_HANDLE", ODBX_ERR_HANDLE, CONST_CS | CONST_PERSISTENT);
	/*
	 *  ODBX result/fetch return values
	*/
        REGISTER_LONG_CONSTANT("ODBX_RES_DONE", ODBX_RES_DONE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_RES_TIMEOUT", ODBX_RES_TIMEOUT, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_RES_NOROWS", ODBX_RES_NOROWS, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_RES_ROWS", ODBX_RES_ROWS, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ROW_DONE", ODBX_ROW_DONE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_ROW_NEXT", ODBX_ROW_NEXT, CONST_CS | CONST_PERSISTENT);
	/*
	 *  ODBX options
	 *
	 *  0x0000 - 0x1fff reserved for api options
	 *  0x2000 - 0x3fff reserved for api extension options
	 *  0x4000 - 0xffff reserved for vendor specific and experimental options
	*/
	/* Informational options */
        REGISTER_LONG_CONSTANT("ODBX_OPT_API_VERSION", ODBX_OPT_API_VERSION, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_OPT_THREAD_SAFE", ODBX_OPT_THREAD_SAFE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_OPT_LIB_VERSION", ODBX_OPT_LIB_VERSION, CONST_CS | CONST_PERSISTENT);
	/* Security related options */
        REGISTER_LONG_CONSTANT("ODBX_OPT_TLS", ODBX_OPT_TLS, CONST_CS | CONST_PERSISTENT);
	/* Implemented options */
        REGISTER_LONG_CONSTANT("ODBX_OPT_MULTI_STATEMENTS", ODBX_OPT_MULTI_STATEMENTS, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_OPT_PAGED_RESULTS", ODBX_OPT_PAGED_RESULTS, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_OPT_COMPRESS", ODBX_OPT_COMPRESS, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_OPT_MODE", ODBX_OPT_MODE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_OPT_CONNECT_TIMEOUT", ODBX_OPT_CONNECT_TIMEOUT, CONST_CS | CONST_PERSISTENT);
	/* SSL/TLS related options */
        REGISTER_LONG_CONSTANT("ODBX_TLS_NEVER", ODBX_TLS_NEVER, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TLS_TRY", ODBX_TLS_TRY, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TLS_ALWAYS", ODBX_TLS_ALWAYS, CONST_CS | CONST_PERSISTENT);

	/*
	 *  ODBX (SQL2003) data types
	*/
        //Exact numeric values:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_BOOLEAN", ODBX_TYPE_BOOLEAN, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_SMALLINT", ODBX_TYPE_SMALLINT, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_INTEGER", ODBX_TYPE_INTEGER, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_BIGINT", ODBX_TYPE_BIGINT, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_DECIMAL", ODBX_TYPE_DECIMAL, CONST_CS | CONST_PERSISTENT);
        //Approximate numeric values:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_REAL", ODBX_TYPE_REAL, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_DOUBLE", ODBX_TYPE_DOUBLE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_FLOAT", ODBX_TYPE_FLOAT, CONST_CS | CONST_PERSISTENT);
        //String values:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_CHAR", ODBX_TYPE_CHAR, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_NCHAR", ODBX_TYPE_NCHAR, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_VARCHAR", ODBX_TYPE_VARCHAR, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_NVARCHAR", ODBX_TYPE_NVARCHAR, CONST_CS | CONST_PERSISTENT);
        //Large objects:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_CLOB", ODBX_TYPE_CLOB, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_NCLOB", ODBX_TYPE_NCLOB, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_XML", ODBX_TYPE_XML, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_BLOB", ODBX_TYPE_BLOB, CONST_CS | CONST_PERSISTENT);
        //Date and time values:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_TIME", ODBX_TYPE_TIME, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_TIMETZ", ODBX_TYPE_TIMETZ, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_TIMESTAMP", ODBX_TYPE_TIMESTAMP, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_TIMESTAMPTZ", ODBX_TYPE_TIMESTAMPTZ, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_DATE", ODBX_TYPE_DATE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_INTERVAL", ODBX_TYPE_INTERVAL, CONST_CS | CONST_PERSISTENT);
        //Arrays and sets:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_ARRAY", ODBX_TYPE_ARRAY, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_TYPE_MULTISET", ODBX_TYPE_MULTISET, CONST_CS | CONST_PERSISTENT);
        //External links:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_DATALINK", ODBX_TYPE_DATALINK, CONST_CS | CONST_PERSISTENT);
        //Vendor specific:
        REGISTER_LONG_CONSTANT("ODBX_TYPE_UNKNOWN", ODBX_TYPE_UNKNOWN, CONST_CS | CONST_PERSISTENT);

	// enable
        REGISTER_LONG_CONSTANT("ODBX_ENABLE", ODBX_ENABLE, CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("ODBX_DISABLE", ODBX_DISABLE, CONST_CS | CONST_PERSISTENT);

        //REGISTER_LONG_CONSTANT("", );

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
	char buf[32];
	php_info_print_table_start();
	php_info_print_table_header(2, "OpenDBX support", "enabled");

        snprintf(buf, sizeof(buf), "%ld", ODBX_G(num_persistent));
        php_info_print_table_row(2, "Active Persistent Links", buf);
        snprintf(buf, sizeof(buf), "%ld", ODBX_G(num_links));
        php_info_print_table_row(2, "Active Links", buf);

	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

PHP_FUNCTION(odbx_init)
{

	char *backend=NULL, *host=NULL, *port=NULL;
	zval **args[4];
	zend_bool persistent=0;

	smart_str str = {0};
	int i;

	odbx_t* odbx;
	int err;

        if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 4
                        || zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args) == FAILURE) {
                WRONG_PARAM_COUNT;
        }

	smart_str_appends(&str, "odbx");

        for (i = 0; i < ZEND_NUM_ARGS(); i++) {
                if (Z_TYPE_PP(args[i]) == IS_BOOL) {
			persistent = Z_BVAL_PP(args[i]);
			break;
                }
		else
		{
			convert_to_string_ex(args[i]);
			smart_str_appendc(&str, '_');
			smart_str_appendl(&str, Z_STRVAL_PP(args[i]), Z_STRLEN_PP(args[i]));

			/* switch args*/
			switch(i) {
			case 0 :
				backend = Z_STRVAL_PP(args[0]);
				break;
			case 1 :
				host = Z_STRVAL_PP(args[1]);
				break;
			case 2 :
				port = Z_STRVAL_PP(args[2]);
				break;
			default :
				break;
			}
		}
        }

        smart_str_0(&str);


	if (host == NULL) {
		host = ODBX_G(default_host);
	}

	if (port == NULL) {
		port = "";
	}

        if (persistent && ODBX_G(allow_persistent)) {
                zend_rsrc_list_entry *le;

                /* try to find if we already have this link in our persistent list */
		if (zend_hash_find(&EG(persistent_list), str.c, str.len+1, (void **) &le)==FAILURE) {  /* we don't */
                        zend_rsrc_list_entry new_le;

                        if (ODBX_G(max_links)!=-1 && ODBX_G(num_links)>=ODBX_G(max_links)) {
                                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                                                 "Cannot create new link. Too many open links (%ld)", ODBX_G(num_links));
				ODBX_DO_INIT_RETURN_FALSE();
                        }
                        if (ODBX_G(max_persistent)!=-1 && ODBX_G(num_persistent)>=ODBX_G(max_persistent)) {
                                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                                                 "Cannot create new link. Too many open persistent links (%ld)", ODBX_G(num_persistent));
				ODBX_DO_INIT_RETURN_FALSE();
                        }

                        /* create the link */
			if ( ( err = odbx_init( &odbx, backend, host, port ) ) < 0 ) {
                                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                                                 "odbx_init(): %s", odbx_error( odbx, err ));
				ODBX_DO_INIT_RETURN_FALSE();
			}

                        /* hash it up */
                        Z_TYPE(new_le) = le_plink;
                        new_le.ptr = odbx;

			if (zend_hash_update(&EG(persistent_list), str.c, str.len+1,(void *) &new_le, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
				free(odbx);
				ODBX_DO_INIT_RETURN_FALSE();
			}
                        ODBX_G(num_links)++;
                        ODBX_G(num_persistent)++;
                } else {  /* we do */
                        if (Z_TYPE_P(le) != le_plink) {
				ODBX_DO_INIT_RETURN_FALSE();
                        }

			if (le->ptr==NULL) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,"Link to server lost, unable to reconnect");
				zend_hash_del(&EG(persistent_list),str.c, str.len+1);
				ODBX_DO_INIT_RETURN_FALSE();
			}

                        odbx = (odbx_t *) le->ptr;
                }
                ZEND_REGISTER_RESOURCE(return_value, odbx, le_plink);
        } else { /* Non persistent connection */

                zend_rsrc_list_entry *index_ptr,new_index_ptr;

                /* first we check the hash for the str.c key.  if it exists,
                 * it should point us to the right offset where the actual opendbx link sits.
                 * if it doesn't, open a new opendbx link, add it to the resource list,
                 * and add a pointer to it with str.c as the key.
                 */
                if ( ! persistent 
			&& zend_hash_find(&EG(regular_list), str.c, str.len+1,(void **) &index_ptr)==SUCCESS) {
                        int type;
                        long link;
                        void *ptr;

                        if (Z_TYPE_P(index_ptr) != le_index_ptr) {
				ODBX_DO_INIT_RETURN_FALSE();
                        }
                        link = (long) index_ptr->ptr;
                        ptr = zend_list_find(link,&type);   /* check if the link is still there */
                        if (ptr && (type==le_link || type==le_plink)) {
                                Z_LVAL_P(return_value) = link;
                                zend_list_addref(link);
				Z_LVAL_P(return_value) = link;
                                php_odbx_set_default_link(link TSRMLS_CC);
                                Z_TYPE_P(return_value) = IS_RESOURCE;
				smart_str_free(&str);
				return;
                        } else {
                                zend_hash_del(&EG(regular_list), str.c, str.len+1);
                        }
                }


                if (ODBX_G(max_links)!=-1 && ODBX_G(num_links)>=ODBX_G(max_links)) {
                        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot create new link. Too many open links (%ld)", ODBX_G(num_links));
			ODBX_DO_INIT_RETURN_FALSE();
                }


		/* create the link */
		if( ( err = odbx_init( &odbx, backend, host, port ) ) < 0 ) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
							 "odbx_init(): %s", odbx_error( odbx, err ));
			ODBX_DO_INIT_RETURN_FALSE();
		}

                if (odbx==NULL) {
                        PHP_PQ_ERROR("Unable to init to : %s", odbx);
			ODBX_DO_INIT_RETURN_FALSE();
                }


                /* add it to the list */
                ZEND_REGISTER_RESOURCE(return_value, odbx, le_link);


                /* add it to the hash */
                new_index_ptr.ptr = (void *) Z_LVAL_P(return_value);
                Z_TYPE(new_index_ptr) = le_index_ptr;

		if (zend_hash_update(&EG(regular_list), str.c, str.len+1,(void *) &new_index_ptr, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
			free(odbx);
			ODBX_DO_INIT_RETURN_FALSE();
                }

                ODBX_G(num_links)++;
	}

	smart_str_free(&str);
        php_odbx_set_default_link(Z_LVAL_P(return_value) TSRMLS_CC);

}
/* }}} */

/* {{{ 
   Close a OpenDBX connection */
PHP_FUNCTION(odbx_finish)
{
        zval **odbx_link=NULL;
        int id;
        odbx_t *odbx;

        switch (ZEND_NUM_ARGS()) {
                case 0:
                        id = ODBX_G(default_link);
			CHECK_DEFAULT_LINK(id);
                        break;
                case 1:
                        if (zend_get_parameters_ex(1, &odbx_link)==FAILURE) {
                                RETURN_FALSE;
                        }
                        id = -1;
                        break;
                default:
                        WRONG_PARAM_COUNT;
                        break;
        }

        if (odbx_link == NULL && id == -1) {
                RETURN_FALSE;
        }

        ZEND_FETCH_RESOURCE2(odbx, odbx_t *, odbx_link, id, "OpenDBX Link", le_link, le_plink);

        if (id==-1) { /* explicit resource number */
                zend_list_delete(Z_RESVAL_PP(odbx_link));
        }

        if (id!=-1
                || (odbx_link && Z_RESVAL_PP(odbx_link)==ODBX_G(default_link))) {
                zend_list_delete(ODBX_G(default_link));
                ODBX_G(default_link) = -1;
        }

        RETURN_TRUE;
}
/* }}} */

PHP_FUNCTION(odbx_bind)
{
	zval **odbx_link;
        char *database, *who, *cred;
        int database_len, who_len, cred_len;

        int method;

        int id;
        odbx_t *odbx;

        if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
                                             "rsssl", &odbx_link, &database, &database_len, &who, &who_len, &cred, &cred_len, &method) == SUCCESS) {
                id = -1;
        } else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
                                             "sssl", &database, &database_len, &who, &who_len, &cred, &cred_len, &method) == SUCCESS) {
                odbx_link = NULL;
                id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
        }
        else {
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 3 or 4 arguments");
                RETURN_FALSE;
        }


        if (odbx_link == NULL && id == -1) {
                RETURN_FALSE;
        }

        ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);

        Z_LVAL_P(return_value) = odbx_bind(odbx, database, who, cred, method);
        Z_TYPE_P(return_value) = IS_LONG;
}

PHP_FUNCTION(odbx_error)
{
	zval **odbx_link;
	int error;

	int id;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rl", &odbx_link, &error) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"l", &error) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 1 or 2 arguments");
		RETURN_FALSE;
	}


	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);


	Z_STRVAL_P(return_value) = odbx_error(odbx, error);
	Z_STRLEN_P(return_value) = strlen(Z_STRVAL_P(return_value));
	Z_STRVAL_P(return_value) = (char *) estrdup(Z_STRVAL_P(return_value));
	Z_TYPE_P(return_value) = IS_STRING;
}


PHP_FUNCTION(odbx_error_type)
{
	zval **odbx_link;
	int error;

	int id;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rl", &odbx_link, &error) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"l", &error) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 1 or 2 arguments");
		RETURN_FALSE;
	}


	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);


	Z_LVAL_P(return_value) = odbx_error_type(odbx, error);
	Z_TYPE_P(return_value) = IS_LONG;
}


PHP_FUNCTION(odbx_capabilities)
{
	zval **odbx_link;
	unsigned int cap;

	int id;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rl", &odbx_link, &cap) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"l", &cap) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 1 or 2 arguments");
		RETURN_FALSE;
	}


	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);


	Z_LVAL_P(return_value) = odbx_capabilities(odbx, cap);
	Z_TYPE_P(return_value) = IS_LONG;
}

PHP_FUNCTION(odbx_escape)
{
	zval **odbx_link;
	char *from = NULL;
	unsigned long from_len;

	int id;
	int err;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rs", &odbx_link, &from, &from_len) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"s", &from, &from_len) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 1 or 2 arguments");
		RETURN_FALSE;
	}

	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);

        // To be precise, the output buffer must be 2 * size of input + 1 bytes long.

        char to[2*(from_len+1)];
        unsigned long to_len = sizeof(to);

	if( ( err = odbx_escape(odbx, from, (unsigned long)from_len, to, &to_len) ) < 0 ) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "odbx_escape(): %s", odbx_error( odbx, err ));
                efree(to);
                RETURN_FALSE;
	}

	Z_STRVAL_P(return_value) = to;
	Z_STRLEN_P(return_value) = strlen(Z_STRVAL_P(return_value));
	Z_STRVAL_P(return_value) = (char *) estrdup(Z_STRVAL_P(return_value));
	Z_TYPE_P(return_value) = IS_STRING;
}

PHP_FUNCTION(odbx_set_option)
{
	zval **odbx_link;
	unsigned long option;
	zval **value;

	int id;
	int err;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rlz", &odbx_link, &option, &value) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"lz", &option, &value) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 2 or 3 arguments");
		RETURN_FALSE;
	}

	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);

	Z_LVAL_P(return_value) = odbx_set_option(odbx, option, (void **)&value);
	Z_TYPE_P(return_value) = IS_LONG;
}

PHP_FUNCTION(odbx_get_option)
{
	zval **odbx_link;
	unsigned long option;
	zval **value;

	int id;
	int err;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rlz", &odbx_link, &option, &value) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"lz", &option, &value) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 2 or 3 arguments");
		RETURN_FALSE;
	}

	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);

	Z_LVAL_P(return_value) = odbx_get_option(odbx, option, (void **)&value);
	Z_TYPE_P(return_value) = IS_LONG;
}

PHP_FUNCTION(odbx_query)
{
	zval **odbx_link;
	char *stmt = NULL;
	unsigned long stmt_len;


	int id;
	int err;
	odbx_t *odbx;
	odbx_result_t* result;
	struct timeval tv = { 3, 0 };

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"rs", &odbx_link, &stmt, &stmt_len) == SUCCESS) {
		id = -1;
	} else if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"s", &stmt, &stmt_len) == SUCCESS) {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Requires 1 or 2 arguments");
		RETURN_FALSE;
	}

	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);

	// query
	if( ( err = odbx_query( odbx, stmt, stmt_len ) ) < 0 ) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "odbx_query(): %s", odbx_error( odbx, err ));
                RETURN_FALSE;
	}

	// result
	if ( ( err = odbx_result( odbx, &result, &tv, 0 ) ) < 0 ) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 "odbx_result(): %s", odbx_error( odbx, err ));
                RETURN_FALSE;
	}


	switch( err )
	{
		case 1:
			// NOTE: Retrieving the result is not (!) canceled
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"odbx_result(): Timeout");
			odbx_result_finish( result );
			RETURN_FALSE;
			break;

		default:
			ZEND_REGISTER_RESOURCE(return_value, result, le_result);
			break;
	}
}

PHP_FUNCTION(odbx_column_count)
{
	zval **odbx_result;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &odbx_result) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	
	Z_LVAL_P(return_value) = odbx_column_count(result);
	Z_TYPE_P(return_value) = IS_LONG;

}

PHP_FUNCTION(odbx_column_name)
{
	zval **odbx_result;
	unsigned long pos;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &odbx_result, &pos) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	RETURN_STRING(odbx_column_name(result, pos), 1);	

}

PHP_FUNCTION(odbx_column_type)
{
	zval **odbx_result;
	unsigned long pos;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &odbx_result, &pos) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	
	Z_LVAL_P(return_value) = odbx_column_type(result, pos);
	Z_TYPE_P(return_value) = IS_LONG;

}

PHP_FUNCTION(odbx_rows_affected)
{
	zval **odbx_result;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &odbx_result) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	
	Z_LVAL_P(return_value) = odbx_rows_affected(result);
	Z_TYPE_P(return_value) = IS_LONG;

}

PHP_FUNCTION(odbx_row_fetch)
{
	zval **odbx_result;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &odbx_result) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	
	Z_LVAL_P(return_value) = odbx_row_fetch(result);
	Z_TYPE_P(return_value) = IS_LONG;

}

PHP_FUNCTION(odbx_field_value)
{
	zval **odbx_result;
	unsigned long pos;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &odbx_result, &pos) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	RETURN_STRING((char *)odbx_field_value(result, pos), 1);

}

PHP_FUNCTION(odbx_field_length)
{
	zval **odbx_result;
	unsigned long pos;

	odbx_t *odbx;
	odbx_result_t* result;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &odbx_result, &pos) == FAILURE) {
		return;
	}

	if (odbx_result == NULL) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, odbx_result_t *, &odbx_result, -1, "OpenDBX result", le_result);

	
	Z_LVAL_P(return_value) = odbx_field_length(result, pos);
	Z_TYPE_P(return_value) = IS_LONG;

}

PHP_FUNCTION(odbx_unbind)
{
	zval **odbx_link;
	int id;
	odbx_t *odbx;

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
				"r", &odbx_link) == SUCCESS) {
		id = -1;
	} else {
		odbx_link = NULL;
		id = ODBX_G(default_link);
		CHECK_DEFAULT_LINK(id);
	}

	if (odbx_link == NULL && id == -1) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE2(odbx, odbx_t *, &odbx_link, id, "OpenDBX Link", le_link, le_plink);


	Z_LVAL_P(return_value) = odbx_unbind(odbx);
	Z_TYPE_P(return_value) = IS_LONG;
}

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
