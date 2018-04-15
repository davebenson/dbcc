#include "dsk.h"

#if 0
typedef struct { int index; size_t weight } PQNode;
#define COMPARE_PQ_NODE(a,b,rv) { if (a.weight < b.weight) rv = -1;         \
                                  else if (a.weight > b.weight) rv = +1;    \
                                  else rv = 0; }
#else
struct HuffmanComputeNode {
  unsigned index;
  uint64_t count;
  ... left/right/isright
};

DskHuffmanTree *dsk_huffman_tree_compute     (size_t           n,
                                              const uint64_t  *counts)
{
  DskMemPoolFixed pool;
  HuffmanComputeNode *buf = DSK_NEW_ARRAY (HuffmanComputeNode, n);
  dsk_mem_pool_fixed_init_buf (&pool, sizeof (HuffmanComputeNode), n, buf);

  HuffmanComputeNode *hufftree_top = NULL;
#define HUFFMAN_TREE() \
  ...
  
  for (size_t i = 0; i < n; i++)
    {
      ...
    }

  while (hufftree_top != NULL)
    {
      HuffmanComputeNode *min_node;
      DSK_RBTREE_REMOVE_MIN (HUFFMAN_TREE (), min_node);
      if (hufftree_top == NULL)
        {
        ...
        }
      ...
    }

  dsk_free (buf);
}

DskHuffmanTree *dsk_huffman_tree_load_memory (size_t           len,
                                              const uint8_t   *data,
                                              DskError       **error);
uint8_t        *dsk_huffman_tree_save_memory (DskHuffmanTree  *tree,
                                              size_t          *size_out);
void            dsk_huffman_tree_free        (DskHuffmanTree  *tree);
