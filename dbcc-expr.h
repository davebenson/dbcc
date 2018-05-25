#ifndef __DBCC_EXPR_H_
#define __DBCC_EXPR_H_

typedef union DBCC_Expr DBCC_Expr;

typedef struct DBCC_Expr_Base DBCC_Expr_Base;
typedef struct DBCC_BinaryOperatorExpr DBCC_BinaryOperatorExpr;
typedef struct DBCC_InplaceUnaryOperatorExpr DBCC_InplaceUnaryOperatorExpr;
typedef struct DBCC_UnaryOperatorExpr DBCC_UnaryOperatorExpr;
typedef struct DBCC_TernaryOperatorExpr DBCC_TernaryOperatorExpr;
typedef struct DBCC_InplaceBinaryOperatorExpr DBCC_InplaceBinaryOperatorExpr;
typedef struct DBCC_IdentifierExpr DBCC_IdentifierExpr;
typedef struct DBCC_ConstantAddressExpr DBCC_ConstantAddressExpr;
typedef struct DBCC_ConstantExpr DBCC_ConstantExpr;
typedef struct DBCC_CallExpr DBCC_CallExpr;
typedef struct DBCC_CastExpr DBCC_CastExpr;
typedef struct DBCC_AccessExpr DBCC_AccessExpr;
typedef struct DBCC_StructuredInitializerPiece DBCC_StructuredInitializerPiece;
typedef struct DBCC_StructuredInitializer DBCC_StructuredInitializer;
typedef struct DBCC_StructuredInitializerExpr DBCC_StructuredInitializerExpr;
typedef struct DBCC_StructuredInitializerFlatPiece DBCC_StructuredInitializerFlatPiece;

typedef enum DBCC_IdentifierType {
  DBCC_IDENTIFIER_TYPE_UNKNOWN,         // must be 0
  DBCC_IDENTIFIER_TYPE_GLOBAL,
  DBCC_IDENTIFIER_TYPE_LOCAL,
  DBCC_IDENTIFIER_TYPE_ENUM_VALUE,
} DBCC_IdentifierType;

typedef enum
{
  DBCC_EXPR_TYPE_UNARY_OP,
  DBCC_EXPR_TYPE_BINARY_OP,
  DBCC_EXPR_TYPE_TERNARY_OP,
  DBCC_EXPR_TYPE_INPLACE_BINARY_OP,
  DBCC_EXPR_TYPE_INPLACE_UNARY_OP,
  DBCC_EXPR_TYPE_CONSTANT,
  DBCC_EXPR_TYPE_CALL,
  DBCC_EXPR_TYPE_CAST,
  DBCC_EXPR_TYPE_ACCESS,
  DBCC_EXPR_TYPE_IDENTIFIER,
  DBCC_EXPR_TYPE_STRUCTURED_INITIALIZER
} DBCC_Expr_Type;

struct DBCC_Expr_Base
{
  DBCC_Expr_Type type;
  DBCC_Type *value_type;
  DBCC_Constant *constant;
  DBCC_CodePosition *code_position;

  unsigned types_inferred : 1;
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
  DBCC_UnaryOperator op;
  DBCC_Expr *a;
};

struct DBCC_TernaryOperatorExpr
{
  DBCC_Expr_Base base;
  DBCC_Expr *condition;
  DBCC_Expr *true_value;
  DBCC_Expr *false_value;
};

// there are four:  ++x --x x++ x--
struct DBCC_InplaceUnaryOperatorExpr
{
  DBCC_Expr_Base base;
  DBCC_InplaceUnaryOperator op;
  DBCC_Expr *inout;
};

// there are:  += -= /= *= %= &= |= ^= >>= <<=
struct DBCC_InplaceBinaryOperatorExpr
{
  DBCC_Expr_Base base;
  DBCC_InplaceBinaryOperator op;
  DBCC_Expr *inout, *b;
};

struct DBCC_IdentifierExpr
{
  DBCC_Expr_Base base;
  DBCC_Symbol *name;
  DBCC_IdentifierType id_type;
  union {
    struct {
      DBCC_Type *enum_type;
      DBCC_EnumValue *enum_value;
    } v_enum_value;
    DBCC_Global *v_global;
    DBCC_Local *v_local;
  };
};

struct DBCC_CallExpr
{
  DBCC_Expr_Base base;
  DBCC_Type *function_type;          // FUNCTION or FUNCTION_KR
  DBCC_Expr *head;
  size_t n_args;
  DBCC_Expr **args;
};

struct DBCC_CastExpr
{
  DBCC_Expr_Base base;
  DBCC_Expr *pre_cast_expr;
};
struct DBCC_AccessExpr
{
  DBCC_Expr_Base base;
  DBCC_Expr *object;
  DBCC_Symbol *name;
  void *sub_info;
  bool is_union;                // else is struct
  bool is_pointer;              // if true, the operator is -> otherwise, .
};
struct DBCC_ConstantExpr
{
  DBCC_Expr_Base base;
};
struct DBCC_ConstantAddressExpr
{
  DBCC_Expr_Base base;
  DBCC_Address *address;
};

/* --- structured initializers --- */
/* Structured initializers start as untyped,
 * which is parsed in a bottom-up fashion,
 * hence no type is available in the initial scanning.
 *
 * Once the context reveals a cast or
 * a declaration, we use the type of that
 * to validate and flatten the structured initializer.
 */
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
struct DBCC_StructuredInitializerFlatPiece
{
  unsigned offset;
  unsigned length;
  DBCC_Expr *piece_expr;                // if NULL, zero the piece
};
struct DBCC_StructuredInitializerExpr
{
  DBCC_Expr_Base base;
  DBCC_StructuredInitializer initializer;

  // valid after set_type()
  size_t n_flat_pieces;
  DBCC_StructuredInitializerFlatPiece *flat_pieces;
};

DBCC_Expr *dbcc_expr_new_structured_initializer
                                          (DBCC_StructuredInitializer *init);
bool dbcc_expr_structured_initializer_set_type (DBCC_Namespace *ns,
                                                DBCC_Expr *si,
                                                DBCC_Type *type,
                                                DBCC_Error **error);
union DBCC_Expr
{
  DBCC_Expr_Type expr_type;
  DBCC_Expr_Base base;

  DBCC_UnaryOperatorExpr v_unary;
  DBCC_BinaryOperatorExpr v_binary;
  DBCC_TernaryOperatorExpr v_ternary;

  DBCC_InplaceUnaryOperatorExpr v_inplace_unary;
  DBCC_InplaceBinaryOperatorExpr v_inplace_binary;

  DBCC_CallExpr v_call;
  DBCC_CastExpr v_cast;
  DBCC_AccessExpr v_access;
  DBCC_IdentifierExpr v_identifier;
  DBCC_ConstantExpr v_constant;
  DBCC_ConstantAddressExpr v_constant_address;
  DBCC_StructuredInitializerExpr v_structured_initializer;
};

typedef struct DBCC_GenericAssociation DBCC_GenericAssociation;
struct DBCC_GenericAssociation
{
  DBCC_Type *type;
  DBCC_Expr *expr;
};

DBCC_Expr *dbcc_expr_new_alignof_type     (DBCC_Namespace     *ns,
                                           DBCC_Type          *type,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_alignof_expr     (DBCC_Namespace     *ns,
                                           DBCC_Expr          *expr,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_binary_operator  (DBCC_Namespace     *ns,
                                           DBCC_BinaryOperator op,
                                           DBCC_Expr          *a,
                                           DBCC_Expr          *b,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_inplace_binary   (DBCC_Namespace     *ns,
                                           DBCC_InplaceBinaryOperator op,
                                           DBCC_Expr          *in_out,
                                           DBCC_Expr          *b,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_call             (DBCC_Namespace     *ns,
                                           DBCC_Expr          *head,
                                           unsigned            n_args,
                                           DBCC_Expr         **args,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_cast             (DBCC_Namespace     *ns,
                                           DBCC_Type          *type,
                                           DBCC_Expr          *value,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_generic_selection(DBCC_Namespace     *ns,
                                           DBCC_Expr          *expr,
                                           size_t              n_associations,
                                           DBCC_GenericAssociation *associations,
                                           DBCC_Expr          *default_expr,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_inplace_unary    (DBCC_Namespace     *ns,
                                           DBCC_InplaceUnaryOperator op,
                                           DBCC_Expr          *in_out,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_access           (DBCC_Namespace     *ns,
                                           bool                is_pointer_access,
                                           DBCC_Expr          *expr,
                                           DBCC_Symbol        *name,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_sizeof_expr      (DBCC_Namespace     *ns,
                                           DBCC_Expr          *expr,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_sizeof_type      (DBCC_Namespace     *ns,
                                           DBCC_CodePosition  *cp,
                                           DBCC_Type          *type,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_subscript        (DBCC_Namespace     *ns,
                                           DBCC_Expr          *object,
                                           DBCC_Expr          *subscript,
                                           DBCC_Error        **error);

// Namespace may be NULL, just for this function,
// to indicate that you want symbol-resolution awaits the next
// phase of analysis.
DBCC_Expr *dbcc_expr_new_identifier       (DBCC_Namespace     *ns,
                                           DBCC_Symbol        *symbol,
                                           DBCC_Error        **error);
DBCC_Expr *dbcc_expr_new_int_constant     (DBCC_Type          *type,
                                           uint64_t            value);
DBCC_Expr *dbcc_expr_new_string_constant  (DBCC_Namespace    *ns,
                                           const DBCC_String *constant);
DBCC_Expr *dbcc_expr_new_enum_constant    (DBCC_EnumValue     *enum_value);
DBCC_Expr *dbcc_expr_new_float_constant   (DBCC_Namespace     *global_ns,
                                           DBCC_FloatType      float_type,
                                           long double         value);
DBCC_Expr *dbcc_expr_new_char_constant    (DBCC_Type          *type,
                                           uint32_t            value);
DBCC_Expr *dbcc_expr_new_unary            (DBCC_Namespace     *ns,
                                           DBCC_UnaryOperator  op,
                                           DBCC_Expr          *a,
                                           DBCC_Error        **error);

void       dbcc_expr_add_code_position    (DBCC_Expr          *expr,
                                           DBCC_CodePosition  *cp);

DBCC_Expr *dbcc_expr_new_ternary (DBCC_Namespace *ns,
                                  DBCC_Expr *cond,
                                  DBCC_Expr *a,
                                  DBCC_Expr *b,
                                  DBCC_Error **error);

bool dbcc_expr_is_lvalue (DBCC_Expr *expr);
bool dbcc_expr_is_null_pointer_constant (DBCC_Expr *expr);

void dbcc_expr_destroy (DBCC_Expr *expr);

bool dbcc_expr_do_type_inference (DBCC_Namespace *ns,
                                  DBCC_Expr *expr,
                                  DBCC_Error **error);
#endif
