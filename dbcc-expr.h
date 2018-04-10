
typedef enum
{
  DBCC_EXPR_UNARY_OP,
  DBCC_EXPR_BINARY_OP,
  DBCC_EXPR_TERNARY_OP,
  DBCC_EXPR_CONSTANT,
  DBCC_EXPR_SUBSCRIPT,
  DBCC_EXPR_DEREFERENCE,
  DBCC_EXPR_REFERENCE,
} DBCC_ExprType;

struct DBCC_Expr_Base
{
  DBCC_ExprType type;
};

struct DBCC_Expr_Constant
{
  DBCC_Expr_Base base;
  uint8_t value[8];
};

struct DBCC_Expr_BinaryOperator
{
  DBCC_Expr_Base base;
  DBCC_BinaryOperator op;
  DBCC_Expr *a, *b;
};

struct DBCC_Expr_UnaryOperator
{
  DBCC_Expr_Base base;
  DBCC_BinaryOperator op;
  DBCC_Expr *a;
};

struct DBCC_Expr_Subscript
{
  DBCC_Expr_Base base;
  DBCC_Expr *ptr;
  DBCC_Expr *index;
};

struct DBCC_Expr_Dereference
{
  DBCC_Expr_Base base;
  DBCC_Expr *ptr;
};

struct DBCC_Expr_Variable
{
  DBCC_Expr_Base base;
  DBCC_Symbol name;
};

struct DBCC_Statement
{
  DBCC_StatementType type;
  DBCC_Statement *prev, *next;
  DBCC_Statement *parent;
};

struct DBCC_CompoundStatement
{
  DBCC_Statement base;
  DBCC_Statement *first_statements;
  DBCC_Statement *last_statements;
};

struct DBCC_ForStatement
{
  DBCC_Statement base;
  DBCC_Statement *init, *next, *body;
  DBCC_Expr *condition;
};
struct DBCC_WhileStatement
{
  DBCC_Statement base;
  DBCC_Expr *condition;
  DBCC_Statement *body;
};
struct DBCC_DoWhileStatement
{
  DBCC_Statement base;
  DBCC_Statement *body;
  DBCC_Expr *condition;
};
struct DBCC_SwitchStatement
{
  DBCC_Statement base;
  DBCC_Statement *body;
  DBCC_Expr *condition;
};
struct DBCC_IfStatement
{
  DBCC_Statement base;
  DBCC_Expr *condition;
  DBCC_Statement *body;
  DBCC_Statement *else_clause;
};

struct DBCC_GotoStatement
{
  DBCC_Statement base;
  DBCC_Expr *condition;
  DBCC_Statement *body;
  DBCC_Statement *else_clause;
};

struct DBCC_LabelStatement
{
  DBCC_Statement base;
  DBCC_Expr *condition;
  DBCC_Statement *body;
  DBCC_Statement *else_clause;
};
