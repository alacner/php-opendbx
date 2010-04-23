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

#define SAFE_STRING(s) ((s)?(s):"")
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


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(odbx)
{
	/* If you have INI entries, uncomment these lines 
	*/
	REGISTER_INI_ENTRIES();
        //le_result = zend_register_list_destructors_ex(_free_odbx_result, NULL, "odbx result", module_number);
        le_link = zend_register_list_destructors_ex(_close_odbx_link, NULL, "odbx link", module_number);
	le_plink = zend_register_list_destructors_ex(NULL, _close_odbx_plink, "odbx link persistent", module_number);
        Z_TYPE(odbx_module_entry) = type;
#if 0
        REGISTER_LONG_CONSTANT("MYSQL_ASSOC", MYSQL_ASSOC, CONST_CS | CONST_PERSISTENT);
#endif
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
        snprintf(buf, sizeof(buf), "%ld", ODBX_G(num_links));
        php_info_print_table_row(2, "Active Links", buf);
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
	zval **args[4];
	zend_bool persistent=0;

	smart_str str = {0};
	int i, arg_offset, persistent_arg=0;

	odbx_t *odbx;

        if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 4
                        || zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args) == FAILURE) {
                WRONG_PARAM_COUNT;
        }

	smart_str_appends(&str, "odbx");

	arg_offset = ZEND_NUM_ARGS();
        for (i = 0; i < ZEND_NUM_ARGS(); i++) {
                if (Z_TYPE_PP(args[i]) == IS_BOOL) {
			persistent = Z_STRVAL_PP(args[i]);
			arg_offset = i+1;
			persistent_arg = 1;
			break;
                }
		else
		{
			convert_to_string_ex(args[i]);
			smart_str_appendc(&str, '_');
			smart_str_appendl(&str, Z_STRVAL_PP(args[i]), Z_STRLEN_PP(args[i]));
		}
        }

        smart_str_0(&str);

	switch((persistent_arg ? arg_offset-1 : arg_offset)) {
	case 0 :
	case 1 :
		backend = Z_STRVAL_PP(args[0]);
	case 2 :
		backend = Z_STRVAL_PP(args[0]);
		host = Z_STRVAL_PP(args[1]);
	case 3 :
		backend = Z_STRVAL_PP(args[0]);
		host = Z_STRVAL_PP(args[1]);
		port = Z_STRVAL_PP(args[2]);
		break;
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
			if ( odbx_init( odbx, backend, host, port ) < 0 ) {
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
                if (persistent 
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
		if ( odbx_init( odbx, backend, host, port ) < 0 ) {
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

		/*
		if (zend_hash_update(&EG(regular_list), str.c, str.len+1,(void *) &new_index_ptr, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
			//free(odbx);
			ODBX_DO_INIT_RETURN_FALSE();
                }
		*/

                ODBX_G(num_links)++;
	}

	//smart_str_free(&str);
        php_odbx_set_default_link(Z_LVAL_P(return_value) TSRMLS_CC);

}
/* }}} */


PHP_FUNCTION(odbx_finish)
{
}
PHP_FUNCTION(odbx_bind)
{
}
#if 0
/* {{{ proto bool mysql_close([int link_identifier])
   Close a OpenDBX connection */
PHP_FUNCTION(odbx_finish)
{
        zval **odbx_link=NULL;
        int id;
        odbx_t *odbx;

        switch (ZEND_NUM_ARGS()) {
                case 0:
                        id = ODBX_G(default_link);
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

        ZEND_FETCH_RESOURCE2(odbx, odbx_t*, odbx_link, id, "OpenDBX-Link", le_link, le_plink);

        if (id==-1) { /* explicit resource number */
                //PHPMY_UNBUFFERED_QUERY_CHECK();
                zend_list_delete(Z_RESVAL_PP(odbx_link));
        }

        if (id!=-1
                || (odbx_link && Z_RESVAL_PP(odbx_link)==ODBX_G(default_link))) {
                //PHPMY_UNBUFFERED_QUERY_CHECK();
                zend_list_delete(ODBX_G(default_link));
                ODBX_G(default_link) = -1;
        }

        RETURN_TRUE;
}
/* }}} */
PHP_FUNCTION(odbx_bind)
{
        zval **database, **who, **cred, **method, **odbx_link;
        int id;
        odbx_t *odbx;

        switch(ZEND_NUM_ARGS()) {
                case 4:
                        if (zend_get_parameters_ex(4, &database, &who, &cred, &method)==FAILURE) {
                                RETURN_FALSE;
                        }
                        id = php_odbx_get_default_link(INTERNAL_FUNCTION_PARAM_PASSTHRU);
                        CHECK_LINK(id);
                        break;
                case 5:
                        if (zend_get_parameters_ex(5, &odbx_link, &database, &who, &cred, &method)==FAILURE) {
                                RETURN_FALSE;
                        }
                        id = -1;
                        break;
                default:
                        WRONG_PARAM_COUNT;
                        break;
        }

        ZEND_FETCH_RESOURCE2(odbx, odbx_t*, odbx_link, id, "OpenDBX-Link", le_link, le_plink);

        Z_LVAL_P(return_value) = (long) odbx_bind(odbx, database, who, cred, method);
        Z_TYPE_P(return_value) = IS_LONG;
}
/* }}} */
#endif



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
