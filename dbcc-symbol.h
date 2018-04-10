

struct _DBCC_SymbolSpace
{
  unsigned ht_size;
  DBCC_Symbol **ht;
};

struct _DBCC_Symbol
{
  uint64_t hash;
  size_t length;
  DBCC_SymbolSpace *symbol_space;
  DBCC_Symbol *hash_next;
  void *global_def;

  /* NUL-terminated string follows immediately */
};

uint64_t                 dbcc_symbol_hash        (const char *str);
DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_force (const char *str);
DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_try   (const char *str);
DBCC_INLINE const char  *dbcc_symbol_get_string  (const DBCC_Symbol *symbol);
  

DBCC_Symbol *_dbcc_symbol_table_do_create_symbol_internal (DBCC_SymbolTable *ns, const char *str, uint64_t hash);

DBCC_INLINE DBCC_Symbol *dbcc_symbol_table_force_symbol (DBCC_SymbolTable *ns, const char *str)
{
  uint64_t h = dbcc_symbol_hash (str);
  size_t bin = h & ((1<<ns->ht_size_log2) - 1);
  for (DBCC_Symbol *at = ns->ht[bin]; at != NULL; at = at->next)
    if (strcmp (dbcc_symbol_get_string (at), str) == 0)
      return at;
  return _dbcc_global_namespace_do_create_symbol_internal (ns, str, h);
}

DBCC_INLINE DBCC_Symbol *dbcc_global_namespace_try_symbol   (const char *str)
{
  uint64_t h = dbcc_symbol_hash (str);
  size_t bin = h & ((1<<ns->ht_size_log2) - 1);
  for (DBCC_Symbol *at = ns->ht[bin]; at != NULL; at = at->next)
    if (strcmp (dbcc_symbol_get_string (at), str) == 0)
      return at;
  return NULL;
}

DBCC_INLINE const char *dbcc_symbol_get_string (const DBCC_Symbol *symbol)
{
  return (const char *) (symbol + 1);
}
