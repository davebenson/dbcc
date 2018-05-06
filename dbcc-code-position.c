#include "dbcc.h"

DBCC_CodePosition *
dbcc_code_position_new  (DBCC_CodePosition *expanded_from,
                         DBCC_CodePosition *included_from,
                         DBCC_Symbol       *filename,
                         unsigned           line_no,
                         unsigned           column,
                         unsigned           byte_offset)
{
  DBCC_CodePosition *cp = malloc (sizeof (DBCC_CodePosition));
  cp->ref_count = 1;
  cp->expanded_from = expanded_from;
  if (expanded_from)
    dbcc_code_position_ref (expanded_from);
  cp->included_from = included_from;
  if (included_from)
    dbcc_code_position_ref (included_from);
  cp->filename = dbcc_symbol_ref (filename);
  cp->line_no = line_no;
  cp->column = column;
  cp->byte_offset = byte_offset;
  return cp;
}

DBCC_CodePosition *dbcc_code_position_ref   (DBCC_CodePosition *cp)
{
  cp->ref_count += 1;
  return cp;
}
void               dbcc_code_position_unref (DBCC_CodePosition *cp)
{
  if (cp->ref_count == 1)
    {
      if (cp->expanded_from)
        dbcc_code_position_unref (cp->expanded_from);
      if (cp->included_from)
        dbcc_code_position_unref (cp->included_from);
      dbcc_symbol_unref (cp->filename);
      free (cp);
    }
  else
    cp->ref_count -= 1;
}

