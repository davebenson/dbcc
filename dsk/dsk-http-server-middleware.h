

typedef enum DskHttpServerMiddlewareResult {
  DSK_HTTP_SERVER_MIDDLEWARE_RESULT_NEXT,
  DSK_HTTP_SERVER_MIDDLEWARE_RESULT_RESPONDED,
  DSK_HTTP_SERVER_MIDDLEWARE_RESULT_FAILED
} DskHttpServerMiddlewareResult;

typedef void (*DskHttpServerMiddlewareNextFunc) (DskHttpServerMiddlewareResult result,
                                                 void *func_data);

struct _DskHttpServerMiddlewareClass
{
  void (*handle_request)(DskHttpServerRequest *request,
                         DskHttpServerMiddlewareNextFunc func,
                         void func_data);
  void (*handle_response)(DskHttpServerRequest *request,
                         DskHttpServerMiddlewareNextFunc func,
                         void func_data);
};


/* --- JSON Parser --- */
struct DskHttpServerMiddlewareJsonParserOptions {
  char **allowed_content_types;
  dsk_boolean required;
};
extern char **dsk_http_server_middleware_json_parser_options_default_allowed_content_types;
#define DSK_HTTP_SERVER_MIDDLEWARE_JSON_PARSER_OPTIONS_INIT { \
  dsk_http_server_middleware_json_parser_options_default_allowed_content_types, \
  DSK_FALSE \
}
DskHttpServerMiddleware *dsk_http_server_middleware_json_parser (const DskHttpServerMiddlewareJsonParserOptions *options);
DskJsonValue *dsk_http_server_request_peek_json_request_body (DskHttpServerRequest *request);


