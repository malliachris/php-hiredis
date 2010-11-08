/* -*- Mode: C; tab-width: 4 -*- */
/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Original author: Alfonso Jimenez <yo@alfonsojimenez.com>             |
  | Maintainer: Nicolas Favre-Felix <n.favre-felix@owlient.eu>           |
  | Maintainer: Nasreddine Bouafif <n.bouafif@owlient.eu>                |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_redis.h"
#include <zend_exceptions.h>

static zend_class_entry *redis_ce;

ZEND_DECLARE_MODULE_GLOBALS(redis)

static zend_function_entry redis_functions[] = {
     PHP_ME(Redis, __construct, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, connect, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, close, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, get, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, set, NULL, ZEND_ACC_PUBLIC)

     {NULL, NULL, NULL}
};

zend_module_entry redis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
     STANDARD_MODULE_HEADER,
#endif
     "redis",
     NULL,
     PHP_MINIT(redis),
     PHP_MSHUTDOWN(redis),
     PHP_RINIT(redis),
     PHP_RSHUTDOWN(redis),
     PHP_MINFO(redis),
#if ZEND_MODULE_API_NO >= 20010901
     PHP_REDIS_VERSION,
#endif
     STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_REDIS
ZEND_GET_MODULE(redis)
#endif


/**
 * PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry redis_class_entry;
    INIT_CLASS_ENTRY(redis_class_entry, "HiRedis", redis_functions);
    redis_ce = zend_register_internal_class(&redis_class_entry TSRMLS_CC);

    return SUCCESS;
}

/**
 * PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(redis)
{
    return SUCCESS;
}

/**
 * PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(redis)
{
    return SUCCESS;
}

/**
 * PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(redis)
{
    return SUCCESS;
}

/**
 * PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(redis)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Redis Support", "enabled");
    php_info_print_table_row(2, "Version", PHP_REDIS_VERSION);
    php_info_print_table_end();
}

/* {{{ proto Redis Redis::__construct()
    Public constructor */
PHP_METHOD(Redis, __construct)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto boolean Redis::connect(string host, int port [, int timeout])
 */
PHP_METHOD(Redis, connect)
{
    zval *object;
    int host_len, id;
    char *host = NULL;
    long port;

    struct timeval timeout = {0L, 0L};
    RedisSock *redis_sock  = NULL;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl|l",
                                     &object, redis_ce, &host, &host_len, &port,
                                     &timeout.tv_sec) == FAILURE) {
       RETURN_FALSE;
    }

    /*
    if (timeout.tv_sec < 0L || timeout.tv_sec > INT_MAX) {
        zend_throw_exception(redis_exception_ce, "Invalid timeout", 0 TSRMLS_CC);
        RETURN_FALSE;
    }
    */

    /*
    id = zend_list_insert(redis_sock, le_redis_sock);
    add_property_resource(object, "socket", id);
    */

    RETURN_TRUE;
}
/* }}} */

/* {{{ proto boolean Redis::close()
 */
PHP_METHOD(Redis, close)
{
    zval *object;
    RedisSock *redis_sock = NULL;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
        &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }
/*
    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    if (redis_sock_disconnect(redis_sock TSRMLS_CC)) {
        RETURN_TRUE;
    }
*/
    RETURN_FALSE;
}
/* }}} */


/* {{{ proto boolean Redis::set(string key, string value)
 */
PHP_METHOD(Redis, set)
{
#if 0
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL, *val = NULL, *cmd;
    int key_len, val_len, cmd_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss",
                                     &object, redis_ce, &key, &key_len,
                                     &val, &val_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    cmd_len = redis_cmd_format(&cmd, 
                    "*3" _NL
                    "$3" _NL
                    "SET" _NL

                    "$%d" _NL   /* key_len */
                    "%s" _NL    /* key */

                    "$%d" _NL   /* val_len */
                    "%s" _NL    /* val */

                    , key_len, key, key_len
                    , val_len, val, val_len);

    if (redis_sock_write(redis_sock, cmd, cmd_len) < 0) {
        efree(cmd);
        RETURN_FALSE;
    }
    efree(cmd);
    redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock TSRMLS_CC);
#endif
}
/* }}} */

/* {{{ proto string Redis::get(string key)
 */
PHP_METHOD(Redis, get)
{
#if 0
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL, *cmd, *response;
    int key_len, cmd_len, response_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
                                     &object, redis_ce,
                                     &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    cmd_len = redis_cmd_format(&cmd, 
                    "*2" _NL
                    "$3" _NL
                    "GET" _NL

                    "$%d" _NL   /* src_len */
                    "%s" _NL    /* src */

                    , key_len, key, key_len);

    if (redis_sock_write(redis_sock, cmd, cmd_len) < 0) {
        efree(cmd);
        RETURN_FALSE;
    }
    efree(cmd);

    if ((response = redis_sock_read(redis_sock, &response_len TSRMLS_CC)) == NULL) {
        RETURN_FALSE;
    }

    RETURN_STRINGL(response, response_len, 0);
#endif
}
/* }}} */

/* vim: set tabstop=4 expandtab: */