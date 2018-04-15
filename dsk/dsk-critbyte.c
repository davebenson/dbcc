#include "dsk.h"
//#include "dsk-critbyte.h"

/* Chosen so that it'll leave the next pointers aligned. */
#define CRITBIT_TABLE_NODE_DATA_SIZE 7

#define ASSERT dsk_assert

typedef struct DskCritbyteTableNode DskCritbyteTableNode;
typedef struct DskCritbyteTable DskCritbyteTable;

struct DskCritbyteTableNode
{
  /* The value is the value BEFORE matching the next bits.  So the value at the top
   * of the tree is always for the empty string, even if top->n_bits > 0. */
  void *value;

  unsigned char n_bits;

  // bits are aligned like the original data
  unsigned char data[CRITBIT_TABLE_NODE_DATA_SIZE];

  DskCritbyteTableNode *children[2];
};

struct DskCritbyteTable
{
  DskCritbyteTableNode top;
};

/* No need to check (table)->top.children[1], because BOTH children
 * must be non-NULL at root to justify n_bits==0. */
#define DSK_CRITBYTE_TABLE_IS_EMPTY(table) \
  ((table)->top.n_bits == 0 && (table)->top.children[0] == NULL)

DskCritbyteTable *
dsk_critbyte_table_new(void)
{
  return DSK_NEW0 (DskCritbyteTable);
}

DskCritbyteTableNode *
dsk_critbyte_table_lookup(DskCritbyteTable *table,
                          size_t            length,
                          const uint8_t    *data)
{
  DskCritbyteTableNode *node = &table->top;
  unsigned rem = length * 8;
  const uint8_t *at = data;
  uint8_t mask_shift = 7;
 
  /* This loop executed once per level of the tree.
   * It aborts when it finds a match or conclusively does not match.
   */
  while (node != NULL)
    {
      if (rem == 0)
        return node;

      unsigned bits_rem = node->n_bits;

      /* If lookup-key terminates in the middle of the node's bits,
       * there's no way a lookup can succeed. Note that there's an implicit bit
       * determined by the direction of the tree branch. */
      if (rem <= bits_rem)
        return NULL;

      const uint8_t *node_at = node->data;
      /* Handle stray leading input bits if any. */
      if (mask_shift != 7)
        {
          unsigned render_bits = 1 + mask_shift;
          if (render_bits > bits_rem)
            {
              // partial byte (ie mask is strictly mid-byte)
              uint8_t mask = ((1 << bits_rem) - 1) << (render_bits - bits_rem);
              if ((*node_at & mask) != (*at & mask))
                return NULL;
              mask_shift -= bits_rem;
              rem -= bits_rem;
              goto test_bit;
            }
          else
            {
              uint8_t mask = (1<< render_bits) - 1;
              if ((*node_at & mask) != (*at & mask))
                return NULL;
              mask_shift = 7;
              at++;
              node_at++;
            }
        }

      /* Handle whole bytes */
      ASSERT (mask_shift == 7);
      while (bits_rem >= 8)
        {
          if (*node_at != *at)
            return NULL;
          node_at++;
          at++;
          bits_rem -= 8;
          rem -= 8;
        }

      if (bits_rem == 0)
        {
          goto test_bit;
        }

      /* Handle stray following input bits. */
      ASSERT (mask_shift == 7);
      ASSERT (0 < bits_rem && bits_rem < 8);
      uint8_t mask = ((1 << bits_rem) - 1) << (8 - bits_rem);
      if ((*node_at & mask) != (*at & mask))
        return NULL;
      mask_shift -= bits_rem;
      rem -= bits_rem;

    test_bit:
      ASSERT (rem > 0);
      {
        /* Get the bit that determines which of children[2] to descend */
        uint8_t tb = (*at >> mask_shift) & 1;
        rem--;
        if (mask_shift == 0)
          {
            mask_shift = 7;
            at++;
          }
        else
          mask_shift--;
        node = node->children[tb];
      }
    }
  return NULL;
}

DskCritbyteTableNode *
dsk_critbyte_table_insert(DskCritbyteTable *table,
                          size_t            length,
                          uint8_t          *data)
{
  DskCritbyteTableNode *node = &table->top;
  unsigned rem = length * 8;
  const uint8_t *at = data;
  uint8_t mask_shift = 7;
  uint8_t test_bit;
  uint8_t mask;
 
  /* This loop executed once per level of the tree.
   * It aborts when it finds a match or conclusively does not match.
   */
  for (;;)
    {
      ASSERT (node != NULL);

      if (rem == 0)
        return node;

      unsigned bits_rem = node->n_bits;
      const uint8_t *node_at = node->data;


      /* If lookup-key terminates in the middle of the node's bits,
       * there's no way a lookup can succeed. Note that there's an implicit bit
       * determined by the direction of the tree branch. */
      if (rem <= bits_rem)
        goto split_node;

      /* Handle stray leading input bits if any. */
      if (mask_shift != 7)
        {
          unsigned render_bits = 1 + mask_shift;
          if (render_bits > bits_rem)
            {
              // partial byte (ie mask is strictly mid-byte)
              mask = ((1 << bits_rem) - 1) << (render_bits - bits_rem);
              if ((*node_at & mask) != (*at & mask))
                goto split_node;
              mask_shift -= bits_rem;
              rem -= bits_rem;
              goto test_bit;
            }
          else
            {
              mask = (1<< render_bits) - 1;
              if ((*node_at & mask) != (*at & mask))
                goto split_node;
              mask_shift = 7;
              at++;
              node_at++;
            }
        }

      /* Handle whole bytes */
      ASSERT (mask_shift == 7);
      while (bits_rem >= 8)
        {
          if (*node_at != *at)
            {
              mask = 0xff;
              goto split_node;
            }
          node_at++;
          at++;
          bits_rem -= 8;
          rem -= 8;
        }

      if (bits_rem == 0)
        {
          goto test_bit;
        }

      /* Handle stray following input bits. */
      ASSERT (mask_shift == 7);
      ASSERT (0 < bits_rem && bits_rem < 8);
      mask = ((1 << bits_rem) - 1) << (8 - bits_rem);
      if ((*node_at & mask) != (*at & mask))
        goto split_node;
      mask_shift -= bits_rem;
      rem -= bits_rem;

    test_bit:
      ASSERT (rem > 0);
      /* Get the bit that determines which of children[2] to descend */
      test_bit = (*at >> mask_shift) & 1;
      rem--;
      if (mask_shift == 0)
        {
          mask_shift = 7;
          at++;
        }
      else
        mask_shift--;
      if (node->children[test_bit] == NULL)
        {
          if (node->children[0] == NULL && node->children[1] == NULL)
            {
              ...
            }
          else
            {
              new_tail = DSK_NEW0 (DskCritbyteTableNode);
              node->children[test_bit] = new_tail;
              goto extend_tree;
            }
        }
      node = node->children[test_bit];
    }
  ASSERT (DSK_FALSE);

split_node:
  ASSERT (mask != 0);
  for (;;)
    {
      uint8_t hi_bit = (mask) & (mask - 1);
      if ((*node_at & hi_bit) != (*at & hi_bit))
        break;
      mask -= hi_bit;
      mask_shift--;
      rem--;
      bits_rem--;
      ASSERT (mask != 0);
    }

  ASSERT (rem > 0);
  ASSERT (bits_rem > 0);
  uint8_t split_node_bit_count = node->n_bits - bits_rem;
  DskCritbyteTableNode *new_tail = DSK_NEW0 (DskCritbyteTableNode);
  DskCritbyteTableNode *old_tail = DSK_NEW0 (DskCritbyteTableNode);
  old_tail->n_bits = bits_rem - 1;
  old_tail->children[0] = node->children[0];
  old_tail->children[1] = node->children[1];
  node->n_bits = split_node_bit_count;
  ASSERT (((*node_at >> mask_shift) & 1) != ((*at >> mask_shift) & 1));
  node->children[(*node_at >> mask_shift) & 1] = old_tail;
  node->children[(*at >> mask_shift) & 1] = new_tail;
  rem--;
maybe_extend_tree:
  while (rem > 0)
    {
      DskCritbyteTableNode *newnewtail = DSK_NEW0 (DskCritbyteTableNode);
      new_tail->n_bits = MIN (rem - 1, 8 * CRITBIT_TABLE_NODE_DATA_SIZE - (7 - mask_shift));
      new_tail->children[...] = newnewtail;
      memcpy (new_tail->data, at, ...);
      mask_shift = 6;           /* we use all the bytes available, then one more bit when deciding on the child. */
      rem -= new_tail->n_bits + 1;
      at += ...;

      new_tail = newnewtail;
    }
  return new_tail;
}

dsk_boolean
dsk_critbyte_table_remove(DskCritbyteTable *table,
                          size_t            length,
                          uint8_t          *data)
{
  //...
}

static void
pr_critbyte_node (DskCritbyteTableNode *node, unsigned level, unsigned mask_shift)
{
}

void
dsk_critbyte_table_debug_dump (DskCritbyteTable *table)
{
}
