typedef struct P_Token P_Token;
typedef struct P_Context P_Context;


P_Context * p_context_new (DBCC_Namespace *ns);

struct P_Token
{
  DBCC_CodePosition *code_position;

  // this is really an enum - whose values are given by 'lemon',
  // so we don't want to export them.
  int token_type;

  // only used for the following token types:
  //     IDENTIFIER
  //     TYPEDEF_NAME
  DBCC_Symbol *symbol;

  // only used for the following token types:
  //     STRING_LITERAL
  DBCC_String string;

  // only used for the following token types:
  //     TYPEDEF_NAME
  DBCC_Type *type;
};

