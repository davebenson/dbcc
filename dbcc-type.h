#ifndef __DBCC_TYPE_H_
#define __DBCC_TYPE_H_

typedef struct DBCC_Type_Base DBCC_Type_Base;
typedef struct DBCC_TypeInt DBCC_TypeInt;
typedef struct DBCC_TypeFloat DBCC_TypeFloat;
typedef struct DBCC_TypeArray DBCC_TypeArray;
typedef struct DBCC_TypeVariableLengthArray DBCC_TypeVariableLengthArray;
typedef struct DBCC_TypeStruct DBCC_TypeStruct;
typedef struct DBCC_TypePointer DBCC_TypePointer;
typedef struct DBCC_TypeTypedef DBCC_TypeTypedef;
typedef union DBCC_Type DBCC_Type;

typedef enum 
{
  DBCC_TYPE_METATYPE_INT,
  DBCC_TYPE_METATYPE_FLOAT,
  DBCC_TYPE_METATYPE_ARRAY,
  DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY,
  DBCC_TYPE_METATYPE_STRUCT,
  DBCC_TYPE_METATYPE_POINTER,
  DBCC_TYPE_METATYPE_TYPEDEF,
} DBCC_Type_Metatype;
struct DBCC_Type_Base
{
  DBCC_Type_Metatype metatype;
  DBCC_Symbol *name;
  size_t ref_count;
};

struct DBCC_TypeInt
{
  DBCC_Type_Base base;
};
struct DBCC_TypeFloat
{
  DBCC_Type_Base base;
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
  DBCC_TypePointer v_pointer;
  DBCC_TypeTypedef v_typedef;
};

DBCC_Type *dbcc_type_ref   (DBCC_Type *type);
void       dbcc_type_unref (DBCC_Type *type);

#endif /* __DBCC_TYPE_H_ */
