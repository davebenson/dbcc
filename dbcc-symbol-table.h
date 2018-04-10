

struct DBCC_SymbolTableEntry {
  DBCC_Symbol *key;
};
struct DBCC_SymbolTable {
  DBCC_SymbolSpace *space;
  size_t entry_value_size;
  DBCC_SymbolTableEntry *top;

  void (*destroy_entry)(void *entry_value, void *destroy_data);
  void *destroy_data;
};
