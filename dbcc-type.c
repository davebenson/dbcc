#include "dbcc.h"
#include "dsk/dsk-qsort-macro.h"

DBCC_Type *
dbcc_type_ref   (DBCC_Type *type)
{
  assert(type->base.ref_count > 0);
  type->base.ref_count += 1;
  return type;
}

void
dbcc_type_unref (DBCC_Type *type)
{
  assert(type->base.ref_count > 0);
  if (type->base.ref_count > 1)
    type->base.ref_count -= 1;
  else
    {
      switch (type->metatype)
        {
        case DBCC_TYPE_METATYPE_VOID: break;
        case DBCC_TYPE_METATYPE_BOOL: break;
        case DBCC_TYPE_METATYPE_INT: break;
        case DBCC_TYPE_METATYPE_FLOAT: break;
        case DBCC_TYPE_METATYPE_ARRAY:
          dbcc_type_unref (type->v_array.element_type);
          break;
        case DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY:
          dbcc_type_unref (type->v_variable_length_array.element_type);
          break;
        case DBCC_TYPE_METATYPE_STRUCT:
          if (type->v_struct.tag != NULL)
            dbcc_symbol_unref (type->v_struct.tag);
          for (size_t i = 0; i < type->v_struct.n_members; i++)
            {
              dbcc_symbol_unref (type->v_struct.members[i].name);
              dbcc_type_unref (type->v_struct.members[i].type);
            }
          break;
        case DBCC_TYPE_METATYPE_UNION:
          if (type->v_union.tag != NULL)
            dbcc_symbol_unref (type->v_union.tag);
          for (size_t i = 0; i < type->v_union.n_branches; i++)
            {
              dbcc_symbol_unref (type->v_union.branches[i].name);
              dbcc_type_unref (type->v_union.branches[i].type);
            }
          break;
        case DBCC_TYPE_METATYPE_ENUM:
          if (type->v_enum.tag != NULL)
            dbcc_symbol_unref (type->v_enum.tag);
          for (size_t i = 0; i < type->v_union.n_branches; i++)
            {
              dbcc_symbol_unref (type->v_enum.values[i].name);
            }
          break;
        case DBCC_TYPE_METATYPE_POINTER:
          dbcc_type_unref (type->v_pointer.target_type);
          break;
        case DBCC_TYPE_METATYPE_TYPEDEF:
          break;
        case DBCC_TYPE_METATYPE_QUALIFIED:
          dbcc_type_unref (type->v_qualified.underlying_type);
          break;
        case DBCC_TYPE_METATYPE_FUNCTION:
          dbcc_type_unref (type->v_function.return_type);
          for (unsigned i = 0; i < type->v_function.n_params; i++)
            {
              dbcc_type_unref (type->v_function.params[i].type);
            }
          free (type->v_function.params);
          break;
        }
      if (type->base.name != NULL)
        dbcc_symbol_unref (type->base.name);
      free (type);
    }
}
static inline DBCC_Type *
new_type (DBCC_Type_Metatype t)
{
  DBCC_Type *rv = calloc (sizeof (DBCC_Type), 1);
  rv->metatype = t;
  rv->base.ref_count = 1;
  return rv;
}

#define NEW_TYPE(shortname) new_type(DBCC_TYPE_METATYPE_##shortname)


DBCC_Type *
dbcc_type_new_enum (DBCC_TargetEnvironment *env,
                    DBCC_Symbol            *optional_tag,
                    size_t                  n_values,
                    DBCC_EnumValue         *values,
                    DBCC_Error            **error)
{
  DBCC_Symbol **s = DBCC_NEW_ARRAY(n_values, DBCC_Symbol *);
  for (size_t i = 0; i < n_values; i++)
    s[i] = values[i].name;
#define COMPARE_BY_PTR(a,b, rv) rv = a < b ? -1 : a > b ? 1 : 0
  DSK_QSORT(s, DBCC_Symbol *, n_values, COMPARE_BY_PTR);
#undef COMPARE_BY_PTR
  for (size_t i = 0; i + 1 < n_values; i++)
    if (s[i] == s[i+1])
      {
        *error = dbcc_error_new (DBCC_ERROR_ENUM_DUPLICATES,
                                 "name '%s' repeated in enum",
                                 dbcc_symbol_get_string (s[i]));
        free (s);
        return NULL;
      }
  free (s);

  DBCC_Type *t = NEW_TYPE(ENUM);
  t->v_enum.tag = optional_tag;
  t->v_enum.n_values = n_values;
  t->v_enum.values = DBCC_NEW_ARRAY(n_values, DBCC_EnumValue);
  t->base.sizeof_instance = env->sizeof_int;
  for (size_t i = 0; i < n_values; i++)
    t->v_enum.values[i] = values[i];

  return t;
}

DBCC_Type *
dbcc_type_new_pointer (DBCC_TargetEnvironment *env,
                       DBCC_Type     *target)
{
  DBCC_Type *t = NEW_TYPE(POINTER);
  t->v_pointer.target_type = dbcc_type_ref (target);
  t->base.sizeof_instance = env->sizeof_pointer;
  return t;
}

DBCC_Type *
dbcc_type_new_array   (DBCC_TargetEnvironment *env,
                       int64_t       count, /* -1 for unspecified */
                       DBCC_Type     *element_type)
{
  DBCC_Type *t = NEW_TYPE(ARRAY);
  t->v_array.n_elements = count;
  t->v_array.element_type = dbcc_type_ref (element_type);
  t->base.sizeof_instance = element_type->base.sizeof_instance * (count < 0 ? 0 : (size_t) count);
  (void) env;
  return t;
}

DBCC_Type *
dbcc_type_new_varlen_array(DBCC_TargetEnvironment *env,
                           DBCC_Type     *element_type)
{
  DBCC_Type *t = NEW_TYPE(VARIABLE_LENGTH_ARRAY);
  t->v_variable_length_array.element_type = dbcc_type_ref (element_type);
  t->base.sizeof_instance = 0;
  (void) env;
  return t;
}

DBCC_Type *
dbcc_type_new_function  (DBCC_Type          *rettype,
                         size_t              n_params,
                         DBCC_Param         *params,
                         bool                has_varargs)
{
  DBCC_Type *t = NEW_TYPE(FUNCTION);
  t->v_function.return_type = dbcc_type_ref(rettype);
  if (n_params == 1 && params[0].name == NULL && params[0].type->metatype == DBCC_TYPE_METATYPE_VOID)
    n_params = 0;
  t->v_function.n_params = n_params;
  t->v_function.params = DBCC_NEW_ARRAY(n_params, DBCC_TypeFunctionParam);
  t->v_function.has_varargs = has_varargs;
  for (unsigned i = 0; i < n_params; i++)
    {
      t->v_function.params[i].name = params[i].name;
      t->v_function.params[i].type = dbcc_type_ref (params[i].type);
    }
  return t;
}

DBCC_Type *dbcc_type_new_qualified(DBCC_TargetEnvironment *env,
                                   DBCC_Type              *base_type,
                                   DBCC_TypeQualifier      qualifiers,
                                   DBCC_Error            **error)
{
  if (qualifiers == 0)
    return dbcc_type_ref (base_type);
  if (base_type->metatype == DBCC_TYPE_METATYPE_QUALIFIED)
    {
      DBCC_TypeQualifier old_q = base_type->v_qualified.qualifiers;
      DBCC_TypeQualifier new_q = old_q | qualifiers;
      if (old_q == new_q) 
        return dbcc_type_ref (base_type);
      else
        {
          qualifiers = new_q;
          base_type = base_type->v_qualified.underlying_type;
        }
    }

  /* 6.7.3p2: "Types other than pointer types whose referenced
   *           type is an object type shall not be restrict-qualified." */
  if ((qualifiers & DBCC_TYPE_QUALIFIER_RESTRICT) != 0)
    {
      if (base_type->metatype != DBCC_TYPE_METATYPE_POINTER)
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_RESTRICTED_TYPE,
                                   "restrict can only be used with pointers");
          return NULL;
        }
     }

  /* 6.7.3p3: "The type modified by the _Atomic qualifier
   *           shall not be an array type or a function type." */
  if ((qualifiers & DBCC_TYPE_QUALIFIER_ATOMIC) != 0)
    {
      if (base_type->metatype == DBCC_TYPE_METATYPE_FUNCTION
       || base_type->metatype == DBCC_TYPE_METATYPE_ARRAY
       || base_type->metatype == DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY)
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_ATOMIC_TYPE,
                                   "arrays and functions must not be atomic");
          return NULL;
        }
    }

  //TODO: verify that type is allowed for target architecture
  (void) env;

  DBCC_Type *t = NEW_TYPE(QUALIFIED);
  t->base.sizeof_instance = base_type->base.sizeof_instance;
  t->v_qualified.qualifiers = qualifiers;
  t->v_qualified.underlying_type = dbcc_type_ref (base_type);
  return t;
}

static size_t *
get_members_by_sym (size_t              n_members,
                    DBCC_Param         *members,
                    DBCC_Error        **error)
{
  if (n_members == 0)
    {
      *error = dbcc_error_new (DBCC_ERROR_STRUCT_EMPTY,
                               "struct must have at least 1 member");
      return NULL;
    }

  size_t *rv = DBCC_NEW_ARRAY(n_members, size_t);
  for (size_t i = 0; i < n_members; i++)
    rv[i] = i;
#define COMP_MIA(a,b, rv) { \
  DBCC_Symbol *symbol_a = members[a].name; \
  DBCC_Symbol *symbol_b = members[b].name; \
  rv = (symbol_a < symbol_b) ? -1 : (symbol_a < symbol_b) ? 1 : 0; \
}
  DSK_QSORT(rv, size_t, n_members, COMP_MIA);
#undef COMP_MIA
  for (size_t i = 0; i + 1 < n_members; i++)
    {
      DBCC_Symbol *at = members[rv[i]].name;
      if (at == NULL)
        continue;
      DBCC_Symbol *next = members[rv[i+1]].name;
      if (at == next)
        {
          *error = dbcc_error_new (DBCC_ERROR_STRUCT_DUPLICATES,
                                   "multiple members named '%s'",
                                   dbcc_symbol_get_string(at));
          return NULL;
        }
    }
  return rv;
}

static void
init_type_struct_members (DBCC_TargetEnvironment *env,
                          DBCC_Type *t,
                          size_t n_members,
                          DBCC_Param *members)
{
  size_t cur_offset = 0;
  size_t align = env->min_struct_alignof;
  t->v_struct.n_members = n_members;
  t->v_struct.members = DBCC_NEW_ARRAY(n_members, DBCC_TypeStructMember);
  for (size_t i = 0; i < n_members; i++)
    {
      t->v_struct.members[i].name = members[i].name;
      t->v_struct.members[i].type = dbcc_type_ref (members[i].type);
      size_t this_align = members[i].type->base.alignof_instance;
      cur_offset = DBCC_ALIGN(cur_offset, this_align);
      if (align < this_align)
        align = this_align;
      t->v_struct.members[i].offset = cur_offset;
      cur_offset += members[i].type->base.sizeof_instance;
    }
  cur_offset = DBCC_ALIGN(cur_offset, align);
  if (cur_offset < env->min_struct_sizeof)
    cur_offset = env->min_struct_sizeof;
  t->base.sizeof_instance = cur_offset;
  t->base.alignof_instance = align;
}

DBCC_Type *
dbcc_type_new_struct  (DBCC_TargetEnvironment *env,
                       DBCC_Symbol        *tag,
                       size_t              n_members,
                       DBCC_Param         *members,
                       DBCC_Error        **error)
{
  size_t *member_indices_bysym = get_members_by_sym (n_members, members, error);
  if (member_indices_bysym == NULL)
    return NULL;
  DBCC_Type *t = NEW_TYPE(STRUCT);
  t->v_struct.members_sorted_by_sym = member_indices_bysym;
  t->v_struct.tag = tag;
  t->v_struct.incomplete = false;
  init_type_struct_members (env, t, n_members, members);
  return t;
}

DBCC_Type *
dbcc_type_new_incomplete_struct (DBCC_Symbol *tag)
{
  DBCC_Type *t = NEW_TYPE(STRUCT);
  t->v_struct.tag = tag;
  t->v_struct.incomplete = true;
  return t;
}

bool
dbcc_type_complete_struct(DBCC_TargetEnvironment *env,
                          DBCC_Type              *type,
                          size_t                  n_members,
                          DBCC_Param             *members,
                          DBCC_Error            **error)
{
  assert(type->metatype == DBCC_TYPE_METATYPE_STRUCT);
  assert(type->v_struct.incomplete);
  type->v_struct.members_sorted_by_sym = get_members_by_sym (n_members, members, error);
  if (type->v_struct.members_sorted_by_sym == NULL)
    return false;
  init_type_struct_members (env, type, n_members, members);
  return true;
}

static void
init_type_union_branches (DBCC_TargetEnvironment *env,
                          DBCC_Type *t,
                          size_t n_members,
                          DBCC_Param *members)
{
  size_t max_size = env->min_struct_sizeof;
  size_t align = env->min_struct_alignof;
  t->v_union.n_branches = n_members;
  t->v_union.branches = DBCC_NEW_ARRAY(n_members, DBCC_TypeUnionBranch);
  for (size_t i = 0; i < n_members; i++)
    {
      t->v_union.branches[i].name = members[i].name;
      t->v_union.branches[i].type = dbcc_type_ref (members[i].type);
      size_t this_align = members[i].type->base.alignof_instance;
      size_t this_size = members[i].type->base.sizeof_instance;
      if (align < this_align)
        align = this_align;
      if (max_size < this_size)
        max_size = this_size;
    }
  max_size = DBCC_ALIGN(max_size, align);
  t->base.sizeof_instance = max_size;
  t->base.alignof_instance = align;
}

DBCC_Type *
dbcc_type_new_union    (DBCC_TargetEnvironment *env,
                        DBCC_Symbol        *tag,
                        size_t              n_branches,
                        DBCC_Param         *branches,
                        DBCC_Error        **error)
{
  size_t *indices_bysym = get_members_by_sym (n_branches, branches, error);
  if (indices_bysym == NULL)
    return NULL;
  DBCC_Type *t = NEW_TYPE(UNION);
  t->v_union.members_sorted_by_sym = indices_bysym;
  t->v_union.tag = tag;
  t->v_union.incomplete = false;
  init_type_union_branches (env, t, n_branches, branches);
  return t;
}

DBCC_Type *
dbcc_type_new_incomplete_union (DBCC_Symbol *tag)
{
  DBCC_Type *t = NEW_TYPE(UNION);
  t->v_union.tag = tag;
  t->v_union.incomplete = true;
  return t;
}

bool
dbcc_type_complete_union(DBCC_TargetEnvironment *env,
                         DBCC_Type         *type,
                         size_t              n_members,
                         DBCC_Param         *members,
                         DBCC_Error        **error)
{
  assert(type->metatype == DBCC_TYPE_METATYPE_UNION);
  assert(type->v_union.incomplete);
  type->v_union.members_sorted_by_sym = get_members_by_sym (n_members, members, error);
  if (type->v_struct.members_sorted_by_sym == NULL)
    return false;
  init_type_union_branches (env, type, n_members, members);
  return true;
}

// int -> int
// int -> float
// float -> int
// float -> float
static bool
dbcc_cast_value_i2i (DBCC_Type  *dst_type,
                     void       *dst_value,
                     DBCC_Type  *src_type,
                     const void *src_value)
{
  if (dst_type->base.sizeof_instance == src_type->base.sizeof_instance)
    {
      memcpy (dst_value, src_value, dst_type->base.sizeof_instance);
      return true;
    }
  int64_t v;
  switch (src_type->base.sizeof_instance)
    {
    case 1:
      if (src_type->v_int.is_signed)
        v = * (int8_t *) src_value;
      else
        v = * (uint8_t *) src_value;
      break;
    case 2:
      if (src_type->v_int.is_signed)
        v = * (int16_t *) src_value;
      else
        v = * (uint16_t *) src_value;
      break;
    case 4:
      if (src_type->v_int.is_signed)
        v = * (int32_t *) src_value;
      else
        v = * (uint32_t *) src_value;
      break;
    case 8:
      if (src_type->v_int.is_signed)
        v = * (int64_t *) src_value;
      else
        v = * (uint64_t *) src_value;
      break;
    default: assert(0);
    }
  switch (dst_type->base.sizeof_instance)
    {
    case 1:
      * (uint8_t *) dst_value = (uint8_t) v;
      break;
    case 2:
      * (uint16_t *) dst_value = (uint16_t) v;
      break;
    case 4:
      * (uint32_t *) dst_value = (uint32_t) v;
      break;
    case 8:
      * (uint64_t *) dst_value = (uint64_t) v;
      break;
    default: assert(0);
    }
  return true;
}

static bool
dbcc_cast_value_i2f (DBCC_Type  *dst_type,
                     void       *dst_value,
                     DBCC_Type  *src_type,
                     const void *src_value)
{
  if (src_type->v_int.is_signed)
    {
      int64_t in;
      switch (src_type->base.sizeof_instance)
        {
        case 1: in = *(int8_t *) src_value; break;
        case 2: in = *(int16_t *) src_value; break;
        case 4: in = *(int32_t *) src_value; break;
        case 8: in = *(int64_t *) src_value; break;
        default: assert(0);
        }
      switch (dst_type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          *(float*) dst_value = in;
          return true;
        case DBCC_FLOAT_TYPE_DOUBLE:
          *(double*) dst_value = in;
          return true;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          *(long double*) dst_value = in;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          ((float*) dst_value)[0] = in;
          ((float*) dst_value)[1] = 0;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          ((double*) dst_value)[0] = in;
          ((double*) dst_value)[1] = 0;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          ((long double*) dst_value)[0] = in;
          ((long double*) dst_value)[1] = 0;
          return true;
        default:
          break;
        }
    }
  else
    {
      uint64_t in;
      switch (src_type->base.sizeof_instance)
        {
        case 1: in = *(uint8_t *) src_value; break;
        case 2: in = *(uint16_t *) src_value; break;
        case 4: in = *(uint32_t *) src_value; break;
        case 8: in = *(uint64_t *) src_value; break;
        default: assert(0);
        }
      switch (dst_type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          *(float*) dst_value = in;
          return true;
        case DBCC_FLOAT_TYPE_DOUBLE:
          *(double*) dst_value = in;
          return true;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          *(long double*) dst_value = in;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          ((float*) dst_value)[0] = in;
          ((float*) dst_value)[1] = 0;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          ((double*) dst_value)[0] = in;
          ((double*) dst_value)[1] = 0;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          ((long double*) dst_value)[0] = in;
          ((long double*) dst_value)[1] = 0;
          return true;
        default:
          break;
        }
    }
  return false;
}
static bool
dbcc_cast_value_f2i (DBCC_Type  *dst_type,
                     void       *dst_value,
                     DBCC_Type  *src_type,
                     const void *src_value)
{
#define IMPL_SET_INT(type, f)                                             \
      if (dst_type->v_int.is_signed)                                      \
        {                                                                 \
          switch (dst_type->base.sizeof_instance)                         \
            {                                                             \
            case 1: * (int8_t *) dst_value = (int8_t) f; return true;     \
            case 2: * (int16_t *) dst_value = (int16_t) f; return true;   \
            case 4: * (int32_t *) dst_value = (int32_t) f; return true;   \
            case 8: * (int64_t *) dst_value = (int64_t) f; return true;   \
            default: assert(0);                                           \
            }                                                             \
        }                                                                 \
      else                                                                \
        {                                                                 \
          switch (dst_type->base.sizeof_instance)                         \
            {                                                             \
            case 1: * (uint8_t *) dst_value =  (uint8_t) f; return true;  \
            case 2: * (uint16_t *) dst_value = (uint16_t) f; return true; \
            case 4: * (uint32_t *) dst_value = (uint32_t) f; return true; \
            case 8: * (uint64_t *) dst_value = (uint64_t) f; return true; \
            default: assert(0);                                           \
            }                                                             \
        }
  switch (src_type->v_float.float_type)
    {
    case DBCC_FLOAT_TYPE_FLOAT:
      {
        float in = * (float *) src_value;
        IMPL_SET_INT(float, in);
        break;
      }
    case DBCC_FLOAT_TYPE_DOUBLE:
      {
        double in = * (double *) src_value;
        IMPL_SET_INT(double, in);
        break;
      }
    case DBCC_FLOAT_TYPE_LONG_DOUBLE:
      {
        long double in = * (long double *) src_value;
        IMPL_SET_INT(long double, in);
        break;
      }
    default:
      return false;
    }
#undef IMPL_SET_INT
}

static bool
dbcc_cast_value_f2f (DBCC_Type  *dst_type,
                     void       *dst_value,
                     DBCC_Type  *src_type,
                     const void *src_value)
{
  if (dst_type->v_float.float_type == src_type->v_float.float_type)
    {
      memcpy (dst_value, src_value, dst_type->base.sizeof_instance);
      return true;
    }
#define COMBINE(dt, st)       (((dt) << 8) | (st))
#define COMBINE_CONSTS(D, S)  COMBINE(DBCC_FLOAT_TYPE_##D, DBCC_FLOAT_TYPE_##S)
  switch (COMBINE(dst_type->v_float.float_type,
                  src_type->v_float.float_type))
    {
    case COMBINE_CONSTS(FLOAT, DOUBLE):
    case COMBINE_CONSTS(IMAGINARY_FLOAT, IMAGINARY_DOUBLE):
      *(float*)dst_value = *(double*)src_value;
      return true;
    case COMBINE_CONSTS(FLOAT, LONG_DOUBLE):
    case COMBINE_CONSTS(IMAGINARY_FLOAT, IMAGINARY_LONG_DOUBLE):
      *(float*)dst_value = *(long double*)src_value;
      return true;
    case COMBINE_CONSTS(DOUBLE, FLOAT):
    case COMBINE_CONSTS(IMAGINARY_DOUBLE, IMAGINARY_FLOAT):
      *(double*)dst_value = *(float*)src_value;
      return true;
    case COMBINE_CONSTS(DOUBLE, LONG_DOUBLE):
    case COMBINE_CONSTS(IMAGINARY_DOUBLE, IMAGINARY_LONG_DOUBLE):
      *(double*)dst_value = *(long double*)src_value;
      return true;
    case COMBINE_CONSTS(LONG_DOUBLE, FLOAT):
    case COMBINE_CONSTS(IMAGINARY_LONG_DOUBLE, IMAGINARY_FLOAT):
      *(long double*)dst_value = *(float*)src_value;
      return true;
    case COMBINE_CONSTS(LONG_DOUBLE, DOUBLE):
    case COMBINE_CONSTS(IMAGINARY_LONG_DOUBLE, IMAGINARY_DOUBLE):
      *(long double*)dst_value = *(double*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, FLOAT):
      ((float *)dst_value)[0] = *(float*)src_value;
      ((float *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, DOUBLE):
      ((float *)dst_value)[0] = *(double*)src_value;
      ((float *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, LONG_DOUBLE):
      ((float *)dst_value)[0] = *(long double*)src_value;
      ((float *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, COMPLEX_DOUBLE):
      ((float *)dst_value)[0] = ((double*)src_value)[0];
      ((float *)dst_value)[1] = ((double*)src_value)[1];
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, COMPLEX_LONG_DOUBLE):
      ((float *)dst_value)[0] = ((long double*)src_value)[0];
      ((float *)dst_value)[1] = ((long double*)src_value)[1];
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, IMAGINARY_FLOAT):
      ((float *)dst_value)[0] = 0;
      ((float *)dst_value)[1] = *(float*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, IMAGINARY_DOUBLE):
      ((float *)dst_value)[0] = 0;
      ((float *)dst_value)[1] = *(double*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_FLOAT, IMAGINARY_LONG_DOUBLE):
      ((float *)dst_value)[0] = 0;
      ((float *)dst_value)[1] = *(long double*)src_value;
      return true;

    case COMBINE_CONSTS(COMPLEX_DOUBLE, FLOAT):
      ((double *)dst_value)[0] = *(float*)src_value;
      ((double *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, DOUBLE):
      ((double *)dst_value)[0] = *(double*)src_value;
      ((double *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, LONG_DOUBLE):
      ((double *)dst_value)[0] = *(long double*)src_value;
      ((double *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, COMPLEX_FLOAT):
      ((double *)dst_value)[0] = ((float*)src_value)[0];
      ((double *)dst_value)[1] = ((float*)src_value)[1];
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, COMPLEX_LONG_DOUBLE):
      ((double *)dst_value)[0] = ((long double*)src_value)[0];
      ((double *)dst_value)[1] = ((long double*)src_value)[1];
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, IMAGINARY_FLOAT):
      ((double *)dst_value)[0] = 0;
      ((double *)dst_value)[1] = *(float*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, IMAGINARY_DOUBLE):
      ((double *)dst_value)[0] = 0;
      ((double *)dst_value)[1] = *(double*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_DOUBLE, IMAGINARY_LONG_DOUBLE):
      ((double *)dst_value)[0] = 0;
      ((double *)dst_value)[1] = *(long double*)src_value;
      return true;

    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, FLOAT):
      ((long double *)dst_value)[0] = *(float*)src_value;
      ((long double *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, DOUBLE):
      ((long double *)dst_value)[0] = *(double*)src_value;
      ((long double *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, LONG_DOUBLE):
      ((long double *)dst_value)[0] = *(long double*)src_value;
      ((long double *)dst_value)[1] = 0;
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, COMPLEX_FLOAT):
      ((long double *)dst_value)[0] = ((float*)src_value)[0];
      ((long double *)dst_value)[1] = ((float*)src_value)[1];
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, COMPLEX_LONG_DOUBLE):
      ((long double *)dst_value)[0] = ((long double*)src_value)[0];
      ((long double *)dst_value)[1] = ((long double*)src_value)[1];
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, IMAGINARY_FLOAT):
      ((long double *)dst_value)[0] = 0;
      ((long double *)dst_value)[1] = *(float*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, IMAGINARY_DOUBLE):
      ((long double *)dst_value)[0] = 0;
      ((long double *)dst_value)[1] = *(double*)src_value;
      return true;
    case COMBINE_CONSTS(COMPLEX_LONG_DOUBLE, IMAGINARY_LONG_DOUBLE):
      ((long double *)dst_value)[0] = 0;
      ((long double *)dst_value)[1] = *(long double*)src_value;
      return true;
    default:
      return false;
    }
#undef COMBINE
#undef COMBINE_CONSTS
}
  
bool
dbcc_cast_value (DBCC_Type  *dst_type,
                 void       *dst_value,
                 DBCC_Type  *src_type,
                 const void *src_value)
{
#define COMBINE_METATYPES(a, b)   (((a) << 16) | (b))
#define COMBINE_METATYPES_CONST(sa, sb) \
        COMBINE_METATYPES(DBCC_TYPE_METATYPE_##sa, DBCC_TYPE_METATYPE_##sb)

  dst_type = dbcc_type_dequalify (dst_type);
  src_type = dbcc_type_dequalify (src_type);
  switch (COMBINE_METATYPES(src_type->metatype, dst_type->metatype))
    {
    case COMBINE_METATYPES_CONST(INT, INT):
      return dbcc_cast_value_i2i (dst_type, dst_value, src_type, src_value);
    case COMBINE_METATYPES_CONST(INT, FLOAT):
      return dbcc_cast_value_i2f (dst_type, dst_value, src_type, src_value);
    case COMBINE_METATYPES_CONST(FLOAT, INT):
      return dbcc_cast_value_f2i (dst_type, dst_value, src_type, src_value);
    case COMBINE_METATYPES_CONST(FLOAT, FLOAT):
      return dbcc_cast_value_f2f (dst_type, dst_value, src_type, src_value);
    default:
      return NULL;
    }

#undef COMBINE_METATYPES_CONST
#undef COMBINE_METATYPES
}

static inline bool
is_signed_int_type (DBCC_Type *mt)
{
// XXX: what about enums!?!
  return mt->metatype == DBCC_TYPE_METATYPE_INT && mt->v_int.is_signed;
}

static void
render_enum_value_as_json  (DBCC_Type *enum_type,
                            int64_t    value,
                            DskBuffer *out)
{
  DBCC_EnumValue *ev = dbcc_type_enum_lookup_value (enum_type, value);
  if (ev == NULL)
    {
      // yes: print as string
      dsk_buffer_append_byte (out, '"');
      dsk_buffer_append (out, ev->name->length, dbcc_symbol_get_string (ev->name));
      dsk_buffer_append_byte (out, '"');
    }
  else
    {
      // no: print as [typename, value]

      dsk_buffer_append_string (out, "[\"");
      if (enum_type->v_enum.tag != NULL)
        dsk_buffer_append_string (out, dbcc_symbol_get_string (enum_type->v_enum.tag));
      dsk_buffer_append_string (out, "\",");
      dsk_buffer_printf (out, "%lld", (long long) value);
      dsk_buffer_append_byte (out, ']');
    }
}

bool
dbcc_type_value_to_json(DBCC_Type   *type,
                        const char  *value,
                        DskBuffer   *out,
                        DBCC_Error **error)
{
  type = dbcc_type_dequalify (type);
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_VOID:
      dsk_buffer_append (out, 4, "null");
      return true;
    case DBCC_TYPE_METATYPE_INT:
      if (type->v_int.is_signed)
        switch (type->base.sizeof_instance)
          {
          case 1:
            dsk_buffer_printf(out, "%d", (int)(*(int8_t*)value));
            break;
          case 2:
            dsk_buffer_printf(out, "%d", (int)(*(int16_t*)value));
            break;
          case 4:
            dsk_buffer_printf(out, "%d", (int)(*(int32_t*)value));
            break;
          case 8:
            dsk_buffer_printf(out, "%lld", (long long )(*(int64_t*)value));
            break;
          default: assert(0);
          }
      else
        switch (type->base.sizeof_instance)
          {
          case 1:
            dsk_buffer_printf(out, "%u", (unsigned)(*(uint8_t*)value));
            break;
          case 2:
            dsk_buffer_printf(out, "%u", (unsigned)(*(uint16_t*)value));
            break;
          case 4:
            dsk_buffer_printf(out, "%u", (unsigned)(*(uint32_t*)value));
            break;
          case 8:
            dsk_buffer_printf(out, "%llu", (long long )(*(uint64_t*)value));
            break;
          default: assert(0);
          }
      return true;

    case DBCC_TYPE_METATYPE_FLOAT:
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          dsk_buffer_printf (out, "%.6f", * (float *) value);
          break;
        case DBCC_FLOAT_TYPE_DOUBLE:
          dsk_buffer_printf (out, "%.13f", * (double *) value);
          break;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          dsk_buffer_printf (out, "%.18Lf", * (long double *) value);
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          dsk_buffer_printf (out, "[%.6f,%.6f]",
                             ((float *) value)[0],
                             ((float *) value)[1]);
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          dsk_buffer_printf (out, "[%.13f,%.13f]",
                             ((double *) value)[0],
                             ((double *) value)[1]);
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          dsk_buffer_printf (out, "[%.18Lf,%.18Lf]",
                             ((long double *) value)[0],
                             ((long double *) value)[1]);
          break;
        case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
          dsk_buffer_printf (out, "[0,%.6f]",
                             ((float *) value)[0]);
          break;
        case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
          dsk_buffer_printf (out, "[0,%.13f]",
                             ((double *) value)[0]);
          break;
        case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
          dsk_buffer_printf (out, "[0,%.18Lf]",
                             ((long double *) value)[0]);
          break;
        default:
          assert(0);
        }
      return true;
    case DBCC_TYPE_METATYPE_ARRAY:
      {
        DBCC_Type *elt_type = type->v_array.element_type;
        size_t n = type->v_array.n_elements < 0 ? 0 : type->v_array.n_elements;
        const char *at = value;
        dsk_buffer_append_byte (out, '[');
        for (size_t i = 0; i < n; i++, at += elt_type->base.sizeof_instance)
          {
            if (i > 0)
              dsk_buffer_append_byte (out, ',');
            if (!dbcc_type_value_to_json (elt_type, at, out, error))
              return false;
          }
        dsk_buffer_append_byte (out, ']');
        return true;
      }
    case DBCC_TYPE_METATYPE_STRUCT:
      dsk_buffer_append_byte (out, '{');
      bool is_first = true;
      for (size_t i = 0; i < type->v_struct.n_members; i++)
        {
          // We do not render unnamed fields, which must be bitfields.
          if (type->v_struct.members[i].name == NULL)
            {
              assert(type->v_struct.members[i].is_bitfield);
              continue;
            }

          // add comma, if needed
          if (!is_first)
            dsk_buffer_append_byte (out, ',');
          else
            is_first = false;

          // member name must be in quotes
          dsk_buffer_append_byte (out, '"');
          dsk_buffer_append_string (out, dbcc_symbol_get_string (type->v_struct.members[i].name));
          dsk_buffer_append_small (out, 2, "\":");

          DBCC_TypeStructMember *m = type->v_struct.members + i;
          DBCC_Type *mt = dbcc_type_dequalify (m->type);
          void *mv = (char *) value + m->offset;
          if (m->is_bitfield)
            {
              uint64_t v = dbcc_type_value_to_uint64 (m->type, mv);
              v >>= m->bit_offset;
              v &= (1ULL << m->bit_length) - 1;
              if (is_signed_int_type (mt))
                {
                  if ((v >> (m->bit_length-1)) == 1)
                    {
                      uint64_t mask = (1ULL << m->bit_length) - 1;
                      v |= ~mask;
                    }
                  int64_t vs = v;
                  switch (mt->metatype)
                    {
                    case DBCC_TYPE_METATYPE_ENUM:
                      {
                        render_enum_value_as_json (mt, vs, out);
                        return true;
                      }

                    case DBCC_TYPE_METATYPE_INT:
                      dsk_buffer_printf (out, "%lld", (long long) vs);
                      return true;

                    default:
                      return false;
                    }
                }
              else
                {
                  switch (mt->metatype)
                    {
                    case DBCC_TYPE_METATYPE_ENUM:
                      render_enum_value_as_json (mt, v, out);
                      return true;

                    case DBCC_TYPE_METATYPE_INT:
                      dsk_buffer_printf(out, "%llu", (unsigned long long) v);
                      return true;

                    default:
                      return false;
                    }
                }
            }
          else
            {
              if (!dbcc_type_value_to_json (m->type, mv, out, error))
                {
                  return false;
                }
            }
        }
      dsk_buffer_append_byte (out, '}');
      return true;

    case DBCC_TYPE_METATYPE_ENUM:
      {
        int64_t v = dbcc_type_value_to_uint64 (type, value);
        render_enum_value_as_json(type, v, out);
        return true;
      }

    case DBCC_TYPE_METATYPE_TYPEDEF:
      // TODO: should pass typedef-name to help with anon structs etc
      return dbcc_type_value_to_json (type->v_typedef.underlying_type,
                                      value, out, error);
    case DBCC_TYPE_METATYPE_QUALIFIED:
      return dbcc_type_value_to_json (type->v_qualified.underlying_type,
                                      value, 
                                      out, error);
    default:
      *error = dbcc_error_new (DBCC_ERROR_UNSERIALIZABLE,
                               "cannot convert type of metatype '%s' to JSON",
                               dbcc_type_metatype_name (type->metatype));
      return false;
    }
}

DBCC_Type *
dbcc_type_dequalify (DBCC_Type *type)
{
  for (;;)
    {
      if (type->metatype == DBCC_TYPE_METATYPE_QUALIFIED)
        type = type->v_qualified.underlying_type;
      else if (type->metatype == DBCC_TYPE_METATYPE_TYPEDEF)
        type = type->v_typedef.underlying_type;
      else 
        return type;
    }
}

uint64_t
dbcc_type_value_to_uint64 (DBCC_Type *type, const void *value)
{
  bool is_signed;
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_INT:
      is_signed = type->v_int.is_signed;
      break;

    case DBCC_TYPE_METATYPE_ENUM:
      is_signed = true;                         //???
      break;    

    default:
      assert(0);
      return -1;
    }

  if (is_signed)
    switch (type->base.sizeof_instance)
      {
      case 1: return (uint64_t) (int64_t) * (const int8_t *) value;
      case 2: return (uint64_t) (int64_t) * (const int16_t *) value;
      case 4: return (uint64_t) (int64_t) * (const int32_t *) value;
      case 8: return (uint64_t) (int64_t) * (const int64_t *) value;
      }
  else
    switch (type->base.sizeof_instance)
      {
      case 1: return * (const uint8_t *) value;
      case 2: return * (const uint16_t *) value;
      case 4: return * (const uint32_t *) value;
      case 8: return * (const uint64_t *) value;
      }
  return -1;    // should not get here
}

bool
dbcc_type_is_complex (DBCC_Type *type)
{
  if (type->metatype != DBCC_TYPE_METATYPE_FLOAT)
    return false;
  switch (type->v_float.float_type)
    {
    case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
    case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
    case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
    case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
    case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
    case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
      return true;
    default:
      return false;
    }
}

