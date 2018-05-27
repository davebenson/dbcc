#include "dbcc.h"

void                 dbcc_ptr_table_init   (DBCC_PtrTable *to_init)
{
  memset (to_init, 0, sizeof (*to_init));
}
DBCC_PtrTable_Entry *
dbcc_ptr_table_lookup (DBCC_PtrTable *table,
                       const void    *key)
{
  if (table->size == 0)
    return NULL;
  size_t entry_index = ((intptr_t) key) % table->size;
  for (DBCC_PtrTable_Entry *entry = table->table[entry_index];
       entry != NULL;
       entry = entry->next)
    if (entry->key == key)
      return entry;
  return NULL;
}

void                *dbcc_ptr_table_lookup_value (DBCC_PtrTable *table,
                                            const void    *key)
{
  DBCC_PtrTable_Entry *entry = dbcc_ptr_table_lookup_value (table, key);
  return entry == NULL ? NULL : entry->value;
}

DBCC_PtrTable_Entry *
dbcc_ptr_table_force   (DBCC_PtrTable *table,
                        const void    *key,
                        bool          *created_opt_out)
{
  if (table->size == 0)
    {
      ... create new table
    }

  size_t entry_index = ((intptr_t) key) % table->size;
  for (DBCC_PtrTable_Entry *entry = table->table[entry_index];
       entry != NULL;
       entry = entry->next)
    if (entry->key == key)
      return entry;

  // create new entry:  do we need to resize?
  ...

  return NULL;
}

void
dbcc_ptr_table_foreach (DBCC_PtrTable *table,
                        DBCC_PtrTable_VisitFunc func,
                        void          *visit_data)
{
  ....
}

void
dbcc_ptr_table_clear   (DBCC_PtrTable *to_clear)
{
  ....
}
