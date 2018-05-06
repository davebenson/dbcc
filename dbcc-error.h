typedef struct DBCC_Error DBCC_Error;
typedef struct DBCC_ErrorList DBCC_ErrorList;
typedef struct DBCC_ErrorNode DBCC_ErrorNode;

typedef enum
{
  /* Preprocessing / lexing/tokenizing Errors */
  DBCC_ERROR_UNTERMINATED_MULTILINE_COMMENT,
  DBCC_ERROR_BAD_NUMBER_CONSTANT,
  DBCC_ERROR_BAD_HEXIDECIMAL_CONSTANT,
  DBCC_ERROR_BAD_DECIMAL_EXPONENT,
  DBCC_ERROR_BAD_BINARY_EXPONENT,
  DBCC_ERROR_MISSING_LINE_TERMINATOR,
  DBCC_ERROR_UNTERMINATED_BACKSLASH_SEQUENCE,
  DBCC_ERROR_UNTERMINATED_DOUBLEQUOTED_STRING,
  DBCC_ERROR_BAD_UNIVERSAL_CHARACTER_SEQUENCE,
  DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT,
  DBCC_ERROR_BAD_BACKSLASH_HEX,
  DBCC_ERROR_UNEXPECTED_CHARACTER,
  DBCC_ERROR_UNEXPECTED_EOF,
  DBCC_ERROR_PARSING_INTEGER,
  DBCC_ERROR_PARSING_FLOAT,
  DBCC_ERROR_INTEGER_CONSTANT_OUT_OF_BOUNDS,

  DBCC_ERROR_UNTERMINATED_PREPROCESSOR_DIRECTIVE,
  DBCC_ERROR_BAD_PREPROCESSOR_DIRECTIVE,
  DBCC_ERROR_PREPROCESSOR_MISSING_LPAREN,
  DBCC_ERROR_PREPROCESSOR_MACRO_INVOCATION_EOF,
  DBCC_ERROR_PREPROCESSOR_MACRO_INVOCATION,
  DBCC_ERROR_PREPROCESSOR_CONCATENATION,
  DBCC_ERROR_PREPROCESSOR_INTEGERS_ONLY,
  DBCC_ERROR_PREPROCESSOR_INTERNAL,
  DBCC_ERROR_PREPROCESSOR_INVALID_OPERATOR,
  DBCC_ERROR_PREPROCESSOR_SYNTAX,
  DBCC_ERROR_PREPROCESSOR_INCOMPLETE_LINE,
  DBCC_ERROR_PREPROCESSOR_UNMATCHED_ELSE,
  DBCC_ERROR_PREPROCESSOR_UNMATCHED_ENDIF,
  DBCC_ERROR_PREPROCESSOR_ELSE_NOT_ALLOWED,
  DBCC_ERROR_PREPROCESSOR_HASH_ERROR,

  /* Token-level parsing error */
  DBCC_ERROR_TOO_MANY_TYPE_SPECIFIERS,
  DBCC_ERROR_CONFLICTING_QUALIFIERS,
  DBCC_ERROR_CONSTANT_REQUIRED,
  DBCC_ERROR_DUPLICATE,
  DBCC_ERROR_NOT_FOUND,
  DBCC_ERROR_EXPECTED_INT,
  DBCC_ERROR_NO_COMPLEX_VARIANT,
  DBCC_ERROR_NONATOMIC,
  DBCC_ERROR_MULTIPLE_DEFINITION,
  DBCC_ERROR_EXPECTED_EXPRESSION,
  DBCC_ERROR_BAD_OPERATOR,

  /* type-sanity errors */
  DBCC_ERROR_STRUCT_EMPTY,
  DBCC_ERROR_STRUCT_DUPLICATES,
  DBCC_ERROR_ENUM_DUPLICATES,

  /* type-checking errors */
  
  /* type-checking errors */
} DBCC_ErrorCode;

const char *dbcc_error_code_name (DBCC_ErrorCode code);

typedef enum
{
  DBCC_ERROR_DATA_TYPE_CAUSE,
  DBCC_ERROR_DATA_TYPE_CODE_POSITION,
} DBCC_ErrorDataType;
typedef struct DBCC_Error DBCC_Error;
typedef struct DBCC_ErrorData DBCC_ErrorData;
typedef struct DBCC_ErrorData_Cause DBCC_ErrorData_Cause;
typedef struct DBCC_ErrorData_CodePosition DBCC_ErrorData_CodePosition;
struct DBCC_ErrorData
{
  DBCC_ErrorDataType type;
  DBCC_ErrorData *next;
};
struct DBCC_ErrorData_Cause
{
  DBCC_ErrorData base;
  DBCC_Error *error;
};
struct DBCC_ErrorData_CodePosition
{
  DBCC_ErrorData base;
  DBCC_CodePosition *code_position;
};

struct DBCC_Error
{
  size_t ref_count;
  DBCC_ErrorCode code;
  char *message;
  DBCC_ErrorData *first_data, *last_data;
};

DBCC_Error *dbcc_error_new       (DBCC_ErrorCode code,
                                  const char *format,
                                  ...);
DBCC_Error *dbcc_error_ref       (DBCC_Error *error);
void        dbcc_error_unref     (DBCC_Error *error);

void        dbcc_error_add_cause (DBCC_Error *error,
                                  DBCC_Error *cause);
void        dbcc_error_add_code_position (DBCC_Error *error,
                                  DBCC_CodePosition *code_position);
