typedef enum
{
  DBCC_STATEMENT_COMPOUND = 10000,
  DBCC_STATEMENT_FOR,
  DBCC_STATEMENT_WHILE,
  DBCC_STATEMENT_DO_WHILE,
  DBCC_STATEMENT_SWITCH,
  DBCC_STATEMENT_IF,
  DBCC_STATEMENT_GOTO,
  DBCC_STATEMENT_LABEL,
  DBCC_STATEMENT_EXPR
} DBCC_StatementType;

#define DBCC_STATEMENT_INTERNAL_START    20000

typedef struct DBCC_Statement_Base DBCC_Statement_Base;
typedef struct DBCC_CompoundStatement DBCC_CompoundStatement;
typedef struct DBCC_ForStatement DBCC_ForStatement;
typedef struct DBCC_WhileStatement DBCC_WhileStatement;
typedef struct DBCC_DoWhileStatement DBCC_DoWhileStatement;
typedef struct DBCC_SwitchStatement DBCC_SwitchStatement;
typedef struct DBCC_IfStatement DBCC_IfStatement;
typedef struct DBCC_GotoStatement DBCC_GotoStatement;
typedef struct DBCC_LabelStatement DBCC_LabelStatement;
typedef struct DBCC_ExprStatement DBCC_ExprStatement;

typedef union DBCC_Statement DBCC_Statement;

struct DBCC_Statement_Base
{
  DBCC_StatementType type;
  DBCC_CodePosition *code_position;
};

struct DBCC_CompoundStatement
{
  DBCC_Statement_Base base;
  unsigned n_statements;
  DBCC_Statement **statements;
};

struct DBCC_ForStatement
{
  DBCC_Statement_Base base;
  DBCC_Statement *init, *next, *body;
  DBCC_Expr *condition;
};
struct DBCC_WhileStatement
{
  DBCC_Statement_Base base;
  DBCC_Expr *condition;
  DBCC_Statement *body;
};
struct DBCC_DoWhileStatement
{
  DBCC_Statement_Base base;
  DBCC_Statement *body;
  DBCC_Expr *condition;
};
struct DBCC_SwitchStatement
{
  DBCC_Statement_Base base;
  DBCC_Statement *body;
  DBCC_Expr *condition;
};
struct DBCC_IfStatement
{
  DBCC_Statement_Base base;
  DBCC_Expr *condition;
  DBCC_Statement *body;
  DBCC_Statement *else_clause;
};

struct DBCC_GotoStatement
{
  DBCC_Statement_Base base;
  DBCC_Symbol *label;
  DBCC_Statement *target;               // if resolved
};

struct DBCC_LabelStatement
{
  DBCC_Statement_Base base;
  DBCC_Symbol *name;
};
struct DBCC_ExprStatement
{
  DBCC_Statement_Base base;
  DBCC_Expr *expr;
};
union DBCC_Statement
{
  DBCC_StatementType type;
  DBCC_Statement_Base base;
  DBCC_CompoundStatement v_compound;
  DBCC_ForStatement v_for;
  DBCC_WhileStatement v_while;
  DBCC_DoWhileStatement v_do_while;
  DBCC_SwitchStatement v_switch;
  DBCC_IfStatement v_if;
  DBCC_GotoStatement v_goto;
  DBCC_LabelStatement v_label;
  DBCC_ExprStatement v_expr;
};
DBCC_Statement *dbcc_statement_new_empty          (void);
DBCC_Statement *dbcc_statement_new_compound       (size_t n_statements,
                                                   DBCC_Statement **statements,
                                                   bool      defines_scope);
DBCC_Statement *dbcc_statement_new_expr           (DBCC_Expr *expr);
DBCC_Statement *dbcc_statement_new_label          (DBCC_Symbol *symbol);
DBCC_Statement *dbcc_statement_new_case           (DBCC_Expr *value);
DBCC_Statement *dbcc_statement_new_default        (DBCC_CodePosition  *cp);
DBCC_Statement *dbcc_statement_new_declaration    (DBCC_StorageClassSpecifier storage_specs,
                                                   DBCC_Type          *type,
                                                   DBCC_Symbol        *name,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);
DBCC_Statement *dbcc_statement_new_if             (DBCC_Expr          *expr,
                                                   DBCC_Statement     *body,
                                                   DBCC_Statement     *else_body,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);
DBCC_Statement *dbcc_statement_new_for            (DBCC_Statement     *init,
                                                   DBCC_Expr          *condition,
                                                   DBCC_Statement     *advance,
                                                   DBCC_Statement     *body,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);
DBCC_Statement *dbcc_statement_new_switch         (DBCC_Expr          *expr,
                                                   DBCC_Statement     *switch_body,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);
DBCC_Statement *dbcc_statement_new_while          (DBCC_Expr          *expr,
                                                   DBCC_Statement     *body,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);
DBCC_Statement *dbcc_statement_new_do_while       (DBCC_Statement     *body,
                                                   DBCC_Expr          *expr,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);
DBCC_Statement *dbcc_statement_new_goto           (DBCC_Symbol        *target,
                                                   DBCC_CodePosition  *cp);
DBCC_Statement *dbcc_statement_new_jump           (DBCC_Symbol        *target,
                                                   DBCC_CodePosition  *cp);
DBCC_Statement *dbcc_statement_new_continue       (DBCC_CodePosition  *cp);
DBCC_Statement *dbcc_statement_new_break          (DBCC_CodePosition  *cp);
DBCC_Statement *dbcc_statement_new_return         (DBCC_Expr          *opt_expr,
                                                   DBCC_CodePosition  *cp,
                                                   DBCC_Error        **error);

void dbcc_statement_destroy (DBCC_Statement *statement);
