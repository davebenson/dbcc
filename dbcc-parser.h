

typedef struct DBCC_Parser DBCC_Parser;

typedef struct DBCC_Parser_Handlers DBCC_Parser_Handlers;
struct DBCC_Parser_Handlers
{
  void (*handle_statement)(DBCC_Statement *stmt,
                           void           *handler_data);

  /* Takes ownership of 'error',
   * so it must call dbcc_error_unref() eventually.
   */
  void (*handle_error)    (DBCC_Error     *error,
                           void           *handler_data);
  void (*handle_destroy)  (void           *handler_data);
};

DBCC_Parser *dbcc_parser_new             (DBCC_Parser_Handlers *handlers,
                                          void          *handler_data);
void         dbcc_parser_add_include_dir (DBCC_Parser   *parser,
                                          const char    *dir);
bool         dbcc_parser_parse_file      (DBCC_Parser   *parser,
                                          const char    *filename);
void         dbcc_parser_destroy         (DBCC_Parser   *parser);
