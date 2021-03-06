/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Omar Kilani <omar@php.net>                                   |
  +----------------------------------------------------------------------+
*/

/* $Id: json.c,v 1.9 2006/03/18 04:15:16 omar Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_smart_str.h"
#include "utf8_to_utf16.h"
#include "JSON_parser.h"
#include "php_json.h"

/* If you declare any globals in php_json.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(json)
*/

/* True global resources - no need for thread safety here */
static int le_json;

/* {{{ json_functions[]
 *
 * Every user visible function must have an entry in json_functions[].
 */
const zend_function_entry  json_functions[] = {
    PHP_FE(fjson_encode, NULL)
    PHP_FE(fjson_decode, NULL)
    {NULL, NULL, NULL}  /* Must be the last line in json_functions[] */
};
/* }}} */

/* {{{ json_module_entry
 */
zend_module_entry fjson_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "fjson",
    json_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_MINFO(fjson),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_JSON_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_JSON
ZEND_GET_MODULE(fjson)
#endif

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(fjson)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "fjson support", "enabled");
    php_info_print_table_row(2, "fjson version", PHP_JSON_VERSION);
    php_info_print_table_end();
}
/* }}} */

static void json_encode_r(smart_str *buf, zval *val TSRMLS_DC);
static void json_escape_string(smart_str *buf, char *s, int len TSRMLS_DC);

static int json_determine_array_type(zval **val TSRMLS_DC) {
    int i;
    HashTable *myht;

    if (Z_TYPE_PP(val) == IS_ARRAY) {
        myht = HASH_OF(*val);
    } else {
        myht = Z_OBJPROP_PP(val);
        return 1;
    }

    i = myht ? zend_hash_num_elements(myht) : 0;
    if (i > 0) {
        char *key;
        ulong index, idx;
        uint key_len;
        HashPosition pos;

        zend_hash_internal_pointer_reset_ex(myht, &pos);
        idx = 0;
        for (;; zend_hash_move_forward_ex(myht, &pos)) {
            i = zend_hash_get_current_key_ex(myht, &key, &key_len, &index, 0, &pos);
            if (i == HASH_KEY_NON_EXISTANT)
                break;

            if (i == HASH_KEY_IS_STRING) {
                return 1;
            } else {
                if (index != idx) {
                    return 1;
                }
            }
            idx++;
        }
    }

    return 0;
}

static void json_encode_array(smart_str *buf, zval **val TSRMLS_DC) {
    int i, r;
    HashTable *myht;

    if (Z_TYPE_PP(val) == IS_ARRAY) {
        myht = HASH_OF(*val);
        r = json_determine_array_type(val TSRMLS_CC);
    } else {
        myht = Z_OBJPROP_PP(val);
        r = 1;
    }

    if (r == 0)
    {
        smart_str_appendc(buf, '[');
    }
    else
    {
        smart_str_appendc(buf, '{');
    }

    i = myht ? zend_hash_num_elements(myht) : 0;
    if (i > 0) {
        char *key;
        zval **data;
        ulong index;
        uint key_len;
        HashPosition pos;
        int need_comma = 0;

        zend_hash_internal_pointer_reset_ex(myht, &pos);
        for (;; zend_hash_move_forward_ex(myht, &pos)) {
            i = zend_hash_get_current_key_ex(myht, &key, &key_len, &index, 0, &pos);
            if (i == HASH_KEY_NON_EXISTANT)
                break;

            if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) == SUCCESS) {
                if (r == 0) {
                    if (need_comma) {
                        smart_str_appendc(buf, ',');
                    } else {
                        need_comma = 1;
                    }
 
                    json_encode_r(buf, *data TSRMLS_CC);
                } else if (r == 1) {
                    if (i == HASH_KEY_IS_STRING) {
                        if (key[0] == '\0') {
                            /* Skip protected and private members. */
                            continue;
                        }

                        if (need_comma) {
                            smart_str_appendc(buf, ',');
                        } else {
                            need_comma = 1;
                        }

                        json_escape_string(buf, key, key_len - 1 TSRMLS_CC);
                        smart_str_appendc(buf, ':');

                        json_encode_r(buf, *data TSRMLS_CC);
                    } else {
                        if (need_comma) {
                            smart_str_appendc(buf, ',');
                        } else {
                            need_comma = 1;
                        }
                        
                        smart_str_appendc(buf, '"');
                        smart_str_append_long(buf, (long) index);
                        smart_str_appendc(buf, '"');
                        smart_str_appendc(buf, ':');

                        json_encode_r(buf, *data TSRMLS_CC);
                    }
                }
            }
        }
    }

    if (r == 0)
    {
        smart_str_appendc(buf, ']');
    }
    else
    {
        smart_str_appendc(buf, '}');
    }
}

#define REVERSE16(us) (((us & 0xf) << 12) | (((us >> 4) & 0xf) << 8) | (((us >> 8) & 0xf) << 4) | ((us >> 12) & 0xf))

static void json_escape_string(smart_str *buf, char *s, int len TSRMLS_DC)
{
    int pos = 0;
    unsigned short us;
    unsigned short *utf16;

    if (len == 0)
    {
        smart_str_appendl(buf, "\"\"", 2);
        return;
    }

    utf16 = (unsigned short *) emalloc(len * sizeof(unsigned short));

    len = utf8_to_utf16(utf16, s, len);
    if (len <= 0)
    {
        if (utf16)
        {
            efree(utf16);
        }

        smart_str_appendl(buf, "\"\"", 2);
        return;
    }

    smart_str_appendc(buf, '"');

    while(pos < len)
    {
        us = utf16[pos++];

        switch (us)
        {
            case '"':
                {
                    smart_str_appendl(buf, "\\\"", 2);
                }
                break;
            case '\\':
                {
                    smart_str_appendl(buf, "\\\\", 2);
                }
                break;
            case '/':
                {
                    smart_str_appendl(buf, "\\/", 2);
                }
                break;
            case '\b':
                {
                    smart_str_appendl(buf, "\\b", 2);
                }
                break;
            case '\f':
                {
                    smart_str_appendl(buf, "\\f", 2);
                }
                break;
            case '\n':
                {
                    smart_str_appendl(buf, "\\n", 2);
                }
                break;
            case '\r':
                {
                    smart_str_appendl(buf, "\\r", 2);
                }
                break;
            case '\t':
                {
                    smart_str_appendl(buf, "\\t", 2);
                }
                break;
            default:
                {
                    if (us < ' ' || (us & 127) == us)
                    {
                        smart_str_appendc(buf, (unsigned char) us);
                    }
                    else
                    {
                        smart_str_appendl(buf, "\\u", 2);
                        us = REVERSE16(us);

                        smart_str_appendc(buf, digits[us & ((1 << 4) - 1)]);
                        us >>= 4;
                        smart_str_appendc(buf, digits[us & ((1 << 4) - 1)]);
                        us >>= 4;
                        smart_str_appendc(buf, digits[us & ((1 << 4) - 1)]);
                        us >>= 4;
                        smart_str_appendc(buf, digits[us & ((1 << 4) - 1)]);
                    }
                }
                break;
        }
    }

    smart_str_appendc(buf, '"');
    efree(utf16);
}

static void json_encode_r(smart_str *buf, zval *val TSRMLS_DC) {
    switch (Z_TYPE_P(val)) {
        case IS_NULL:
            // install(buf, "null", 4);

            convert_to_string(val)
            json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            break;
        case IS_BOOL:

            if (Z_BVAL_P(val))
            {
                // smart_str_appendl(buf, "true", 4);
                convert_to_string(val)
                json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            }
            else
            {
                // smart_str_appendl(buf, "false", 5);
                convert_to_string(val)
                Z_STRLEN_P(val) = 1;
                Z_STRVAL_P(val) = estrndup("0", 1);
                json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            }

            break;
        case IS_LONG:
            // smart_str_append_long(buf, Z_LVAL_P(val));
            // zval zval_s;  
            // Z_STRLEN_P(zval_s) = ;
            // Z_STRVAL_P(zval_s) = estrndup("1", 1);
            convert_to_string(val)
            json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            break;
        case IS_DOUBLE:
            {
                // char *d = NULL;
                // int len;
                // double dbl = Z_DVAL_P(val);

                // if (!zend_isinf(dbl) && !zend_isnan(dbl))
                // {
                //     len = spprintf(&d, 0, "%.9g", dbl);
                //     if (d)
                //     {
                //         smart_str_appendl(buf, d, len);
                //         efree(d);
                //     }
                // }
                // else
                // {
                //     zend_error(E_WARNING, "[json] (json_encode_r) double %.9g does not conform to the JSON spec, encoded as 0.", dbl);
                //     smart_str_appendc(buf, '0');
                // }
                convert_to_string(val)
                json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            }
            break;
        case IS_STRING:
            json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            break;
        case IS_ARRAY:
        case IS_OBJECT:
            json_encode_array(buf, &val TSRMLS_CC);
            break;
        default:
            zend_error(E_WARNING, "[json] (json_encode_r) type is unsupported, encoded as null.");
            // smart_str_appendl(buf, "null", 4);
            convert_to_string(val)
            json_escape_string(buf, Z_STRVAL_P(val), Z_STRLEN_P(val) TSRMLS_CC);
            break;
    }

    return;
}

PHP_FUNCTION(fjson_encode)
{
    zval *parameter;
    smart_str buf = {0};

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &parameter) == FAILURE) {
        return;
    }

    json_encode_r(&buf, parameter TSRMLS_CC);

    ZVAL_STRINGL(return_value, buf.c, buf.len, 1);

    smart_str_free(&buf);
}

PHP_FUNCTION(fjson_decode)
{
    char *parameter;
    int parameter_len, utf16_len;
    zend_bool assoc = 0; /* return JS objects as PHP objects by default */
    zval *z;
    unsigned short *utf16;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &parameter, &parameter_len, &assoc) == FAILURE) {
        return;
    }

    if (!parameter_len)
    {
        RETURN_NULL();
    }

    utf16 = (unsigned short *) emalloc((parameter_len+1) * sizeof(unsigned short));

    utf16_len = utf8_to_utf16(utf16, parameter, parameter_len);
    if (utf16_len <= 0)
    {
        if (utf16)
        {
            efree(utf16);
        }

        RETURN_NULL();
    }

    ALLOC_INIT_ZVAL(z);
    if (JSON_parser(z, utf16, utf16_len, assoc TSRMLS_CC))
    {
        *return_value = *z;

        FREE_ZVAL(z);
        efree(utf16);
    }
    else
    {
        zval_dtor(z);
        FREE_ZVAL(z);
        efree(utf16);
        RETURN_NULL();
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4
 * vim<600: noet sw=4 ts=4
 */
