
#include "dbcc.h"
#include "p-token.h"

void *DBCC_Lemon_ParserAlloc(void *(*mallocProc)(size_t size));
void DBCC_Lemon_ParserInit(void *yypParser);
void DBCC_Lemon_Parser(
  void *parser,                   /* The parser */
  int token_type,                 /* The major token code number */
  P_Token token,
  P_Context *context
);
void DBCC_Lemon_ParserFree(
  void *parser,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
);
