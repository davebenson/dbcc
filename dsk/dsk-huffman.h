

/* Compute a static huffman tree given a pile of weights */
struct DskHuffmanTreeBranchNode
{
  int left, right;
};
struct DskHuffmanTreeValue
{
  uint64_t value;
  uint8_t n_bits;
};
struct DskHuffmanTree
{
  size_t n_nodes;
  DskHuffmanTreeNode *nodes;

  unsigned n_values;
  DskHuffmanTreeValue *values;
};

#define DSK_HUFFMAN_TREE_IS_LEAF(intvalue) \
  ((intvalue) < 0)
#define DSK_HUFFMAN_TREE_LEAF_TO_INDEX(intvalue) \
  (-((intvalue) + 1))


DskHuffmanTree *dsk_huffman_tree_compute     (size_t           n,
                                              const uint64_t  *counts);
DskHuffmanTree *dsk_huffman_tree_load_memory (size_t           len,
                                              const uint8_t   *data,
                                              DskError       **error);
uint8_t        *dsk_huffman_tree_save_memory (DskHuffmanTree  *tree,
                                              size_t          *size_out);
void            dsk_huffman_tree_free        (DskHuffmanTree  *tree);


uint8_t        *dsk_huffman_tree_compress    (DskHuffmanTree  *tree,
                                              unsigned         n_codes,
                                              const uint32_t  *codes,
                                              size_t          *n_bits_out);
uint32_t       *dsk_huffman_tree_decompress  (DskHuffmanTree  *tree,
                                              unsigned         n_bits,
                                              const uint8_t   *comp_data,
                                              size_t          *n_codes_out);

