
typedef struct DBCC_CodePosition DBCC_CodePosition;
struct DBCC_CodePosition
{
  size_t ref_count;
  DBCC_CodePosition *expanded_from;
  DBCC_Symbol *filename;
  unsigned line_no;
  unsigned column;
  unsigned byte_offset;
};

DBCC_CodePosition *dbcc_code_position_new   (DBCC_CodePosition *expanded_from,
                                             DBCC_Symbol       *filename,
                                             unsigned           line_no,
                                             unsigned           column,
                                             unsigned           byte_offset);

DBCC_CodePosition *dbcc_code_position_ref   (DBCC_CodePosition *cp);
void               dbcc_code_position_unref (DBCC_CodePosition *cp);

