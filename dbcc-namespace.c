#include "dbcc.h"

#define INT_TYPES_OFFSET     0
#define UINT_TYPES_OFFSET    4
#define BOOL_TYPE_OFFSET     8
#define FLOAT_TYPES_OFFSET   9
#define VOID_TYPE_OFFSET     18

#define N_NAMESPACE_BUILTINS 19

struct DBCC_NamespaceBuiltins
{
  DBCC_Type btypes[N_NAMESPACE_BUILTINS];
};

static void
init_ns_builtins (DBCC_Namespace *globals,
                  DBCC_NamespaceBuiltins *b)
{
  b->btypes[INT_TYPES_OFFSET + 0] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "int8"),
      .base.ref_count = 1,
      .base.sizeof_instance = 1,
      .base.alignof_instance = 1,
      .is_signed = true
    }
  };
  b->btypes[INT_TYPES_OFFSET + 1] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "int16"),
      .base.ref_count = 1,
      .base.sizeof_instance = 2,
      .base.alignof_instance = globals->target_env->alignof_int16,
      .is_signed = true
    }
  };
  b->btypes[INT_TYPES_OFFSET + 2] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "int32"),
      .base.ref_count = 1,
      .base.sizeof_instance = 4,
      .base.alignof_instance = globals->target_env->alignof_int32,
      .is_signed = true
    }
  };
  b->btypes[INT_TYPES_OFFSET + 3] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "int64"),
      .base.ref_count = 1,
      .base.sizeof_instance = 8,
      .base.alignof_instance = globals->target_env->alignof_int64,
      .is_signed = true
    }
  };

  b->btypes[UINT_TYPES_OFFSET + 0] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "uint8"),
      .base.ref_count = 1,
      .base.sizeof_instance = 1,
      .base.alignof_instance = 1,
      .is_signed = false,
    }
  };
  b->btypes[UINT_TYPES_OFFSET + 1] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "uint16"),
      .base.ref_count = 1,
      .base.sizeof_instance = 2,
      .base.alignof_instance = globals->target_env->alignof_int16,
      .is_signed = false,
    }
  };
  b->btypes[UINT_TYPES_OFFSET + 2] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "uint32"),
      .base.ref_count = 1,
      .base.sizeof_instance = 4,
      .base.alignof_instance = globals->target_env->alignof_int32,
      .is_signed = false,
    }
  };
  b->btypes[UINT_TYPES_OFFSET + 3] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_INT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "uint64"),
      .base.ref_count = 1,
      .base.sizeof_instance = 8,
      .base.alignof_instance = globals->target_env->alignof_int64,
      .is_signed = false,
    }
  };
  b->btypes[BOOL_TYPE_OFFSET] = (DBCC_Type) {
    .v_int = {
      .base.metatype = DBCC_TYPE_METATYPE_BOOL,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "bool"),
      .base.ref_count = 1,
      .base.sizeof_instance = globals->target_env->sizeof_bool,
      .base.alignof_instance = globals->target_env->alignof_bool,
      .is_signed = false,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_FLOAT] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "float"),
      .base.ref_count = 1,
      .base.sizeof_instance = 4,
      .base.alignof_instance = globals->target_env->alignof_float,
      .float_type = DBCC_FLOAT_TYPE_FLOAT,
      .is_complex = false,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_DOUBLE] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "double"),
      .base.ref_count = 1,
      .base.sizeof_instance = 8,
      .base.alignof_instance = globals->target_env->alignof_double,
      .float_type = DBCC_FLOAT_TYPE_DOUBLE,
      .is_complex = false,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_LONG_DOUBLE] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "long double"),
      .base.ref_count = 1,
      .base.sizeof_instance = globals->target_env->sizeof_long_double,
      .base.alignof_instance = globals->target_env->alignof_long_double,
      .float_type = DBCC_FLOAT_TYPE_LONG_DOUBLE,
      .is_complex = false,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_IMAGINARY_FLOAT] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "imaginary float"),
      .base.ref_count = 1,
      .base.sizeof_instance = 4,
      .base.alignof_instance = globals->target_env->alignof_float,
      .float_type = DBCC_FLOAT_TYPE_IMAGINARY_FLOAT,
      .is_complex = true,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "imaginary double"),
      .base.ref_count = 1,
      .base.sizeof_instance = 8,
      .base.alignof_instance = globals->target_env->alignof_double,
      .float_type = DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE,
      .is_complex = true,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "imaginary long double"),
      .base.ref_count = 1,
      .base.sizeof_instance = globals->target_env->sizeof_long_double,
      .base.alignof_instance = globals->target_env->alignof_long_double,
      .float_type = DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE,
      .is_complex = true,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_COMPLEX_FLOAT] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "complex float"),
      .base.ref_count = 1,
      .base.sizeof_instance = 4 * 2,
      .base.alignof_instance = globals->target_env->alignof_float,
      .float_type = DBCC_FLOAT_TYPE_COMPLEX_FLOAT,
      .is_complex = true,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_COMPLEX_DOUBLE] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "complex double"),
      .base.ref_count = 1,
      .base.sizeof_instance = 8 * 2,
      .base.alignof_instance = globals->target_env->alignof_double,
      .float_type = DBCC_FLOAT_TYPE_COMPLEX_DOUBLE,
      .is_complex = true,
    }
  };
  b->btypes[FLOAT_TYPES_OFFSET + DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE] = (DBCC_Type) {
    .v_float = {
      .base.metatype = DBCC_TYPE_METATYPE_FLOAT,
      .base.name = dbcc_symbol_space_force (globals->symbol_space, "complex long double"),
      .base.ref_count = 1,
      .base.sizeof_instance = globals->target_env->sizeof_long_double * 2,
      .base.alignof_instance = globals->target_env->alignof_long_double,
      .float_type = DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE,
      .is_complex = true,
    }
  };
  b->btypes[VOID_TYPE_OFFSET] = (DBCC_Type) {
    .base = {
      .metatype = DBCC_TYPE_METATYPE_FLOAT,
      .name = dbcc_symbol_space_force (globals->symbol_space, "void"),
      .ref_count = 1,
      .sizeof_instance = 0,
      .alignof_instance = 1,
    }
  };
}


DBCC_Namespace *
dbcc_namespace_new_global        (DBCC_TargetEnvironment *target_env)
{
  DBCC_Namespace *ns = calloc (sizeof (DBCC_Namespace), 1);
  ns->is_global = true;
  ns->symbol_space = dbcc_symbol_space_new ();
  ns->target_env = target_env;
  DBCC_NamespaceBuiltins *b = malloc (sizeof (DBCC_NamespaceBuiltins));
  init_ns_builtins (ns, b);
  ns->builtins = b;
  ns->ref_count = 1;
  return ns;
}

DBCC_Namespace *
dbcc_namespace_new_scope         (void);

bool
dbcc_namespace_lookup            (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol,
                                  DBCC_NamespaceEntry *out);

DBCC_Type *
dbcc_namespace_lookup_struct_tag (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol)
{
  return dbcc_ptr_table_lookup_value (&ns->struct_tag_symbols, symbol);
}

DBCC_Type *
dbcc_namespace_lookup_union_tag (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol)
{
  return dbcc_ptr_table_lookup_value (&ns->union_tag_symbols, symbol);
}

DBCC_Type *
dbcc_namespace_lookup_enum_tag   (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol)
{
  return dbcc_ptr_table_lookup_value (&ns->enum_tag_symbols, symbol);
}
void dbcc_namespace_add_by_tag   (DBCC_Namespace      *ns,
                                  DBCC_Type           *type);   // must be struct, union or enum with tag



void dbcc_namespace_add_enum_value (DBCC_Namespace *ns,
                                    DBCC_EnumValue *enum_value);

/* Global namespace functions to create DBCC_Addresses from
 * constant data etc.
 */
DBCC_Address *dbcc_namespace_global_allocate_constant (DBCC_Namespace *ns,
                                                       size_t length,
                                                       const uint8_t *data);
DBCC_Address *dbcc_namespace_global_allocate_constant0(DBCC_Namespace *ns,
                                                       size_t length,
                                                       const uint8_t *data);

/* Fixed-width types. */
DBCC_Type * dbcc_namespace_get_integer_type           (DBCC_Namespace  *ns,
                                                       bool is_signed,
                                                       unsigned size)
{
  unsigned index = is_signed ? INT_TYPES_OFFSET : UINT_TYPES_OFFSET;
  switch (size)
    {
    case 1: index += 0; break;
    case 2: index += 1; break;
    case 4: index += 2; break;
    case 8: index += 3; break;
    default: assert(0);
    }
  return ns->builtins->btypes + index;
}

/* ... */
DBCC_Type *
dbcc_namespace_get_char_type              (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, ns->target_env->is_char_signed, 1);
}
DBCC_Type * dbcc_namespace_get_float_type             (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_FLOAT);
}
DBCC_Type * dbcc_namespace_get_double_type            (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_DOUBLE);
}

DBCC_Type * dbcc_namespace_get_long_double_type       (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_LONG_DOUBLE);
}

DBCC_Type * dbcc_namespace_get_long_long_type         (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, ns->target_env->sizeof_long_long_int);
}
DBCC_Type * dbcc_namespace_get_long_type              (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, ns->target_env->sizeof_long_int);
}
DBCC_Type * dbcc_namespace_get_short_type             (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, 2);
}
DBCC_Type * dbcc_namespace_get_signed_char_type       (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, 1);
}
DBCC_Type * dbcc_namespace_get_unsigned_int_type      (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, false, ns->target_env->sizeof_int);
}
DBCC_Type * dbcc_namespace_get_unsigned_long_long_type(DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, false, ns->target_env->sizeof_long_long_int);
}
DBCC_Type * dbcc_namespace_get_unsigned_long_type     (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, false, ns->target_env->sizeof_long_int);
}
DBCC_Type * dbcc_namespace_get_unsigned_short_type    (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, false, 2);
}
DBCC_Type * dbcc_namespace_get_int_type               (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, ns->target_env->sizeof_int);
}
DBCC_Type * dbcc_namespace_get_size_type              (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, false, ns->target_env->sizeof_pointer);
}
DBCC_Type * dbcc_namespace_get_ssize_type             (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, ns->target_env->sizeof_pointer);
}
DBCC_Type * dbcc_namespace_get_ptrdiff_type           (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_integer_type (ns, true, ns->target_env->sizeof_pointer);
}

DBCC_Type * dbcc_namespace_get_imaginary_float_type   (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_IMAGINARY_FLOAT);
}

DBCC_Type * dbcc_namespace_get_imaginary_double_type  (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE);
}

DBCC_Type * dbcc_namespace_get_imaginary_long_double_type(DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE);
}

DBCC_Type * dbcc_namespace_get_complex_float_type     (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_COMPLEX_FLOAT);
}

DBCC_Type * dbcc_namespace_get_complex_double_type    (DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_COMPLEX_DOUBLE);
}

DBCC_Type * dbcc_namespace_get_complex_long_double_type(DBCC_Namespace *ns)
{
  return dbcc_namespace_get_floating_point_type(ns, DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE);
}


// '\0', 'L', 'u', 'U'
DBCC_Type * dbcc_namespace_get_constant_char_type     (char  prefix);

DBCC_Type * dbcc_namespace_get_floating_point_type    (DBCC_Namespace *ns,
                                                       DBCC_FloatType  ftype)
{
  assert(ftype < 9);
  return ns->builtins->btypes + FLOAT_TYPES_OFFSET + ftype;
}
