
typedef struct DBCC_GlobalNamespace DBCC_GlobalNamespace;

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "dbcc-symbol.h"
#include "dbcc-code-position.h"
#include "dbcc-error.h"
#include "dbcc-ptr-table.h"

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
  DBCC_BINARY_OPERATOR_REMAINDER,
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


typedef enum
{
  DBCC_INPLACE_UNARY_OPERATOR_PRE_INCR,
  DBCC_INPLACE_UNARY_OPERATOR_PRE_DECR,
  DBCC_INPLACE_UNARY_OPERATOR_POST_INCR,
  DBCC_INPLACE_UNARY_OPERATOR_POST_DECR,
} DBCC_InplaceUnaryOperator;

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
  int value;
  DBCC_Symbol* name;
} DBCC_EnumValue;

typedef struct DBCC_String DBCC_String;
struct DBCC_String
{
  size_t length;
  char *str;
};
DBCC_INLINE void dbcc_string_clear (DBCC_String *clear)
{
  if (clear->str)
    free (clear->str);
  *clear = (DBCC_String){0, NULL};
}

#include "dbcc-type.h"
#include "dbcc-expr.h"
#include "dbcc-statement.h"
#include "dbcc-namespace.h"

// The C Grammar we use defines a Declaration as a tricky beast with a list of "Declarators",
// which are intended to generally handle reasonable cases like:
//       int x,y,z;
// and unreasonable cases like:
//       int x[10] = { [5] = 42 }, *y, (*z)(void*) = free;
//
// We will "normalize" the above so that each "declarator" becomes its own "declaration".
// So it'd be equivalent to the longer form for the above "unreasonable" case:
//       int x[10] = { [5] = 42 };
//       int *y;
//       int (*z)(void*) = free;
struct DBCC_Declaration
{
  DBCC_Type *type;
  DBCC_Symbol *name;
  DBCC_Expr *init;              /* optional */
};


