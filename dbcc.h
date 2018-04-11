
typedef enum
{
  DBCC_UNARY_OPERATOR_MOVE,
  DBCC_UNARY_OPERATOR_NOT,
  DBCC_UNARY_OPERATOR_NEG,
} DBCC_UnaryOperator;

typedef enum
{
  DBCC_BINARY_OPERATOR_ADD,
  DBCC_BINARY_OPERATOR_SUB,
  DBCC_BINARY_OPERATOR_MUL,
  DBCC_BINARY_OPERATOR_DIV,
  DBCC_BINARY_OPERATOR_UNSIGNED_LT,
  DBCC_BINARY_OPERATOR_UNSIGNED_LTEQ,
  DBCC_BINARY_OPERATOR_UNSIGNED_GT,
  DBCC_BINARY_OPERATOR_UNSIGNED_GTEQ,
  DBCC_BINARY_OPERATOR_SIGNED_LT,
  DBCC_BINARY_OPERATOR_SIGNED_LTEQ,
  DBCC_BINARY_OPERATOR_SIGNED_GT,
  DBCC_BINARY_OPERATOR_SIGNED_GTEQ,
  DBCC_BINARY_OPERATOR_EQ,
  DBCC_BINARY_OPERATOR_NE,
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


typedef struct
{
  int value;
  DBCC_Symbol name;
} DBCC_EnumValue;

typedef struct DBCC_String DBCC_String;
struct DBCC_String
{
  size_t length;
  char *str;
};

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
...
};
