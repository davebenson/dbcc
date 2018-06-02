#ifndef __DBCC_H_
#define __DBCC_H_

typedef struct DBCC_Namespace DBCC_Namespace;
typedef struct DBCC_Param DBCC_Param;
typedef union DBCC_Type DBCC_Type;
typedef union DBCC_Statement DBCC_Statement;
typedef union DBCC_Expr DBCC_Expr;
typedef union DBCC_Address DBCC_Address;
typedef struct DBCC_String DBCC_String;
typedef struct DBCC_TargetEnvironment DBCC_TargetEnvironment;
typedef struct DBCC_Global DBCC_Global;
typedef struct DBCC_Local DBCC_Local;
typedef struct DBCC_Constant DBCC_Constant;

#define DBCC_INLINE static inline
#define DBCC_CAN_INLINE 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "dsk/dsk.h"
#include "dbcc-symbol.h"
#include "dbcc-code-position.h"
#include "dbcc-error.h"
#include "dbcc-ptr-table.h"
#include "dbcc-target-environment.h"


#define DBCC_ALIGN(offset, align) \
   ( ((offset) + (align) - 1) & (~(size_t)((align) - 1)) )
#define DBCC_NEW_ARRAY(n, type)   ((type *)(malloc(sizeof(type) * (n))))
#define DBCC_NEW(type)   ((type *)(malloc(sizeof(type))))
#define DBCC_MIN(a,b)    ((a) < (b) ? (a) : (b))
#define DBCC_MAX(a,b)    ((a) > (b) ? (a) : (b))

typedef enum
{
  DBCC_UNARY_OPERATOR_NOOP,
  DBCC_UNARY_OPERATOR_LOGICAL_NOT,
  DBCC_UNARY_OPERATOR_BITWISE_NOT,
  DBCC_UNARY_OPERATOR_NEGATE,
  DBCC_UNARY_OPERATOR_REFERENCE,
  DBCC_UNARY_OPERATOR_DEREFERENCE,
} DBCC_UnaryOperator;

typedef enum
{
  DBCC_BINARY_OPERATOR_ADD,
  DBCC_BINARY_OPERATOR_SUB,
  DBCC_BINARY_OPERATOR_MUL,
  DBCC_BINARY_OPERATOR_DIV,
  DBCC_BINARY_OPERATOR_REM,
  DBCC_BINARY_OPERATOR_LT,
  DBCC_BINARY_OPERATOR_LTEQ,
  DBCC_BINARY_OPERATOR_GT,
  DBCC_BINARY_OPERATOR_GTEQ,
  DBCC_BINARY_OPERATOR_EQ,
  DBCC_BINARY_OPERATOR_NE,
  DBCC_BINARY_OPERATOR_SHIFT_LEFT,
  DBCC_BINARY_OPERATOR_SHIFT_RIGHT,
  DBCC_BINARY_OPERATOR_LOGICAL_AND,
  DBCC_BINARY_OPERATOR_LOGICAL_OR,
  DBCC_BINARY_OPERATOR_BITWISE_AND,
  DBCC_BINARY_OPERATOR_BITWISE_OR,
  DBCC_BINARY_OPERATOR_BITWISE_XOR,
  DBCC_BINARY_OPERATOR_COMMA
} DBCC_BinaryOperator;
const char *dbcc_binary_operator_name (DBCC_BinaryOperator op);

#define DBCC_N_BINARY_OPERATORS (DBCC_BINARY_OPERATOR_COMMA+1)


typedef enum
{
  DBCC_INPLACE_UNARY_OPERATOR_PRE_INCR,
  DBCC_INPLACE_UNARY_OPERATOR_PRE_DECR,
  DBCC_INPLACE_UNARY_OPERATOR_POST_INCR,
  DBCC_INPLACE_UNARY_OPERATOR_POST_DECR,
} DBCC_InplaceUnaryOperator;
const char *dbcc_inplace_unary_operator_name (DBCC_InplaceUnaryOperator op);

typedef enum
{
  DBCC_INPLACE_BINARY_OPERATOR_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_MUL_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_DIV_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_REM_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_ADD_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_SUB_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_LEFT_SHIFT_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_RIGHT_SHIFT_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_AND_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_XOR_ASSIGN,
  DBCC_INPLACE_BINARY_OPERATOR_OR_ASSIGN,
} DBCC_InplaceBinaryOperator;
const char * dbcc_inplace_binary_operator_name (DBCC_InplaceBinaryOperator op);

typedef enum
{
  DBCC_TYPE_QUALIFIER_CONST = (1<<0),
  DBCC_TYPE_QUALIFIER_RESTRICT = (1<<1),
  DBCC_TYPE_QUALIFIER_VOLATILE = (1<<2),
  DBCC_TYPE_QUALIFIER_ATOMIC = (1<<3),
} DBCC_TypeQualifier;

typedef enum
{
  DBCC_STORAGE_CLASS_SPECIFIER_TYPEDEF       = (1<<0),
  DBCC_STORAGE_CLASS_SPECIFIER_EXTERN        = (1<<1),
  DBCC_STORAGE_CLASS_SPECIFIER_STATIC        = (1<<2),
  DBCC_STORAGE_CLASS_SPECIFIER_THREAD_LOCAL  = (1<<3),
  DBCC_STORAGE_CLASS_SPECIFIER_AUTO          = (1<<4),
  DBCC_STORAGE_CLASS_SPECIFIER_REGISTER      = (1<<5),
} DBCC_StorageClassSpecifier;

typedef enum
{
  DBCC_FUNCTION_SPECIFIER_NORETURN           = (1<<0),
  DBCC_FUNCTION_SPECIFIER_INLINE             = (1<<1),
} DBCC_FunctionSpecifiers;
typedef struct
{
  DBCC_Symbol* name;
  int value;
} DBCC_EnumValue;
DBCC_EnumValue *dbcc_enum_value_new (DBCC_Symbol *symbol,
                                     int          value);

typedef enum DBCC_TriState
{
  DBCC_NO,
  DBCC_YES,
  DBCC_MAYBE
} DBCC_TriState;

struct DBCC_String
{
  size_t length;

  // 1 (for utf-8), 2 (for utf-16), 4 (for utf-32)
  unsigned sizeof_elt;

  void *str;
};
DBCC_INLINE void dbcc_string_clear (DBCC_String *clear)
{
  if (clear->str)
    free (clear->str);
  *clear = (DBCC_String){0, 1, NULL};
}

// for struct members, (formal) function parameters, union cases,
// maybe globals.
struct DBCC_Param
{
  DBCC_Type *type;
  DBCC_Symbol *name;
  int bit_width;                // -1 usually, >= 0 for bit-fields
};

typedef enum
{
  DBCC_CONSTANT_TYPE_VALUE,

  /* outside current unit,
   * but expected to be constant at run-time. */
  DBCC_CONSTANT_TYPE_LINK_ADDRESS,

  /* within current translation unit, and defined. */
  DBCC_CONSTANT_TYPE_UNIT_ADDRESS,

  /* in current address space, only for passing data into JIT code. */
  DBCC_CONSTANT_TYPE_LOCAL_ADDRESS,

  /* an offset to another address */
  DBCC_CONSTANT_TYPE_OFFSET,

} DBCC_ConstantType;

struct DBCC_Constant
{
  DBCC_ConstantType constant_type;
  union {
    struct {
      void *data;
    } v_value;
    struct {
      DBCC_Symbol *name;
    } v_link_address;
    struct {
      DBCC_Symbol *name;
      uint64_t address;
    } v_unit_address;
    struct {
      void *local;
    } v_local_address;
    struct {
      DBCC_Constant *base;
      int64_t offset;
    } v_offset;
  };
};
DBCC_Constant *dbcc_constant_new_value (DBCC_Type *type,
                                        const void *optional_value);
DBCC_Constant *dbcc_constant_new_value0(DBCC_Type *type);
DBCC_Constant *dbcc_constant_copy      (DBCC_Type *type, 
                                        DBCC_Constant *to_copy);
void           dbcc_constant_free      (DBCC_Type *type, 
                                        DBCC_Constant *to_free);


#include "dbcc-type.h"
#include "dbcc-expr.h"
#include "dbcc-statement.h"
#include "dbcc-namespace.h"
#include "dbcc-common.h"
#include "dbcc-parser.h"

#endif
