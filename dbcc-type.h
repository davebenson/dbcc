#ifndef __DBCC_TYPE_H_
#define __DBCC_TYPE_H_

typedef struct DBCC_Type_Base DBCC_Type_Base;
typedef struct DBCC_TypeVoid DBCC_TypeVoid;
typedef struct DBCC_TypeInt DBCC_TypeInt;
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

typedef enum 
{
  DBCC_TYPE_METATYPE_VOID,
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
  DBCC_TYPE_METATYPE_FUNCTION
} DBCC_Type_Metatype;

const char * dbcc_type_metatype_name (DBCC_Type_Metatype metatype);
struct DBCC_Type_Base
{
  DBCC_Type_Metatype metatype;
  DBCC_Symbol *name;
  size_t ref_count;
  size_t sizeof_instance;
  size_t alignof_instance;                      // must be power-of-two
};

struct DBCC_TypeInt
{
  DBCC_Type_Base base;
  bool is_signed;
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
  DBCC_Type *return_type;
  unsigned n_params;
  DBCC_TypeFunctionParam *params;
  bool has_varargs;
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
};

DBCC_Type *dbcc_type_ref   (DBCC_Type *type);
void       dbcc_type_unref (DBCC_Type *type);

DBCC_Type *dbcc_type_new_enum (DBCC_TargetEnvironment *target_env,
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
DBCC_Type *dbcc_type_new_bitfield    (unsigned       bit_width,
                                      DBCC_Type     *element_type);

DBCC_Type *dbcc_type_new_function (DBCC_Type          *rettype,
                                   size_t              n_params,
                                   DBCC_Param         *params,
                                   bool                has_varargs);

DBCC_Type *dbcc_type_new_qualified(DBCC_TargetEnvironment *env,
                                   DBCC_Type              *base_type,
                                   DBCC_TypeQualifier      qualifiers,
                                   DBCC_Error            **error);

DBCC_Type *dbcc_type_new_struct   (DBCC_TargetEnvironment *env,
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

DBCC_Type *dbcc_type_new_union    (DBCC_TargetEnvironment *env,
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

bool  dbcc_cast_value (DBCC_Type  *dst_type,
                       void       *dst_value,
                       DBCC_Type  *src_type,
                       const void *src_value);

const char *dbcc_type_to_cstring (DBCC_Type *type);

bool dbcc_type_value_to_json (DBCC_Type   *type,
                              const char  *value,
                              DskBuffer   *out,
                              DBCC_Error **error);

DBCC_Type *dbcc_type_dequalify (DBCC_Type *type);


// Can the type be used in a condition expression?
// (as used in for-loops, if-statements, and do-while-loops)
bool   dbcc_type_is_scalar (DBCC_Type *type);

// Is enum or int?   This is the condition required by switch-statements.
bool   dbcc_type_is_integer (DBCC_Type *type);

// Can (most) arithmetic operators be applied to this type?
// Alternately: does it represent a number?
// (Enums are considered arithmetic, as well as int and float types.)
bool   dbcc_type_is_arithmetic (DBCC_Type *type);

// Is this a complex or imaginary floating-point type.?
bool dbcc_type_is_complex (DBCC_Type *type);

// A qualified- or unqualified-pointer to any type.
bool dbcc_type_is_pointer (DBCC_Type *type);

// This will do sign-extension as needed.
// Type must be INT or ENUM.
uint64_t   dbcc_type_value_to_uint64 (DBCC_Type *type,
                                      const void *value);
#endif /* __DBCC_TYPE_H_ */
