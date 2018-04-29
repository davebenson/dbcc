#ifndef __DBCC_EXPR_H_
#define __DBCC_EXPR_H_

typedef union DBCC_Expr DBCC_Expr;

typedef struct DBCC_Expr_Base DBCC_Expr_Base;
typedef struct DBCC_BinaryOperatorExpr DBCC_BinaryOperatorExpr;
typedef struct DBCC_UnaryOperatorExpr DBCC_UnaryOperatorExpr;
typedef struct DBCC_SubscriptExpr DBCC_SubscriptExpr;
typedef struct DBCC_DereferenceExpr DBCC_DereferenceExpr;
typedef struct DBCC_VariableExpr DBCC_VariableExpr;
typedef struct DBCC_StructuredInitializerPiece DBCC_StructuredInitializerPiece;
typedef struct DBCC_StructuredInitializer DBCC_StructuredInitializer;
typedef struct DBCC_StructuredInitializerExpr DBCC_StructuredInitializerExpr;

typedef enum
{
  DBCC_EXPR_TYPE_UNARY_OP,
  DBCC_EXPR_TYPE_BINARY_OP,
  DBCC_EXPR_TYPE_TERNARY_OP,
  DBCC_EXPR_TYPE_CONSTANT,
  DBCC_EXPR_TYPE_SUBSCRIPT,
  DBCC_EXPR_TYPE_DEREFERENCE,
  DBCC_EXPR_TYPE_REFERENCE,
} DBCC_Expr_Type;

struct DBCC_Expr_Base
{
  DBCC_Expr_Type type;
  DBCC_Type *value_type;
  void *constant_value;
  DBCC_CodePosition *code_position;
};

struct DBCC_BinaryOperatorExpr
{
  DBCC_Expr_Base base;
  DBCC_BinaryOperator op;
  DBCC_Expr *a, *b;
};

struct DBCC_UnaryOperatorExpr
{
  DBCC_Expr_Base base;
  DBCC_BinaryOperator op;
  DBCC_Expr *a;
};

struct DBCC_SubscriptExpr
{
  DBCC_Expr_Base base;
  DBCC_Expr *ptr;
  DBCC_Expr *index;
};

struct DBCC_DereferenceExpr
{
  DBCC_Expr_Base base;
  DBCC_Expr *ptr;
};

struct DBCC_VariableExpr
{
  DBCC_Expr_Base base;
  DBCC_Symbol name;
};

union DBCC_Expr
{
  DBCC_Expr_Type expr_type;
  DBCC_Expr_Base base;
  DBCC_DereferenceExpr v_dereference;
};

typedef struct DBCC_GenericAssociation DBCC_GenericAssociation;
struct DBCC_GenericAssociation
{
  DBCC_Type *type;
  DBCC_Expr *expr;
};

DBCC_Expr *dbcc_expr_new_alignof_type     (DBCC_Type          *type);
DBCC_Expr *dbcc_expr_new_binary_operator  (DBCC_BinaryOperator op,
                                           DBCC_Expr          *a,
                                           DBCC_Expr          *b);
DBCC_Expr *dbcc_expr_new_inplace_binary   (DBCC_InplaceBinaryOperator op,
                                           DBCC_Expr          *in_out,
                                           DBCC_Expr          *b);
DBCC_Expr *dbcc_expr_new_call             (DBCC_Expr          *head,
                                           unsigned            n_args,
                                           DBCC_Expr         **args);
DBCC_Expr *dbcc_expr_new_cast             (DBCC_Type          *type,
                                           DBCC_Expr          *value);
DBCC_Expr *dbcc_expr_new_generic_selection(DBCC_Expr          *expr,
                                           size_t              n_associations,
                                           DBCC_GenericAssociation *associations,
                                           DBCC_Expr          *default_expr);
DBCC_Expr *dbcc_expr_new_inplace_unary    (DBCC_InplaceUnaryOperator  op,
                                           DBCC_Expr          *in_out);
DBCC_Expr *dbcc_expr_new_member_access    (DBCC_Expr          *expr,
                                           DBCC_Symbol        *name);
DBCC_Expr *dbcc_expr_new_pointer_access   (DBCC_Expr          *expr,
                                           DBCC_Symbol        *name);
DBCC_Expr *dbcc_expr_new_sizeof_expr      (DBCC_Expr          *expr);
DBCC_Expr *dbcc_expr_new_sizeof_type      (DBCC_Type          *type);
DBCC_Expr *dbcc_expr_new_subscript        (DBCC_Expr          *object,
                                           DBCC_Expr          *subscript);
DBCC_Expr *dbcc_expr_new_symbol           (DBCC_Symbol        *symbol);
DBCC_Expr *dbcc_expr_new_int_constant     (bool                is_signed,
                                           size_t              sizeof_value,
                                           uint64_t            value);
DBCC_Expr *dbcc_expr_new_string_constant  (DBCC_String        *constant);
DBCC_Expr *dbcc_expr_new_enum_constant    (DBCC_EnumValue     *enum_value);
DBCC_Expr *dbcc_expr_new_float_constant   (size_t              sizeof_value,
                                           long double         value);
DBCC_Expr *dbcc_expr_new_char_constant    (bool                is_long,
                                           uint32_t            value);
DBCC_Expr *dbcc_expr_new_unary            (DBCC_UnaryOperator  op,
                                           DBCC_Expr          *a);

void       dbcc_expr_add_code_position    (DBCC_Expr          *expr,
                                           DBCC_CodePosition  *cp);
typedef enum
{
  DBCC_DESIGNATOR_MEMBER,
  DBCC_DESIGNATOR_INDEX,
} DBCC_DesignatorType;

typedef struct DBCC_Designator DBCC_Designator;
struct DBCC_Designator
{
  DBCC_DesignatorType type;
  union {
    DBCC_Symbol *v_member;
    DBCC_Expr *v_index;
  };
};
struct DBCC_StructuredInitializer {
  unsigned n_pieces;               // may be 0
  DBCC_StructuredInitializerPiece *pieces;
};
struct DBCC_StructuredInitializerPiece {
  unsigned n_designators;               // may be 0
  DBCC_Designator *designators;
  bool is_expr;
  union {
    DBCC_Expr *v_expr;
    DBCC_StructuredInitializer v_structured_initializer;
  };
};
struct DBCC_StructuredInitializerExpr
{
  DBCC_Expr_Base base;
  DBCC_StructuredInitializer initializer;
};

DBCC_Expr *dbcc_expr_new_structured_initializer
                                          (DBCC_Type          *type,
                                           DBCC_StructuredInitializer *init);

struct DBCC_TernaryExpr
{
  DBCC_Expr_Base base;
  DBCC_Expr *cond;
  DBCC_Expr *a;
  DBCC_Expr *b;
};
DBCC_Expr *dbcc_expr_new_ternary (DBCC_Expr *cond,
                                  DBCC_Expr *a,
                                  DBCC_Expr *b);

void dbcc_expr_destroy (DBCC_Expr *expr);

#endif
