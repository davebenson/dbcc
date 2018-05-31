#ifndef __DBCC_PTR_TABLE_H_
#define __DBCC_PTR_TABLE_H_

typedef struct DBCC_PtrTable_Entry DBCC_PtrTable_Entry;
typedef struct DBCC_PtrTable DBCC_PtrTable;
struct DBCC_PtrTable_Entry
{
  DBCC_PtrTable_Entry *next;
  void *key;
  void *value;
};
struct DBCC_PtrTable
{
  size_t size;          /* prime or 0*/
  unsigned size_index;
  size_t occupancy;
  DBCC_PtrTable_Entry **table;
};

typedef void (*DBCC_PtrTable_VisitFunc)(DBCC_PtrTable_Entry *entry,
                                        void                *visit_data);

void                 dbcc_ptr_table_init   (DBCC_PtrTable *to_init);
DBCC_PtrTable_Entry *dbcc_ptr_table_lookup (DBCC_PtrTable *table,
                                            const void    *key);
void                *dbcc_ptr_table_lookup_value (DBCC_PtrTable *table,
                                            const void    *key);
DBCC_PtrTable_Entry *dbcc_ptr_table_force  (DBCC_PtrTable *table,
                                            const void    *key,
                                            bool          *created_opt_out);
void                 dbcc_ptr_table_set    (DBCC_PtrTable *table,
                                            const void    *key,
                                            void          *value);
void                 dbcc_ptr_table_foreach(DBCC_PtrTable *table,
                                            DBCC_PtrTable_VisitFunc func,
                                            void          *visit_data);
void                 dbcc_ptr_table_clear  (DBCC_PtrTable *to_clear);

#endif
