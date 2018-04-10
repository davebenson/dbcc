
struct DBCC_LocalNamespace
{
  DBCC_LocalNamespace *up;
  DBCC_LocalNamespaceEntry *treetop;
};

DBCC_LocalNamespace *dbcc_local_namespace_new (DBCC_LocalNamespace *up);

// returns parent namespace
void dbcc_local_namespace_destroy (DBCC_LocalNamespace *);

struct DBCC_Context
{
  // global variables and all symbols (even local variables)
  DBCC_SymbolTable *symbol_table;

  // only if parsing a function
  DBCC_LocalNamespace *local_namespace;
};
