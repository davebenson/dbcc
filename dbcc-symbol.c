#include "dbcc.h"


/* Resize if 
 *
 *        table size 
 *        ----------   < RESIZE_UPWARD_OCCUPANCY_RATE
 *         n symbols
 *
 * NOTE: NOT TESTED for OCCUPANCY_RATE==2 which is a
 * corner-case b/c table-size is a power-of-two.
 */
#define RESIZE_UPWARD_OCCUPANCY_RATE      3

/* Lookup3 - simplified version taken from 
 *    https://stackoverflow.com/questions/14409466/simple-hash-functions
 */
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define lookup3_mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

#define lookup3_final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

static uint32_t lookup3 (
  const void *key,
  size_t      length,
  uint32_t    initval
) {
  uint32_t  a,b,c;
  const uint8_t  *k;
  const uint32_t *data32Bit;

  data32Bit = key;
  a = b = c = 0xdeadbeef + (((uint32_t)length)<<2) + initval;

  while (length > 12) {
    a += *(data32Bit++);
    b += *(data32Bit++);
    c += *(data32Bit++);
    lookup3_mix(a,b,c);
    length -= 12;
  }

  k = (const uint8_t *)data32Bit;
  switch (length) {
    case 12: c += ((uint32_t)k[11])<<24;
    case 11: c += ((uint32_t)k[10])<<16;
    case 10: c += ((uint32_t)k[9])<<8;
    case 9 : c += k[8];
    case 8 : b += ((uint32_t)k[7])<<24;
    case 7 : b += ((uint32_t)k[6])<<16;
    case 6 : b += ((uint32_t)k[5])<<8;
    case 5 : b += k[4];
    case 4 : a += ((uint32_t)k[3])<<24;
    case 3 : a += ((uint32_t)k[2])<<16;
    case 2 : a += ((uint32_t)k[1])<<8;
    case 1 : a += k[0];
             break;
    case 0 : return c;
  }
  lookup3_final(a,b,c);
  return c;
}
#define LOOKUP3_HASH_INITVAL   0x125df2a7
uint32_t dbcc_symbol_hash_len (size_t len, const char *str)
{
  return lookup3(str, len, LOOKUP3_HASH_INITVAL);
}

DBCC_Symbol *
dbcc_symbol_space_force (DBCC_SymbolSpace *ns, const char *str)
{
  return dbcc_symbol_space_force_len (ns, strlen (str), str);
}

DBCC_Symbol *
dbcc_symbol_space_try   (DBCC_SymbolSpace *ns, const char *str)
{
  return dbcc_symbol_space_try_len (ns, strlen (str), str);
}

DBCC_Symbol *
dbcc_symbol_space_force_len (DBCC_SymbolSpace *ns, size_t len, const char *str)
{
  uint64_t h = dbcc_symbol_hash_len (len, str);
  size_t bin = h & ((1<<ns->ht_size_log2) - 1);
  for (DBCC_Symbol *at = ns->ht[bin]; at != NULL; at = at->hash_next)
    if (strcmp (dbcc_symbol_get_string (at), str) == 0)
      return at;

  uint64_t min_ht_size = (uint64_t) ns->n_symbols * RESIZE_UPWARD_OCCUPANCY_RATE;
  if ((1ULL << ns->ht_size_log2) < min_ht_size)
    {
      unsigned new_ht_size_log2 = ns->ht_size_log2 + 1;
      while ((1ULL << new_ht_size_log2) < min_ht_size)
        new_ht_size_log2++;
      size_t new_ht_size = ((size_t) 1) << new_ht_size_log2;
      DBCC_Symbol **new_ht = calloc (sizeof (DBCC_Symbol *), new_ht_size);
      size_t old_ht_size = 1ULL << ns->ht_size_log2;
      for (size_t i = 0; i < old_ht_size; i++)
        {
          DBCC_Symbol *at = ns->ht[i];
          while (at != NULL)
            {
              size_t b = at->hash & (new_ht_size - 1);
              DBCC_Symbol *next = at->hash_next;
              at->hash_next = new_ht[b];
              new_ht[b] = at;
              at = next;
            }
        }
      free (ns->ht);
      ns->ht = new_ht;
      ns->ht_size_log2 = new_ht_size_log2;

      // sync bin with the new hash table size.
      bin = h & ((1<<ns->ht_size_log2) - 1);
    }

  DBCC_Symbol *rv = malloc (sizeof (DBCC_Symbol) + len + 1);
  rv->hash = h;
  rv->length = len;
  rv->symbol_space = ns;
  rv->hash_next = ns->ht[bin];
  ns->ht[bin] = rv;
  memcpy ((char *) (rv + 1), str, len);
  ((char *) (rv + 1))[len] = '\0';
  return rv;
}

DBCC_Symbol *
dbcc_symbol_space_try_len   (DBCC_SymbolSpace *ns, size_t len, const char *str)
{
  uint64_t h = dbcc_symbol_hash_len (len, str);
  size_t bin = h & ((1<<ns->ht_size_log2) - 1);
  for (DBCC_Symbol *at = ns->ht[bin]; at != NULL; at = at->hash_next)
    if (len == at->length && memcmp (dbcc_symbol_get_string (at), str, len) == 0)
      return at;
  return NULL;
}

DBCC_Symbol *
dbcc_symbol_ref (DBCC_Symbol *symbol)
{
  return symbol;
}
void
dbcc_symbol_unref (DBCC_Symbol *symbol)
{
  (void) symbol;
}
