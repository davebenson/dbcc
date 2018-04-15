/* A hashtable that resizes incrementally, avoiding bad worst-case times. */

typedef struct DskIncrHashtable DskIncrHashtable;

DskIncrHashtable *dsk_incr_hashtable_new (DskIncrHashtableHashFunc hash,
                                          DskIncrHashtableEqualFunc equal,
                                          void *func_data,
                                          DskDestroyNotify func_data_destroy);
void              dsk_incr_hashtable_set (DskIncrHashtable *table,
                                          void *key,
                                          void *value);
void *            dsk_incr_hashtable_get (DskIncrHashtable *table,
                                          const void *key);
