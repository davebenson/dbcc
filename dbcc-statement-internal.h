
typedef enum
{
  DBCC_STATEMENT_INTERNAL_LABELLED = DBCC_STATEMENT_INTERNAL_START,
  DBCC_STATEMENT_INTERNAL_CASE,
  DBCC_STATEMENT_INTERNAL_DEFAULT,
} DBCC_StatementType_Internal;

DBCC_Statement *dbcc_statement_internal_new_labelled (DBCC_Symbol *name,
                                                     DBCC_Statement *substatement);
DBCC_Statement *dbcc_statement_internal_new_case (DBCC_Expr *expr,
                                                     DBCC_Statement *substatement);
DBCC_Statement *dbcc_statement_internal_new_default (DBCC_Statement *substatement);
