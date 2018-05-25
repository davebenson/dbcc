#include "dbcc.h"
#define _GNU_SOURCE
#include <stdio.h>

static const char *error_code_names[] =
{
#define CODE(shortname) [DBCC_ERROR_##shortname] = #shortname,
#include "generated/dbcc-error-codes.inc"
#undef CODE
};
#define n_error_code_names  ((sizeof(error_code_names)/sizeof(error_code_names[0])))

const char *dbcc_error_code_name (DBCC_ErrorCode code)
{
  if (code >= n_error_code_names)
    return NULL;
  return error_code_names[code];
}

DBCC_Error *dbcc_error_new       (DBCC_ErrorCode code,
                                  const char *format,
                                  ...)
{
  char *str;
  va_list args;
  va_start (args, format);
  vasprintf (&str, format, args);
  va_end (args);

  DBCC_Error *error = malloc (sizeof (DBCC_Error));
  error->ref_count = 1;
  error->code = code;
  error->message = str;
  error->first_data = error->last_data = NULL;
  return error;
}

DBCC_Error *dbcc_error_ref       (DBCC_Error *error)
{
  assert(error->ref_count > 0);
  error->ref_count += 1;
  return error;
}
void        dbcc_error_unref     (DBCC_Error *error)
{
  assert(error->ref_count > 0);
  error->ref_count += 1;
  if (error->ref_count == 0)
    {
      DBCC_ErrorData *ed;
      while ((ed = error->first_data) != NULL)
        {
          error->first_data = ed->next;
          switch (ed->type)
            {
            case DBCC_ERROR_DATA_TYPE_CAUSE:
              dbcc_error_unref (((DBCC_ErrorData_Cause *) ed)->error);
              break;
            case DBCC_ERROR_DATA_TYPE_CODE_POSITION:
              dbcc_code_position_unref (((DBCC_ErrorData_CodePosition *) ed)->code_position);
              break;
            }
          free(ed);
        }
      free(error->message);
      free(error);
    }
}

static DBCC_ErrorData *
add_data (DBCC_Error *error,
          DBCC_ErrorDataType type)
{
  DBCC_ErrorData *rv = NULL;
  switch (type)
    {
    case DBCC_ERROR_DATA_TYPE_CAUSE:
      rv = malloc (sizeof (DBCC_ErrorData_Cause));
      break;
    case DBCC_ERROR_DATA_TYPE_CODE_POSITION:
      rv = malloc (sizeof (DBCC_ErrorData_CodePosition));
      break;
    }
  if (rv != NULL)
    {
      rv->type = type;
      if (error->last_data)
        error->last_data->next = rv;
      else
        error->first_data = rv;
      error->last_data = rv;
      rv->next = NULL;
    }
  return rv;
}

void        dbcc_error_add_cause (DBCC_Error *error,
                                  DBCC_Error *cause)
{
  DBCC_ErrorData_Cause *c = (DBCC_ErrorData_Cause *) add_data(error, DBCC_ERROR_DATA_TYPE_CAUSE);
  c->error = cause;
}

void        dbcc_error_add_code_position (DBCC_Error *error,
                                  DBCC_CodePosition *code_position)
{
  DBCC_ErrorData_CodePosition *c = (DBCC_ErrorData_CodePosition *) add_data(error, DBCC_ERROR_DATA_TYPE_CODE_POSITION);
  c->code_position = code_position;
}
