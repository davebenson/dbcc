/* A Symbol is an immutable string that's guaranteed to be unique
 * within a SymbolSpace.
 *
 * Therefore, you can assume that if two symbols are equal, then they
 * will be equal pointer-wise, and hence you can use == instead of strcmp to
 * compare them, and likewise for mappings.
 *
 * The decision to make the SymbolSpace object non-global may
 * be a bad choice, and may be re-evaluated at a future date.
 */

typedef struct DBCC_Symbol DBCC_Symbol;
typedef struct DBCC_SymbolSpace DBCC_SymbolSpace;

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

  /* NUL-terminated string follows immediately */
};

DBCC_SymbolSpace *dbcc_symbol_space_new (void);
DBCC_Symbol       *dbcc_symbol_space_force     (DBCC_SymbolSpace *space,
                                                const char *str);
DBCC_Symbol       *dbcc_symbol_space_try       (DBCC_SymbolSpace *space,
                                                const char *str);
DBCC_Symbol       *dbcc_symbol_space_force_len (DBCC_SymbolSpace *space,
                                                size_t      len,
                                                const char *str);
DBCC_Symbol       *dbcc_symbol_space_try_len   (DBCC_SymbolSpace *space,
                                                size_t      len,
                                                const char *str);
DBCC_INLINE const char *dbcc_symbol_get_string  (const DBCC_Symbol *symbol);
  

/* These functions are essentially implementation details. */
uint32_t           dbcc_symbol_hash_len        (size_t      len,
                                                const char *str);

#define dbcc_symbol_ref(symbol)   (symbol)
#define dbcc_symbol_unref(symbol)   (void)(symbol)

#if DBCC_CAN_INLINE || defined(DBCC_IMPLEMENT_INLINES)
DBCC_INLINE const char *dbcc_symbol_get_string (const DBCC_Symbol *symbol)
{
  return (const char *) (symbol + 1);
}
#endif

