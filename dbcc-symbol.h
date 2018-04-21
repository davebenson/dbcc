
typedef struct DBCC_Symbol DBCC_Symbol;
typedef struct DBCC_SymbolSpace DBCC_SymbolSpace;
#define DBCC_INLINE inline

struct DBCC_SymbolSpace
{
  unsigned ht_size_log2;
  DBCC_Symbol **ht;
  size_t n_symbols;
};

struct DBCC_Symbol
{
  uint64_t hash;
  size_t length;
  DBCC_SymbolSpace *symbol_space;
  DBCC_Symbol *hash_next;
  void *global_def;

  /* NUL-terminated string follows immediately */
};

uint64_t                 dbcc_symbol_hash        (const char *str);
DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_force (DBCC_SymbolSpace *space,
                                                  const char *str);
DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_try   (DBCC_SymbolSpace *space,
                                                  const char *str);
DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_force_len (DBCC_SymbolSpace *space,
                                                  unsigned    len,
                                                  const char *str);
DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_try_len   (DBCC_SymbolSpace *space,
                                                  unsigned    len,
                                                  const char *str);
DBCC_INLINE const char  *dbcc_symbol_get_string  (const DBCC_Symbol *symbol);
  

DBCC_Symbol *_dbcc_symbol_space_do_create_symbol_internal (DBCC_SymbolSpace *ns, const char *str, uint64_t hash);

DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_force (DBCC_SymbolSpace *ns, const char *str)
{
  uint64_t h = dbcc_symbol_hash (str);
  size_t bin = h & ((1<<ns->ht_size_log2) - 1);
  for (DBCC_Symbol *at = ns->ht[bin]; at != NULL; at = at->hash_next)
    if (strcmp (dbcc_symbol_get_string (at), str) == 0)
      return at;
  return _dbcc_symbol_space_do_create_symbol_internal (ns, str, h);
}

DBCC_INLINE DBCC_Symbol *dbcc_symbol_space_try   (DBCC_SymbolSpace *ns, const char *str)
{
  uint64_t h = dbcc_symbol_hash (str);
  size_t bin = h & ((1<<ns->ht_size_log2) - 1);
  for (DBCC_Symbol *at = ns->ht[bin]; at != NULL; at = at->hash_next)
    if (strcmp (dbcc_symbol_get_string (at), str) == 0)
      return at;
  return NULL;
}

DBCC_INLINE const char *dbcc_symbol_get_string (const DBCC_Symbol *symbol)
{
  return (const char *) (symbol + 1);
}
