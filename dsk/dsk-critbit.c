#define SLAB_SIZE     128
#define DEFAULT_N_SLABS_ALLOCED 16
#define INVALID_NODE_INDEX 0x7fffffff

struct Node
{
  uint32_t is_set : 1;
  uint32_t zero_node : 31;
  uint32_t one_node;
  void *data;
};

struct _DskCritbit
{
  size_t n_slabs;
  size_t slabs_alloced;
  struct Node **slabs;
  size_t next_use;
  uint32_t free_list_index;
};

/* Returns a "struct Node*" */
#define CRITBIT_GET_NODE_FROM_INDEX(critbit, index) \
  (critbit->slabs[(index) / SLAB_SIZE] + ((index) % SLAB_SIZE))

DskCritbit *dsk_critbit_new    (void)
{
  DskCritbit *rv = DSK_NEW (DskCritbit);
  rv->n_slabs = 0;
  rv->slabs = DSK_NEW_ARRAY (struct Node *, DEFAULT_N_SLABS_ALLOCED);
  rv->slabs_alloced = DEFAULT_N_SLABS_ALLOCED;
  rv->next_use = SLAB_SIZE;
  rv->top.is_set = DSK_FALSE;
  rv->top.zero_node = INVALID_NODE_INDEX;
  rv->top.one_node = INVALID_NODE_INDEX;
  rv->free_list_index = INVALID_NODE_INDEX;
  return rv;
}

void dsk_critbit_destroy (DskCritbit *critbit)
{
  size_t i;
  for (i = 0; i < critbit->n_slabs; i++)
    dsk_free (critbit->slabs[i]);
  dsk_free (critbit->slabs);
  dsk_free (critbit);
}

dsk_boolean dsk_critbit_set    (DskCritbit    *critbit,
                                size_t         len,
                                const uint8_t *data,
                                void          *value)
{
  void **p_value;
  dsk_boolean existed = dsk_critbit_access (critbit, len, data, &p_value, DSK_CRITBIT_ACCESS_CREATE_IF_NOT_EXISTS);
  *p_value = value;
  return existed;                       /// kinda a weird thing to return
}

dsk_boolean dsk_critbit_get    (DskCritbit    *critbit,
                                size_t         len,
                                const uint8_t *data,
                                void         **value_out)
{
  void **p_value;
  if (!dsk_critbit_access (critbit, len, data, &p_value, 0))
    return DSK_FALSE;
  *value_out = *p_value
  return DSK_TRUE;
}

dsk_boolean dsk_critbit_remove (DskCritbit    *critbit,
                                size_t         len,
                                const uint8_t *data)
{
  return dsk_critbit_access (critbit, len, data, NULL, DSK_CRITBIT_ACCESS_DELETE_IF_EXISTS);
}

/* The overall algorithm implemented in
 * _dsk_critbit_create_new_subtree is pretty simple:
         while (rem > 0) { 
           next = alloc_node()
           if (is_zero_bit) {
             at->zero = next
           } else {
             at->one = next
           }
           rem--; (and is_zero_bit or whatever the loop iteration requires)
           at = next;
         }
 *
 * But, with the intention of allowing the compiler to
 * better vectorize the operation, we split the loop into
 * two parts:
 *      if (has free list)
 *        // important, but hard to optimize / vectorize, b/c we have
 *        // a linked-list.
 *        while (rem > 0) {
 *          ... alloc_node uses the free_list ...
 *        }
 *      do {
 *        if (has slab remaining) {
 *          // very important and easily optimized (for the compiler) loop.
 *          while (rem > 0 etc) {
 *            ... alloc_node uses the slab ...
 *          }
 *        }
 *        if (rem > 0)
 *          allocate_new_slab();
 *      } while(rem > 0)
 */
static dsk_boolean
_dsk_critbit_create_new_subtree(DskCritbit       *critbit,
                                size_t            len,
                                const uint8_t    *data,
                                void           ***pointer_to_value_out,
                                struct Node      *at,
                                size_t            rem,
                                const uint8_t    *data_at,
                                uint8_t           mask)
{
  while (rem > 0 && critbit->free_list_index != INVALID_NODE_INDEX)
    {
      dsk_boolean is_zero = (mask & *data_at) == 0;
      uint32_t next_index = critbit->free_list_index;
      struct Node *next = CRITBIT_GET_NODE_FROM_INDEX (critbit, next_index);
      critbit->free_list_index = n->zero_node;

      at->
      ...
    }
  while (rem > 0)
    {
      /* Figure out how many to process in a tight loop. */
      size_t available_in_slab = SLAB_SIZE - critbit->next_use;
      size_t n_process = DSK_MIN (rem, available_in_slab);
      
      if (n_process > 0)
        {
          /* Process in tight loop: this is loop is VERY commonly
           * executed in building up tables.  Which is why we've tried to
           * move all the conditionals outside it. */
          ...

          /* Update for next iteration. */
          rem -= n_process;
          ...
        }
      if (n_process == 0 || rem > 0)
        {
          /* Allocate new slab. */
          ...
        }

  return DSK_FALSE;
}
 
static void
_dsk_critbit_cleanup_delete    (DskCritbit *critbit,
                                size_t      len,
                                const uint8_t *data)
{
  ...
}

dsk_boolean
dsk_critbit_access (DskCritbit *critbit,
                    size_t      len,
                    const uint8_t *data,
                    void     ***pointer_to_value_out,
                    DskCritbitAccessFlags access_flags)
{
  unsigned char mask = 128;
  const uint8_t *data_at = data;
  size_t rem = len;
  struct Node *at = &critbit->top;
  while (rem > 0)
    {
      dsk_boolean is_zero = (mask & *data_at) == 0;
      uint32_t idx = is_zero ? at->zero_node : at->one_node;
      if (idx == INVALID_NODE_INDEX)
        {
          /* If we are not supposed to create a new subtree, abort. */
          if ((access_flags & DSK_CRITBIT_ACCESS_CREATE_IF_NOT_EXISTS) == 0)
            return DSK_FALSE;
          return _dsk_critbit_create_new_subtree (
            ...
          );
        }
      else
        {
          at = critbit->slabs[idx / SLAB_SIZE] + (idx % SLAB_SIZE);
          mask >>= 1;
          if (mask == 0)
            {
              /* move onto new byte */
              ...
            }
        }
    }
  if (at->is_set)
    {
      if ((access_flags & DSK_CRITBIT_ACCESS_DELETE_IF_EXISTS) != 0)
        {
          at->is_set = DSK_FALSE;
          if (at->zero_node != INVALID_NODE_INDEX 
           || at->one_node != INVALID_NODE_INDEX)
            {
              return DSK_TRUE;
            }
          /* we are a leaf-node in the tree,
           * so deletion can liberate some internal nodes. */
          _dsk_critbit_cleanup_delete (critbit, len, data);
          return DSK_TRUE;
        }
      if (pointer_to_value_out != NULL)
        *pointer_to_value_out = &at->data;
      return DSK_TRUE;
    }
  else
    {
      if ((access_flags & DSK_CRITBIT_ACCESS_CREATE_IF_NOT_EXISTS) == 0)
        {
          at->is_set = DSK_TRUE;
          assert (pointer_to_value_out != NULL);
          *pointer_to_value_out = &at->data;
        }
      return DSK_FALSE;
    }
}
