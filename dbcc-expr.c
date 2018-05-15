#include "dbcc.h"

static inline DBCC_Expr *
expr_alloc (DBCC_Expr_Type t)
{
  DBCC_Expr *rv = calloc (sizeof (DBCC_Expr), 1);
  rv->expr_type = t;
  return rv;
}

DBCC_Expr *
dbcc_expr_new_alignof_type     (DBCC_Namespace         *ns,
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
  expr = dbcc_expr_new_int_constant (false, ns->target_env->sizeof_pointer, type->base.alignof_instance);
  return expr;
}

DBCC_Expr *
dbcc_expr_new_alignof_expr      (DBCC_Namespace         *ns,
                                 DBCC_Expr              *expr,
                                 DBCC_Error            **error)
{
  assert (expr->base.value_type != NULL);
  return dbcc_expr_new_alignof_type (ns, expr->base.value_type, error);
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


/* This handles add (ie the binary operator '+') */
static bool
handle_add (DBCC_Namespace *ns, DBCC_Expr *expr, DBCC_Error **error)
{
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);

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

  if (both_arith)
    {
      /* 6.5.6p4 "If both operands have arithmetic type,
                  the usual arithmetic conversions are performed on them." */
      DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
      assert(type != NULL);
      expr->base.value_type = type;

      /* constant folding */
      if (aexpr->base.constant_value != NULL
       && bexpr->base.constant_value != NULL)
        {
          void *acasted = alloca (type->base.sizeof_instance);
          void *bcasted = alloca (type->base.sizeof_instance);
          void *const_value = malloc (type->base.sizeof_instance);

          // cast both values
          if (!dbcc_cast_value (type, acasted,
                                atype, aexpr->base.constant_value)
           || !dbcc_cast_value (type, bcasted,
                                btype, bexpr->base.constant_value))
            assert (0);

          dbcc_typed_value_add (type, const_value, acasted, bcasted);

          expr->base.constant_value = const_value;
        }
    }
  else
    {
      /* XXX: we should really cache these subexpressions, but
       * all we return is the ptr's type. */
      DBCC_Expr *ptr, *offset;
      if (int_pointer)
        offset = aexpr, ptr = bexpr;
      else
        ptr = aexpr, offset = bexpr;

      DBCC_Type *pointed_at = dbcc_type_dequalify (dbcc_type_pointer_dereference(ptr->base.value_type));
      if (dbcc_type_is_incomplete (pointed_at))
        {
          *error = dbcc_error_new (DBCC_ERROR_POINTER_MATH_ON_IMCOMPLETE_TYPE,
                                   "pointer to incomplete type may not be added to");
          return false;
        }
      /* the whole expression has the value type of
       * the pointer type.
       */
      assert(ptr->base.value_type != NULL);
      expr->base.value_type = ptr->base.value_type;
    }
  return true;
}

static bool
handle_subtract (DBCC_Namespace *ns, DBCC_Expr *expr, DBCC_Error **error)
{
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);

  /* 6.5.6p3 "For subtraction, one of the following shall hold:
                — both operands have arithmetic type;
                — both operands are pointers to qualified or unqualified
                  versions of compatible complete object types; or
                — the left operand is a pointer to a complete object type
                  and the right operand has integer type. */
  bool both_arith = dbcc_type_is_arithmetic (atype)
                 && dbcc_type_is_arithmetic (btype);
  if (both_arith)
    {
      /* 6.5.6p4 "If both operands have arithmetic type,
                  the usual arithmetic conversions are performed on them." */
      DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
      assert(type != NULL);
      expr->base.value_type = type;

      /* constant folding */
      if (aexpr->base.constant_value != NULL
       && bexpr->base.constant_value != NULL)
        {
          void *acasted = alloca (type->base.sizeof_instance);
          void *bcasted = alloca (type->base.sizeof_instance);
          void *const_value = malloc (type->base.sizeof_instance);

          // cast both values
          if (!dbcc_cast_value (type, acasted,
                                atype, aexpr->base.constant_value)
           || !dbcc_cast_value (type, bcasted,
                                btype, bexpr->base.constant_value))
            assert (0);

          dbcc_typed_value_subtract (type, const_value, acasted, bcasted);

          expr->base.constant_value = const_value;
        }
      return true;
    }

  bool pointer_pointer = dbcc_type_is_pointer (atype)
                      && dbcc_type_is_pointer (btype);
  if (pointer_pointer)
    {
      DBCC_Type *a_pointed_at = dbcc_type_dequalify (dbcc_type_pointer_dereference(atype));
      DBCC_Type *b_pointed_at = dbcc_type_dequalify (dbcc_type_pointer_dereference(atype));
      if (!dbcc_types_compatible (a_pointed_at, b_pointed_at))
        {
          *error = dbcc_error_new (DBCC_ERROR_POINTER_SUBTRACTION_TYPE_MISMATCH,
                                   "pointer subtraction between incompatible types");
          return false;
        }
      expr->base.value_type = dbcc_namespace_get_ptrdiff_type (ns);
      return true;
    }

  bool pointer_int = dbcc_type_is_pointer (atype)
                  && dbcc_type_is_integer (btype);
  if (pointer_int) 
    {
      DBCC_Type *a_pointed_at = dbcc_type_dequalify (dbcc_type_pointer_dereference(atype));
      if (dbcc_type_is_incomplete (a_pointed_at))
        {
          *error = dbcc_error_new (DBCC_ERROR_POINTER_MATH_ON_IMCOMPLETE_TYPE,
                                   "pointer to incomplete type may not be subtracted from");
          return false;
        }
      expr->base.value_type = atype;
      return true;
    }

  *error = dbcc_error_new(DBCC_ERROR_BAD_OPERATOR_TYPES,
                          "operator '+' accepts either two numbers, or an inger and a pointer");
  return false;
}

static bool
handle_multiply (DBCC_Namespace *ns, DBCC_Expr *expr, DBCC_Error **error)
{
  /* 6.5.5 Multiplicative operators
   * p3: The usual arithmetic conversions are performed on the operands.
   * p4: The result of the binary * operator is the product of the operands.
   */
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);
  if (!dbcc_type_is_arithmetic (atype)
   || !dbcc_type_is_arithmetic (btype))
    {
      *error = dbcc_error_new (DBCC_ERROR_NONARITHMETIC_MULTIPLY,
                               "multiply (binary operator *) must be applied to arithmetic types");
      return false;
    }

  DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
  expr->base.value_type = type;

  /* constant folding */
  if (aexpr->base.constant_value != NULL
   && bexpr->base.constant_value != NULL)
    {
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant_value)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant_value))
        assert (0);

      dbcc_typed_value_multiply (type, const_value, acasted, bcasted);

      expr->base.constant_value = const_value;
    }
  return true;
}

static bool
handle_divide (DBCC_Namespace *ns, DBCC_Expr *expr, DBCC_Error **error)
{
  /* 6.5.5 Multiplicative operators
   * p5: The result of the / operator is the quotient from the division
         of the first operand by the second; the result of the % operator
         is the remainder. In both operations, if the value of
         the second operand is zero, the behavior is undefined.
   * p6: [dbcc_typed_value_divide uses the compiler to implement this,
          so it should obey the standard.]
   */
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);
  if (!dbcc_type_is_arithmetic (atype)
   || !dbcc_type_is_arithmetic (btype))
    {
      *error = dbcc_error_new (DBCC_ERROR_NONARITHMETIC_MULTIPLY,
                               "multiply (binary operator *) must be applied to arithmetic types");
      return false;
    }

  DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
  expr->base.value_type = type;

  /* constant folding */
  if (aexpr->base.constant_value != NULL
   && bexpr->base.constant_value != NULL)
    {
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant_value)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant_value))
        assert (0);

      dbcc_typed_value_multiply (type, const_value, acasted, bcasted);

      expr->base.constant_value = const_value;
    }
  return true;
}

static bool
handle_remainder (DBCC_Namespace *ns, DBCC_Expr *expr, DBCC_Error **error)
{
  /* 6.5.5 Multiplicative operators
   * p2: ... The operands of the % operator shall have integer type.
   * p5: ...  the result of the % operator
         is the remainder. In both operations, if the value of
         the second operand is zero, the behavior is undefined.
   * p6: [dbcc_typed_value_remainder uses the compiler to implement this,
          so it should obey the standard.]
   */
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);
  if (!dbcc_type_is_arithmetic (atype)
   || !dbcc_type_is_arithmetic (btype))
    {
      *error = dbcc_error_new (DBCC_ERROR_NONARITHMETIC_MULTIPLY,
                               "multiply (binary operator *) must be applied to arithmetic types");
      return false;
    }

  DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
  expr->base.value_type = type;

  /* constant folding */
  if (aexpr->base.constant_value != NULL
   && bexpr->base.constant_value != NULL)
    {
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant_value)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant_value))
        assert (0);

      dbcc_typed_value_multiply (type, const_value, acasted, bcasted);

      expr->base.constant_value = const_value;
    }
  return true;
}

/* This handles all inequality/ordering operators.
 *
 * The operators == and != are handled separately in handle_equality_operator.
 *
 * The operators handled by this function are:
 *         <      <=      >=    >
 *         LT     LTEQ    GTEQ  GT
 * 
 * See 6.5.8 Relational operators
 */
static bool
handle_relational_operator (DBCC_Namespace *ns,
                            DBCC_Expr *expr,
                            DBCC_Error **error)
{
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);

  /* p2: One of the following shall hold:
        — both operands have real type; or
        — both operands are pointers to qualified or unqualified versions
          of compatible object types  */
  if (dbcc_type_is_arithmetic (atype)
   && dbcc_type_is_arithmetic (btype))
    {
      if (!dbcc_type_is_real (atype)
       || !dbcc_type_is_real (btype))
        {
          *error = dbcc_error_new (DBCC_ERROR_CANNOT_COMPARE_COMPLEX,
                                   "complex numbers cannot be used with inequalities");
          return false;
        }
      /* p6: "... The result has type int." */
      expr->base.value_type = dbcc_namespace_get_int_type (ns);

      if (aexpr->base.constant_value != NULL
       && bexpr->base.constant_value != NULL)
        {
          DBCC_Type *cmp_type = usual_arithment_conversion_get_type (ns, atype, btype);
          void *acasted = alloca (cmp_type->base.sizeof_instance);
          void *bcasted = alloca (cmp_type->base.sizeof_instance);
          dbcc_cast_value (cmp_type, acasted, atype, aexpr->base.constant_value);
          dbcc_cast_value (cmp_type, bcasted, btype, bexpr->base.constant_value);
          void *res = malloc (ns->target_env->sizeof_int);
          dbcc_typed_value_compare (ns,
                                    cmp_type,
                                    expr->v_binary.op,
                                    res, acasted, bcasted);
          expr->base.constant_value = res;
        }
    }
  else if (dbcc_type_is_pointer (atype)
        && dbcc_type_is_pointer (btype))
    {
      /* Check types compatible. */
      DBCC_Type *aref = dbcc_type_pointer_dereference (atype);
      DBCC_Type *bref = dbcc_type_pointer_dereference (btype);
      if (!dbcc_types_compatible(aref, bref))
        {
          *error = dbcc_error_new (DBCC_ERROR_CANNOT_COMPARE,
                                   "pointers point at incompatible types");
          return false;
        }
      expr->base.value_type = dbcc_namespace_get_int_type (ns);
    }
  else
    {
      *error = dbcc_error_new (DBCC_ERROR_CANNOT_COMPARE,
                               "types not comparable");
      return false;
    }
  return true;
}

/* This handles all equal and not-equal.
 *
 * The operators handled by this function are:
 *         ==      !=
 *         EQ      NE
 * 
 * See 6.5.9 Equality operators
 */
static bool
handle_equality_operator   (DBCC_Namespace *ns,
                            DBCC_Expr *expr,
                            DBCC_Error **error)
{
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);

  /* p2: One of the following shall hold:
        — both operands have arithmetic type; or
        — both operands are pointers to qualified or unqualified versions
          of compatible object types; or
        - one operand is a pointer to an object type and the other
          is a pointer to a qualified or unqualified version of void; or
        — one operand is a pointer and the other is a null pointer constant. */
  if (dbcc_type_is_arithmetic (atype)
   && dbcc_type_is_arithmetic (btype))
    {
      /* p6: "... The result has type int." */
      expr->base.value_type = dbcc_namespace_get_int_type (ns);

      if (aexpr->base.constant_value != NULL
       && bexpr->base.constant_value != NULL)
        {
          DBCC_Type *cmp_type = usual_arithment_conversion_get_type (ns, atype, btype);
          void *acasted = alloca (cmp_type->base.sizeof_instance);
          void *bcasted = alloca (cmp_type->base.sizeof_instance);
          dbcc_cast_value (cmp_type, acasted, atype, aexpr->base.constant_value);
          dbcc_cast_value (cmp_type, bcasted, btype, bexpr->base.constant_value);
          void *res = malloc (ns->target_env->sizeof_int);
          dbcc_typed_value_compare (ns,
                                    cmp_type,
                                    expr->v_binary.op,
                                    res, acasted, bcasted);
          expr->base.constant_value = res;
        }
    }
  else if (dbcc_type_is_pointer (atype)
        && dbcc_type_is_pointer (btype))
    {
      /* Check types compatible. */
      DBCC_Type *aref = dbcc_type_dequalify (dbcc_type_pointer_dereference (atype));
      DBCC_Type *bref = dbcc_type_dequalify (dbcc_type_pointer_dereference (btype));
      if (aref->metatype == DBCC_TYPE_METATYPE_VOID
       || bref->metatype == DBCC_TYPE_METATYPE_VOID
       || dbcc_types_compatible (aref, bref))
       // TODO: "— one operand is a pointer and the other is a null pointer constant."
        {
          /* ok */
        }
      else
        {
          *error = dbcc_error_new (DBCC_ERROR_CANNOT_COMPARE,
                                   "pointers point at incompatible types");
          return false;
        }
      expr->base.value_type = dbcc_namespace_get_int_type (ns);
    }
  else
    {
      *error = dbcc_error_new (DBCC_ERROR_CANNOT_COMPARE,
                               "types not comparable");
      return false;
    }
  return true;
}

static ExprHandler binary_operator_handlers[DBCC_N_BINARY_OPERATORS] =
{
  [DBCC_BINARY_OPERATOR_ADD] = handle_add,
  [DBCC_BINARY_OPERATOR_SUB] = handle_subtract,
  [DBCC_BINARY_OPERATOR_MUL] = handle_multiply,
  [DBCC_BINARY_OPERATOR_DIV] = handle_divide,
  [DBCC_BINARY_OPERATOR_REM] = handle_remainder,
  [DBCC_BINARY_OPERATOR_LT] = handle_relational_operator,
  [DBCC_BINARY_OPERATOR_LTEQ] = handle_relational_operator,
  [DBCC_BINARY_OPERATOR_GT] = handle_relational_operator,
  [DBCC_BINARY_OPERATOR_GTEQ] = handle_relational_operator,
  [DBCC_BINARY_OPERATOR_EQ] = handle_equality_operator,
  [DBCC_BINARY_OPERATOR_NE] = handle_equality_operator,
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
dbcc_expr_new_binary_operator  (DBCC_Namespace     *ns,
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
  assert(op < DBCC_N_BINARY_OPERATORS);
  assert(binary_operator_handlers[op] != NULL);
  if (!(*(binary_operator_handlers[op]))(ns, rv, error))
    {
      dbcc_expr_destroy (rv);
      return NULL;
    }

  return rv;
}

static bool
check_simple_assignment (DBCC_Expr          *in_out,
                         DBCC_Expr          *b,
                         DBCC_Error        **error)
{
  DBCC_Type *inout_type = in_out->base.value_type;
  DBCC_Type *b_type = b->base.value_type;
  DBCC_Type *dequal_a = dbcc_type_dequalify (inout_type);
  DBCC_Type *dequal_b = dbcc_type_dequalify (b_type);

  /* 6.5.16.1 Simple assignment.
     p1 One of the following shall hold:
     — the left operand has atomic, qualified, or unqualified arithmetic type,
       and the right has arithmetic type;
   */
  bool arithmetic = dbcc_type_is_arithmetic (inout_type)
                 && dbcc_type_is_arithmetic (b_type);

  /* — the left operand has an atomic, qualified, or unqualified version
       of a structure or union type compatible with the type of the right; */
  bool struct_assign = (dequal_a->metatype == DBCC_TYPE_METATYPE_STRUCT
                     || dequal_a->metatype == DBCC_TYPE_METATYPE_UNION)
                     && dequal_a == dequal_b;

  /* — the left operand has atomic, qualified, or unqualified
       pointer type, and (considering the type the left operand would
       have after lvalue conversion) both operands are pointers to
       qualified or unqualified versions of compatible types,
       and the type pointed to by the left has all the qualifiers of the type
       pointed to by the right; */
  bool compatible_ptr_assign = dbcc_type_is_pointer (dequal_a) 
                            && dbcc_type_is_pointer (dequal_b)
                            && dbcc_types_compatible (
                                  dbcc_type_pointer_dereference (dequal_a),
                                  dbcc_type_pointer_dereference (dequal_b));
//XXX: we need to enforce the last clause (that is that qualifiers must remain)

  /* — the left operand has atomic, qualified, or unqualified pointer type,
       and (considering the type the left operand would have after
       lvalue conversion) one operand is a pointer to an object type,
       and the other is a pointer to a qualified or unqualified version of
       void, and the type pointed to by the left has all the qualifiers
       of the type pointed to by the right; */
  bool void_ptr_assign = dbcc_type_is_pointer (dequal_a) 
                      && dbcc_type_is_pointer (dequal_b) 
                      && dbcc_type_dequalify (
                           dbcc_type_pointer_dereference (dequal_b))->metatype
                                == DBCC_TYPE_METATYPE_VOID;
//XXX: we need to enforce the last clause (that is that qualifiers must remain)

  /* — the left operand is an atomic, qualified, or unqualified pointer,
       and the right is a null pointer constant; or */
  bool assign_null = dbcc_type_is_pointer (inout_type)
                  && dbcc_expr_is_null_pointer_constant (b);

  /* — the left operand has type atomic, qualified, or unqualified _Bool,
       and the right is a pointer. */
  bool bool_from_ptr = dequal_a->metatype == DBCC_TYPE_METATYPE_BOOL
                    && dbcc_type_is_pointer (b_type);

  if (struct_assign && !dbcc_type_is_incomplete (dequal_a))
    {
      *error = dbcc_error_new (DBCC_ERROR_ASSIGNMENT_TYPE_INCOMPLETE,
                               "cannot assign an incomplete type");
      return false;
    }
  if (!arithmetic
   && !struct_assign
   && !compatible_ptr_assign
   && !void_ptr_assign
   && !assign_null
   && !bool_from_ptr)
    {
      *error = dbcc_error_new (DBCC_ERROR_ASSIGNMENT_TYPES_INCOMPATIBLE,
                               "cannot cast from %s to %s",
                               dbcc_type_to_cstring (inout_type),
                               dbcc_type_to_cstring (b_type));
      return false;
    }

  return true;
}

DBCC_Expr *
dbcc_expr_new_inplace_binary   (DBCC_Namespace     *ns,
                                DBCC_InplaceBinaryOperator op,
                                DBCC_Expr          *in_out,
                                DBCC_Expr          *b,
                                DBCC_Error        **error)
{
  (void) ns;

  if (!dbcc_expr_is_lvalue (in_out))
    {
      *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                               "left-side of '%s' must be assignable (aka an lvalue)",
                               dbcc_inplace_binary_operator_name (op));
      return NULL;
    }

  DBCC_Type *inout_type = in_out->base.value_type;
  DBCC_Type *b_type = b->base.value_type;

  /* No assignment construct allows the LHS to be const, so
   * just check for that at the outset. */
  if (inout_type->metatype == DBCC_TYPE_METATYPE_QUALIFIED
   && (inout_type->v_qualified.qualifiers & DBCC_TYPE_QUALIFIER_CONST) != 0)
    {
      *error = dbcc_error_new (DBCC_ERROR_CONST_LVALUE,
                               "cannot assign to const-qualified type");
      return NULL;
    }

  if (op == DBCC_INPLACE_BINARY_OPERATOR_ASSIGN)
    {
      if (!check_simple_assignment (in_out, b, error))
        return false;
    }
  else if (op == DBCC_INPLACE_BINARY_OPERATOR_ADD_ASSIGN
        || op == DBCC_INPLACE_BINARY_OPERATOR_SUB_ASSIGN)
    {
      // both arithmetic or pointer/integer
      bool arith = dbcc_type_is_arithmetic (inout_type)
              &&   dbcc_type_is_arithmetic (b_type);
      bool pointer_int = dbcc_type_is_pointer (inout_type)
              &&   dbcc_type_is_integer (b_type);
      if (!arith && !pointer_int)
        {
          *error = dbcc_error_new (DBCC_ERROR_NONARITHMETIC_VALUES,
                                   "operator '%s' requires numbers or pointer/integer",
                                   dbcc_inplace_binary_operator_name (op));
          return false;
        }
    }
  else
    {
      /* p2 "For the other operators,
             the left operand shall have atomic,
             qualified, or unqualified arithmetic type,
             and (considering the type the left operand
             would have after lvalue conversion)
             each operand shall have arithmetic type consistent
             with those allowed by the corresponding binary operator." */
      if (!dbcc_type_is_arithmetic (inout_type)
       || !dbcc_type_is_arithmetic (b_type))
        {
          *error = dbcc_error_new (DBCC_ERROR_NONARITHMETIC_VALUES,
                                   "operator '%s' requires numbers",
                                   dbcc_inplace_binary_operator_name (op));
          return false;
        }

      /* enforce type constraints wrt the corresponding binary operator */
      switch (op)
        {
        case DBCC_INPLACE_BINARY_OPERATOR_LEFT_SHIFT_ASSIGN:
        case DBCC_INPLACE_BINARY_OPERATOR_RIGHT_SHIFT_ASSIGN:
          if (!dbcc_type_is_integer (inout_type)
           || !dbcc_type_is_integer (b_type))
            {
              *error = dbcc_error_new (DBCC_ERROR_SHIFT_OF_NONINTEGER_VALUE,
                                       "operator '%s' requires integer",
                                       dbcc_inplace_binary_operator_name (op));
              return false;
            }
          break;
        default:
          break;
        }
    }
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_INPLACE_BINARY_OP);
  rv->base.value_type = inout_type;
  rv->v_inplace_binary.inout = in_out;
  rv->v_inplace_binary.b = b;
  rv->v_inplace_binary.op = op;
  return rv;
}

/* See 6.5.2.2 Function calls
*/
DBCC_Expr *
dbcc_expr_new_call  (DBCC_Namespace     *ns,
                     DBCC_Expr          *head,
                     unsigned            n_args,
                     DBCC_Expr         **args,
                     DBCC_Error        **error)
{
  DBCC_Type *head_type = head->base.value_type;
  DBCC_Type *tmp;
  DBCC_Type *fct = head_type->metatype == DBCC_TYPE_METATYPE_FUNCTION
                 ? head_type
                 : dbcc_type_is_pointer (head_type)
                   && (tmp=dbcc_type_pointer_dereference (head_type))->metatype
                            == DBCC_TYPE_METATYPE_FUNCTION
                 ? tmp
                 : NULL;
  DBCC_Type *value_type;
  DBCC_Type *kr_fct = head_type->metatype == DBCC_TYPE_METATYPE_KR_FUNCTION
                 ? head_type
                 : dbcc_type_is_pointer (head_type)
                   && (tmp=dbcc_type_pointer_dereference (head_type))->metatype
                            == DBCC_TYPE_METATYPE_KR_FUNCTION
                 ? tmp
                 : NULL;
  (void) ns;

  if (fct != NULL)
    {
      /* arity match? */
      if (n_args < fct->v_function.n_params) 
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_FEW_ARGUMENTS,
                                   "function given too few arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return NULL;
        }
      else if (n_args > fct->v_function.n_params && !fct->v_function.has_varargs)
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_MANY_ARGUMENTS,
                                   "function given too many arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return NULL;
        }

      /* Type check arguments. */
      for (unsigned i = 0; i < fct->v_function.n_params; i++)
        {
          /* p7: If the expression that denotes the called function
                 has a type that does include a prototype,
                 the arguments are implicitly converted,
                 as if by assignment, to the types of the
                 corresponding parameters, taking the type of each parameter
                 to be the unqualified version of its declared type. */
          /* Section 6.3 "Conversions" gives the rules for what can
             be implicitly converted. */
          if (!dbcc_type_implicitly_convertable (fct->v_function.params[i].type,
                                                args[i]->base.value_type))
            {
              *error = dbcc_error_new (DBCC_ERROR_TYPE_MISMATCH,
                                       "cannot convert argument %u from %s to %s",
                                       (unsigned)(i+1),
                                       dbcc_type_to_cstring (args[i]->base.value_type),
                                       dbcc_type_to_cstring (fct->v_function.params[i].type));
              dbcc_error_add_code_position (*error, args[i]->base.code_position);
              return NULL;
            }
        }

      /* Set expression value_type to return-type of function */
      value_type = fct->v_function.return_type;
    }
  else if (kr_fct != NULL)
    {
      if (n_args < fct->v_function_kr.n_params) 
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_FEW_ARGUMENTS,
                                   "K+R function given too few arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function_kr.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return NULL;
        }
      else if (n_args > fct->v_function_kr.n_params)
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_MANY_ARGUMENTS,
                                   "K+R function given too many arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function_kr.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return NULL;
        }
      value_type = fct->v_function_kr.return_type;
      assert (value_type != NULL);
    }
  else
    { 
      *error = dbcc_error_new (DBCC_ERROR_NOT_A_FUNCTION,
                               "called object not a function (%s)",
                               dbcc_type_to_cstring (head_type));
      return NULL;
    }

  DBCC_Expr *call = expr_alloc (DBCC_EXPR_TYPE_CALL);
  call->v_call.function_type = fct != NULL ? fct : kr_fct;
  call->v_call.head = head;
  call->v_call.n_args = n_args;
  call->v_call.args = args;
  call->base.value_type = value_type;
  call->base.code_position = dbcc_code_position_ref (head->base.code_position);
  return call;
}

/* 6.5.4 Cast operators.
 *
 * p2: Unless the type name specifies a void type,
 *     the type name shall specify atomic, qualified,
 *     or unqualified scalar type, and the operand shall have scalar type.
 * p3: Conversions that involve pointers, other than where permitted
 *     by the constraints of 6.5.16.1, shall be specified by means
 *     of an explicit cast.
 * p4: A pointer type shall not be converted to any floating type.
 *     A floating type shall not be converted to any pointer type.
 *
 * Note that p3 implies (i guess) and any two pointer types
 * can be converted by an explicit cast, but obviously
 * there could be type-qualifiers that can't be converted.
 */
DBCC_Expr *
dbcc_expr_new_cast             (DBCC_Namespace     *ns,
                                DBCC_Type          *type,
                                DBCC_Expr          *value,
                                DBCC_Error        **error)
{
  void *constant_value = NULL;
  (void) ns;
  if (type->metatype == DBCC_TYPE_METATYPE_VOID)
    {
      // nothing to do
    }
  else if (dbcc_type_is_scalar (type)
   && dbcc_type_is_scalar (value->base.value_type))
    {
      if (value->base.constant_value != NULL)
        {
          constant_value = malloc (type->base.sizeof_instance);
          dbcc_cast_value (type, constant_value,
                           value->base.value_type, value->base.constant_value);
        }
    }
  else if (dbcc_type_is_pointer (type)
        && dbcc_type_is_pointer (value->base.value_type))
    {
      // ok: nothing to do
    }
  else if (dbcc_type_is_pointer (type)
        && dbcc_type_is_integer (value->base.value_type))
    {
      // ok: nothing to do
    }
  else if (dbcc_type_is_pointer (type)
        && dbcc_type_is_floating_point (value->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_CAST,
                               "cannot floating-point type to pointer");
      dbcc_error_add_code_position (*error, value->base.code_position);
      return NULL;
    }
  else if (dbcc_type_is_floating_point (type)
        && dbcc_type_is_pointer (value->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_CAST,
                               "cannot pointer to floating-point type");
      dbcc_error_add_code_position (*error, value->base.code_position);
      return NULL;
    }
  else
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_CAST,
                               "cannot cast %s to %s",
                               dbcc_type_to_cstring (value->base.value_type),
                               dbcc_type_to_cstring (type));
      dbcc_error_add_code_position (*error, value->base.code_position);
      return NULL;
    }

  DBCC_Expr *expr = expr_alloc (DBCC_EXPR_TYPE_CAST);
  expr->v_cast.pre_cast_expr = value;
  expr->base.value_type = dbcc_type_ref (type);
  expr->base.constant_value = constant_value;
  expr->base.code_position = dbcc_code_position_ref (value->base.code_position);
  return expr;
}


/* This returns one of the input expressions, and assumes ownership
 * of all args (consist w all other expr constructors)
 */
DBCC_Expr *
dbcc_expr_new_generic_selection(DBCC_Namespace     *ns,
                                DBCC_Expr          *expr,
                                size_t              n_associations,
                                DBCC_GenericAssociation *associations,
                                DBCC_Expr          *default_expr,
                                DBCC_Error        **error)
{
  DBCC_Expr *rv = NULL;
  assert (expr->base.value_type != NULL);
  (void) ns;
  size_t i;
  for (i = 0; i < n_associations; i++)
    {
      if (dbcc_types_compatible (expr->base.value_type, associations[i].type))
        {
          rv = associations[i].expr;
          break;
        }
    }
  if (i == n_associations)
    {
      if (default_expr == NULL)
        {
          *error = dbcc_error_new (DBCC_ERROR_NO_GENERIC_SELECTION,
                                   "no _Generic() options matched");
          return NULL;
        }
      else
        {
          rv = default_expr;
          default_expr = NULL;
        }
    }
  else
    {
      // clear selected expression, so that we don't destroy it.
      associations[i].expr = NULL;
    }

  for (unsigned i = 0; i < n_associations; i++)
    if (associations[i].expr != NULL)
      dbcc_expr_destroy (associations[i].expr);
  if (default_expr != NULL)
    dbcc_expr_destroy (default_expr);
  free (associations);
  return rv;
}

DBCC_Expr *
dbcc_expr_new_inplace_unary    (DBCC_Namespace     *ns,
                                DBCC_InplaceUnaryOperator op,
                                DBCC_Expr          *in_out,
                                DBCC_Error        **error)
{
  (void) ns;

  if (!dbcc_expr_is_lvalue (in_out))
    {
      *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                               "operator %s requires assignment (lvalue) expression",
                               dbcc_inplace_unary_operator_name (op));
      dbcc_error_add_code_position (*error, in_out->base.code_position);
      return NULL;
    }

  if (!dbcc_type_is_scalar (in_out->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                               "operator %s requires numeric expression",
                               dbcc_inplace_unary_operator_name (op));
      dbcc_error_add_code_position (*error, in_out->base.code_position);
      return NULL;
    }

  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_INPLACE_UNARY_OP);
  rv->base.value_type = dbcc_type_ref (in_out->base.value_type);
  rv->v_inplace_unary.op = op;
  rv->v_inplace_unary.inout = in_out;
  return rv;
}

DBCC_Expr *
dbcc_expr_new_member_access    (DBCC_Namespace     *ns,
                                DBCC_Expr          *expr,
                                DBCC_Symbol        *name,
                                DBCC_Error        **error)
{
  DBCC_Type *deq_type = dbcc_type_dequalify (expr->base.value_type);
  switch (deq_type->metatype)
    {
    case DBCC_TYPE_METATYPE_STRUCT:
      if (deq_type->v_struct.incomplete)
        {
          ... error
        }
      ...
      break;
    case DBCC_TYPE_METATYPE_UNION:
      if (deq_type->v_union.incomplete)
        {
          ... error
        }
      ...
      break;
    default:
      ...
    }

  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_MEMBER_ACCESS);
  rv->base.value_type = ...;
  rv->v_member_access.object = expr;
  rv->v_member_access.name = name;
  rv->v_member_access.sub_info = sub_info;

  // TODO: CONSTANT FOLDING???
  //rv->base.constant_value = 

  return rv;
}

DBCC_Expr *
dbcc_expr_new_pointer_access   (DBCC_Expr          *expr,
                                DBCC_Symbol        *name)
{
  if (!dbcc_type_is_pointer (expr->base.value_type))
    {
      ...
    }
  if (...) 
    {
      ...
    }
}

DBCC_Expr *
dbcc_expr_new_sizeof_expr      (DBCC_Expr          *expr)
{
  assert(expr->base.value_type != NULL);
  return dbcc_expr_new_sizeof_type (expr->base.value_type);
}

DBCC_Expr *
dbcc_expr_new_sizeof_type      (DBCC_Type          *type)
{
  assert(!dbcc_type_is_incomplete (type));
  ... create CONSTANT expr
  return ...
}

DBCC_Expr *
dbcc_expr_new_subscript        (DBCC_Expr          *object,
                                DBCC_Expr          *subscript)
{
...
}

DBCC_Expr *
dbcc_expr_new_symbol           (DBCC_Namespace     *ns,
                                DBCC_Symbol        *symbol)
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

  free (expr);
}

void
dbcc_expr_add_code_position    (DBCC_Expr          *expr,
                                DBCC_CodePosition  *cp)
{
  expr->base.code_position = dbcc_code_position_ref (cp);
}

/* 6.3.2.3p3: "An integer constant expression with the value 0,
               or such an expression cast to type void *,
               is called a _null pointer constant_.

               If a null pointer constant is converted to a
               pointer type, the resulting pointer, called a _null pointer_,
               is guaranteed to compare unequal
               to a pointer to any object or function."   */
bool dbcc_expr_is_null_pointer_constant (DBCC_Expr *expr)
{
  DBCC_Type *type = dbcc_type_dequalify (expr->base.value_type);
  if (expr->expr_type == DBCC_EXPR_TYPE_CONSTANT)
    {
      if (dbcc_type_is_floating_point (expr->expr_type))
        return false;
      return is_zero (maybe_zero->base.value_type->base.sizeof_instance,
                      maybe_zero->base.constant_value);
    }
  else if (expr->expr_type == DBCC_EXPR_TYPE_CAST)
    {
      DBCC_Type *t = expr->base.value_type;
      if (!dbcc_type_is_pointer (t));
        return false;
      t = dbcc_type_pointer_dereference (expr->base.value_type);
      t = dbcc_type_dequalify (t);
      if (t->metatype != DBCC_TYPE_METATYPE_VOID)
        return false;
      DBCC_Expr *maybe_zero = expr->v_cast.pre_cast_expr;
      return (maybe_zero->expr_type == DBCC_EXPR_TYPE_CONSTANT
           && dbcc_type_is_integer (maybe_zero->base.value_type)
           && is_zero (maybe_zero->base.value_type->base.sizeof_instance,
                       maybe_zero->base.constant_value));
    }
  else
    return false;
}
