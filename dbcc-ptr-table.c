#include "dbcc.h"

#define MAX_OCC_NUMER    1
#define MAX_OCC_DENOM    4

static uint32_t ptr_table_sizes[] =
{
  7,
  19,
  43,
  97,
  271,
  571,
  1171,
  2341,
  4993,
  10211,
  19183,
  40039,
  80021,
};

static inline unsigned ptr_to_int (const void *key)
{
  return (intptr_t) key;
}

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
  size_t entry_index = ptr_to_int (key) % table->size;
  for (DBCC_PtrTable_Entry *entry = table->table[entry_index];
       entry != NULL;
       entry = entry->next)
    if (entry->key == key)
      return entry;
  return NULL;
}

void *
dbcc_ptr_table_lookup_value (DBCC_PtrTable *table,
                              const void    *key)
{
  DBCC_PtrTable_Entry *entry = dbcc_ptr_table_lookup (table, key);
  return entry == NULL ? NULL : entry->value;
}

DBCC_PtrTable_Entry *
dbcc_ptr_table_force   (DBCC_PtrTable *table,
                        const void    *key,
                        bool          *created_opt_out)
{
  if (table->size == 0)
    {
      table->size = ptr_table_sizes[0];
      table->size_index = 0;
      table->occupancy = 0;
      table->table = calloc (sizeof (DBCC_PtrTable_Entry *), table->size);
    }
  else if ((uint64_t) table->size * MAX_OCC_DENOM >= table->occupancy * MAX_OCC_NUMER)
    {
      size_t new_size = ptr_table_sizes[table->size_index + 1];
      DBCC_PtrTable_Entry **newtab = calloc (sizeof (DBCC_PtrTable_Entry *), new_size);
      for (size_t i = 0; i < table->size; i++)
        {
          DBCC_PtrTable_Entry *e;
          while ((e = table->table[i]) != NULL)
            {
              table->table[i] = e->next;
              e->next = newtab[ptr_to_int(e->key) % new_size];
              newtab[ptr_to_int(e->key) % new_size] = e;
            }
        }
      table->size = new_size;
      table->size_index += 1;
      free (table->table);
      table->table = newtab;
    }

  size_t entry_index = ptr_to_int (key) % table->size;
  for (DBCC_PtrTable_Entry *entry = table->table[entry_index];
       entry != NULL;
       entry = entry->next)
    if (entry->key == key)
      {
        if (created_opt_out != NULL)
          *created_opt_out = false;
        return entry;
      }

  // create new entry:  do we need to resize?
  DBCC_PtrTable_Entry *rv = malloc (sizeof (DBCC_PtrTable_Entry));
  rv->next = table->table[entry_index];
  rv->key = (void*) key;
  rv->value = NULL;
  table->table[entry_index] = rv;
  table->occupancy += 1;
  if (created_opt_out != NULL)
    *created_opt_out = true;
  return rv;
}

void                 dbcc_ptr_table_set    (DBCC_PtrTable *table,
                                            const void    *key,
                                            void          *value)
{
  DBCC_PtrTable_Entry *e = dbcc_ptr_table_force (table, key, NULL);
  e->value = value;
}
  
void
dbcc_ptr_table_foreach (DBCC_PtrTable *table,
                        DBCC_PtrTable_VisitFunc func,
                        void          *visit_data)
{
  for (size_t i = 0; i < table->size; i++)
    for (DBCC_PtrTable_Entry *e = table->table[i]; e; e = e->next)
      {
        func (e, visit_data);
      }
}

void
dbcc_ptr_table_clear   (DBCC_PtrTable *table)
{
  for (size_t i = 0; i < table->size; i++)
    {
      DBCC_PtrTable_Entry **p = table->table + i;
      while (*p != NULL)
        {
          DBCC_PtrTable_Entry *kill = *p;
          *p = kill->next;
          free (kill);
        }
    }
  free (table->table);
}
