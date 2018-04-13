
typedef enum
{
  DBCC_ERROR_TOO_MANY_TYPE_SPECIFIERS,
  DBCC_ERROR_CONFLICTING_QUALIFIERS,
  DBCC_ERROR_CONSTANT_REQUIRED,
  DBCC_ERROR_DUPLICATE,
  DBCC_ERROR_NOT_FOUND,
} DBCC_ErrorCode;


typedef enum
{
  DBCC_ERROR_DATA_TYPE_CAUSE
} DBCC_ErrorDataType;
typedef struct DBCC_Error DBCC_Error;
typedef struct DBCC_ErrorData DBCC_ErrorData;
struct DBCC_ErrorData
{
  DBCC_ErrorDataType type;
  DBCC_ErrorData *next;
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
