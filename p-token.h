/* PRIVATE HEADER:
 *   P_Token is a fully-preprocessed token (with string-literals joined).
 *   P_Context contains references to the namespaces etc.
 */
#ifndef __DBCC_P_TOKEN_H_
#define __DBCC_P_TOKEN_H_

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

  // for CHAR_CONSTANT
  uint32_t i_value;
};

#define P_TOKEN_INIT(shortname, cp) \
  (P_Token) { (cp), P_TOKEN_##shortname, NULL, {0,NULL}, NULL, 0 }

#endif
