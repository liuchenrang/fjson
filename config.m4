PHP_ARG_ENABLE(json, whether to enable fjson support,
[ --enable-json Enable fjson  support])

if test "$PHP_JSON" != "no"; then
    AC_DEFINE(HAVE_JSON, 1, [Whether you have Hello World])
    PHP_NEW_EXTENSION(json, json.c utf8_to_utf16.c utf8_decode.c JSON_parser.c, $ext_shared)
fi