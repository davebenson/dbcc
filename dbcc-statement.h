typedef enum
{
  DBCC_STATEMENT_COMPOUND = 10000,
  DBCC_STATEMENT_FOR,
  DBCC_STATEMENT_WHILE,
  DBCC_STATEMENT_DO_WHILE,
  DBCC_STATEMENT_SWITCH,
  DBCC_STATEMENT_IF,
  DBCC_STATEMENT_GOTO,
  DBCC_STATEMENT_LABEL
} DBCC_StatementType;

typedef struct DBCC_Statement_Base DBCC_Statement_Base;
typedef struct DBCC_CompoundStatement DBCC_CompoundStatement;
typedef struct DBCC_ForStatement DBCC_ForStatement;
typedef struct DBCC_WhileStatement DBCC_WhileStatement;
typedef struct DBCC_DoWhileStatement DBCC_DoWhileStatement;
typedef struct DBCC_SwitchStatement DBCC_SwitchStatement;
typedef struct DBCC_IfStatement DBCC_IfStatement;
typedef struct DBCC_GotoStatement DBCC_GotoStatement;
typedef struct DBCC_LabelStatement DBCC_LabelStatement;

typedef union DBCC_Statement DBCC_Statement;

struct DBCC_Statement_Base
{
  DBCC_StatementType type;
};

struct DBCC_CompoundStatement
{
  DBCC_Statement_Base base;
  DBCC_Statement *first_statements;
  DBCC_Statement *last_statements;
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
  DBCC_Expr *condition;
  DBCC_Statement *body;
  DBCC_Statement *else_clause;
};

struct DBCC_LabelStatement
{
  DBCC_Statement_Base base;
  DBCC_Symbol *name;
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
};
void dbcc_statement_destroy (DBCC_Statement *statement);
