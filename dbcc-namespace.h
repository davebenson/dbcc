
typedef struct DBCC_NamespaceEntry DBCC_NamespaceEntry;
typedef struct DBCC_Namespace DBCC_Namespace;

typedef enum
{
  DBCC_NAMESPACE_ENTRY_TYPEDEF,
  DBCC_NAMESPACE_ENTRY_ENUM_VALUE,
  DBCC_NAMESPACE_ENTRY_GLOBAL
} DBCC_NamespaceEntryType;

struct DBCC_NamespaceEntry
{
  DBCC_NamespaceEntryType entry_type;
  union {
    DBCC_Type *v_typedef;
    DBCC_EnumValue *v_enum_value;
    struct {
      DBCC_Type *type;
      DBCC_Statement *impl;             /* only for inline functions */
    } v_global;
  };
};

struct DBCC_Namespace
{
  DBCC_SymbolSpace *symbol_space;
  DBCC_PtrTable symbols;                /* typedefs, enum-values, function names, global variables */
  DBCC_PtrTable struct_tag_symbols;
  DBCC_PtrTable enum_tag_symbols;
  DBCC_PtrTable union_tag_symbols;

  DBCC_Namespace *chain;                /* if lookups fail, go to parent */
};

bool
dbcc_namespace_lookup            (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol,
                                  DBCC_NamespaceEntry *out);

DBCC_Type *
dbcc_namespace_lookup_struct_tag (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol);
DBCC_Type *
dbcc_namespace_lookup_union_tag  (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol);
DBCC_Type *
dbcc_namespace_lookup_enum_tag   (DBCC_Namespace      *ns,
                                  DBCC_Symbol         *symbol);

