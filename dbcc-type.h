#ifndef __DBCC_TYPE_H_
#define __DBCC_TYPE_H_

typedef struct DBCC_Type_Base DBCC_Type_Base;
typedef struct DBCC_TypeVoid DBCC_TypeVoid;
typedef struct DBCC_TypeInt DBCC_TypeInt;
typedef struct DBCC_TypeBool DBCC_TypeBool;
typedef struct DBCC_TypeFloat DBCC_TypeFloat;
typedef struct DBCC_TypeArray DBCC_TypeArray;
typedef struct DBCC_TypeVariableLengthArray DBCC_TypeVariableLengthArray;
typedef struct DBCC_TypeStruct DBCC_TypeStruct;
typedef struct DBCC_TypeUnion DBCC_TypeUnion;
typedef struct DBCC_TypePointer DBCC_TypePointer;
typedef struct DBCC_TypeBitfield DBCC_TypeBitfield;
typedef struct DBCC_TypeTypedef DBCC_TypeTypedef;
typedef struct DBCC_TypeQualified DBCC_TypeQualified;
typedef struct DBCC_TypeEnum DBCC_TypeEnum;
typedef struct DBCC_TypeFunctionParam DBCC_TypeFunctionParam;
typedef struct DBCC_TypeFunction DBCC_TypeFunction;
typedef struct DBCC_TypeFunctionKR DBCC_TypeFunctionKR;

typedef enum 
{
  DBCC_TYPE_METATYPE_VOID,
  DBCC_TYPE_METATYPE_BOOL,
  DBCC_TYPE_METATYPE_INT,
  DBCC_TYPE_METATYPE_FLOAT,
  DBCC_TYPE_METATYPE_ARRAY,
  DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY,
  DBCC_TYPE_METATYPE_STRUCT,
  DBCC_TYPE_METATYPE_UNION,
  DBCC_TYPE_METATYPE_ENUM,
  DBCC_TYPE_METATYPE_POINTER,
  DBCC_TYPE_METATYPE_TYPEDEF,
  DBCC_TYPE_METATYPE_QUALIFIED,         // includes _Atomic
  DBCC_TYPE_METATYPE_FUNCTION,
  DBCC_TYPE_METATYPE_KR_FUNCTION
} DBCC_Type_Metatype;

const char * dbcc_type_metatype_name (DBCC_Type_Metatype metatype);
struct DBCC_Type_Base
{
  DBCC_Type_Metatype metatype;
  DBCC_Symbol *name;
  char *private_cstring;
  size_t ref_count;
  size_t sizeof_instance;
  size_t alignof_instance;     // must be power-of-two (6.2.8p4)
};

struct DBCC_TypeInt
{
  DBCC_Type_Base base;
  bool is_signed;
};

struct DBCC_TypeBool
{
  DBCC_Type_Base base;
};

typedef enum
{
  DBCC_FLOAT_TYPE_FLOAT,
  DBCC_FLOAT_TYPE_DOUBLE,
  DBCC_FLOAT_TYPE_LONG_DOUBLE,

  /* Annex G requires these 6 types.
   *
   * Imaginary types should have * storage like the corresponding reals,
   * but semantics like the corresponding complex numbers.
   */
  DBCC_FLOAT_TYPE_COMPLEX_FLOAT,
  DBCC_FLOAT_TYPE_COMPLEX_DOUBLE,
  DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE,
  DBCC_FLOAT_TYPE_IMAGINARY_FLOAT,
  DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE,
  DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE,
} DBCC_FloatType;
struct DBCC_TypeFloat
{
  DBCC_Type_Base base;
  DBCC_FloatType float_type;
  bool is_complex;
};
struct DBCC_TypeEnum
{
  DBCC_Type_Base base;
  DBCC_Symbol *tag;
  bool is_signed;
  size_t n_values;
  DBCC_EnumValue *values;
};
struct DBCC_TypeArray
{
  DBCC_Type_Base base;
  ssize_t n_elements;                   // -1 for unlengthed array
  DBCC_Type *element_type;
};
struct DBCC_TypeVariableLengthArray
{
  DBCC_Type_Base base;
  DBCC_Type *element_type;
};

typedef struct {
  DBCC_Type *type;
  DBCC_Symbol *name;
  size_t offset;

  bool is_bitfield;
  uint8_t bit_length;
  uint8_t bit_offset;           /* max size is 8*type->sizeof_instance - 1 */
} DBCC_TypeStructMember;
struct DBCC_TypeStruct
{
  DBCC_Type_Base base;
  DBCC_Symbol *tag;             // optional
  size_t n_members;
  DBCC_TypeStructMember *members;
  size_t *members_sorted_by_sym;
  bool incomplete;
};

typedef struct {
  DBCC_Type *type;
  DBCC_Symbol *name;

  bool is_bitfield;
  uint8_t bit_length;
  uint8_t bit_offset;           /* max size is 8*type->sizeof_instance - 1 */
} DBCC_TypeUnionBranch;
struct DBCC_TypeUnion
{
  DBCC_Type_Base base;
  DBCC_Symbol *tag;             // optional
  size_t n_branches;
  DBCC_TypeUnionBranch *branches;
  size_t *members_sorted_by_sym;
  bool incomplete;
};

struct DBCC_TypeBitfield
{
  DBCC_Type_Base base;
  unsigned bit_width;
  DBCC_Type *underlying_type;
};
struct DBCC_TypePointer
{
  DBCC_Type_Base base;
  DBCC_Type *target_type;
};
struct DBCC_TypeTypedef
{
  DBCC_Type_Base base;
  DBCC_Type *underlying_type;
};
struct DBCC_TypeQualified
{
  DBCC_Type_Base base;
  DBCC_Type *underlying_type;
  DBCC_TypeQualifier qualifiers;
};

struct DBCC_TypeFunctionParam
{
  DBCC_Type *type;
  DBCC_Symbol *name;
};
struct DBCC_TypeFunction
{
  DBCC_Type_Base base;
  DBCC_Type *return_type;
  unsigned n_params;
  DBCC_TypeFunctionParam *params;
  bool has_varargs;
};

struct DBCC_TypeFunctionKR
{
  DBCC_Type *return_type;
  unsigned n_params;
  DBCC_Symbol **params;
};

union DBCC_Type
{
  DBCC_Type_Metatype metatype;
  DBCC_Type_Base base;

  DBCC_TypeInt v_int;
  DBCC_TypeFloat v_float;
  DBCC_TypeArray v_array;
  DBCC_TypeVariableLengthArray v_variable_length_array;
  DBCC_TypeStruct v_struct;
  DBCC_TypeUnion v_union;
  DBCC_TypeEnum v_enum;
  DBCC_TypePointer v_pointer;
  DBCC_TypeTypedef v_typedef;
  DBCC_TypeBitfield v_bitfield;
  DBCC_TypeQualified v_qualified;
  DBCC_TypeFunction v_function;
  DBCC_TypeFunctionKR v_function_kr;
};

DBCC_Type *dbcc_type_ref   (DBCC_Type *type);
void       dbcc_type_unref (DBCC_Type *type);

DBCC_Type *dbcc_type_new_enum (DBCC_Namespace  *ns,
                               DBCC_Symbol     *optional_tag,
                               size_t           n_values,
                               DBCC_EnumValue  *values,
                               DBCC_Error     **error);
DBCC_EnumValue *dbcc_type_enum_lookup_value (DBCC_Type *type,
                                             int64_t value);
DBCC_Type *dbcc_type_new_pointer (DBCC_TargetEnvironment *target_env,
                                  DBCC_Type     *target);
DBCC_Type *dbcc_type_new_array   (DBCC_TargetEnvironment *target,
                                  int64_t        size, // -1 for unspecified
                                  DBCC_Type     *element_type);
DBCC_Type *dbcc_type_new_varlen_array(DBCC_TargetEnvironment *target,
                                      DBCC_Type     *element_type);

DBCC_Type *dbcc_type_new_function (DBCC_Type          *rettype,
                                   size_t              n_params,
                                   DBCC_Param         *params,
                                   bool                has_varargs);

DBCC_Type *dbcc_type_new_qualified(DBCC_TargetEnvironment *env,
                                   DBCC_Type              *base_type,
                                   DBCC_TypeQualifier      qualifiers,
                                   DBCC_Error            **error);

DBCC_Type *dbcc_type_new_struct   (DBCC_Namespace     *ns,
                                   DBCC_Symbol        *tag,
                                   size_t              n_members,
                                   DBCC_Param         *members,
                                   DBCC_Error        **error);
DBCC_Type *dbcc_type_new_incomplete_struct (DBCC_Symbol *tag);
bool       dbcc_type_complete_struct(DBCC_TargetEnvironment *env,
                                     DBCC_Type              *type,
                                     size_t                  n_members,
                                     DBCC_Param             *members,
                                     DBCC_Error            **error);
DBCC_TypeStructMember *dbcc_type_struct_lookup_member (DBCC_Type *type, DBCC_Symbol *name);

DBCC_Type *dbcc_type_new_union    (DBCC_Namespace     *ns,
                                   DBCC_Symbol        *tag,
                                   size_t              n_cases,
                                   DBCC_Param         *cases,
                                   DBCC_Error        **error);
DBCC_Type *dbcc_type_new_incomplete_union (DBCC_Symbol *tag);
bool       dbcc_type_complete_union(DBCC_TargetEnvironment *env,
                                   DBCC_Type         *type,
                                   size_t              n_members,
                                   DBCC_Param         *members,
                                   DBCC_Error        **error);
DBCC_TypeUnionBranch *dbcc_type_union_lookup_branch (DBCC_Type *type, DBCC_Symbol *name);

bool  dbcc_cast_value (DBCC_Type  *dst_type,
                       void       *dst_value,
                       DBCC_Type  *src_type,
                       const void *src_value);

const char *dbcc_type_to_cstring (DBCC_Type *type);

bool dbcc_type_value_to_json (DBCC_Type   *type,
                              const char  *value,
                              DskBuffer   *out,
                              DBCC_Error **error);

DBCC_Type *dbcc_type_detypedef (DBCC_Type *type);
DBCC_Type *dbcc_type_dequalify (DBCC_Type *type);
DBCC_Type *dbcc_type_pointer_dereference (DBCC_Type *type);
bool dbcc_type_is_incomplete (DBCC_Type *type);
DBCC_TypeQualifier dbcc_type_get_qualifiers (DBCC_Type *type);

/* see 6.2.7 Compatible type and composite type.
 * Note that types MUST be compatible to be composited.
 *
 * Also worth noting:  much of this section is devoted to
 * types in different translation units.
 *
 * Mostly, this is the notion of binary-compatible,
 * ie, what can be linked together.
 */
bool dbcc_types_compatible (DBCC_Type *a, DBCC_Type *b);
DBCC_Type * dbcc_types_make_composite (DBCC_Namespace *ns,
                                       DBCC_Type *a,
                                       DBCC_Type *b,
                                       DBCC_Error **error);

bool dbcc_type_implicitly_convertable (DBCC_Type *dst_type,
                                       DBCC_Type *src_type);

// Can the type be used in a condition expression?
// (as used in for-loops, if-statements, and do-while-loops)
bool   dbcc_type_is_scalar (DBCC_Type *type);

// Is enum or int?   This is the condition required by switch-statements.
bool   dbcc_type_is_integer (DBCC_Type *type);

bool   dbcc_type_is_const   (DBCC_Type *type);
bool   dbcc_type_is_unsigned (DBCC_Type *type);

// Can (most) arithmetic operators be applied to this type?
// Alternately: does it represent a number?
// (Enums are considered arithmetic, as well as int and float types.)
bool   dbcc_type_is_arithmetic (DBCC_Type *type);

// Is this a complex or imaginary floating-point type.?
bool dbcc_type_is_complex (DBCC_Type *type);

bool dbcc_type_is_floating_point (DBCC_Type *type);

// A qualified- or unqualified-pointer to any type.
bool dbcc_type_is_pointer (DBCC_Type *type);

// an integer type (int or enum), or non-complex floating-type.
bool dbcc_type_is_real (DBCC_Type *type);

// This will do sign-extension as needed.
// Type must be INT or ENUM.
uint64_t   dbcc_typed_value_get_int64 (DBCC_Type *type,
                                       const void *value);
void       dbcc_typed_value_set_int64(DBCC_Type  *type,
                                      void *out,
                                      int64_t     v);
void       dbcc_typed_value_set_long_double (DBCC_Type *type,
                                      void *out,
                                      long double v);

void *dbcc_typed_value_alloc_raw (DBCC_Type *type);
void *dbcc_typed_value_alloc0 (DBCC_Type *type);
void dbcc_typed_value_add (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);
void dbcc_typed_value_subtract (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);
void dbcc_typed_value_multiply (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);
bool dbcc_typed_value_divide (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);
bool dbcc_typed_value_remainder (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);
// type must be integer (or enum- only size/signedness is used) for shifting
bool dbcc_typed_value_shift_left (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);
bool dbcc_typed_value_shift_right (DBCC_Type *type,
                           void *out,
                           const void *a,
                           const void *b);

bool dbcc_typed_value_bitwise_and (DBCC_Type *type,
                                   void *out,
                                   const void *a,
                                   const void *b);
bool dbcc_typed_value_bitwise_or  (DBCC_Type *type,
                                   void *out,
                                   const void *a,
                                   const void *b);
bool dbcc_typed_value_bitwise_xor (DBCC_Type *type,
                                   void *out,
                                   const void *a,
                                   const void *b);

bool dbcc_typed_value_negate (DBCC_Type *type,
                              void *out,
                              const void *a);
bool dbcc_typed_value_bitwise_not (DBCC_Type *type,
                                   void *out,
                                   const void *in);
/* out is of type int, and will always be 0 or 1. */
bool dbcc_typed_value_compare (DBCC_Namespace *ns, 
                               DBCC_Type *type,
                               DBCC_BinaryOperator op,
                               void *out,
                               const void *a,
                               const void *b);

DBCC_TriState dbcc_typed_value_scalar_to_tristate (DBCC_Type *type,
                                                   DBCC_Constant *constant);
#endif /* __DBCC_TYPE_H_ */
