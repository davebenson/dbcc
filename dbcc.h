
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

typedef enum
{
  DBCC_TYPE_SPECIFIER_FLAG_VOID      = (1<<0),
  DBCC_TYPE_SPECIFIER_FLAG_CHAR      = (1<<1),
  DBCC_TYPE_SPECIFIER_FLAG_INT       = (1<<2),
  DBCC_TYPE_SPECIFIER_FLAG_SHORT     = (1<<3),
  DBCC_TYPE_SPECIFIER_FLAG_LONG      = (1<<4),
  DBCC_TYPE_SPECIFIER_FLAG_LONG_LONG = (1<<5),
  DBCC_TYPE_SPECIFIER_FLAG_UNSIGNED  = (1<<6),
  DBCC_TYPE_SPECIFIER_FLAG_SIGNED    = (1<<7),
  DBCC_TYPE_SPECIFIER_FLAG_FLOAT     = (1<<8),
  DBCC_TYPE_SPECIFIER_FLAG_DOUBLE    = (1<<9),
  DBCC_TYPE_SPECIFIER_FLAG_BOOL      = (1<<10),
  DBCC_TYPE_SPECIFIER_FLAG_ATOMIC    = (1<<11),
  DBCC_TYPE_SPECIFIER_FLAG_COMPLEX   = (1<<12),
  DBCC_TYPE_SPECIFIER_FLAG_IMAGINARY = (1<<13),
  DBCC_TYPE_SPECIFIER_FLAG_TYPEDEF   = (1<<14),
  DBCC_TYPE_SPECIFIER_FLAG_STRUCT    = (1<<15),
  DBCC_TYPE_SPECIFIER_FLAG_UNION     = (1<<16),
  DBCC_TYPE_SPECIFIER_FLAG_ENUM      = (1<<17)
} DBCC_TypeSpecifierFlags;

bool dbcc_type_specifier_flags_combine (DBCC_TypeSpecifierFlags in1,
                                        DBCC_TypeSpecifierFlags in2,
                                        DBCC_TypeSpecifierFlags *out);

// a single, but complicated, declaration.
//
//    const int a, b[10], (*c)(int x), *d[3][3];
struct _DBCC_Declaration
{
  DBCC_TypeSpecifier specifier;
  DBCC_TypeQualifier qualifier;
  unsigned n_declarator;
  DBCC_Declarator **declarator;
};

typedef struct
{
  DBCC_DeclaratorType type;
  DBCC_Symbol *name;            // for non-abstract declarator
  unsigned n_ptr;
  DBCC_TypeQualifier *nonzero_ptr_tqs;
} DBCC_Declarator;

typedef struct
{
  int value;
  DBCC_Symbol name;
} DBCC_EnumValue;
typedef struct
{
  DBCC_TypeSpecifierFlags flags;
  DBCC_Symbol *id;              // for named structs, unions, enums
  bool is_stub;                 // for forward-declared structs+unions+enums
  unsigned n_elements;          // for structs,unions,enums
  void *elements;
} DBCC_TypeSpecifier;

typedef struct DBCC_String DBCC_String;
struct DBCC_String
{
  size_t length;
  char *str;
};
