

typedef struct DBCC_Parser DBCC_Parser;

typedef struct DBCC_Parser_NewOptions DBCC_Parser_NewOptions;
struct DBCC_Parser_NewOptions
{

  DBCC_TargetEnvironment *target_env;
  void (*handle_statement)(DBCC_Statement *stmt,
                           void           *handler_data);

  /* Takes ownership of 'error',
   * so it must call dbcc_error_unref() eventually.
   */
  void (*handle_error)    (DBCC_Error     *error,
                           void           *handler_data);
  void (*handle_destroy)  (void           *handler_data);

  void *handler_data;
};

#define DBCC_PARSER_NEW_OPTIONS (DBCC_Parser_NewOptions) {  \
  .handle_statement = NULL                                  \
}

DBCC_Parser *dbcc_parser_new             (DBCC_Parser_NewOptions *options);
void         dbcc_parser_add_include_dir (DBCC_Parser   *parser,
                                          const char    *dir);
bool         dbcc_parser_parse_file      (DBCC_Parser   *parser,
                                          const char    *filename);
bool         dbcc_parser_parse_file_data (DBCC_Parser   *parser,
                                          const char    *filename,
                                          size_t         file_size,
                                          const uint8_t *file_data);
void         dbcc_parser_destroy         (DBCC_Parser   *parser);
