#ifndef __DBCC_TYPE_H_
#define __DBCC_TYPE_H_

typedef struct DBCC_Type_Base DBCC_Type_Base;
typedef struct DBCC_TypeInt DBCC_TypeInt;
typedef struct DBCC_TypeFloat DBCC_TypeFloat;
typedef struct DBCC_TypeArray DBCC_TypeArray;
typedef struct DBCC_TypeVariableLengthArray DBCC_TypeVariableLengthArray;
typedef struct DBCC_TypeStruct DBCC_TypeStruct;
typedef struct DBCC_TypePointer DBCC_TypePointer;
typedef struct DBCC_TypeBitfield DBCC_TypeBitfield;
typedef struct DBCC_TypeTypedef DBCC_TypeTypedef;
typedef struct DBCC_TypeEnum DBCC_TypeEnum;

typedef enum 
{
  DBCC_TYPE_METATYPE_INT,
  DBCC_TYPE_METATYPE_FLOAT,
  DBCC_TYPE_METATYPE_ARRAY,
  DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY,
  DBCC_TYPE_METATYPE_STRUCT,
  DBCC_TYPE_METATYPE_UNION,
  DBCC_TYPE_METATYPE_ENUM,
  DBCC_TYPE_METATYPE_POINTER,
  DBCC_TYPE_METATYPE_TYPEDEF,
} DBCC_Type_Metatype;
struct DBCC_Type_Base
{
  DBCC_Type_Metatype metatype;
  DBCC_Symbol *name;
  size_t ref_count;
  bool complete;
};

struct DBCC_TypeInt
{
  DBCC_Type_Base base;
};
struct DBCC_TypeFloat
{
  DBCC_Type_Base base;
};
struct DBCC_TypeEnum
{
  DBCC_Type_Base base;
  size_t n_values;
  DBCC_EnumValue **values;
};
struct DBCC_TypeArray
{
  DBCC_Type_Base base;
  size_t n_elements;
  DBCC_Type *element_type;
};
struct DBCC_TypeVariableLengthArray
{
  DBCC_Type_Base base;
  DBCC_TypeQualifier qualifiers;
  DBCC_Type *element_type;
};
struct DBCC_TypeStruct
{
  DBCC_Type_Base base;
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
  DBCC_TypeQualifier qualifiers;
  DBCC_Type *target_type;
};
struct DBCC_TypeTypedef
{
  DBCC_Type_Base base;
  DBCC_Type *alias_type;
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
  DBCC_TypeStruct v_union;
  DBCC_TypeEnum v_enum;
  DBCC_TypePointer v_pointer;
  DBCC_TypeTypedef v_typedef;
  DBCC_TypeBitfield v_bit_field;
};

DBCC_Type *dbcc_type_ref   (DBCC_Type *type);
void       dbcc_type_unref (DBCC_Type *type);

DBCC_Type *dbcc_type_new_enum (DBCC_Symbol     *optional_tag,
                               size_t           n_values,
                               DBCC_EnumValue **values);
DBCC_Type *dbcc_type_new_pointer (DBCC_Type     *target,
                                  DBCC_TypeQualifier qualifiers);
DBCC_Type *dbcc_type_new_array   (DBCC_TypeQualifier qualifiers,
                                  unsigned       count, /* 0 for unspecified */
                                  DBCC_Type     *element_type);
DBCC_Type *dbcc_type_new_varlen_array(DBCC_TypeQualifier qualifiers,
                                      DBCC_Type     *element_type);
DBCC_Type *dbcc_type_new_bitfield    (unsigned       bit_width,
                                      DBCC_Type     *element_type);

DBCC_Type *dbcc_type_new_function (DBCC_Type          *rettype,
                                   size_t              n_params,
                                   DBCC_Param         *params,
                                   bool                has_varargs);
DBCC_Type *dbcc_type_new_qualified(DBCC_Type          *base_type,
                                   DBCC_TypeQualifier  qualifiers);
DBCC_Type *dbcc_type_new_struct   (DBCC_Symbol        *tag,
                                   size_t              n_members,
                                   DBCC_Param         *members,
                                   DBCC_Error        **error);
DBCC_Type *dbcc_type_new_union    (DBCC_Symbol        *tag,
                                   size_t              n_cases,
                                   DBCC_Param         *cases,
                                   DBCC_Error        **error);
/// note that only integer, enum_value, pointer types may be atomic
DBCC_Type *dbcc_type_new_atomic   (DBCC_Type          *non_atomic_type);

bool  dbcc_cast_value (DBCC_Type  *dst_type,
                       void       *dst_value,
                       DBCC_Type  *src_type,
                       const void *src_value);

const char *dbcc_type_to_cstring (DBCC_Type *type);
#endif /* __DBCC_TYPE_H_ */
