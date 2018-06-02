#include "dbcc.h"
#include "dsk/dsk-qsort-macro.h"
#include <stdio.h>

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
        case DBCC_TYPE_METATYPE_KR_FUNCTION:
          for (unsigned i = 0; i < type->v_function_kr.n_params; i++)
            dbcc_symbol_unref (type->v_function_kr.params[i]);
          free (type->v_function_kr.params);
          break;
        }
      if (type->base.name != NULL)
        dbcc_symbol_unref (type->base.name);
      free (type);
    }
}

const char * dbcc_type_metatype_name (DBCC_Type_Metatype metatype)
{
  switch (metatype)
    {
    #define CASE(shortname) case DBCC_TYPE_METATYPE_##shortname: return #shortname;
    CASE(VOID)
    CASE(BOOL)
    CASE(INT)
    CASE(FLOAT)
    CASE(ARRAY)
    CASE(VARIABLE_LENGTH_ARRAY)
    CASE(STRUCT)
    CASE(UNION)
    CASE(ENUM)
    CASE(POINTER)
    CASE(TYPEDEF)
    CASE(QUALIFIED)
    CASE(FUNCTION)
    CASE(KR_FUNCTION)
    #undef CASE
    }
  return NULL;
}

const char *
dbcc_type_to_cstring (DBCC_Type *type)
{
  if (type->base.private_cstring == NULL)
    {
      const char *static_cstring = NULL;
      char *dynamic_cstring = NULL;
      switch (type->metatype)
        {
        case DBCC_TYPE_METATYPE_VOID:
          static_cstring = "void";
          break;
        case DBCC_TYPE_METATYPE_BOOL:
          static_cstring = "_Bool";
          break;
        case DBCC_TYPE_METATYPE_INT:
          asprintf (&dynamic_cstring,
                    "_%s%u",
                    type->v_int.is_signed ? "Int" : "UInt",
                    (unsigned)(8 * type->base.sizeof_instance));
          break;
        case DBCC_TYPE_METATYPE_FLOAT:
          switch (type->v_float.float_type)
            {
            case DBCC_FLOAT_TYPE_FLOAT:
              static_cstring = "float";
              break;
            case DBCC_FLOAT_TYPE_DOUBLE:
              static_cstring = "double";
              break;
            case DBCC_FLOAT_TYPE_LONG_DOUBLE:
              static_cstring = "long double";
              break;
            case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
              static_cstring = "_Complex float";
              break;
            case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
              static_cstring = "_Complex double";
              break;
            case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
              static_cstring = "_Complex long double";
              break;
            case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
              static_cstring = "_Imag float";
              break;
            case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
              static_cstring = "_Imag double";
              break;
            case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
              static_cstring = "_Imag long double";
              break;
            }
          break;
        case DBCC_TYPE_METATYPE_ARRAY:
          if (type->v_array.n_elements < 0)
            asprintf (&dynamic_cstring, "%s[]",
                      dbcc_type_to_cstring (type->v_array.element_type));
          else
            asprintf (&dynamic_cstring, "%s[%llu]",
                      dbcc_type_to_cstring (type->v_array.element_type),
                      (unsigned long long) type->v_array.n_elements);
          break;
        case DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY:
          asprintf (&dynamic_cstring, "%s[*]",
                      dbcc_type_to_cstring (type->v_variable_length_array.element_type));
          break;
        case DBCC_TYPE_METATYPE_STRUCT:
          if (type->v_struct.tag != NULL)
            asprintf (&dynamic_cstring,
                      "struct %s",
                      dbcc_symbol_get_string (type->v_struct.tag));
          else
            static_cstring = "struct";
          break;
        case DBCC_TYPE_METATYPE_UNION:
          if (type->v_union.tag != NULL)
            asprintf (&dynamic_cstring,
                      "union %s",
                      dbcc_symbol_get_string (type->v_union.tag));
          else
            static_cstring = "union";
          break;
        case DBCC_TYPE_METATYPE_ENUM:
          if (type->v_enum.tag != NULL)
            asprintf (&dynamic_cstring,
                      "enum %s",
                      dbcc_symbol_get_string (type->v_enum.tag));
          else
            static_cstring = "enum";
          break;
        case DBCC_TYPE_METATYPE_POINTER:
          asprintf (&dynamic_cstring,
                    "pointer<%s>",
                    dbcc_type_to_cstring (type->v_pointer.target_type));
          break;
        case DBCC_TYPE_METATYPE_TYPEDEF:
          static_cstring = dbcc_symbol_get_string (type->base.name);
          break;
        case DBCC_TYPE_METATYPE_QUALIFIED:
          // ConstAtomicVolatileRestrict<subtype>
          {
          DBCC_Type *subtype = type->v_qualified.underlying_type;
          const char *sub = dbcc_type_to_cstring (subtype);
          DBCC_TypeQualifier q = type->v_qualified.qualifiers;
          char qs[64];
          qs[0] = 0;
          if ((q & DBCC_TYPE_QUALIFIER_CONST) != 0)
            strcat (qs, "Const");
          if ((q & DBCC_TYPE_QUALIFIER_ATOMIC) != 0)
            strcat (qs, "Atomic");
          if ((q & DBCC_TYPE_QUALIFIER_RESTRICT) != 0)
            strcat (qs, "Restrict");
          if ((q & DBCC_TYPE_QUALIFIER_VOLATILE) != 0)
            strcat (qs, "Volatile");
          asprintf (&dynamic_cstring, "%s<%s>", qs, sub);
          }
          break;
        case DBCC_TYPE_METATYPE_FUNCTION:
          {
            DskBuffer buf = DSK_BUFFER_INIT;
            DBCC_TypeFunction *f = &type->v_function;
            const char *tstr = dbcc_type_to_cstring (f->return_type);
            dsk_buffer_append_string (&buf, tstr);
            dsk_buffer_append_string (&buf, " function (");
            for (unsigned i = 0; i < f->n_params; i++)
              { 
                if (i > 0)
                  dsk_buffer_append_string (&buf, ", ");
                const char *pstr = dbcc_type_to_cstring (f->params[i].type);
                dsk_buffer_printf (&buf,
                                   "%s %s",
                                   pstr,
                                   dbcc_symbol_get_string (f->params[i].name));
              }
            if (f->has_varargs)
              {
                if (f->n_params > 0)
                  dsk_buffer_append_string (&buf, ", ");
                dsk_buffer_append_string (&buf, "...");
              }
            dsk_buffer_append_string (&buf, ")");
            dynamic_cstring = dsk_buffer_empty_to_string (&buf);
            break;
          }
        case DBCC_TYPE_METATYPE_KR_FUNCTION:
          {
            DskBuffer buf = DSK_BUFFER_INIT;
            DBCC_TypeFunctionKR *f = &type->v_function_kr;
            dsk_buffer_append_string (&buf, "function (");
            for (unsigned i = 0; i < f->n_params; i++)
              { 
                if (i > 0)
                  dsk_buffer_append_string (&buf, ", ");
                dsk_buffer_append_string (&buf, dbcc_symbol_get_string (f->params[i]));
              }
            dsk_buffer_append_string (&buf, ")");
            dynamic_cstring = dsk_buffer_empty_to_string (&buf);
            break;
          }
        }
      if (static_cstring)
        type->base.private_cstring = strdup (static_cstring);
      else
        type->base.private_cstring = dynamic_cstring;
    }
  return type->base.private_cstring;
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
dbcc_type_new_enum (DBCC_Namespace         *ns,
                    DBCC_Symbol            *optional_tag,
                    size_t                  n_values,
                    DBCC_EnumValue         *values,
                    DBCC_Error            **error)
{
  if (optional_tag != NULL
   && dbcc_ptr_table_lookup_value (&ns->enum_tag_symbols, optional_tag) != NULL)
    {
      *error = dbcc_error_new (DBCC_ERROR_DUPLICATE_TAG,
                               "multiple enums with tag '%s'",
                               dbcc_symbol_get_string (optional_tag));
      return NULL;
    }
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
  t->base.sizeof_instance = ns->target_env->sizeof_int;
  t->v_enum.values_sorted_by_sym = malloc (sizeof (size_t) * n_values);
  for (size_t i = 0; i < n_values; i++)
    {
      t->v_enum.values[i] = values[i];
      t->v_enum.values_sorted_by_sym[i] = i;
      t->v_enum.values_sorted_by_value[i] = i;
    }
#define COMP_VIA(a,b, rv) { \
  DBCC_Symbol *symbol_a = values[a].name; \
  DBCC_Symbol *symbol_b = values[b].name; \
  rv = (symbol_a < symbol_b) ? -1 : (symbol_a < symbol_b) ? 1 : 0; \
}
  DSK_QSORT(t->v_enum.values_sorted_by_sym, size_t, n_values, COMP_VIA);
#undef COMP_VIA

#define COMP_VIAV(a,b, rv) { \
  int64_t value_a = values[a].value; \
  int64_t value_b = values[b].value; \
  rv = (value_a < value_b) ? -1 : (value_a < value_b) ? 1 : 0; \
}
  DSK_QSORT(t->v_enum.values_sorted_by_value, size_t, n_values, COMP_VIAV);
#undef COMP_VIAV

  if (optional_tag != NULL)
    dbcc_namespace_add_by_tag (ns, t);

  return t;
}

DBCC_EnumValue *
dbcc_type_enum_lookup_value_by_name    (DBCC_Type *type,
                                        DBCC_Symbol *name)
{
  size_t start = 0, n = type->v_enum.n_values;
  while (n > 0)
    {
      size_t mid = start + n / 2;
      DBCC_EnumValue *midval = type->v_enum.values
                             + type->v_enum.values_sorted_by_sym[mid];
      DBCC_Symbol *midsym = midval->name;
      if (name < midsym)
        {
          n /= 2;
        }
      else if (name > midsym)
        {
          size_t new_start = mid + 1;
          size_t new_n = start + n - new_start;
          start = new_start;
          n = new_n;
        }
      else
        return midval;
    }
  return NULL;
}

DBCC_EnumValue *
dbcc_type_enum_lookup_value            (DBCC_Type *type,
                                        int64_t    value)
{
  size_t start = 0, n = type->v_enum.n_values;
  while (n > 0)
    {
      size_t mid = start + n / 2;
      DBCC_EnumValue *midval = type->v_enum.values
                             + type->v_enum.values_sorted_by_value[mid];
      int64_t midvalue = midval->value;
      if (value < midvalue)
        {
          n /= 2;
        }
      else if (value > midvalue)
        {
          size_t new_start = mid + 1;
          size_t new_n = start + n - new_start;
          start = new_start;
          n = new_n;
        }
      else
        return midval;
    }
  return NULL;
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
dbcc_type_new_struct  (DBCC_Namespace     *ns,
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
  init_type_struct_members (ns->target_env, t, n_members, members);
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

DBCC_TypeStructMember *
dbcc_type_struct_lookup_member (DBCC_Type *type,
                                DBCC_Symbol *name)
{
  size_t start = 0, n = type->v_struct.n_members;
  while (n > 0)
    {
      size_t mid = start + n / 2;
      DBCC_TypeStructMember *midmem = type->v_struct.members
                                    + type->v_struct.members_sorted_by_sym[mid];
      DBCC_Symbol *midsym = midmem->name;
      if (name < midsym)
        {
          n /= 2;
        }
      else if (name > midsym)
        {
          size_t new_start = mid + 1;
          size_t new_n = start + n - new_start;
          start = new_start;
          n = new_n;
        }
      else
        return midmem;
    }
  return NULL;
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
dbcc_type_new_union    (DBCC_Namespace     *ns,
                        DBCC_Symbol        *tag,
                        size_t              n_branches,
                        DBCC_Param         *branches,
                        DBCC_Error        **error)
{
  if (tag != NULL
   && dbcc_ptr_table_lookup_value (&ns->union_tag_symbols, tag) != NULL)
    {
      *error = dbcc_error_new (DBCC_ERROR_DUPLICATE_TAG,
                               "multiple unions with tag '%s'",
                               dbcc_symbol_get_string (tag));
      return NULL;
    }
  size_t *indices_bysym = get_members_by_sym (n_branches, branches, error);
  if (indices_bysym == NULL)
    return NULL;
  DBCC_Type *t = NEW_TYPE(UNION);
  t->v_union.members_sorted_by_sym = indices_bysym;
  t->v_union.tag = tag;
  t->v_union.incomplete = false;
  init_type_union_branches (ns->target_env, t, n_branches, branches);
  if (tag != NULL)
    dbcc_ptr_table_set (&ns->union_tag_symbols, tag, t);
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

DBCC_TypeUnionBranch *
dbcc_type_union_lookup_branch (DBCC_Type *type, DBCC_Symbol *name)
{
  size_t start = 0, n = type->v_union.n_branches;
  while (n)
    {
      size_t mid = start + n / 2;
      DBCC_TypeUnionBranch *mid_branch = &type->v_union.branches[type->v_union.members_sorted_by_sym[mid]];
      DBCC_Symbol *mid_sym = mid_branch->name;
      if (name < mid_sym)
        {
          n /= 2;
        }
      else if (name > mid_sym)
        {
          size_t new_start = mid + 1;
          size_t end = start + n;
          size_t new_n = end - new_start;
          start = new_start;
          n = new_n;
        }
      else
        {
          return mid_branch;
        }
    }
  return NULL;
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
              uint64_t v = dbcc_typed_value_get_int64 (m->type, mv);
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
        int64_t v = dbcc_typed_value_get_int64 (type, value);
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

DBCC_Type *dbcc_type_pointer_dereference (DBCC_Type *type)
{
  type = dbcc_type_dequalify(type);
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_POINTER:
      return type->v_pointer.target_type;
    //TODO: arrays?
    default:
      return NULL;
    }
}
bool dbcc_type_is_incomplete (DBCC_Type *type)
{
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_VOID:
      return true;
    case DBCC_TYPE_METATYPE_STRUCT:
      return type->v_struct.incomplete;
    case DBCC_TYPE_METATYPE_UNION:
      return type->v_union.incomplete;
    default:
      return false;
    }
}
DBCC_TypeQualifier dbcc_type_get_qualifiers (DBCC_Type *type)
{
  return type->metatype == DBCC_TYPE_METATYPE_QUALIFIED
       ? type->v_qualified.qualifiers
       : 0;
}

/* see 6.2.7 Compatible type and composite type.
 * Note that types MUST be compatible to be composited.
 *
 * Also worth noting:  much of this section is devoted to
 * types in different translation units.
 *
 * Mostly, this is the notion of binary-compatible,
 * ie, what can be linked together.
 */
bool
dbcc_types_compatible (DBCC_Type *a, DBCC_Type *b)
{
  a = dbcc_type_dequalify (a);
  b = dbcc_type_dequalify (b);
  return a == b;
}

DBCC_Type *
dbcc_types_make_composite (DBCC_Namespace *ns,
                           DBCC_Type *a,
                           DBCC_Type *b,
                           DBCC_Error **error)
{
  DBCC_Type *base = dbcc_type_dequalify (a);    // equal to b dequalified
  assert(dbcc_type_dequalify (b) == base);
  DBCC_TypeQualifier q = dbcc_type_get_qualifiers (a)
                       | dbcc_type_get_qualifiers (b);
  return dbcc_type_new_qualified (ns->target_env, base, q, error);
}

bool
dbcc_type_implicitly_convertable (DBCC_Type *dst_type,
                                  DBCC_Type *src_type);

// Can the type be used in a condition expression?
// (as used in for-loops, if-statements, and do-while-loops)
bool   dbcc_type_is_scalar (DBCC_Type *type)
{
  switch (dbcc_type_dequalify (type)->metatype)
    {
    case DBCC_TYPE_METATYPE_BOOL:
    case DBCC_TYPE_METATYPE_INT:
    case DBCC_TYPE_METATYPE_FLOAT:
    case DBCC_TYPE_METATYPE_ENUM:
    case DBCC_TYPE_METATYPE_POINTER:
      return true;
    default:
      return false;
    }
}

// Is enum or int?   This is the condition required by switch-statements.
bool   dbcc_type_is_integer (DBCC_Type *type)
{
  switch (dbcc_type_dequalify (type)->metatype)
    {
    case DBCC_TYPE_METATYPE_BOOL:
    case DBCC_TYPE_METATYPE_INT:
    case DBCC_TYPE_METATYPE_ENUM:
      return true;
    default:
      return false;
    }
}

bool   dbcc_type_is_const   (DBCC_Type *type)
{
  return (dbcc_type_get_qualifiers (type) & DBCC_TYPE_QUALIFIER_CONST) != 0;
}
bool   dbcc_type_is_unsigned (DBCC_Type *type)
{
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_BOOL:
      return true;
    case DBCC_TYPE_METATYPE_INT:
      return !type->v_int.is_signed;
    case DBCC_TYPE_METATYPE_ENUM:
      return true;
    default:
      return false;
    }
}

bool   dbcc_type_is_arithmetic (DBCC_Type *type)
{
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_BOOL:
    case DBCC_TYPE_METATYPE_INT:
    case DBCC_TYPE_METATYPE_ENUM:
    case DBCC_TYPE_METATYPE_FLOAT:
      return true;
    default:
      return false;
    }
}

bool dbcc_type_is_floating_point (DBCC_Type *type)
{
  return dbcc_type_dequalify (type)->metatype == DBCC_TYPE_METATYPE_FLOAT;
}

// A qualified- or unqualified-pointer to any type.
bool dbcc_type_is_pointer (DBCC_Type *type)
{
  type = dbcc_type_dequalify (type);
  return type->metatype == DBCC_TYPE_METATYPE_POINTER;
}

// an integer type (int or enum), or non-complex floating-type.
bool dbcc_type_is_real (DBCC_Type *type)
{
  type = dbcc_type_dequalify (type);
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_INT:
    case DBCC_TYPE_METATYPE_BOOL:
    case DBCC_TYPE_METATYPE_ENUM:
      return true;
    case DBCC_TYPE_METATYPE_FLOAT:
      return type->v_float.float_type < 3;
    default:
      return false;
    }
}

// This will do sign-extension as needed.
// Type must be INT or ENUM.
uint64_t   dbcc_typed_value_get_int64 (DBCC_Type *type,
                                       const void *value)
{
  type = dbcc_type_dequalify (type);
  if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          return (uint64_t) (int64_t) (* (const float *) value);
        case DBCC_FLOAT_TYPE_DOUBLE:
          return (uint64_t) (int64_t) (* (const double *) value);
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          return (uint64_t) (int64_t) (* (const long double *) value);
        default:
          return 0;                     // TODO
        }
    }
  else if (dbcc_type_is_unsigned (type))
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          return (* (const uint8_t *) value);
        case 2:
          return (* (const uint16_t *) value);
        case 4:
          return (* (const uint32_t *) value);
        case 8:
          return (* (const uint64_t *) value);
        default:
          return 0;                     // TODO
        }
    }
  else
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          return (int64_t) (* (const int8_t *) value);
        case 2:
          return (int64_t) (* (const int16_t *) value);
        case 4:
          return (int64_t) (* (const int32_t *) value);
        case 8:
          return (int64_t) (* (const int64_t *) value);
        default:
          return 0;                     // TODO
        }
    }
}
      
void       dbcc_typed_value_set_int64(DBCC_Type  *type,
                                      void *out,
                                      int64_t     v)
{
  type = dbcc_type_dequalify (type);
  if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          * (float *) out = v;
          return;
        case DBCC_FLOAT_TYPE_DOUBLE:
          * (double *) out = v;
          return;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          * (long double *) out = v;
          return;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          * (float *) out = v;
          ((float *) out)[1] = 0;
          return;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          * (double *) out = v;
          ((double *) out)[1] = 0;
          return;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          * (long double *) out = v;
          ((long double *) out)[1] = 0;
          return;
        default:
          // imaginary types can't be set.
          // fall to error case.
          break;
        }
    }
  else if (dbcc_type_is_unsigned (type))
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          * (uint8_t *) out = v;
          return;
        case 2:
          * (uint16_t *) out = v;
          return;
        case 4:
          * (uint32_t *) out = v;
          return;
        case 8:
          * (uint64_t *) out = v;
          return;
        }
    }
  else
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          * (int8_t *) out = v;
          return;
        case 2:
          * (int16_t *) out = v;
          return;
        case 4:
          * (int32_t *) out = v;
          return;
        case 8:
          * (int64_t *) out = v;
          return;
        }
    }
  assert(0);
  return;
}
void       dbcc_typed_value_set_long_double (DBCC_Type *type,
                                      void *out,
                                      long double v)
{
  type = dbcc_type_dequalify (type);
  if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          * (float *) out = v;
          return;
        case DBCC_FLOAT_TYPE_DOUBLE:
          * (double *) out = v;
          return;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          * (long double *) out = v;
          return;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          * (float *) out = v;
          ((float *) out)[1] = 0;
          return;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          * (double *) out = v;
          ((double *) out)[1] = 0;
          return;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          * (long double *) out = v;
          ((long double *) out)[1] = 0;
          return;
        default:
          // imaginary types can't be set.
          // fall to error case.
          break;
        }
    }
  else if (dbcc_type_is_unsigned (type))
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          * (uint8_t *) out = (int8_t)(int) v;
          return;
        case 2:
          * (uint16_t *) out = (int16_t) v;
          return;
        case 4:
          * (uint32_t *) out = (int32_t) v;
          return;
        case 8:
          * (uint64_t *) out = (int64_t) v;
          return;
        }
    }
  else
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          * (int8_t *) out = (int8_t)(int)v;
          return;
        case 2:
          * (int16_t *) out = (int16_t) v;
          return;
        case 4:
          * (int32_t *) out = (int32_t) v;
          return;
        case 8:
          * (int64_t *) out = (int64_t) v;
          return;
        }
    }
  assert(0);
  return;
}

void *dbcc_typed_value_alloc_raw (DBCC_Type *type)
{
  return malloc (type->base.sizeof_instance);
}
void *dbcc_typed_value_alloc0 (DBCC_Type *type)
{
  return calloc (type->base.sizeof_instance, 1);
}
void dbcc_typed_value_add (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b)
{
  type = dbcc_type_dequalify(type);
  if (type->metatype == DBCC_TYPE_METATYPE_INT
   || type->metatype == DBCC_TYPE_METATYPE_ENUM)
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          *(uint8_t*)out = *(const uint8_t *) a + *(const uint8_t *) b;
          break;
        case 2:
          *(uint16_t*)out = *(const uint16_t *) a + *(const uint16_t *) b;
          break;
        case 4:
          *(uint32_t*)out = *(const uint32_t *) a + *(const uint32_t *) b;
          break;
        case 8:
          *(uint64_t*)out = *(const uint64_t *) a + *(const uint64_t *) b;
          break;
        default:
          assert(0);
        }
    }
  else if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
        case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
          *(float*)out = *(const float *) a + *(const float *) b;
          break;
        case DBCC_FLOAT_TYPE_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
          *(double*)out = *(const double *) a + *(const double *) b;
          break;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
          *(long double*)out = *(const long double *) a + *(const long double *) b;
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          ((float*)out)[0] = ((const float *) a)[0] + ((const float *) b)[0];
          ((float*)out)[1] = ((const float *) a)[1] + ((const float *) b)[1];
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          ((double*)out)[0] = ((const double *) a)[0] + ((const double *) b)[0];
          ((double*)out)[1] = ((const double *) a)[1] + ((const double *) b)[1];
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          ((long double*)out)[0] = ((const long double *) a)[0] + ((const long double *) b)[0];
          ((long double*)out)[1] = ((const long double *) a)[1] + ((const long double *) b)[1];
          break;
        default:
          assert(0);
        }
    }
  else
    {
      assert(0);
    }
}

void dbcc_typed_value_subtract (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b)
{
  type = dbcc_type_dequalify(type);
  if (type->metatype == DBCC_TYPE_METATYPE_INT
   || type->metatype == DBCC_TYPE_METATYPE_ENUM)
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          *(uint8_t*)out = *(const uint8_t *) a - *(const uint8_t *) b;
          break;
        case 2:
          *(uint16_t*)out = *(const uint16_t *) a - *(const uint16_t *) b;
          break;
        case 4:
          *(uint32_t*)out = *(const uint32_t *) a - *(const uint32_t *) b;
          break;
        case 8:
          *(uint64_t*)out = *(const uint64_t *) a - *(const uint64_t *) b;
          break;
        default:
          assert(0);
        }
    }
  else if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
        case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
          *(float*)out = *(const float *) a - *(const float *) b;
          break;
        case DBCC_FLOAT_TYPE_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
          *(double*)out = *(const double *) a - *(const double *) b;
          break;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
          *(long double*)out = *(const long double *) a - *(const long double *) b;
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          ((float*)out)[0] = ((const float *) a)[0] - ((const float *) b)[0];
          ((float*)out)[1] = ((const float *) a)[1] - ((const float *) b)[1];
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          ((double*)out)[0] = ((const double *) a)[0] - ((const double *) b)[0];
          ((double*)out)[1] = ((const double *) a)[1] - ((const double *) b)[1];
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          ((long double*)out)[0] = ((const long double *) a)[0] - ((const long double *) b)[0];
          ((long double*)out)[1] = ((const long double *) a)[1] - ((const long double *) b)[1];
          break;
        default:
          assert(0);
        }
    }
  else
    {
      assert(0);
    }
}
void dbcc_typed_value_multiply (DBCC_Type *type,
                                void *out,
                                const void *a,
                                const void *b)
{
  type = dbcc_type_dequalify(type);
  if (type->metatype == DBCC_TYPE_METATYPE_INT
   || type->metatype == DBCC_TYPE_METATYPE_ENUM)
    {
      if (dbcc_type_is_unsigned (type))
        switch (type->base.sizeof_instance)
          {
          case 1:
            *(uint8_t*)out = *(const uint8_t *) a * *(const uint8_t *) b;
            break;
          case 2:
            *(uint16_t*)out = *(const uint16_t *) a * *(const uint16_t *) b;
            break;
          case 4:
            *(uint32_t*)out = *(const uint32_t *) a * *(const uint32_t *) b;
            break;
          case 8:
            *(uint64_t*)out = *(const uint64_t *) a * *(const uint64_t *) b;
            break;
          default:
            assert(0);
          }
      else
        switch (type->base.sizeof_instance)
          {
          case 1:
            *(int8_t*)out = *(const int8_t *) a * *(const int8_t *) b;
            break;
          case 2:
            *(int16_t*)out = *(const int16_t *) a * *(const int16_t *) b;
            break;
          case 4:
            *(int32_t*)out = *(const int32_t *) a * *(const int32_t *) b;
            break;
          case 8:
            *(int64_t*)out = *(const int64_t *) a * *(const int64_t *) b;
            break;
          default:
            assert(0);
          }
    }
  else if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
        case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
          *(float*)out = *(const float *) a * *(const float *) b;
          break;
        case DBCC_FLOAT_TYPE_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
          *(double*)out = *(const double *) a * *(const double *) b;
          break;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
          *(long double*)out = *(const long double *) a * *(const long double *) b;
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          ((float*)out)[0] = ((const float *) a)[0] * ((const float *) b)[0];
          ((float*)out)[1] = ((const float *) a)[1] * ((const float *) b)[1];
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          ((double*)out)[0] = ((const double *) a)[0] * ((const double *) b)[0];
          ((double*)out)[1] = ((const double *) a)[1] * ((const double *) b)[1];
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          ((long double*)out)[0] = ((const long double *) a)[0] * ((const long double *) b)[0];
          ((long double*)out)[1] = ((const long double *) a)[1] * ((const long double *) b)[1];
          break;
        default:
          assert(0);
        }
    }
  else
    {
      assert(0);
    }
}

#define DO_COMPLEX_FLOAT_DIVIDE(float, out, a, b)                     \
    do {                                                              \
            float ar = ((const float *) a)[0];                        \
            float ai = ((const float *) a)[1];                        \
            float br = ((const float *) b)[0];                        \
            float bi = ((const float *) b)[1];                        \
            float br_mag = br < 0 ? -br : br;                         \
            float bi_mag = bi < 0 ? -bi : bi;                         \
            if (br_mag >= bi_mag)                                     \
              {                                                       \
                float bi_over_br = bi / br;                           \
                float denom = br + bi * bi_over_br;                   \
                ((float *)out)[0] = (ar + ai * bi_over_br) / denom;   \
                ((float *)out)[1] = (ai + ar * bi_over_br) / denom;   \
              }                                                       \
            else                                                      \
              {                                                       \
                float br_over_bi = br / bi;                           \
                float denom = br * br_over_bi + bi;                   \
                ((float *)out)[0] = (ar * br_over_bi + ai) / denom;   \
                ((float *)out)[1] = (ai * br_over_bi - ar) / denom;   \
              }                                                       \
          }   while(0)
bool dbcc_typed_value_divide (DBCC_Type *type,
                              void *out,
                              const void *a,
                              const void *b)
{
  type = dbcc_type_dequalify(type);
  if (type->metatype == DBCC_TYPE_METATYPE_INT
   || type->metatype == DBCC_TYPE_METATYPE_ENUM)
    {
      if (dbcc_type_is_unsigned (type))
        switch (type->base.sizeof_instance)
          {
          case 1:
            *(uint8_t*)out = *(const uint8_t *) a / *(const uint8_t *) b;
            break;
          case 2:
            *(uint16_t*)out = *(const uint16_t *) a / *(const uint16_t *) b;
            break;
          case 4:
            *(uint32_t*)out = *(const uint32_t *) a / *(const uint32_t *) b;
            break;
          case 8:
            *(uint64_t*)out = *(const uint64_t *) a / *(const uint64_t *) b;
            break;
          default:
            assert(0);
          }
      else
        switch (type->base.sizeof_instance)
          {
          case 1:
            *(int8_t*)out = *(const int8_t *) a / *(const int8_t *) b;
            break;
          case 2:
            *(int16_t*)out = *(const int16_t *) a / *(const int16_t *) b;
            break;
          case 4:
            *(int32_t*)out = *(const int32_t *) a / *(const int32_t *) b;
            break;
          case 8:
            *(int64_t*)out = *(const int64_t *) a / *(const int64_t *) b;
            break;
          default:
            assert(0);
          }
      return true;
    }
  else if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
          *(float*)out = *(const float *) a / *(const float *) b;
          break;
        case DBCC_FLOAT_TYPE_DOUBLE:
          *(double*)out = *(const double *) a / *(const double *) b;
          break;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
          *(long double*)out = *(const long double *) a / *(const long double *) b;
          break;
        case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
          assert(0);
          break;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          DO_COMPLEX_FLOAT_DIVIDE(float, out, a, b);
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          DO_COMPLEX_FLOAT_DIVIDE(double, out, a, b);
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          DO_COMPLEX_FLOAT_DIVIDE(long double, out, a, b);
          return true;
        default:
          assert(0);
        }
      return true;
    }
  else
    {
      assert(0);
      return false;
    }
}

bool
dbcc_typed_value_remainder (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b)
{
  if (!dbcc_type_is_integer (type))
    return false;
  if (dbcc_type_is_unsigned (type))
    {
      switch (type->base.sizeof_instance)
        {
        case 1: * (uint8_t *) out = *(uint8_t*)a % *(uint8_t*)b; return true;
        case 2: * (uint16_t *) out = *(uint16_t*)a % *(uint16_t*)b; return true;
        case 4: * (uint32_t *) out = *(uint32_t*)a % *(uint32_t*)b; return true;
        case 8: * (uint64_t *) out = *(uint64_t*)a % *(uint64_t*)b; return true;
        default: assert(0); return false;
        }
    }
  else
    {
      switch (type->base.sizeof_instance)
        {
        case 1: * (int8_t *) out = *(int8_t*)a % *(int8_t*)b; return true;
        case 2: * (int16_t *) out = *(int16_t*)a % *(int16_t*)b; return true;
        case 4: * (int32_t *) out = *(int32_t*)a % *(int32_t*)b; return true;
        case 8: * (int64_t *) out = *(int64_t*)a % *(int64_t*)b; return true;
        default: assert(0); return false;
        }
    }
}
// type must be integer (or enum- only size/signedness is used) for shifting
bool dbcc_typed_value_shift_left (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b)
{
  if (!dbcc_type_is_integer (type))
    return false;
  if (dbcc_type_is_unsigned (type))
    {
      switch (type->base.sizeof_instance)
        {
        case 1: * (uint8_t *) out = *(uint8_t*)a << *(uint8_t*)b; return true;
        case 2: * (uint16_t *) out = *(uint16_t*)a << *(uint16_t*)b; return true;
        case 4: * (uint32_t *) out = *(uint32_t*)a << *(uint32_t*)b; return true;
        case 8: * (uint64_t *) out = *(uint64_t*)a << *(uint64_t*)b; return true;
        default: assert(0); return false;
        }
    }
  else
    {
      switch (type->base.sizeof_instance)
        {
        case 1: * (int8_t *) out = *(int8_t*)a << *(int8_t*)b; return true;
        case 2: * (int16_t *) out = *(int16_t*)a << *(int16_t*)b; return true;
        case 4: * (int32_t *) out = *(int32_t*)a << *(int32_t*)b; return true;
        case 8: * (int64_t *) out = *(int64_t*)a << *(int64_t*)b; return true;
        default: assert(0); return false;
        }
    }
}
bool dbcc_typed_value_shift_right (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b)
{
  if (!dbcc_type_is_integer (type))
    return false;
  if (dbcc_type_is_unsigned (type))
    {
      switch (type->base.sizeof_instance)
        {
        case 1: * (uint8_t *) out = *(uint8_t*)a >> *(uint8_t*)b; return true;
        case 2: * (uint16_t *) out = *(uint16_t*)a >> *(uint16_t*)b; return true;
        case 4: * (uint32_t *) out = *(uint32_t*)a >> *(uint32_t*)b; return true;
        case 8: * (uint64_t *) out = *(uint64_t*)a >> *(uint64_t*)b; return true;
        default: assert(0); return false;
        }
    }
  else
    {
      switch (type->base.sizeof_instance)
        {
        case 1: * (int8_t *) out = *(int8_t*)a >> *(int8_t*)b; return true;
        case 2: * (int16_t *) out = *(int16_t*)a >> *(int16_t*)b; return true;
        case 4: * (int32_t *) out = *(int32_t*)a >> *(int32_t*)b; return true;
        case 8: * (int64_t *) out = *(int64_t*)a >> *(int64_t*)b; return true;
        default: assert(0); return false;
        }
    }
}

bool dbcc_typed_value_bitwise_and (DBCC_Type *type,
                                   void *out,
                                   const void *a,
                                   const void *b)
{
  uint8_t *oo = out;
  const uint8_t *aa = a;
  const uint8_t *bb = b;
  unsigned N = type->base.sizeof_instance;
  while (N--)
    *oo++ = *aa++ & *bb++;
  return true;
}
bool dbcc_typed_value_bitwise_or  (DBCC_Type *type,
                                   void *out,
                                   const void *a,
                                   const void *b)
{
  uint8_t *oo = out;
  const uint8_t *aa = a;
  const uint8_t *bb = b;
  unsigned N = type->base.sizeof_instance;
  while (N--)
    *oo++ = *aa++ | *bb++;
  return true;
}
bool dbcc_typed_value_bitwise_xor (DBCC_Type *type,
                                   void *out,
                                   const void *a,
                                   const void *b)
{
  uint8_t *oo = out;
  const uint8_t *aa = a;
  const uint8_t *bb = b;
  unsigned N = type->base.sizeof_instance;
  while (N--)
    *oo++ = *aa++ ^ *bb++;
  return true;
}

bool dbcc_typed_value_negate (DBCC_Type *type,
                              void *out,
                              const void *a)
{
  type = dbcc_type_dequalify(type);
  if (type->metatype == DBCC_TYPE_METATYPE_INT
   || type->metatype == DBCC_TYPE_METATYPE_ENUM)
    {
      switch (type->base.sizeof_instance)
        {
        case 1:
          *(int8_t*)out = -*(const int8_t *) a;
          return true;
        case 2:
          *(int16_t*)out = -*(const int16_t *) a;
          return true;
        case 4:
          *(int32_t*)out = -*(const int32_t *) a;
          return true;
        case 8:
          *(int64_t*)out = -*(const int64_t *) a;
          return true;
        default:
          assert(0);
        }
    }
  else if (type->metatype == DBCC_TYPE_METATYPE_FLOAT)
    {
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT:
        case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT:
          *(float*)out = -*(const float *) a;
          return true;
        case DBCC_FLOAT_TYPE_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE:
          *(double*)out = -*(const double *) a;
          return true;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE:
        case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE:
          *(long double*)out = -*(const long double *) a;
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_FLOAT:
          ((float*)out)[0] = -((const float *) a)[0];
          ((float*)out)[1] = -((const float *) a)[1];
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE:
          ((double*)out)[0] = -((const double *) a)[0];
          ((double*)out)[1] = -((const double *) a)[1];
          return true;
        case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE:
          ((long double*)out)[0] = -((const long double *) a)[0];
          ((long double*)out)[1] = -((const long double *) a)[1];
          return true;
        default:
          assert(0);
        }
    }
  else
    {
      assert(0);
    }
  return false;
}
bool dbcc_typed_value_bitwise_not (DBCC_Type *type,
                                   void *out,
                                   const void *in)
{
  uint8_t *oo = out;
  const uint8_t *ii = in;
  unsigned N = type->base.sizeof_instance;
  while (N--)
    *oo++ = ~(*ii++);
  return true;
}
/* out is of type int, and will always be 0 or 1. */
bool dbcc_typed_value_compare (DBCC_Namespace *ns, 
                               DBCC_Type *type,
                               DBCC_BinaryOperator op,
                               void *out,
                               const void *a,
                               const void *b)
{
  type = dbcc_type_dequalify (type);
#define IMPLEMENT_OP(bin_op, ctype)                                     \
  do{                                                                   \
    ctype a_value = * (const ctype *) a;                                \
    ctype b_value = * (const ctype *) b;                                \
    dbcc_typed_value_set_int64 (dbcc_namespace_get_int_type (ns),       \
                                out, a_value bin_op b_value);           \
  }while(0)
#define IMPLEMENT_ALL_OPS(ctype)                                        \
  switch (op)                                                           \
    {                                                                   \
    case DBCC_BINARY_OPERATOR_LT:   IMPLEMENT_OP(<, ctype);  break;     \
    case DBCC_BINARY_OPERATOR_LTEQ: IMPLEMENT_OP(<=, ctype); break;     \
    case DBCC_BINARY_OPERATOR_GT:   IMPLEMENT_OP(>, ctype);  break;     \
    case DBCC_BINARY_OPERATOR_GTEQ: IMPLEMENT_OP(>=, ctype); break;     \
    case DBCC_BINARY_OPERATOR_EQ:   IMPLEMENT_OP(==, ctype); break;     \
    case DBCC_BINARY_OPERATOR_NE:   IMPLEMENT_OP(!=, ctype); break;     \
    default: assert(0);                                                 \
    }
  switch (type->metatype)
    {
    case DBCC_TYPE_METATYPE_BOOL:
    case DBCC_TYPE_METATYPE_INT:
    case DBCC_TYPE_METATYPE_ENUM:
      if (dbcc_type_is_unsigned (type))
        {
        switch (type->base.sizeof_instance)
          {
          case 1: IMPLEMENT_ALL_OPS(uint8_t); break;
          case 2: IMPLEMENT_ALL_OPS(uint16_t); break;
          case 4: IMPLEMENT_ALL_OPS(uint32_t); break;
          case 8: IMPLEMENT_ALL_OPS(uint64_t); break;
          }
        }
      else
        {
        switch (type->base.sizeof_instance)
          {
          case 1: IMPLEMENT_ALL_OPS(int8_t); break;
          case 2: IMPLEMENT_ALL_OPS(int16_t); break;
          case 4: IMPLEMENT_ALL_OPS(int32_t); break;
          case 8: IMPLEMENT_ALL_OPS(int64_t); break;
          }
        }
      break;
    case DBCC_TYPE_METATYPE_FLOAT:
      switch (type->v_float.float_type)
        {
        case DBCC_FLOAT_TYPE_FLOAT: IMPLEMENT_ALL_OPS(float); break;
        case DBCC_FLOAT_TYPE_DOUBLE: IMPLEMENT_ALL_OPS(double); break;
        case DBCC_FLOAT_TYPE_LONG_DOUBLE: IMPLEMENT_ALL_OPS(long double); break;
        default:
          assert(0);
        }
      break;
    default:
      assert(0);
    }
  return true;
}

DBCC_TriState
dbcc_typed_value_scalar_to_tristate (DBCC_Type  *type,
                                     DBCC_Constant *constant)
{
  if (type->metatype == DBCC_TYPE_METATYPE_FLOAT
   || type->metatype == DBCC_TYPE_METATYPE_INT
   || type->metatype == DBCC_TYPE_METATYPE_ENUM)
    {
      if (constant->constant_type != DBCC_CONSTANT_TYPE_VALUE)
        return DBCC_MAYBE;
      if (dbcc_is_zero (type->base.sizeof_instance,
                        constant->v_value.data))
        return DBCC_NO;
      else
        return DBCC_YES;
    }
  else if (type->metatype == DBCC_TYPE_METATYPE_POINTER)
    {
      if (constant->constant_type != DBCC_CONSTANT_TYPE_VALUE)
        return DBCC_MAYBE;
      // TODO: maybe constant-type for null-ptr?
      if (dbcc_is_zero (type->base.sizeof_instance,
                        constant->v_value.data))
        return DBCC_NO;
      else
        return DBCC_YES;
    }
  else
    return DBCC_MAYBE;
}
