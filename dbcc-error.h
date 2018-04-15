typedef struct DBCC_Error DBCC_Error;
typedef struct DBCC_ErrorList DBCC_ErrorList;
typedef struct DBCC_ErrorNode DBCC_ErrorNode;

struct DBCC_ErrorNode
{
  DBCC_ErrorNode *next;
  DBCC_Error *error;
};
struct DBCC_ErrorList
{
  DBCC_Error *first_error;
  DBCC_Error *last_error;
};

typedef enum
{
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
} DBCC_ErrorCode;


typedef enum
{
  DBCC_ERROR_DATA_TYPE_CAUSES
} DBCC_ErrorDataType;
typedef struct DBCC_Error DBCC_Error;
typedef struct DBCC_ErrorData DBCC_ErrorData;
struct DBCC_ErrorData
{
  DBCC_ErrorDataType type;
  DBCC_ErrorData *next;
};
struct DBCC_ErrorData_Causes
{
  DBCC_ErrorData base;
  DBCC_ErrorList causes;
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
