/* -*- Mode: C; tab-width: 8 -*- */
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
#include "macros.h"

#ifdef __linux__
/* setsockopt */
#include <sys/types.h>
#include <netinet/tcp.h>  /* TCP_NODELAY */
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <zend_exceptions.h>


static int le_redis_sock;
static zend_class_entry *hiredis_ce;

ZEND_DECLARE_MODULE_GLOBALS(hiredis)

static zend_function_entry hiredis_functions[] = {
     PHP_ME(HiRedis, __construct, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, connect, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, close, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, get, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, set, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, delete, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, pipeline, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, multi, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, exec, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, incr, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, decr, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, hset, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, hgetall, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(HiRedis, hmget, NULL, ZEND_ACC_PUBLIC)

     {NULL, NULL, NULL}
};

zend_module_entry hiredis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
     STANDARD_MODULE_HEADER,
#endif
     "hiredis",
     NULL,
     PHP_MINIT(hiredis),
     PHP_MSHUTDOWN(hiredis),
     PHP_RINIT(hiredis),
     PHP_RSHUTDOWN(hiredis),
     PHP_MINFO(hiredis),
#if ZEND_MODULE_API_NO >= 20010901
     PHP_HIREDIS_VERSION,
#endif
     STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_HIREDIS
ZEND_GET_MODULE(hiredis)
#endif

static void *tryParentize(const redisReadTask *task, zval *v) {
        // php_printf("CALLBACK: %s\n", __FUNCTION__);
        if (task && task->parent != NULL) {
                // php_printf("INSIDE\n");
                zval *parent = (zval *)task->parent;
                assert(Z_TYPE_P(parent) == IS_ARRAY);
                add_index_zval(parent, task->idx, v);
        }
        return (void*)v;
}

static void *createStringObject(const redisReadTask *task, char *str, size_t len) {

        // php_printf("CALLBACK: %s\n", __FUNCTION__);
        zval *z_ret;
        MAKE_STD_ZVAL(z_ret);

        switch(task->type) {
                case REDIS_REPLY_ERROR:
                    ZVAL_BOOL(z_ret, 0);
                    break;

                case REDIS_REPLY_STATUS:
                    ZVAL_BOOL(z_ret, 1);
                    break;

                case REDIS_REPLY_STRING:
                    ZVAL_STRINGL(z_ret, str, len, 1);
                    break;
        }
        // php_printf("created string object (%zd)[%s], z_ret=%p\n", len, str, z_ret);

        return tryParentize(task, z_ret);
}

static void *createArrayObject(const redisReadTask *task, int elements) {
        // php_printf("CALLBACK: %s\n", __FUNCTION__);
        zval *z_ret;
        MAKE_STD_ZVAL(z_ret);
        array_init(z_ret);

        return tryParentize(task, z_ret);
}

static void *createIntegerObject(const redisReadTask *task, long long value) {
        // php_printf("CALLBACK: %s\n", __FUNCTION__);
        zval *z_ret;
        MAKE_STD_ZVAL(z_ret);
        ZVAL_LONG(z_ret, value);
        return tryParentize(task, z_ret);
}

static void *createNilObject(const redisReadTask *task) {
        // php_printf("CALLBACK: %s\n", __FUNCTION__);
        zval *z_ret;
        MAKE_STD_ZVAL(z_ret);
        ZVAL_NULL(z_ret);
        return tryParentize(task, z_ret);
}

static void freeObject(void *ptr) {
        // php_printf("CALLBACK: %s\n", __FUNCTION__);
        /* TODO */
}




redisReplyObjectFunctions redisExtReplyObjectFunctions = {
    createStringObject,
    createArrayObject,
    createIntegerObject,
    createNilObject,
    freeObject
};


PHPAPI int redis_sock_disconnect(RedisSock *redis_sock TSRMLS_DC)
{
    if(!redis_sock || !redis_sock->ctx) {
            return 0;
    }

    redisFree(redis_sock->ctx);
    return 1;
}

/**
 * redis_destructor_redis_sock
 */
static void redis_destructor_redis_sock(zend_rsrc_list_entry * rsrc TSRMLS_DC)
{
    RedisSock *redis_sock = (RedisSock*) rsrc->ptr;
    /* TODO */
 /*   redis_sock_disconnect(redis_ctx TSRMLS_CC);
    redis_free_socket(redis_ctx);
    */
}

/**
 * PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(hiredis)
{
    zend_class_entry hiredis_class_entry;
    INIT_CLASS_ENTRY(hiredis_class_entry, "HiRedis", hiredis_functions);
    hiredis_ce = zend_register_internal_class(&hiredis_class_entry TSRMLS_CC);

    le_redis_sock = zend_register_list_destructors_ex(
        redis_destructor_redis_sock,
        NULL,
        redis_sock_name, module_number
    );

    return SUCCESS;
}

/**
 * PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(hiredis)
{
    return SUCCESS;
}

/**
 * PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(hiredis)
{
    return SUCCESS;
}

/**
 * PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(hiredis)
{
    return SUCCESS;
}

/**
 * PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(hiredis)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "HiRedis Support", "enabled");
    php_info_print_table_row(2, "Version", PHP_HIREDIS_VERSION);
    php_info_print_table_end();
}

/* {{{ proto HiRedis HiRedis::__construct()
    Public constructor */
PHP_METHOD(HiRedis, __construct)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}
/* }}} */

/**
 * redis_sock_get
 */
PHPAPI int redis_sock_get(zval *id, RedisSock **redis_sock TSRMLS_DC)
{

    zval **socket;
    int resource_type;

    if (Z_TYPE_P(id) != IS_OBJECT || zend_hash_find(Z_OBJPROP_P(id), "socket",
                                  sizeof("socket"), (void **) &socket) == FAILURE) {
        return -1;
    }

    *redis_sock = (RedisSock *) zend_list_find(Z_LVAL_PP(socket), &resource_type);

    if (!*redis_sock || resource_type != le_redis_sock) {
            return -1;
    }

    (*redis_sock)->ctx->reader = redisReplyReaderCreate(); /* TODO: add to phpredis object */
    redisReplyReaderSetReplyObjectFunctions((*redis_sock)->ctx->reader, &redisExtReplyObjectFunctions);

    return Z_LVAL_PP(socket);
}

/* {{{ proto boolean HiRedis::connect(string host, int port [, int timeout])
 */
PHP_METHOD(HiRedis, connect)
{
    zval *object;
    int host_len, id;
    char *host = NULL;
    long port = 6379;

    struct timeval timeout = {0L, 0L};
    RedisSock *redis_sock  = NULL;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|ll",
                                     &object, hiredis_ce, &host, &host_len, &port,
                                     &timeout.tv_sec) == FAILURE) {
       RETURN_FALSE;
    }

    redis_sock = emalloc(sizeof(RedisSock));
    redis_sock->ctx = redisConnect(host, port);
    if (!redis_sock || redis_sock->ctx->errstr != NULL) {
            printf("Error: %s\n", redis_sock->ctx->errstr);
            RETURN_FALSE;
    }

#ifdef __linux__
    int tcp_flag = 1;
    int result = setsockopt(redis_sock->ctx->fd, IPPROTO_TCP, TCP_NODELAY, (char *) &tcp_flag, sizeof(int));
#endif

    redis_sock->mode = REDIS_MODE_BLOCKING;

    /* TODO: add timeout support */
    /*
    if (timeout.tv_sec < 0L || timeout.tv_sec > INT_MAX) {
        zend_throw_exception(redis_exception_ce, "Invalid timeout", 0 TSRMLS_CC);
        RETURN_FALSE;
    }
    */

    id = zend_list_insert(redis_sock, le_redis_sock);
    add_property_resource(object, "socket", id);

    RETURN_TRUE;
}
/* }}} */

/* {{{ proto boolean HiRedis::close()
 */
PHP_METHOD(HiRedis, close)
{
    zval *object;
    RedisSock *redis_sock = NULL;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
        &object, hiredis_ce) == FAILURE) {
        RETURN_FALSE;
    }
    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    if (redis_sock_disconnect(redis_sock TSRMLS_CC)) {
        RETURN_TRUE;
    }

    RETURN_FALSE;
}
/* }}} */


/* {{{ proto boolean HiRedis::set(string key, string value)
 */
PHP_METHOD(HiRedis, set)
{
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL, *val = NULL;
    int key_len, val_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss",
                                     &object, hiredis_ce, &key, &key_len,
                                     &val, &val_len) == FAILURE) {
        RETURN_FALSE;
    }

    REDIS_SOCK_GET(redis_sock);
    REDIS_RUN(redis_sock, redis_reply_status, "SET %b %b", key, (size_t)key_len, val, (size_t)val_len);
}
/* }}} */

/* {{{ proto string HiRedis::get(string key)
 */
PHP_METHOD(HiRedis, get)
{
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL;
    int key_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
                                     &object, hiredis_ce,
                                     &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    REDIS_SOCK_GET(redis_sock);
    REDIS_RUN(redis_sock, redis_reply_string, "GET %b", key, (size_t)key_len);
}
/* }}} */

/* {{{ proto long HiRedis::incr(long key)
 */
PHP_METHOD(HiRedis, incr)
{
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL;
    int key_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
                                     &object, hiredis_ce,
                                     &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    REDIS_SOCK_GET(redis_sock);
    REDIS_RUN(redis_sock, redis_reply_long, "INCR %b", key, (size_t)key_len);
}
/* }}} */

/* {{{ proto long HiRedis::decr(long key)
 */
PHP_METHOD(HiRedis, decr)
{
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL;
    int key_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
                                     &object, hiredis_ce,
                                     &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    REDIS_SOCK_GET(redis_sock);
    REDIS_RUN(redis_sock, redis_reply_long, "DECR %b", key, (size_t)key_len);
}
/* }}} */


PHPAPI int
redis_reply_string(zval *return_value, redis_mode mode, zval *z_reply, zval **z_args) {

    if(Z_TYPE_P(z_reply) == IS_STRING) { /* valid */
	    if(mode == REDIS_MODE_BLOCKING) { /* copy directly into return_value */
		    Z_TYPE_P(return_value) = IS_STRING;
		    Z_STRVAL_P(return_value) = Z_STRVAL_P(z_reply);
		    Z_STRLEN_P(return_value) = Z_STRLEN_P(z_reply);
	    } else { /* append */
		    add_next_index_stringl(return_value, Z_STRVAL_P(z_reply), Z_STRLEN_P(z_reply), 0);
	    }
            efree(z_reply);
            return 0;
    } else { /* invalid */
            zval_dtor(z_reply);
            efree(z_reply);
	    if(mode == REDIS_MODE_BLOCKING) { /* return false */
            	ZVAL_BOOL(return_value, 0);
	    } else { /* append false. */
	        add_next_index_bool(return_value, 0);
	    }
            return 1;
    }
}


PHPAPI int
redis_reply_long(zval *return_value, redis_mode mode, zval *z_reply, zval **z_args) {
    if(Z_TYPE_P(z_reply) == IS_LONG) { /* valid */
	    if(mode == REDIS_MODE_BLOCKING) { /* copy directly into return_value */
		    ZVAL_LONG(return_value, Z_LVAL_P(z_reply));
	    } else { /* append long */
		    add_next_index_long(return_value, Z_LVAL_P(z_reply));
	    }
            efree(z_reply);
            return 0;
    } else {
            zval_dtor(z_reply);
            efree(z_reply);
	    if(mode == REDIS_MODE_BLOCKING) { /* return false */
            	ZVAL_BOOL(return_value, 0);
	    } else { /* append false. */
	        add_next_index_bool(return_value, 0);
	    }
            return 1;
    }
}

PHPAPI int
redis_reply_status(zval *return_value, redis_mode mode, zval *z_reply, zval **z_args) {
        int success = 0;
        if(z_reply && Z_TYPE_P(z_reply) == IS_BOOL) {
                success = Z_BVAL_P(z_reply);
        }

        zval_dtor(z_reply);
        efree(z_reply);

	if(mode == REDIS_MODE_BLOCKING) { /* return bool directly */
                ZVAL_BOOL(return_value, success);
        } else {
                add_next_index_bool(return_value, success); /* append bool */
        }
        return 0;
}

PHPAPI int
redis_reply_zip(zval *return_value, redis_mode mode, zval *z_reply, zval **z_args) {

        zval *z_ret;
        MAKE_STD_ZVAL(z_ret);

        int use_atof = 0; /* FIXME */

        if(Z_TYPE_P(z_reply) != IS_ARRAY) {
                ZVAL_NULL(z_ret);
        } else {
            array_init(z_ret);

            HashTable *keytable = Z_ARRVAL_P(z_reply);

            for(zend_hash_internal_pointer_reset(keytable);
                            zend_hash_has_more_elements(keytable) == SUCCESS;
                            zend_hash_move_forward(keytable)) {

                    char *tablekey, *hkey, *hval;
                    int tablekey_len, hkey_len, hval_len;
                    unsigned long idx;
                    int type;
                    zval **z_value_pp;

                    type = zend_hash_get_current_key_ex(keytable, &tablekey, &tablekey_len, &idx, 0, NULL);
                    if(zend_hash_get_current_data(keytable, (void**)&z_value_pp) == FAILURE) {
                            continue; 	/* this should never happen, according to the PHP people. */
                    }

                    /* get current value, a key */
                    hkey = Z_STRVAL_PP(z_value_pp);
                    hkey_len = Z_STRLEN_PP(z_value_pp);

                    /* move forward */
                    zend_hash_move_forward(keytable);

                    /* fetch again */
                    type = zend_hash_get_current_key_ex(keytable, &tablekey, &tablekey_len, &idx, 0, NULL);
                    if(zend_hash_get_current_data(keytable, (void**)&z_value_pp) == FAILURE) {
                            continue; 	/* this should never happen, according to the PHP people. */
                    }

                    /* get current value, a hash value now. */
                    hval = Z_STRVAL_PP(z_value_pp);
                    hval_len = Z_STRLEN_PP(z_value_pp);

                    if(use_atof) {
                            add_assoc_double_ex(z_ret, hkey, 1+hkey_len, atof(hval));
                    } else {
                            add_assoc_stringl_ex(z_ret, hkey, 1+hkey_len, hval, hval_len, 1);
                    }
            }

        }
	if(mode == REDIS_MODE_BLOCKING) { /* copy z_ret into return_value directly */
		*return_value = *z_ret;
		zval_copy_ctor(return_value);
		zval_dtor(z_ret);
		efree(z_ret);
	} else { /* append z_ret to return_value array */
		add_next_index_zval(return_value, z_ret);
	}

        return 0;
}

PHPAPI int
redis_reply_zip_closure(zval *return_value, redis_mode mode, zval *z_reply, zval **z_args) {

	int i;

        zval *z_ret;
        MAKE_STD_ZVAL(z_ret);

        if(Z_TYPE_P(z_reply) != IS_ARRAY) {
                ZVAL_NULL(z_ret);
        } else {
            array_init(z_ret);

            HashTable *ht_vals = Z_ARRVAL_P(z_reply);

	    /* zip together z_args as keys, z_reply as values */

            for(i = 1, zend_hash_internal_pointer_reset(ht_vals);
                            zend_hash_has_more_elements(ht_vals) == SUCCESS;
                            i++, zend_hash_move_forward(ht_vals)) {

                    zval **z_val_pp;

                    if(zend_hash_get_current_data(ht_vals, (void**)&z_val_pp) == FAILURE) {
                            continue; 	/* this should never happen, according to the PHP people. */
                    }

                    add_assoc_stringl_ex(z_ret, Z_STRVAL_P(z_args[i]), 1+Z_STRLEN_P(z_args[i]), Z_STRVAL_PP(z_val_pp), Z_STRLEN_PP(z_val_pp), 1);
            }
        }

	if(mode == REDIS_MODE_BLOCKING) { /* copy z_ret into return_value directly */
		*return_value = *z_ret;
		zval_copy_ctor(return_value);
		zval_dtor(z_ret);
		efree(z_ret);
	} else { /* append z_ret to return_value array */
		add_next_index_zval(return_value, z_ret);
	}

	efree(z_args);

        return 0;
}

void redis_enqueue(RedisSock *redis_sock, void *fun, zval **z_args) {

	redis_command *c = ecalloc(1, sizeof(redis_command));
	c->fun = fun;

	if(z_args) {
		c->z_args = z_args;
	}

	/* enqueue */
	if(redis_sock->queue_tail == NULL) {
		redis_sock->queue_tail = redis_sock->queue_head = c;
	} else {
		redis_sock->queue_tail->next = c;
	}
	redis_sock->enqueued_commands++;
}


PHP_METHOD(HiRedis, pipeline)
{
    zval *object = getThis();
    RedisSock *redis_sock;

    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    if(redis_sock->mode == REDIS_MODE_BLOCKING) {
            redis_sock->mode = REDIS_MODE_PIPELINE;
            redis_sock->enqueued_commands = 0;
	    redis_sock->queue_head = NULL;
	    redis_sock->queue_tail = NULL;
            RETURN_ZVAL(object, 1, 0);
    }
    RETURN_FALSE;
}

PHP_METHOD(HiRedis, multi)
{
    zval *object = getThis();
    RedisSock *redis_sock;

    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    if(redis_sock->mode == REDIS_MODE_BLOCKING) {
            zval *z_reply = redisCommand(redis_sock->ctx, "MULTI");
            if(Z_TYPE_P(z_reply) == IS_BOOL && Z_BVAL_P(z_reply) == 1) {
                    efree(z_reply);
                    redis_sock->mode = REDIS_MODE_TRANSACTION;
                    redis_sock->enqueued_commands = 0;
		    redis_sock->queue_head = NULL;
		    redis_sock->queue_tail = NULL;
                    RETURN_ZVAL(object, 1, 0);
            } else {
                    zval_dtor(z_reply);
                    efree(z_reply);
            }
    }
    RETURN_FALSE;
}

PHP_METHOD(HiRedis, exec)
{
    zval *object = getThis(), *z_reply, *z_raw_tab;
    RedisSock *redis_sock;
    int i, count;
	redis_mode mode;

    if (redis_sock_get(object, &redis_sock TSRMLS_CC) < 0) {
        RETURN_FALSE;
    }

    count = redis_sock->enqueued_commands;
    redis_sock->enqueued_commands = 0;

    mode = redis_sock->mode;
    redis_sock->mode = REDIS_MODE_BLOCKING;

    if(mode != REDIS_MODE_BLOCKING) {
	    MAKE_STD_ZVAL(z_raw_tab); // collection array for values
    }

    switch(mode) {
            case REDIS_MODE_BLOCKING:
                    RETURN_FALSE;

            case REDIS_MODE_TRANSACTION:
                    z_reply = redisCommand(redis_sock->ctx, "EXEC");
                    Z_TYPE_P(z_raw_tab) = IS_ARRAY;
                    Z_ARRVAL_P(z_raw_tab) = Z_ARRVAL_P(z_reply);
                    efree(z_reply);
                    break;

            case REDIS_MODE_PIPELINE:
                    array_init(z_raw_tab);

                    for(i = 0; i < count; ++i) {
                            redisGetReply(redis_sock->ctx, (void**)&z_reply);
                            add_next_index_zval(z_raw_tab, z_reply);
                    }
                    break;
    }

    array_init(return_value);

    redis_command *c;
    HashTable *raw_ht = Z_ARRVAL_P(z_raw_tab);
    for(c = redis_sock->queue_head, zend_hash_internal_pointer_reset(raw_ht); c; c = c->next, zend_hash_move_forward(raw_ht)) {

	    zval **z_value_pp;
	    zend_hash_get_current_data(raw_ht, (void**)&z_value_pp);

	c->fun(return_value, mode, *z_value_pp, c->z_args);
    }
}

/* {{{ proto long HiRedis::hset(string key, string field, string val)
 */
PHP_METHOD(HiRedis, hset)
{
    zval *object;
    RedisSock *redis_sock;
    char *key, *field, *val;
    int key_len, field_len, val_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osss",
                                     &object, hiredis_ce,
                                     &key, &key_len, &field, &field_len, &val, &val_len) == FAILURE) {
        RETURN_FALSE;
    }

    REDIS_SOCK_GET(redis_sock);
    REDIS_RUN(redis_sock, redis_reply_long, "HSET %b %b %b",
                    key, (size_t)key_len,
                    field, (size_t)field_len,
                    val, (size_t)val_len);
}
/* }}} */

/* {{{ proto array HiRedis::hgetall(string key)
 */
PHP_METHOD(HiRedis, hgetall)
{
    zval *object;
    RedisSock *redis_sock;
    char *key;
    int key_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
                                     &object, hiredis_ce,
                                     &key, &key_len) == FAILURE) {
        RETURN_FALSE;
    }

    REDIS_SOCK_GET(redis_sock);
    REDIS_RUN(redis_sock, redis_reply_zip, "HGETALL %b", key, (size_t)key_len);
}
/* }}} */

/* {{{ proto array HiRedis::hmget(string key, array fields)
 */
PHP_METHOD(HiRedis, hmget) {
	redis_varg_run(INTERNAL_FUNCTION_PARAM_PASSTHRU, "HMGET", redis_reply_zip_closure, 1);
}


PHP_METHOD(HiRedis, delete) {

	redis_varg_run(INTERNAL_FUNCTION_PARAM_PASSTHRU, "DEL", redis_reply_long, 0);
}

PHPAPI void
redis_varg_run(INTERNAL_FUNCTION_PARAMETERS, char *keyword, void *fun, int keep_args) {


    zval *object = getThis();
    RedisSock *redis_sock;
    int argc = ZEND_NUM_ARGS(), i;

    const char **args;
    size_t *arglen;

    zval *z_reply;

    /* get all args into the zval array z_args */
    zval **z_args = emalloc(argc * sizeof(zval*));
    if(zend_get_parameters_array(ht, argc, z_args) == FAILURE) {
        efree(z_args);
        RETURN_FALSE;
    }

    /* check if there is only one argument, an array */
    if(argc == 2 && Z_TYPE_P(z_args[1]) == IS_ARRAY) {
	    zval *z_key = z_args[0];
	    zval *z_array = z_args[1];
	    efree(z_args);
	    argc = zend_hash_num_elements(Z_ARRVAL_P(z_array));
	    z_args = ecalloc(argc, sizeof(zval*));

	    MAKE_STD_ZVAL(z_args[0]);
	    *z_args[0] = *z_key;
	    zval_copy_ctor(z_args[0]);

	    for(i = 1; i <= argc; ++i) {
		    zval **z_i_pp;
		    if(zend_hash_index_find(Z_ARRVAL_P(z_array), i-1, (void **)&z_i_pp) == FAILURE) {
			    efree(z_args);
			    RETURN_FALSE;
		    }
		    MAKE_STD_ZVAL(z_args[i]);
		    *z_args[i] = **z_i_pp;
		    zval_copy_ctor(z_args[i]);
	    }
    }

    /* copy all args as strings */
    args = emalloc((argc+1) * sizeof(char*));
    arglen = emalloc((argc+1) * sizeof(size_t));
    for(i = 0; i < argc; ++i) {
        convert_to_string(z_args[i]);
        args[i+1] = Z_STRVAL_P(z_args[i]);
        arglen[i+1] = (size_t)Z_STRLEN_P(z_args[i]);
    }

    args[0] = keyword;
    arglen[0] = strlen(keyword);

    REDIS_SOCK_GET(redis_sock);
    if(keep_args) {
	    REDIS_RUN_ARGS(redis_sock, fun, argc+1, args, arglen, z_args);
    } else {
	    REDIS_RUN_ARGS(redis_sock, fun, argc+1, args, arglen, NULL);
    }
}

/* vim: set tabstop=8 expandtab: */
