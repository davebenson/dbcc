#include "dbcc.h"
#include "dbcc-macros.h"

const char *dbcc_unary_operator_name (DBCC_UnaryOperator op)
{
  switch (op)
    {
#define CASE(shortname)  \
        case DBCC_UNARY_OPERATOR_##shortname: return #shortname;
    DBCC_UNARY_OPERATOR_FOREACH(CASE)
#undef CASE
    }
}
const char *dbcc_binary_operator_name (DBCC_BinaryOperator op)
{
  switch (op)
    {
#define CASE(shortname)  \
        case DBCC_BINARY_OPERATOR_##shortname: return #shortname;
    DBCC_BINARY_OPERATOR_FOREACH(CASE)
#undef CASE
    }
}



const char *dbcc_inplace_unary_operator_name (DBCC_InplaceUnaryOperator op)
{
  switch (op)
    {
#define CASE(shortname)  \
        case DBCC_INPLACE_UNARY_OPERATOR_##shortname: return #shortname;
    DBCC_INPLACE_UNARY_OPERATOR_FOREACH(CASE)
#undef CASE
    }
}
const char *dbcc_inplace_binary_operator_name (DBCC_InplaceBinaryOperator op)
{
  switch (op)
    {
#define CASE(shortname)  \
        case DBCC_INPLACE_BINARY_OPERATOR_##shortname: return #shortname;
    DBCC_INPLACE_BINARY_OPERATOR_FOREACH(CASE)
#undef CASE
    }
}


