#include "dbcc.h"

static inline DBCC_Expr *
expr_alloc (DBCC_Expr_Type t)
{
  DBCC_Expr *rv = calloc (sizeof (DBCC_Expr), 1);
  rv->expr_type = t;
  return rv;
}

DBCC_Expr *
dbcc_expr_new_alignof_type     (DBCC_TargetEnvironment *env,
                                DBCC_Type              *type,
                                DBCC_Error            **error)
{
  DBCC_Expr *expr;

  /* 6.5.3.4. The sizeof and alignof operators.
   * 6.5.3.4p1. "... The alignof operator shall not be applied
   *            to a function type or an incomplete type.". */
  if (type->metatype == DBCC_TYPE_METATYPE_FUNCTION)
    {
       *error = dbcc_error_new (DBCC_ERROR_BAD_ALIGNOF_ARGUMENT,
                                "alignof must not be invoked on a function type");
       return NULL;
     }
  if ((type->metatype == DBCC_TYPE_METATYPE_STRUCT
    && type->v_struct.incomplete)
   || (type->metatype == DBCC_TYPE_METATYPE_UNION
    && type->v_union.incomplete))
     {
       *error = dbcc_error_new (DBCC_ERROR_BAD_ALIGNOF_ARGUMENT,
                                "alignof must not be invoked on an incomplete type");
       return NULL;
     }
  expr = dbcc_expr_new_int_constant (false, env->sizeof_pointer, type->base.alignof_instance);
  return expr;
}

DBCC_Expr *
dbcc_expr_new_alignof_expr      (DBCC_TargetEnvironment *env,
                                 DBCC_Expr              *expr,
                                 DBCC_Error            **error)
{
  assert (expr->base.value_type != NULL);
  return dbcc_expr_new_alignof_type (env, expr->base.value_type, error);
}

static inline bool
float_type_real_type_is_long_double  (DBCC_FloatType t)
{
  return t == DBCC_FLOAT_TYPE_LONG_DOUBLE
      || t == DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE
      || t == DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE;
}
static inline bool
float_type_real_type_is_double  (DBCC_FloatType t)
{
  return t == DBCC_FLOAT_TYPE_DOUBLE
      || t == DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE
      || t == DBCC_FLOAT_TYPE_COMPLEX_DOUBLE;
}
static inline bool
float_type_real_type_is_float  (DBCC_FloatType t)
{
  return t == DBCC_FLOAT_TYPE_FLOAT
      || t == DBCC_FLOAT_TYPE_IMAGINARY_FLOAT
      || t == DBCC_FLOAT_TYPE_COMPLEX_FLOAT;
}

static bool
get_int_or_enum_is_signed (DBCC_Type *t)
{
  if (t->metatype == DBCC_TYPE_METATYPE_INT)
    return t->v_int.is_signed;
  else if (t->metatype == DBCC_TYPE_METATYPE_ENUM)
    return t->v_enum.is_signed;
  else
    assert(0);

  return false;
}

typedef bool (*ExprHandler) (DBCC_Namespace *ns,
                             DBCC_Expr *expr,
                             DBCC_Error **error);

/* 6.3.1.8: Usual arithmetic conversions. */
static DBCC_Type *
usual_arithment_conversion_get_type  (DBCC_Namespace *ns,
                                      DBCC_Type *a,
                                      DBCC_Type *b)
{
  assert(dbcc_type_is_arithmetic (a));
  assert(dbcc_type_is_arithmetic (b));
  a = dbcc_type_dequalify (a);
  b = dbcc_type_dequalify (b);

  /* Paragraph 1 is very long. */

  /* TODO: long double, double, float should maybe be handled by a loop
   * instead of coding each case separately.  OTOH the advantage of
   * this construction is that it follows the standard directly. */

  /* "First, if the corresponding real type of either operand is long double,
      the other operand is converted, without change of type domain,
      to a type whose corresponding real type is long double." */
  if (    (a->metatype == DBCC_TYPE_METATYPE_FLOAT
        && float_type_real_type_is_long_double (a->v_float.float_type))
   ||     (b->metatype == DBCC_TYPE_METATYPE_FLOAT
        && float_type_real_type_is_long_double (b->v_float.float_type))     )
    {
      if (dbcc_type_is_complex (a)
       || dbcc_type_is_complex (b))
        {
          return dbcc_namespace_get_complex_long_double_type (ns);
        }
      else
        {
          return dbcc_namespace_get_long_double_type (ns);
        }
    }

  /* "Otherwise, if the corresponding real type of either operand is double,
      the other operand is converted, without change of type domain,
      to a type whose corresponding real type is double." */
  if (    (a->metatype == DBCC_TYPE_METATYPE_FLOAT
        && float_type_real_type_is_double (a->v_float.float_type))
   ||     (b->metatype == DBCC_TYPE_METATYPE_FLOAT
        && float_type_real_type_is_double (b->v_float.float_type))     )
    {
      if (dbcc_type_is_complex (a)
       || dbcc_type_is_complex (b))
        {
          return dbcc_namespace_get_complex_double_type (ns);
        }
      else
        {
          return dbcc_namespace_get_double_type (ns);
        }
    }
  
  /* "Otherwise, if the corresponding real type of either operand is float,
      the other operand is converted, without change of type domain,
      to a type whose corresponding real type is float." */
  if (    (a->metatype == DBCC_TYPE_METATYPE_FLOAT
        && float_type_real_type_is_float (a->v_float.float_type))
   ||     (b->metatype == DBCC_TYPE_METATYPE_FLOAT
        && float_type_real_type_is_float (b->v_float.float_type))     )
    {
      if (dbcc_type_is_complex (a)
       || dbcc_type_is_complex (b))
        {
          return dbcc_namespace_get_complex_float_type (ns);
        }
      else
        {
          return dbcc_namespace_get_float_type (ns);
        }
    }
  
  /* "Otherwise, the integer promotions are performed on both operands.
      Then the following rules are applied to the promoted operands:" */

  /*   *   "If both operands have the same type,
            then no further conversion is needed." */
  bool a_is_signed = get_int_or_enum_is_signed (a);
  bool b_is_signed = get_int_or_enum_is_signed (b);
  unsigned a_size = a->base.sizeof_instance;
  unsigned b_size = b->base.sizeof_instance;

  /*   *   "Otherwise, if both operands have signed integer types or
            both have unsigned integer types, the operand with the type
            of lesser integer conversion rank is converted to the type
            of the operand with greater rank."
   *   *   "Otherwise, if the operand that has unsigned integer type has
            rank greater or equal to the rank of the type of the other operand,
            then the operand with signed integer type is converted to the type
            of the operand with unsigned integer type."
   */
  if (a_is_signed == b_is_signed)
    {
      size_t new_size = DBCC_MAX (a_size, b_size);
      return dbcc_namespace_get_integer_type (ns, a_is_signed, new_size);
    }

  /* "Otherwise, if the type of the operand with signed integer type
      can represent all of the values of the type of the operand with
      unsigned integer type, then the operand with unsigned integer type
      is converted to the type of the operand with signed integer type." */
  // XXX: for consistency, maybe this should return an dbcc_namespace_get_integer_type()-derived type (this is mostly for enum handling on systems with signed enums) 
  if (a_is_signed && a_size > b_size)
    return a;
  if (b_is_signed && a_size < b_size)
    return b;

  /* "Otherwise, both operands are converted to the unsigned integer type
      corresponding to the type of the operand with signed integer type." */
  return dbcc_namespace_get_integer_type (ns, false, DBCC_MAX(a_size, b_size));
}


/* This handles add + subtract (ie the binary operators '-' and '+') */
static bool
handle_add (DBCC_Namespace *ns, DBCC_Expr *expr, DBCC_Error **error)
{
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);

  /* if !is_addition, then it is subtraction (DBCC_BINARY_OPERATOR_SUB). */
  bool is_addition = expr->v_binary.op == DBCC_BINARY_OPERATOR_ADD;


  /* 6.5.6p2 "For addition, either both operands shall have arithmetic type,
              or one operand shall be a pointer to a complete object type
              and the other shall have integer type." */
  bool both_arith = dbcc_type_is_arithmetic (atype)
                 && dbcc_type_is_arithmetic (btype);
  bool pointer_int = dbcc_type_is_pointer (atype)
                 && dbcc_type_is_integer (btype);
  bool int_pointer = dbcc_type_is_integer (atype)
                 && dbcc_type_is_pointer (btype);
  if (!(both_arith || pointer_int || int_pointer))
    {
      *error = dbcc_error_new(DBCC_ERROR_BAD_OPERATOR_TYPES,
                              "operator '+' accepts either two numbers, or an inger and a pointer");
      return false;
    }

  DBCC_Type *rv_type;

  if (both_arith)
    {
      /* 6.5.6p4 "If both operands have arithmetic type,
                  the usual arithmetic conversions are performed on them." */
      rv_type = usual_arithment_conversion_get_type (ns, atype, btype);
      assert(rv_type != NULL);
      ...
    }
  else
    {
      /* XXX: we should really cache these subexpressions, but
       * all we return is the ptr's type. */
      DBCC_Expr *ptr, *offset;
      if (int_pointer)
        offset = a_expr, ptr = b_expr;
      else
        ptr = a_expr, offset = b_expr;

      /* the whole expression has the value type of
       * the pointer type.
       */
      assert(ptr->base.value_type != NULL);
      expr->base.value_type = ptr->base.value_type;
      ...
    }
}

static bool
handle_subtract (DBCC_Expr *expr, DBCC_Error **error)
{
...
}

static ExprHandler binary_operator_handlers[] =
{
  .DBCC_BINARY_OPERATOR_ADD = handle_add,
  .DBCC_BINARY_OPERATOR_SUB = handle_subtract,
  //.DBCC_BINARY_OPERATOR_MUL = handle_multiply,
  //.DBCC_BINARY_OPERATOR_DIV = handle_divide,
  //.DBCC_BINARY_OPERATOR_REMAINDER = handle_remainder,
  //.DBCC_BINARY_OPERATOR_LT = handle_lt,
  //.DBCC_BINARY_OPERATOR_LTEQ = handle_lteq,
  //.DBCC_BINARY_OPERATOR_GT = handle_gt,
  //.DBCC_BINARY_OPERATOR_GTEQ = handle_gteq,
  //.DBCC_BINARY_OPERATOR_EQ = handle_eq,
  //.DBCC_BINARY_OPERATOR_NE = handle_ne,
  //.DBCC_BINARY_OPERATOR_SHIFT_LEFT = handle_
  //.DBCC_BINARY_OPERATOR_SHIFT_RIGHT = handle_
  //.DBCC_BINARY_OPERATOR_LOGICAL_AND = handle_
  //.DBCC_BINARY_OPERATOR_LOGICAL_OR = handle_
  //.DBCC_BINARY_OPERATOR_BITWISE_AND = handle_
  //.DBCC_BINARY_OPERATOR_BITWISE_OR = handle_
  //.DBCC_BINARY_OPERATOR_BITWISE_XOR = handle_
  //.DBCC_BINARY_OPERATOR_COMMA = handle_
};

DBCC_Expr *
dbcc_expr_new_binary_operator  (DBCC_TargetEnvironment *env,
                                DBCC_BinaryOperator op,
                                DBCC_Expr          *a,
                                DBCC_Expr          *b,
                                DBCC_Error        **error)
{
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_BINARY_OP);
  rv->v_binary.op = op;
  rv->v_binary.a = a;
  rv->v_binary.b = b;
  
  // Compute type and fold constant values.
  BinaryOperatorHandler h = op < DBCC_N_ELEMENTS (binary_operator_handlers)
                          ? binary_operator_handlers[op]
                          : NULL;
  ...

  // Compute constant value if appropriate.
  ...

  return rv;
}

DBCC_Expr *
dbcc_expr_new_inplace_binary   (DBCC_InplaceBinaryOperator op,
                                DBCC_Expr          *in_out,
                                DBCC_Expr          *b);
{
...
}

DBCC_Expr *
dbcc_expr_new_call  (DBCC_Expr          *head,
                     unsigned            n_args,
                     DBCC_Expr         **args);
{
...
}

DBCC_Expr *
dbcc_expr_new_cast             (DBCC_Type          *type,
                                DBCC_Expr          *value);
{
...
}

DBCC_Expr *
dbcc_expr_new_generic_selection(DBCC_Expr          *expr,
                                size_t              n_associations,
                                DBCC_GenericAssociation *associations,
                                DBCC_Expr          *default_expr);
{
...
}

DBCC_Expr *
dbcc_expr_new_inplace_unary    (DBCC_InplaceUnaryOperator  op,
                                DBCC_Expr          *in_out);
{
...
}

DBCC_Expr *
dbcc_expr_new_member_access    (DBCC_Expr          *expr,
                                DBCC_Symbol        *name)
{
...
}

DBCC_Expr *
dbcc_expr_new_pointer_access   (DBCC_Expr          *expr,
                                DBCC_Symbol        *name)
{
...
}

DBCC_Expr *
dbcc_expr_new_sizeof_expr      (DBCC_Expr          *expr)
{
...
}

DBCC_Expr *
dbcc_expr_new_sizeof_type      (DBCC_Type          *type)
{
...
}

DBCC_Expr *
dbcc_expr_new_subscript        (DBCC_Expr          *object,
                                DBCC_Expr          *subscript)
{
...
}

DBCC_Expr *
dbcc_expr_new_symbol           (DBCC_Symbol        *symbol)
{
...
}

DBCC_Expr *
dbcc_expr_new_int_constant     (bool                is_signed,
                                size_t              sizeof_value,
                                uint64_t            value)
{
...
}

DBCC_Expr *
dbcc_expr_new_string_constant  (DBCC_String        *constant)
{
...
}

DBCC_Expr *
dbcc_expr_new_enum_constant    (DBCC_EnumValue     *enum_value)
{
...
}

DBCC_Expr *
dbcc_expr_new_float_constant   (size_t              sizeof_value,
                                long double         value)
{
...
}

DBCC_Expr *
dbcc_expr_new_char_constant    (bool                is_long,
                                uint32_t            value)
{
...
}

DBCC_Expr *
dbcc_expr_new_unary            (DBCC_UnaryOperator  op,
                                DBCC_Expr          *a)
{
...
}

DBCC_Expr *
dbcc_expr_new_structured_initializer (DBCC_Type          *type,
                                      DBCC_StructuredInitializer *init)
{
...
}

DBCC_Expr *
dbcc_expr_new_ternary (DBCC_Expr *cond,
                       DBCC_Expr *a,
                       DBCC_Expr *b)
{
...
}

void
dbcc_expr_destroy (DBCC_Expr *expr)
{
  switch (expr->type)
    {
    ...
    }
}

void
dbcc_expr_add_code_position    (DBCC_Expr          *expr,
                                DBCC_CodePosition  *cp)
{
  expr->base.code_position = dbcc_code_position_ref (cp);
}
