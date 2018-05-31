#include "dbcc.h"

#define is_zero   dbcc_is_zero

bool
dbcc_expr_is_lvalue (DBCC_Expr *expr)
{
  if (expr->base.value_type != NULL
   && (dbcc_type_get_qualifiers (expr->base.value_type)
       & DBCC_TYPE_QUALIFIER_CONST) != 0)
    {
      // lvalue should not be const
      return false;
    }

  switch (expr->expr_type)
    {
    case DBCC_EXPR_TYPE_IDENTIFIER:
      switch (expr->v_identifier.id_type)
        {
        case DBCC_IDENTIFIER_TYPE_UNKNOWN:
          // just assume the best if type-inference is not complete
          return true;

        case DBCC_IDENTIFIER_TYPE_GLOBAL:
        case DBCC_IDENTIFIER_TYPE_LOCAL:
          /* const-check is above. */
          return true;

        case DBCC_IDENTIFIER_TYPE_ENUM_VALUE:
          return false;
        }

    case DBCC_EXPR_TYPE_ACCESS:
      if (expr->v_access.is_pointer)
        return true;
      else
        return dbcc_expr_is_lvalue (expr->v_access.object);

    case DBCC_EXPR_TYPE_UNARY_OP:
      return expr->v_unary.op == DBCC_UNARY_OPERATOR_DEREFERENCE;

    default:
      return false;
    }
}

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
  expr = dbcc_expr_new_int_constant (dbcc_namespace_get_size_type (ns),
                                     type->base.alignof_instance);
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
      if (aexpr->base.constant != NULL
       && bexpr->base.constant != NULL)
        {
          assert(aexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          assert(bexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          void *acasted = alloca (type->base.sizeof_instance);
          void *bcasted = alloca (type->base.sizeof_instance);

          // cast both values
          if (!dbcc_cast_value (type, acasted,
                                atype, aexpr->base.constant->v_value.data)
           || !dbcc_cast_value (type, bcasted,
                                btype, bexpr->base.constant->v_value.data))
            assert (0);

          DBCC_Constant *result = malloc (sizeof (DBCC_Constant));
          result->constant_type = DBCC_CONSTANT_TYPE_VALUE;
          result->v_value.data = malloc (type->base.sizeof_instance);
          dbcc_typed_value_add (type, result->v_value.data,
                                acasted, bcasted);
          expr->base.constant = result;
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
      if (aexpr->base.constant != NULL
       && bexpr->base.constant != NULL)
        {
          assert(aexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          assert(bexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          void *acasted = alloca (type->base.sizeof_instance);
          void *bcasted = alloca (type->base.sizeof_instance);
          void *const_value = malloc (type->base.sizeof_instance);

          // cast both values
          if (!dbcc_cast_value (type, acasted,
                                atype, aexpr->base.constant->v_value.data)
           || !dbcc_cast_value (type, bcasted,
                                btype, bexpr->base.constant->v_value.data))
            assert (0);

          dbcc_typed_value_subtract (type, const_value, acasted, bcasted);

          expr->base.constant->v_value.data = const_value;
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
  if (aexpr->base.constant != NULL
   && bexpr->base.constant != NULL)
    {
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant->v_value.data)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant->v_value.data))
        assert (0);

      dbcc_typed_value_multiply (type, const_value, acasted, bcasted);

      expr->base.constant = malloc (sizeof (DBCC_Constant));
      expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
      expr->base.constant->v_value.data = const_value;
      free (acasted);
      free (bcasted);
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
  if (aexpr->base.constant != NULL
   && bexpr->base.constant != NULL)
    {
      assert(aexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
      assert(bexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant->v_value.data)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant->v_value.data))
        assert (0);

      dbcc_typed_value_multiply (type, const_value, acasted, bcasted);

      expr->base.constant = malloc (sizeof (DBCC_Constant));
      expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
      expr->base.constant->v_value.data = const_value;
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
  if (aexpr->base.constant != NULL
   && bexpr->base.constant != NULL)
    {
      assert(aexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
      assert(bexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant->v_value.data)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant->v_value.data))
        assert (0);

      dbcc_typed_value_multiply (type, const_value, acasted, bcasted);

      expr->base.constant = malloc (sizeof (DBCC_Constant));
      expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
      expr->base.constant->v_value.data = const_value;
      free (acasted);
      free (bcasted);
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

      if (aexpr->base.constant != NULL
       && bexpr->base.constant != NULL)
        {
          assert(aexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          assert(bexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          DBCC_Type *cmp_type = usual_arithment_conversion_get_type (ns, atype, btype);
          void *acasted = alloca (cmp_type->base.sizeof_instance);
          void *bcasted = alloca (cmp_type->base.sizeof_instance);
          dbcc_cast_value (cmp_type, acasted, atype, aexpr->base.constant->v_value.data);
          dbcc_cast_value (cmp_type, bcasted, btype, bexpr->base.constant->v_value.data);
          void *res = malloc (ns->target_env->sizeof_int);
          dbcc_typed_value_compare (ns,
                                    cmp_type,
                                    expr->v_binary.op,
                                    res, acasted, bcasted);

          expr->base.constant = malloc (sizeof (DBCC_Constant));
          expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
          expr->base.constant->v_value.data = res;
          free (acasted);
          free (bcasted);
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

      if (aexpr->base.constant != NULL
       && bexpr->base.constant != NULL)
        {
          assert(aexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          assert(bexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
          DBCC_Type *cmp_type = usual_arithment_conversion_get_type (ns, atype, btype);
          void *acasted = alloca (cmp_type->base.sizeof_instance);
          void *bcasted = alloca (cmp_type->base.sizeof_instance);
          dbcc_cast_value (cmp_type, acasted, atype, aexpr->base.constant->v_value.data);
          dbcc_cast_value (cmp_type, bcasted, btype, bexpr->base.constant->v_value.data);
          void *res = malloc (ns->target_env->sizeof_int);
          dbcc_typed_value_compare (ns,
                                    cmp_type,
                                    expr->v_binary.op,
                                    res, acasted, bcasted);

          expr->base.constant = malloc (sizeof (DBCC_Constant));
          expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
          expr->base.constant->v_value.data = res;
          free (acasted);
          free (bcasted);
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

/* 6.5.7 Bitwise shift operators
 */
static bool
handle_shift_operator   (DBCC_Namespace *ns,
                         DBCC_Expr *expr,
                         DBCC_Error **error)
{
  /* 6.5.7p1 Each of the operands shall have integer type.  */
  DBCC_Expr *a = expr->v_binary.a;
  DBCC_Expr *b = expr->v_binary.b;
  DBCC_Type *a_type = a->base.value_type;
  DBCC_Type *b_type = b->base.value_type;
  if (!dbcc_type_is_integer (a_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_SHIFT_OF_NONINTEGER_VALUE,
                               "operator %s requires first argument be integer, was %s",
                               dbcc_binary_operator_name (expr->v_binary.op),
                               dbcc_type_to_cstring (a_type));
      dbcc_error_add_code_position (*error, a->base.code_position);
      return false;
    }
  if (!dbcc_type_is_integer (b_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_SHIFT_OF_NONINTEGER_VALUE,
                               "operator %s requires second argument be integer, was %s",
                               dbcc_binary_operator_name (expr->v_binary.op),
                               dbcc_type_to_cstring (b_type));
      dbcc_error_add_code_position (*error, b->base.code_position);
      return false;
    }
  DBCC_Type *rv_type = usual_arithment_conversion_get_type (ns, a_type, b_type);
  assert (rv_type != NULL);
  expr->base.value_type = rv_type;

  if (a->base.constant->v_value.data != NULL
   && b->base.constant->v_value.data != NULL)
    {
      assert(a->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);
      assert(b->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE);

      const void *ac_orig = a->base.constant->v_value.data;
      const void *bc_orig = b->base.constant->v_value.data;
      void *a_casted_to_rvtype = alloca (rv_type->base.sizeof_instance);
      void *b_casted_to_rvtype = alloca (rv_type->base.sizeof_instance);
      void *rv_value = alloca (rv_type->base.sizeof_instance);
      dbcc_cast_value (rv_type, a_casted_to_rvtype, a->base.value_type, ac_orig);
      dbcc_cast_value (rv_type, b_casted_to_rvtype, b->base.value_type, bc_orig);
      switch (expr->v_binary.op)
        {
        case DBCC_BINARY_OPERATOR_SHIFT_LEFT:
          dbcc_typed_value_shift_left (rv_type, rv_value,
                                       a_casted_to_rvtype,
                                       b_casted_to_rvtype);
          break;
        case DBCC_BINARY_OPERATOR_SHIFT_RIGHT:
          dbcc_typed_value_shift_right(rv_type, rv_value,
                                       a_casted_to_rvtype,
                                       b_casted_to_rvtype);
          break;
        default:
          assert(0);
        }

      expr->base.constant = malloc (sizeof (DBCC_Constant));
      expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
      expr->base.constant->v_value.data = rv_value;
      free (a_casted_to_rvtype);
      free (b_casted_to_rvtype);
      return false;
    }

  return true;
}

/* 6.5.13. Logical AND operator
 *
 * p2: Each of the operands shall have scalar type.
 *
 *
 * 6.5.14. Logical OR operator.
 *
 * p2: Each of the operands shall have scalar type.
 */
static bool
handle_logical_op       (DBCC_Namespace *ns,
                         DBCC_Expr *expr,
                         DBCC_Error **error)
{
  DBCC_Expr *a = expr->v_binary.a;
  DBCC_Expr *b = expr->v_binary.b;
  if (!dbcc_type_is_scalar (a->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_LOGICAL_OPERATOR_REQUIRES_SCALARS,
                               "left-side of %s is not a scalar (is %s)",
                               dbcc_binary_operator_name (expr->v_binary.op),
                               dbcc_type_to_cstring (a->base.value_type));
      dbcc_error_add_code_position (*error, a->base.code_position);
      return false;
    }
  if (!dbcc_type_is_scalar (b->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_LOGICAL_OPERATOR_REQUIRES_SCALARS,
                               "right-side of %s is not a scalar (is %s)",
                               dbcc_binary_operator_name (expr->v_binary.op),
                               dbcc_type_to_cstring (b->base.value_type));
      dbcc_error_add_code_position (*error, b->base.code_position);
      return false;
    }
  expr->base.value_type = dbcc_namespace_get_int_type (ns);
  if (a->base.constant != NULL
   && b->base.constant != NULL)
    {
      DBCC_TriState a_truth = dbcc_typed_value_scalar_to_tristate (a->base.value_type, a->base.constant);
      DBCC_TriState b_truth = dbcc_typed_value_scalar_to_tristate (b->base.value_type, b->base.constant);
      if (a_truth != DBCC_MAYBE && b_truth != DBCC_MAYBE)
        {
          int value = expr->v_binary.op == DBCC_BINARY_OPERATOR_LOGICAL_AND
                    ? (a_truth && b_truth)
                    : (a_truth || b_truth);
          expr->base.constant = malloc (sizeof (DBCC_Constant));
          expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
          expr->base.constant->v_value.data = malloc (ns->target_env->sizeof_int);
          dbcc_typed_value_set_int64 (expr->base.value_type,
                                      expr->base.constant->v_value.data,
                                      value);
        }
    }
  return true;
}

static bool
handle_bitwise_op       (DBCC_Namespace *ns,
                         DBCC_Expr *expr,
                         DBCC_Error **error)
{
  /* 6.5.10 Bitwise AND operator
   * 6.5.11 Bitwise exclusive OR operator
   * 6.5.12 Bitwise inclusive OR operator
   *
   * p2: Each of the operands shall have integer type.
   * p3: The usual arithmetic conversions are performed on the operands.
   */
  DBCC_Expr *aexpr = expr->v_binary.a;
  DBCC_Expr *bexpr = expr->v_binary.b;
  DBCC_Type *atype = dbcc_type_dequalify (aexpr->base.value_type);
  DBCC_Type *btype = dbcc_type_dequalify (bexpr->base.value_type);
  if (!dbcc_type_is_integer (atype)
   || !dbcc_type_is_integer (btype))
    {
      *error = dbcc_error_new (DBCC_ERROR_NONINTEGER_BITWISE_OP,
                               "%s must be applied to integer types",
                               dbcc_binary_operator_name (expr->v_binary.op));
      dbcc_error_add_code_position (*error, expr->base.code_position);
      return false;
    }

  DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
  expr->base.value_type = type;

  /* constant folding */
  if (aexpr->base.constant->v_value.data != NULL
   && bexpr->base.constant->v_value.data != NULL)
    {
      void *acasted = alloca (type->base.sizeof_instance);
      void *bcasted = alloca (type->base.sizeof_instance);
      void *const_value = malloc (type->base.sizeof_instance);

      // cast both values
      if (!dbcc_cast_value (type, acasted,
                            atype, aexpr->base.constant->v_value.data)
       || !dbcc_cast_value (type, bcasted,
                            btype, bexpr->base.constant->v_value.data))
        assert (0);

      switch (expr->v_binary.op)
        {
        case DBCC_BINARY_OPERATOR_BITWISE_AND:
          dbcc_typed_value_bitwise_and (type, const_value, acasted, bcasted);
          break;
          
        case DBCC_BINARY_OPERATOR_BITWISE_OR:
          dbcc_typed_value_bitwise_or (type, const_value, acasted, bcasted);
          break;
        case DBCC_BINARY_OPERATOR_BITWISE_XOR:
          dbcc_typed_value_bitwise_xor (type, const_value, acasted, bcasted);
          break;
        default:
          assert(0);
        }

      expr->base.constant = malloc (sizeof (DBCC_Constant));
      expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
      expr->base.constant->v_value.data = const_value;
    }
  return true;
}

/* 6.5.17 Comma operator
 */
static bool
handle_comma_operator   (DBCC_Namespace *ns,
                         DBCC_Expr *expr,
                         DBCC_Error **error)
{
  (void) error;
  (void) ns;

  expr->base.value_type = expr->v_binary.b->base.value_type;

  // TODO cannot constant fold unless we can prove no side-effects.

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
  [DBCC_BINARY_OPERATOR_SHIFT_LEFT] = handle_shift_operator,
  [DBCC_BINARY_OPERATOR_SHIFT_RIGHT] = handle_shift_operator,
  [DBCC_BINARY_OPERATOR_LOGICAL_AND] = handle_logical_op,
  [DBCC_BINARY_OPERATOR_LOGICAL_OR] = handle_logical_op,
  [DBCC_BINARY_OPERATOR_BITWISE_AND] = handle_bitwise_op,
  [DBCC_BINARY_OPERATOR_BITWISE_OR] = handle_bitwise_op,
  [DBCC_BINARY_OPERATOR_BITWISE_XOR] = handle_bitwise_op,
  [DBCC_BINARY_OPERATOR_COMMA] = handle_comma_operator,
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

  if (a->base.value_type == NULL || b->base.value_type == NULL)
    {
      return rv;
    }
  
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


static bool
do_inplace_binary_type_inference (DBCC_Namespace *ns,
                                  DBCC_Expr *expr,
                                  DBCC_Error **error)
{
  DBCC_Expr *inout = expr->v_inplace_binary.inout;
  DBCC_Expr *b = expr->v_inplace_binary.b;
  DBCC_Type *inout_type = inout->base.value_type;
  if (inout_type == NULL)
    {
      if (!dbcc_expr_do_type_inference (ns, inout, error))
        return false;
      inout_type = inout->base.value_type;
      assert (inout_type != NULL);
    }
  DBCC_Type *b_type = b->base.value_type;
  if (inout_type == NULL)
    {
      if (!dbcc_expr_do_type_inference (ns, b, error))
        return false;
      b_type = b->base.value_type;
      assert(b_type != NULL);
    }

  /* No assignment construct allows the LHS to be const, so
   * just check for that at the outset. */
  if (inout_type->metatype == DBCC_TYPE_METATYPE_QUALIFIED
   && (inout_type->v_qualified.qualifiers & DBCC_TYPE_QUALIFIER_CONST) != 0)
    {
      *error = dbcc_error_new (DBCC_ERROR_CONST_LVALUE,
                               "cannot assign to const-qualified type");
      return NULL;
    }

  DBCC_InplaceBinaryOperator op = expr->v_inplace_binary.op;
  if (op == DBCC_INPLACE_BINARY_OPERATOR_ASSIGN)
    {
      if (!check_simple_assignment (inout, b, error))
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
  return true;
}

DBCC_Expr *
dbcc_expr_new_inplace_binary   (DBCC_Namespace     *ns,
                                DBCC_InplaceBinaryOperator op,
                                DBCC_Expr          *inout,
                                DBCC_Expr          *b,
                                DBCC_Error        **error)
{
  (void) ns;

  if (!dbcc_expr_is_lvalue (inout))
    {
      *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                               "left-side of '%s' must be assignable (aka an lvalue)",
                               dbcc_inplace_binary_operator_name (op));
      return NULL;
    }

  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_INPLACE_BINARY_OP);
  rv->v_inplace_binary.inout = inout;
  rv->v_inplace_binary.b = b;
  rv->v_inplace_binary.op = op;

  if (inout->base.value_type != NULL && b->base.value_type != NULL)
    {
      if (!do_inplace_binary_type_inference (ns, rv, error))
        {
          dbcc_expr_destroy (rv);
          return NULL;
        }
    }

  return rv;
}

/* See 6.5.2.2 Function calls
*/
static bool
do_call_type_inference (DBCC_Namespace *ns,
                        DBCC_Expr      *call_expr,
                        DBCC_Error    **error)
{
  DBCC_Expr *head = call_expr->v_call.head;
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
  size_t n_args = call_expr->v_call.n_args;

  if (fct != NULL)
    {
      /* Function must have the correct number of arguments.  */
      if (n_args < fct->v_function.n_params) 
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_FEW_ARGUMENTS,
                                   "function given too few arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return false;
        }
      else if (n_args > fct->v_function.n_params && !fct->v_function.has_varargs)
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_MANY_ARGUMENTS,
                                   "function given too many arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return false;
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
                                                call_expr->v_call.args[i]->base.value_type))
            {
              *error = dbcc_error_new (DBCC_ERROR_TYPE_MISMATCH,
                                       "cannot convert argument %u from %s to %s",
                                       (unsigned)(i+1),
                                       dbcc_type_to_cstring (call_expr->v_call.args[i]->base.value_type),
                                       dbcc_type_to_cstring (fct->v_function.params[i].type));
              dbcc_error_add_code_position (*error, call_expr->v_call.args[i]->base.code_position);
              return false;
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
          return false;
        }
      else if (n_args > fct->v_function_kr.n_params)
        {
          *error = dbcc_error_new (DBCC_ERROR_TOO_MANY_ARGUMENTS,
                                   "K+R function given too many arguments: %u arguments, requires %u",
                                   (unsigned) n_args,
                                   (unsigned) fct->v_function_kr.n_params);
          dbcc_error_add_code_position (*error, head->base.code_position);
          return false;
        }
      value_type = fct->v_function_kr.return_type;
      assert (value_type != NULL);
    }
  else
    { 
      *error = dbcc_error_new (DBCC_ERROR_NOT_A_FUNCTION,
                               "called object not a function (%s)",
                               dbcc_type_to_cstring (head_type));
      return false;
    }
  return true;
}
DBCC_Expr *
dbcc_expr_new_call  (DBCC_Namespace     *ns,
                     DBCC_Expr          *head,
                     unsigned            n_args,
                     DBCC_Expr         **args,
                     DBCC_Error        **error)
{
  DBCC_Expr *call = expr_alloc (DBCC_EXPR_TYPE_CALL);
  call->v_call.function_type = NULL;
  call->v_call.head = head;
  call->v_call.n_args = n_args;
  call->v_call.args = args;
  call->base.value_type = NULL;
  call->base.code_position = dbcc_code_position_ref (head->base.code_position);

  if (head->base.value_type != NULL)
    {
      size_t i;
      for (i = 0; i < n_args; i++)
        if (args[i]->base.value_type == NULL)
          break;
      if (i == n_args)
        {
          //if head and all args are resolved, do type inference
          if (!do_call_type_inference (ns, call, error))
            {
              dbcc_expr_destroy (call);
              return NULL;
            }
        }
    }
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
static bool
do_cast_type_inference (DBCC_Namespace *ns,
                        DBCC_Expr *expr,
                        DBCC_Error **error)
{
  DBCC_Type *cast_type = expr->base.value_type;
  DBCC_Expr *value = expr->v_cast.pre_cast_expr;
  (void) ns;

  if (cast_type->metatype == DBCC_TYPE_METATYPE_VOID)
    {
      // nothing to do
    }
  else if (dbcc_type_is_scalar (cast_type)
        && dbcc_type_is_scalar (value->base.value_type))
    {
      if (value->base.constant != NULL
       && value->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE)
        {
          
          expr->base.constant = dbcc_constant_new_value (cast_type, NULL);
          dbcc_cast_value (cast_type,
                           expr->base.constant->v_value.data,
                           value->base.value_type,
                           value->base.constant->v_value.data);
        }
    }
  else if (dbcc_type_is_pointer (cast_type)
        && dbcc_type_is_pointer (value->base.value_type))
    {
      // ok: nothing to do
    }
  else if (dbcc_type_is_pointer (cast_type)
        && dbcc_type_is_integer (value->base.value_type))
    {
      // ok: nothing to do
    }
  else if (dbcc_type_is_pointer (cast_type)
        && dbcc_type_is_floating_point (value->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_CAST,
                               "cannot floating-point type to pointer");
      dbcc_error_add_code_position (*error, value->base.code_position);
      return NULL;
    }
  else if (dbcc_type_is_floating_point (cast_type)
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
                               dbcc_type_to_cstring (cast_type));
      dbcc_error_add_code_position (*error, value->base.code_position);
      return NULL;
    }

  return true;
}

DBCC_Expr *
dbcc_expr_new_cast             (DBCC_Namespace     *ns,
                                DBCC_Type          *type,
                                DBCC_Expr          *value,
                                DBCC_Error        **error)
{
  (void) ns;
  DBCC_Expr *expr = expr_alloc (DBCC_EXPR_TYPE_CAST);
  expr->v_cast.pre_cast_expr = value;
  expr->base.value_type = dbcc_type_ref (type);
  expr->base.code_position = dbcc_code_position_ref (value->base.code_position);
  if (!do_cast_type_inference (ns, expr, error))
    {
      dbcc_expr_destroy (expr);
      return NULL;
    }
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

static bool
do_inplace_unary_type_inference (DBCC_Namespace *ns,
                                 DBCC_Expr      *expr,
                                 DBCC_Error    **error)
{
  (void) ns;

  // Call is_lvalue again, because type is required
  // to determine constness.
  DBCC_Expr *sub = expr->v_inplace_unary.inout;

  if (dbcc_type_is_const (sub->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                               "operator %s requires non-const lefthand-side",
                               dbcc_inplace_unary_operator_name (expr->v_inplace_unary.op));
      dbcc_error_add_code_position (*error, sub->base.code_position);
      return false;
    }
  if (!dbcc_type_is_scalar (sub->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                               "operator %s requires numeric expression",
                               dbcc_inplace_unary_operator_name (expr->v_inplace_unary.op));
      dbcc_error_add_code_position (*error, sub->base.code_position);
      return false;
    }
  return true;
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

  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_INPLACE_UNARY_OP);
  rv->base.value_type = dbcc_type_ref (in_out->base.value_type);
  rv->v_inplace_unary.op = op;
  rv->v_inplace_unary.inout = in_out;
  return rv;
}

static bool
do_access_type_inference (DBCC_Namespace *ns,
                          DBCC_Expr *access,
                          DBCC_Error **error)
{
  DBCC_Expr *object = access->v_access.object;
  if (object->base.value_type == NULL
   && !dbcc_expr_do_type_inference (ns, object, error))
    return false;

  DBCC_Type *deq_type = dbcc_type_dequalify (object->base.value_type);
  if (access->v_access.is_pointer)
    {
      if (!dbcc_type_is_pointer (deq_type))
        {
          *error = dbcc_error_new (DBCC_ERROR_EXPECTED_POINTER,
                                   "operator -> must apply to a pointer, got %s",
                                   dbcc_type_to_cstring (deq_type));
          dbcc_error_add_code_position (*error, access->base.code_position);
          return NULL;
        }
      deq_type = dbcc_type_pointer_dereference (deq_type);
      deq_type = dbcc_type_dequalify (deq_type);
    }
  DBCC_Symbol *name = access->v_access.name;
  switch (deq_type->metatype)
    {
    case DBCC_TYPE_METATYPE_STRUCT:
      if (deq_type->v_struct.incomplete)
        {
          *error = dbcc_error_new (DBCC_ERROR_MEMBER_ACCESS_INCOMPLETE_TYPE,
                                   "structure incomplete, so cannot access member");
          dbcc_error_add_code_position (*error, access->base.code_position);
          return false;
        }
      DBCC_TypeStructMember *member = dbcc_type_struct_lookup_member (deq_type, name);
      if (member == NULL)
        {
          *error = dbcc_error_new (DBCC_ERROR_UNKNOWN_MEMBER_ACCESS,
                                   "unknown member %s",
                                   dbcc_symbol_get_string (name));
          dbcc_error_add_code_position (*error, access->base.code_position);
          return false;
        }
      access->base.value_type = member->type;
      access->v_access.sub_info = member;
      access->v_access.is_union = false;
      return true;
    case DBCC_TYPE_METATYPE_UNION:
      if (deq_type->v_union.incomplete)
        {
          *error = dbcc_error_new (DBCC_ERROR_MEMBER_ACCESS_INCOMPLETE_TYPE,
                                   "union incomplete, so cannot access branch");
          dbcc_error_add_code_position (*error, access->base.code_position);
          return false;
        }
      DBCC_TypeUnionBranch *branch = dbcc_type_union_lookup_branch (deq_type, name);
      if (branch == NULL)
        {
          *error = dbcc_error_new (DBCC_ERROR_UNKNOWN_MEMBER_ACCESS,
                                   "unknown member %s",
                                   dbcc_symbol_get_string (name));
          dbcc_error_add_code_position (*error, access->base.code_position);
          return false;
        }
      access->base.value_type = branch->type;
      access->v_access.sub_info = branch;
      access->v_access.is_union = true;
      return true;
    default:
      *error = dbcc_error_new (DBCC_ERROR_BAD_TYPE_FOR_MEMBER_ACCESS,
                               "no members of type %s",
                               dbcc_type_to_cstring (deq_type));
      dbcc_error_add_code_position (*error, access->base.code_position);
      return false;
    }
}

DBCC_Expr *
dbcc_expr_new_access           (DBCC_Namespace     *ns,
                                bool                is_pointer,
                                DBCC_Expr          *expr,
                                DBCC_Symbol        *name,
                                DBCC_Error        **error)
{

  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_ACCESS);
  rv->v_access.object = expr;
  rv->v_access.name = name;
  rv->v_access.is_pointer = is_pointer;

  if (expr->base.value_type != NULL
   && !do_access_type_inference (ns, rv, error))
    {
      dbcc_expr_destroy (rv);
      return NULL;
    }

  // TODO: CONSTANT FOLDING???
  //rv->base.constant_value = 

  return rv;
}

DBCC_Expr *
dbcc_expr_new_sizeof_expr      (DBCC_Namespace     *ns,
                                DBCC_Expr          *expr,
                                DBCC_Error        **error)
{
  assert(expr->base.value_type != NULL);
  return dbcc_expr_new_sizeof_type (ns,
                                    expr->base.code_position,
                                    expr->base.value_type,
                                    error);
}

DBCC_Expr *
dbcc_expr_new_sizeof_type      (DBCC_Namespace     *ns,
                                DBCC_CodePosition  *cp,
                                DBCC_Type          *type,
                                DBCC_Error        **error)
{
  if (!dbcc_type_is_incomplete (type))
    {
      *error = dbcc_error_new (DBCC_ERROR_SIZEOF_INCOMPLETE_TYPE,
                               "cannot take sizeof incomplete type");
      dbcc_error_add_code_position (*error, cp);
      return NULL;
    }
  size_t size = type->base.sizeof_instance;
  DBCC_Expr *rv = dbcc_expr_new_int_constant (dbcc_namespace_get_size_type (ns),
                                              size);
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

DBCC_Expr *
dbcc_expr_new_subscript        (DBCC_Namespace     *ns,
                                DBCC_Expr          *object,
                                DBCC_Expr          *subscript,
                                DBCC_Error        **error)
{
  DBCC_Expr *sum = dbcc_expr_new_binary_operator (ns, DBCC_BINARY_OPERATOR_ADD,
                                                  object, subscript, error);
  if (sum == NULL)
    return NULL;
  return dbcc_expr_new_unary (ns, DBCC_UNARY_OPERATOR_DEREFERENCE, sum, error);
}

static bool
do_symbol_type_inference (DBCC_Namespace     *ns,
                          DBCC_Expr          *id,
                          DBCC_Error        **error)
{
  DBCC_NamespaceEntry entry;
  if (!dbcc_namespace_lookup (ns, id->v_identifier.name, &entry))
    {
      *error = dbcc_error_new (DBCC_ERROR_IDENTIFIER_NOT_FOUND,
                               "identifier %s not found",
                               dbcc_symbol_get_string (id->v_identifier.name));
      dbcc_error_add_code_position (*error, id->base.code_position);
      return false;
    }
  switch (entry.entry_type)
    {
    case DBCC_NAMESPACE_ENTRY_TYPEDEF:
      // I don't think this error is possible,
      // because the lexer uses this lookup and converts typedefs into 
      // a new token.
      *error = dbcc_error_new (DBCC_ERROR_IDENTIFIER_IS_TYPEDEF,
                               "identifier refers to a type, expected value");
      return false;

    case DBCC_NAMESPACE_ENTRY_ENUM_VALUE:
      // store constant value
      {
      DBCC_Type *etype = entry.v_enum_value.enum_type;
      id->base.value_type = etype;
      id->v_identifier.id_type = DBCC_IDENTIFIER_TYPE_ENUM_VALUE;
      id->v_identifier.v_enum_value.enum_type = etype;
      id->v_identifier.v_enum_value.enum_value = entry.v_enum_value.enum_value;

      // setup constant value
      id->base.constant = malloc (sizeof (DBCC_Constant));
      id->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
      id->base.constant->v_value.data = malloc (etype->base.sizeof_instance);
      dbcc_typed_value_set_int64 (etype, id->base.constant->v_value.data,
                                  entry.v_enum_value.enum_value->value);
      return true;
      }

    case DBCC_NAMESPACE_ENTRY_GLOBAL:
      id->base.value_type = entry.v_global->type;
      id->v_identifier.v_global = entry.v_global;

      // XXX: constant folding?
      return true;

    case DBCC_NAMESPACE_ENTRY_LOCAL:
      id->base.value_type = entry.v_local->type;
      id->v_identifier.v_local = entry.v_local;
      return true;
    }
}

DBCC_Expr *
dbcc_expr_new_identifier       (DBCC_Namespace     *ns,
                                DBCC_Symbol        *symbol,
                                DBCC_Error        **error)
{
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_IDENTIFIER);
  (void) ns;
  rv->v_identifier.name = symbol;
  (void) error;
  return rv;
}

DBCC_Expr *
dbcc_expr_new_int_constant     (DBCC_Type          *type,
                                uint64_t            value)
{
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_CONSTANT);
  rv->base.value_type = type;
  rv->base.constant = malloc (sizeof (DBCC_Constant));
  rv->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
  rv->base.constant->v_value.data = malloc (type->base.sizeof_instance);
  dbcc_typed_value_set_int64 (type, rv->base.constant->v_value.data, value);
  return rv;
}

DBCC_Expr *
dbcc_expr_new_string_constant  (DBCC_Namespace    *ns,
                                const DBCC_String *constant)
{
  DBCC_Expr *expr = expr_alloc (DBCC_EXPR_TYPE_CONSTANT);
  DBCC_Address *address;
  address = dbcc_namespace_global_allocate_constant0
                  (ns, constant->length, (const uint8_t *) constant->str);
  expr->base.value_type = dbcc_type_new_array (ns->target_env,
                                 constant->length,
                                 dbcc_namespace_get_char_type(ns));
  expr->v_constant_address.address = address;
  return expr;
}

DBCC_Expr *
dbcc_expr_new_float_constant   (DBCC_Namespace     *global_ns,
                                DBCC_FloatType      float_type,
                                long double         value)
{
  DBCC_Expr *expr = expr_alloc (DBCC_EXPR_TYPE_CONSTANT);
  DBCC_Type *type = dbcc_namespace_get_floating_point_type (global_ns, float_type);
  expr->base.value_type = type;
  expr->base.constant = malloc (sizeof (DBCC_Constant));
  expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
  expr->base.constant->v_value.data = malloc (type->base.sizeof_instance);
  dbcc_typed_value_set_long_double (type, expr->base.constant->v_value.data, value);
  return expr;
}

/* 6.4.4.4 Character constants.
 */
DBCC_Expr *
dbcc_expr_new_char_constant    (DBCC_Type          *type,
                                uint32_t            value)
{
  DBCC_Expr *expr = expr_alloc (DBCC_EXPR_TYPE_CONSTANT);
  expr->base.value_type = type;
  expr->base.constant = malloc (sizeof (DBCC_Constant));
  expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
  expr->base.constant->v_value.data = malloc (type->base.sizeof_instance);
  dbcc_typed_value_set_int64 (type, expr->base.constant->v_value.data, value);
  return expr;
}

/* 6.5.3.3 Unary arithmetic operators
 */
static bool
do_unary_operator_type_inference (DBCC_Namespace *ns,
                                  DBCC_Expr      *expr,
                                  DBCC_Error    **error)
{
  if (!dbcc_expr_do_type_inference (ns, expr->v_unary.a, error))
    return false;
  DBCC_Expr *sub = expr->v_unary.a;
  DBCC_Type *subtype = sub->base.value_type;
  switch (expr->v_unary.op)
    {
    /* p1:  "The operand of the unary + or - operator
             shall have arithmetic type" */
    case DBCC_UNARY_OPERATOR_NOOP:
    case DBCC_UNARY_OPERATOR_NEGATE:
      // operator prefix-plus prefix-minus
      if (!dbcc_type_is_arithmetic (subtype))
        {
          *error = dbcc_error_new (DBCC_ERROR_NONARITHMETIC_VALUES,
                                   "unary + and - require a numeric argument");
          dbcc_error_add_code_position (*error, sub->base.code_position);
          return false;
        }
      expr->base.value_type = dbcc_type_dequalify (subtype);
      if (expr->v_unary.base.constant != NULL
       && expr->v_unary.base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE)
        {
          const void *in = sub->base.constant->v_value.data;
          void *out = malloc (subtype->base.sizeof_instance);
           if (expr->v_unary.op == DBCC_UNARY_OPERATOR_NOOP)
             {
               /* "p2: The result of the unary + operator is the value
                       of its (promoted) operand. The integer promotions are
                       performed on the operand, and the result has the promoted
                       type." */
               memcpy (out, in,
                       expr->base.value_type->base.sizeof_instance);
             }
           else
             {
               /* "p3: The result of the unary - operator is the negative of its
                       (promoted) operand. The integer promotions are performed
                       on the operand, and the result has the promoted type." */
               dbcc_typed_value_negate (expr->base.value_type, out, in);
             }
        }
      return true;

    case DBCC_UNARY_OPERATOR_LOGICAL_NOT:
      if (!dbcc_type_is_scalar (subtype))
        {
          *error = dbcc_error_new (DBCC_ERROR_NONSCALAR_VALUES,
                                   "operator! requires a scalar value, got %s",
                                   dbcc_type_to_cstring (subtype));
          dbcc_error_add_code_position (*error, sub->base.code_position);
          return false;
        }
      expr->base.value_type = dbcc_namespace_get_int_type (ns);
      if (sub->base.constant != NULL)
        {
          DBCC_TriState v = dbcc_typed_value_scalar_to_tristate (subtype, sub->base.constant);
          if (v != DBCC_MAYBE)
            {
              expr->base.constant = malloc (sizeof (DBCC_Constant));
              expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
              expr->base.constant->v_value.data = malloc (expr->base.value_type->base.sizeof_instance);
              dbcc_typed_value_set_int64 (expr->base.value_type,
                                          expr->base.constant->v_value.data,
                                          v);
            }
        }
      return true;
    case DBCC_UNARY_OPERATOR_BITWISE_NOT:
      if (!dbcc_type_is_integer (subtype))
        {
          *error = dbcc_error_new (DBCC_ERROR_NONINTEGER_BITWISE_OP,
                                   "operator~ requires an integer value, got %s",
                                   dbcc_type_to_cstring (subtype));
          dbcc_error_add_code_position (*error, sub->base.code_position);
          return false;
        }
      expr->base.value_type = dbcc_type_dequalify (subtype);
      if (sub->base.constant != NULL
       && sub->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE)
        {
          const void *in = sub->base.constant->v_value.data;
          void *out = malloc (subtype->base.sizeof_instance);
          expr->base.constant = malloc (sizeof (DBCC_Constant));
          expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
          expr->base.constant->v_value.data = out;
          dbcc_typed_value_bitwise_not (expr->base.value_type, out, in);
        }
      return true;
    case DBCC_UNARY_OPERATOR_REFERENCE:
      if (!dbcc_expr_is_lvalue (sub))
        {
          *error = dbcc_error_new (DBCC_ERROR_LVALUE_REQUIRED,
                                   "cannot take the address of a non-lvalue");
          dbcc_error_add_code_position (*error, sub->base.code_position);
          return false;
        }
      expr->base.value_type = dbcc_type_new_pointer (ns->target_env, subtype);
      return true;
    case DBCC_UNARY_OPERATOR_DEREFERENCE:
      if (!dbcc_type_is_pointer (subtype))
        {
          *error = dbcc_error_new (DBCC_ERROR_POINTER_REQUIRED,
                                   "cannot dereference non-pointer");
          dbcc_error_add_code_position (*error, sub->base.code_position);
          return false;
        }
      expr->base.value_type = dbcc_type_pointer_dereference (subtype);
      return true;
    }
  return false;
}

DBCC_Expr *
dbcc_expr_new_unary            (DBCC_Namespace     *ns,
                                DBCC_UnaryOperator  op,
                                DBCC_Expr          *a,
                                DBCC_Error        **error)
{
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_UNARY_OP);
  rv->v_unary.op = op;
  rv->v_unary.a = a;
  rv->base.code_position = dbcc_code_position_ref (a->base.code_position);
  if (!do_unary_operator_type_inference (ns, rv, error))
    {
      dbcc_expr_destroy (rv);
      return NULL;
    }
  return rv;
}

DBCC_Expr *
dbcc_expr_new_structured_initializer (DBCC_StructuredInitializer *init)
{
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_STRUCTURED_INITIALIZER);
  rv->v_structured_initializer.initializer = *init;
  return rv;
}

typedef struct FlatPieceContext FlatPieceContext;
struct FlatPieceContext
{
  DBCC_Namespace *namespace;
  size_t n_flat_pieces;
  DBCC_StructuredInitializerFlatPiece *flat_pieces;
  size_t flat_pieces_alloced;
};
#define FLAT_PIECE_CONTEXT_INIT_ALLOC_SIZE 16
#define FLAT_PIECE_CONTEXT_INIT \
  (FlatPieceContext) { \
    .n_flat_pieces = 0, \
    .flat_pieces = malloc (FLAT_PIECE_CONTEXT_INIT_ALLOC_SIZE \
                        * sizeof (DBCC_StructuredInitializerFlatPiece)),\
    .flat_pieces_alloced = FLAT_PIECE_CONTEXT_INIT_ALLOC_SIZE \
  }

static void
flat_piece_context_append (FlatPieceContext *ctx,
                           DBCC_StructuredInitializerFlatPiece *piece)
{
  if (ctx->n_flat_pieces == ctx->flat_pieces_alloced)
    {
      size_t new_alloced = ctx->flat_pieces_alloced * 2;
      ctx->flat_pieces = realloc (ctx->flat_pieces, sizeof (DBCC_StructuredInitializerFlatPiece));
      ctx->flat_pieces_alloced = new_alloced;
    }
  ctx->flat_pieces[ctx->n_flat_pieces++] = *piece;
}

static bool
fpc_flatten_recursive (FlatPieceContext *ctx,
                       DBCC_StructuredInitializer *init,
                       DBCC_Type *type,
                       size_t     offset,
                       DBCC_Error **error)
{
  size_t i;
  for (i = 0; i < init->n_pieces; i++)
    {
      DBCC_StructuredInitializerPiece *piece = init->pieces + i;

      /* Focus in on the exact member that needs
       * initialization and compute flat-piece,
       * or recurse if it's another structured-initializer.
       */
      DBCC_Type *subtype = dbcc_type_dequalify (type);
      unsigned suboffset = offset;
      size_t d;
      for (d = 0; d < piece->n_designators; d++)
        {
          switch (piece->designators[d].type)
            {
            case DBCC_DESIGNATOR_INDEX:
              {
              DBCC_Expr *index = piece->designators[d].v_index;
              if (subtype->metatype != DBCC_TYPE_METATYPE_ARRAY)
                {
                  *error = dbcc_error_new (DBCC_ERROR_EXPECTED_ARRAY,
                                           "array-style designator for static initializer not allowed for type %s",
                                           dbcc_type_to_cstring (subtype));
                  dbcc_error_add_code_position (*error, index->base.code_position);
                  return false;
                }
              if (dbcc_type_is_integer (index->base.value_type))
                {
                  *error = dbcc_error_new (DBCC_ERROR_NONINTEGER_DESIGNATOR,
                                           "designator index in structured-initializer is non-integer, was type %s",
                                           dbcc_type_to_cstring (index->base.value_type));
                  dbcc_error_add_code_position (*error, index->base.code_position);
                  return false;
                }
              if (index->base.constant == NULL
               || index->base.constant->constant_type != DBCC_CONSTANT_TYPE_VALUE)
                {
                  *error = dbcc_error_new (DBCC_ERROR_CONSTANT_REQUIRED,
                                           "designator indexing expression must be a constant");
                  //TODO set code position
                  return false;
                }
              int64_t v = dbcc_typed_value_get_int64 (index->base.value_type, index->base.constant->v_value.data);
              subtype = dbcc_type_dequalify (subtype->v_array.element_type);
              suboffset += subtype->base.sizeof_instance * v;
              break;
              }

            case DBCC_DESIGNATOR_MEMBER:
              {
              DBCC_Symbol *name = piece->designators[d].v_member;
              if (subtype->metatype == DBCC_TYPE_METATYPE_STRUCT)
                {
                  DBCC_TypeStructMember *member = dbcc_type_struct_lookup_member (subtype, name);
                  if (member == NULL)
                    {
                      *error = dbcc_error_new (DBCC_ERROR_DESIGNATOR_NOT_FOUND,
                                               "struct does not have member %s",
                                               dbcc_symbol_get_string (name));
                      return false;
                    }
                  subtype = member->type;
                  suboffset += member->offset;
                }
              else if (subtype->metatype == DBCC_TYPE_METATYPE_UNION)
                {
                  DBCC_TypeUnionBranch *branch = dbcc_type_union_lookup_branch (subtype, name);
                  if (branch == NULL)
                    {
                      *error = dbcc_error_new (DBCC_ERROR_DESIGNATOR_NOT_FOUND,
                                               "union does not have branch %s",
                                               dbcc_symbol_get_string (name));
                      return false;
                    }
                  subtype = branch->type;
                }
              else
                {
                  *error = dbcc_error_new (DBCC_ERROR_CONSTANT_REQUIRED,
                                           "named designator must be struct or union");
                  //TODO set code position
                  return false;
                }
              break;
              }
            }
        }

      /* If it is an expression, output a FlatPiece.
       * If it is a structured-initializer, recurse.
       */
      if (piece->is_expr)
        {
          if (!dbcc_expr_do_type_inference (ctx->namespace, piece->v_expr, error))
            return false;
          DBCC_StructuredInitializerFlatPiece fp = {
            .offset = suboffset,
            .length = subtype->base.sizeof_instance,
            .piece_expr = piece->v_expr
          };
          flat_piece_context_append (ctx, &fp);
        }
      else
        {
          if (!fpc_flatten_recursive (ctx,
                                      &piece->v_structured_initializer,
                                      subtype,
                                      suboffset,
                                      error))
            {
              /* TODO: sometimes, adorning the error with the
               * structured-initializer trace might
               * be useful, but for normal error presentation,
               * i think it's probably more confusing than helpful.
               */
              return false;
            }
        }
    }
  return true;
}

bool
dbcc_expr_structured_initializer_set_type (DBCC_Namespace *ns,
                                           DBCC_Expr *si,
                                           DBCC_Type *type,
                                           DBCC_Error **error)
{
  /* Type must be array (fixed or unsized), or struct/union.  */
  FlatPieceContext fpc = FLAT_PIECE_CONTEXT_INIT;
  fpc.namespace = ns;
  if (!fpc_flatten_recursive (&fpc, &si->v_structured_initializer.initializer,
                              type, 0,
                              error))
    return false;

  si->base.value_type = type;
  si->v_structured_initializer.n_flat_pieces = fpc.n_flat_pieces;
  si->v_structured_initializer.flat_pieces = fpc.flat_pieces;

  // TODO: constant folding?

  return true;
}

/* For ternary operator.
 *
 * "If both the second and third operands are pointers
 *  or one is a null pointer constant and the
 *  other is a pointer, the result type is a pointer
 *  to a type qualified with all the type qualifiers
 *  of the types referenced by both operands." [6.5.15p6]
 */
static DBCC_Type *
combine_ptr_and_npc (DBCC_Namespace *ns,
                     DBCC_Type *ptr, 
                     DBCC_Expr *null_ptr_constant,
                     DBCC_Error **error)
{
  DBCC_TypeQualifier null_qual = 0;
  DBCC_Type *pointed_at = dbcc_type_pointer_dereference (ptr);
  if (dbcc_type_is_pointer (null_ptr_constant->base.value_type))
    null_qual = dbcc_type_get_qualifiers (dbcc_type_pointer_dereference (null_ptr_constant->base.value_type));
  DBCC_TypeQualifier pqual = dbcc_type_get_qualifiers (pointed_at);
  DBCC_TypeQualifier qualifiers = null_qual | pqual;
  if (qualifiers == pqual)
    return dbcc_type_ref (dbcc_type_dequalify (ptr));
  else
    {
      DBCC_Type *qtype = dbcc_type_new_qualified (ns->target_env, pointed_at, qualifiers, error);
      if (qtype == NULL)
        return NULL;
      return dbcc_type_new_pointer (ns->target_env, qtype);
    }
}

/* 6.5.15 Conditional operator.
 * p2 the first operand shall have scalar type.
 * p3: One of the following shall hold for the second and third operands:
 *   — both operands have arithmetic type;   [CASE1]
 *   — both operands have the same structure or union type; [CASE2]
 *   — both operands have void type; [CASE3]
 *
 *   — both operands are pointers to qualified or unqualified versions
 *     of compatible types;
 *   — one operand is a pointer and the other is a null pointer constant; or
 *   — one operand is a pointer to an object type and the other is a pointer
 *     to a qualified or unqualified version of void.
 *   [CASE4 - handle all these 3 pointer cases]
 */
static bool
do_ternary_operator_type_inference (DBCC_Namespace *ns,
                                    DBCC_Expr *expr,
                                    DBCC_Error **error)
{
  DBCC_Expr *cond = expr->v_ternary.condition;
  if (!dbcc_type_is_scalar (cond->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_NONSCALAR_VALUES,
                               "first argument to ? : must be scalar (boolean-izeable)");
      dbcc_error_add_code_position (*error, expr->base.code_position);
      return false;
    }
  DBCC_TriState const_cond = cond->base.constant == NULL
                           ? DBCC_MAYBE
                           : dbcc_typed_value_scalar_to_tristate (cond->base.value_type,
                                                                  cond->base.constant);
  DBCC_Expr *aexpr = expr->v_ternary.true_value;
  DBCC_Type *atype = aexpr->base.value_type;
  DBCC_Expr *bexpr = expr->v_ternary.false_value;
  DBCC_Type *btype = bexpr->base.value_type;
  assert(atype != NULL && btype != NULL);

  /* CASE1: both types arithmetic. */
  if (dbcc_type_is_arithmetic (atype) && dbcc_type_is_arithmetic (btype))
    {
      /* p5: usual arithmetic conversions */
      DBCC_Type *type = usual_arithment_conversion_get_type (ns, atype, btype);
      if (type == NULL)
        {
          assert(0);
          return false;
        }
      expr->base.value_type = type;

      /* constant folding */
      if (const_cond != DBCC_MAYBE)
        {
          DBCC_Expr *vexpr = const_cond ? aexpr : bexpr;
          if (vexpr->base.constant != NULL
           && vexpr->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE)
            {
              expr->base.constant = malloc (sizeof (DBCC_Constant));
              expr->base.constant->constant_type = DBCC_CONSTANT_TYPE_VALUE;
              expr->base.constant->v_value.data = malloc (type->base.sizeof_instance);
              dbcc_cast_value (type,
                               expr->base.constant->v_value.data,
                               vexpr->base.value_type,
                               vexpr->base.constant->v_value.data);
            }
        }
      return true;
    }

  DBCC_Type *adeq = dbcc_type_dequalify (atype);
  DBCC_Type *bdeq = dbcc_type_dequalify (btype);
 
  if (adeq->metatype == bdeq->metatype)
    {
      /* CASE2: both operands have the same structure or union type */
      if (adeq->metatype == DBCC_TYPE_METATYPE_STRUCT
       || adeq->metatype == DBCC_TYPE_METATYPE_UNION)
        {
          if (adeq == bdeq)
            {
              expr->base.value_type = adeq;
            }
          else
            {
              *error = dbcc_error_new (DBCC_ERROR_TYPE_MISMATCH,
                                       "struct/union types must match exactly on branches of a conditional operator");
              dbcc_error_add_code_position (*error, expr->base.code_position);
              return false;
            }
        }
      /* CASE3: both operands are of type void */
      if (adeq->metatype == DBCC_TYPE_METATYPE_VOID
       && bdeq->metatype == DBCC_TYPE_METATYPE_VOID)
        {
          expr->base.value_type = adeq;
          return true;
        }
    }

  /* CASE4: both operands are pointers.
   * This is governed by p6, which we break into clauses as follows:
   */

  DBCC_Type *a_pointed_at = adeq->metatype == DBCC_TYPE_METATYPE_ARRAY
                          ? adeq->v_array.element_type
                          : adeq->metatype == DBCC_TYPE_METATYPE_POINTER
                          ? adeq->v_pointer.target_type
                          : NULL;
  DBCC_Type *b_pointed_at = bdeq->metatype == DBCC_TYPE_METATYPE_ARRAY
                          ? bdeq->v_array.element_type
                          : bdeq->metatype == DBCC_TYPE_METATYPE_POINTER
                          ? bdeq->v_pointer.target_type
                          : NULL;

  /* "If both the second and third operands are pointers
      or one is a null pointer constant and the
      other is a pointer, the result type is a pointer
      to a type qualified with all the type qualifiers
      of the types referenced by both operands." [p6] */
  if (a_pointed_at != NULL
   && dbcc_expr_is_null_pointer_constant (bexpr))
    {
      expr->base.value_type = combine_ptr_and_npc (ns, atype, bexpr, error);
      if (expr->base.value_type == NULL)
        return false;
      if (const_cond == DBCC_NO
       && bexpr->base.constant != NULL)
        {
          expr->base.constant = dbcc_constant_copy (bexpr->base.value_type, 
                                                    bexpr->base.constant);
        }
      else if (const_cond == DBCC_YES)
        {
          expr->base.constant = dbcc_typed_value_alloc0 (expr->base.value_type);
        }
      return true;
    }
  else if (b_pointed_at != NULL
        && dbcc_expr_is_null_pointer_constant (aexpr))
    {
      expr->base.value_type = combine_ptr_and_npc (ns, btype, aexpr, error);
      if (expr->base.value_type == NULL)
        return false;
      if (const_cond == DBCC_YES
       && aexpr->base.constant != NULL)
        {
          expr->base.constant = dbcc_constant_copy (aexpr->base.value_type, 
                                                    aexpr->base.constant);
        }
      else if (const_cond == DBCC_NO)
        {
          expr->base.constant = dbcc_constant_new_value0 (expr->base.value_type);
        }
      return true;
    }
      
  /* "Furthermore, if both operands are pointers to
      compatible types or to differently qualified versions
      of compatible types, the result type is
      a pointer to an appropriately qualified version of
      the composite type; if one operand is a
      null pointer constant, the result has the type of the other operand;
      otherwise, one operand is a pointer to void or a qualified version
      of void, in which case the result type is a
      pointer to an appropriately qualified version of void." [p6] */
  if (a_pointed_at != NULL && b_pointed_at != NULL)
    {
      if (!dbcc_types_compatible (a_pointed_at, b_pointed_at))
        {
          *error = dbcc_error_new (DBCC_ERROR_TYPE_MISMATCH,
                                   "pointers to incompatible types on each branch of ternary operator (%s and %s)",
                                   dbcc_type_to_cstring (a_pointed_at),
                                   dbcc_type_to_cstring (b_pointed_at));
          dbcc_error_add_code_position (*error, expr->base.code_position);
          return false;
        }
      DBCC_Type *composite = dbcc_types_make_composite (ns, a_pointed_at, b_pointed_at, error);
      if (composite == NULL)
        goto incompatible_types;

      /* Although we could use dbcc_type_new_pointer()
       * in all cases here, it is very common that the
       * composite equals a_pointed_at or b_pointed_at
       * (more often than not, they are equal).
       *
       * Hence we use this conditional to try and avoid allocating
       * a new type in the common case.
       *
       * It might be better to just cache all types in the
       * global namespace.
       */
      DBCC_Type *ptr_type = (atype->metatype == DBCC_TYPE_METATYPE_POINTER
                          && atype->v_pointer.target_type == composite)
                          ? dbcc_type_ref (atype)
                          : (btype->metatype == DBCC_TYPE_METATYPE_POINTER
                          && btype->v_pointer.target_type == composite)
                          ? dbcc_type_ref (btype)
                          : dbcc_type_new_pointer (ns->target_env, composite);

      // maybe add qualifiers
      DBCC_TypeQualifier q = dbcc_type_get_qualifiers (atype)
                           | dbcc_type_get_qualifiers (btype);
      DBCC_Type *qtype = q == 0
                       ? dbcc_type_ref (ptr_type)
                       : dbcc_type_new_qualified (ns->target_env, ptr_type, q, error);
      if (qtype == NULL)
        return false;

      expr->base.value_type = qtype;
      dbcc_type_unref (ptr_type);

      // TODO: is it possible that we have a constant pointer?

      return true;
    }

incompatible_types:
  if (*error == NULL)
    *error = dbcc_error_new (DBCC_ERROR_TYPE_MISMATCH,
                             "incompatible types %s and %s for ? :",
                             dbcc_type_to_cstring (atype),
                             dbcc_type_to_cstring (btype));
  dbcc_error_add_code_position (*error, aexpr->base.code_position);
  return false;
}

DBCC_Expr *
dbcc_expr_new_ternary (DBCC_Namespace *ns,
                       DBCC_Expr *cond,
                       DBCC_Expr *a,
                       DBCC_Expr *b,
                       DBCC_Error **error)
{
  if (cond->base.value_type != NULL
   && !dbcc_type_is_scalar (cond->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_NONSCALAR_VALUES,
                               "conditional expression for ? : operator was not a scalar");
      dbcc_error_add_code_position (*error, cond->base.code_position);
      return NULL;
    }
  DBCC_Expr *rv = expr_alloc (DBCC_EXPR_TYPE_TERNARY_OP);
  rv->v_ternary.condition = cond;
  rv->v_ternary.true_value = a;
  rv->v_ternary.false_value = b;
  rv->base.code_position = dbcc_code_position_ref (cond->base.code_position);
  if (cond->base.value_type != NULL
   && a->base.value_type != NULL
   && b->base.value_type != NULL)
    {
      if (!do_ternary_operator_type_inference (ns, rv, error))
        {
          dbcc_expr_destroy (rv);
          return NULL;
        }
    }
  return rv;
}

#if 0
void
dbcc_expr_destroy (DBCC_Expr *expr)
{
  switch (expr->expr_type)
    {
    ...
    }

  free (expr);
}
#endif

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
  //DBCC_Type *type = dbcc_type_dequalify (expr->base.value_type);
  if (expr->expr_type == DBCC_EXPR_TYPE_CONSTANT)
    {
      if (dbcc_type_is_floating_point (expr->base.value_type))
        return false;
      switch (expr->base.constant->constant_type)
        {
        case DBCC_CONSTANT_TYPE_VALUE:
          return dbcc_is_zero (expr->base.value_type->base.sizeof_instance,
                               expr->base.constant->v_value.data);
        default:
          return false;
        }
    }
  else if (expr->expr_type == DBCC_EXPR_TYPE_CAST)
    {
      DBCC_Type *t = expr->base.value_type;
      if (!dbcc_type_is_pointer (t))
        return false;
      t = dbcc_type_pointer_dereference (expr->base.value_type);
      t = dbcc_type_dequalify (t);
      if (t->metatype != DBCC_TYPE_METATYPE_VOID)
        return false;
      DBCC_Expr *maybe_zero = expr->v_cast.pre_cast_expr;
      return (maybe_zero->expr_type == DBCC_EXPR_TYPE_CONSTANT
           && dbcc_type_is_integer (maybe_zero->base.value_type)
           && maybe_zero->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE
           && is_zero (maybe_zero->base.value_type->base.sizeof_instance,
                       maybe_zero->base.constant->v_value.data));
    }
  else
    return false;
}

static DBCC_Constant *
mk_enum_constant (DBCC_Type *enum_type, DBCC_EnumValue *ev)
{
  DBCC_Constant *rv = dbcc_constant_new_value (enum_type, NULL);
  dbcc_typed_value_set_int64 (enum_type, rv->v_value.data, ev->value);
  return rv;
}

bool
dbcc_expr_do_type_inference (DBCC_Namespace *ns,
                             DBCC_Expr *expr,
                             DBCC_Error **error)
{
  if (expr->base.types_inferred)
    return true;

  switch (expr->expr_type)
    {
    case DBCC_EXPR_TYPE_UNARY_OP:
      if (!dbcc_expr_do_type_inference (ns, expr->v_unary.a, error))
        return false;
      if (!do_unary_operator_type_inference (ns, expr, error))
        return false;
      goto success;

    case DBCC_EXPR_TYPE_BINARY_OP:
      if (!dbcc_expr_do_type_inference (ns, expr->v_binary.a, error)
       || !dbcc_expr_do_type_inference (ns, expr->v_binary.b, error))
        return false;
      assert(expr->v_binary.op < DBCC_N_BINARY_OPERATORS);
      assert(binary_operator_handlers[expr->v_binary.op] != NULL);
      if (!(*(binary_operator_handlers[expr->v_binary.op]))(ns, expr, error))
        return false;
      goto success;

    case DBCC_EXPR_TYPE_TERNARY_OP:
      if (!dbcc_expr_do_type_inference (ns, expr->v_ternary.condition, error)
       || !dbcc_expr_do_type_inference (ns, expr->v_binary.a, error)
       || !dbcc_expr_do_type_inference (ns, expr->v_binary.b, error))
        return false;

      /* Compute type and do constant folding.  */
      if (!do_ternary_operator_type_inference (ns, expr, error))
        return false;

      goto success;

    case DBCC_EXPR_TYPE_INPLACE_BINARY_OP:
      if (expr->v_inplace_binary.inout->base.value_type == NULL
       && !dbcc_expr_do_type_inference (ns,
                                        expr->v_inplace_binary.inout,
                                        error))
        return false;
      if (expr->v_inplace_binary.b->base.value_type == NULL
       && !dbcc_expr_do_type_inference (ns,
                                        expr->v_inplace_binary.b,
                                        error))
        return false;
      if (!do_inplace_binary_type_inference (ns, expr, error))
        return false;
      goto success;

    case DBCC_EXPR_TYPE_INPLACE_UNARY_OP:
      if (expr->v_inplace_unary.inout->base.value_type == NULL
       && !dbcc_expr_do_type_inference (ns,
                                        expr->v_inplace_unary.inout,
                                        error))
        return false;

      if (!do_inplace_unary_type_inference (ns, expr, error))
        return false;
      goto success;

    case DBCC_EXPR_TYPE_CONSTANT:
      goto success;

    case DBCC_EXPR_TYPE_CALL:
      if (!do_call_type_inference (ns, expr, error))
        return false;
      goto success;

    case DBCC_EXPR_TYPE_CAST:
      {
        DBCC_Expr *sub_expr = expr->v_cast.pre_cast_expr;
        DBCC_Expr_Type sub_expr_type = sub_expr->expr_type;
        if (sub_expr_type == DBCC_EXPR_TYPE_STRUCTURED_INITIALIZER)
          {
            if (!dbcc_expr_structured_initializer_set_type
                     (ns, sub_expr, expr->base.value_type, error))
              return false;
          }
        else
          {
            if (!do_cast_type_inference (ns, expr, error))
              return false;
          }
        goto success;
      }

    case DBCC_EXPR_TYPE_ACCESS:
      if (!do_access_type_inference (ns, expr, error))
        return false;
      goto success;

    case DBCC_EXPR_TYPE_STRUCTURED_INITIALIZER:
      *error = dbcc_error_new (DBCC_ERROR_CANNOT_INFER_TYPE,
                               "cannot infer type from initializer");
      return false;

    case DBCC_EXPR_TYPE_IDENTIFIER:
      {
        DBCC_NamespaceEntry entry;
        if (!dbcc_namespace_lookup (ns, expr->v_identifier.name, &entry))
          {
            *error = dbcc_error_new (DBCC_ERROR_NOT_FOUND,
                                     "identifier '%s' not found",
                                     dbcc_symbol_get_string (expr->v_identifier.name));
            dbcc_error_add_code_position (*error, expr->base.code_position);
            return false;
          }
        switch (entry.entry_type)
          {
          case DBCC_NAMESPACE_ENTRY_TYPEDEF:
            assert(0);
            return false;
          case DBCC_NAMESPACE_ENTRY_ENUM_VALUE:
            {
              DBCC_Type *enum_type = entry.v_enum_value.enum_type;
              DBCC_EnumValue *ev = entry.v_enum_value.enum_value;
              expr->base.value_type = enum_type;
              expr->base.constant = mk_enum_constant (enum_type, ev);
            }
            break;
          case DBCC_NAMESPACE_ENTRY_GLOBAL:
            expr->base.value_type = entry.v_global->type;
            break;
          case DBCC_NAMESPACE_ENTRY_LOCAL:
            expr->base.value_type = entry.v_local->type;
            break;
          }
        goto success;
      }
    }

  assert(0);

success:
  expr->base.types_inferred = true;
  return true;
}
