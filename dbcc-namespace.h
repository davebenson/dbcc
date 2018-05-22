
typedef struct DBCC_NamespaceEntry DBCC_NamespaceEntry;
typedef struct DBCC_NamespaceScope DBCC_NamespaceScope;

typedef struct DBCC_Address_Offset DBCC_Address_Offset;
typedef struct DBCC_Address_Base DBCC_Address_Base;

typedef enum
{
  DBCC_NAMESPACE_ENTRY_TYPEDEF,
  DBCC_NAMESPACE_ENTRY_ENUM_VALUE,
  DBCC_NAMESPACE_ENTRY_GLOBAL,
  DBCC_NAMESPACE_ENTRY_LOCAL
} DBCC_NamespaceEntryType;

typedef enum
{
  DBCC_ADDRESS_TYPE_EXTERNAL,
  DBCC_ADDRESS_TYPE_GLOBAL,
  DBCC_ADDRESS_TYPE_CONSTANT,
  DBCC_ADDRESS_TYPE_OFFSET,
} DBCC_Address_Type;
struct DBCC_Address_Base
{
  DBCC_Address_Type type;
};

struct DBCC_Address_Offset
{
  DBCC_Address_Base address_base;
  DBCC_Address *underlying_address;
  int64_t delta;
};

  
struct DBCC_Global
{
  DBCC_Address_Base address_base;
  DBCC_Namespace *global_ns;
  DBCC_Type *type;
  DBCC_Symbol *name;

  // note: this is into the pointer into the code (text) segment.
  // only if 'generated' is true.
  //
  // This is for GLOBAL and CONSTANT addresses.
  size_t address_offset;
};

struct DBCC_Local
{
  DBCC_Namespace *local_ns;
  DBCC_Type *type;
  DBCC_Symbol *name;
  DBCC_NamespaceScope *scope;
};

struct DBCC_NamespaceEntry
{
  DBCC_NamespaceEntryType entry_type;
  union {
    DBCC_Type *v_typedef;
    struct {
      DBCC_Type *enum_type;
      DBCC_EnumValue *enum_value;
    } v_enum_value;
    DBCC_Global *v_global;
    DBCC_Local *v_local;
  };
};

struct DBCC_Namespace
{
  DBCC_SymbolSpace *symbol_space;
  DBCC_PtrTable symbols;                /* typedefs, enum-values, function names, global variables */
  DBCC_PtrTable struct_tag_symbols;
  DBCC_PtrTable enum_tag_symbols;
  DBCC_PtrTable union_tag_symbols;

  DBCC_TargetEnvironment *target_env;

  DBCC_Namespace *chain;                /* if lookups fail, go to parent */
};

DBCC_Namespace *
dbcc_namespace_new_global        (void);

DBCC_Namespace *
dbcc_namespace_new_scope         (void);

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
void dbcc_namespace_add_by_tag   (DBCC_Namespace      *ns,
                                  DBCC_Type           *type);   // must be struct, union or enum with tag



void dbcc_namespace_add_enum_value (DBCC_Namespace *ns,
                                    DBCC_EnumValue *enum_value);

/* Complex Types. TODO: spec ref */
DBCC_Type * dbcc_namespace_get_float_complex_type     (DBCC_Namespace *ns);
DBCC_Type * dbcc_namespace_get_double_complex_type    (DBCC_Namespace *ns);
DBCC_Type * dbcc_namespace_get_long_double_complex_type (DBCC_Namespace *ns);
DBCC_Type * dbcc_namespace_get_float_imaginary_type   (DBCC_Namespace *ns);
DBCC_Type * dbcc_namespace_get_double_imaginary_type  (DBCC_Namespace *ns);
DBCC_Type * dbcc_namespace_get_long_double_imaginary_type (DBCC_Namespace *ns);

/* Global namespace functions to create DBCC_Addresses from
 * constant data etc.
 */
DBCC_Address *dbcc_namespace_global_allocate_constant (DBCC_Namespace *ns,
                                                       size_t length,
                                                       const uint8_t *data);
DBCC_Address *dbcc_namespace_global_allocate_constant0(DBCC_Namespace *ns,
                                                       size_t length,
                                                       const uint8_t *data);

/* Fixed-width types. */
DBCC_Type * dbcc_namespace_get_integer_type           (DBCC_Namespace  *ns,
                                                       bool is_signed,
                                                       unsigned size);

/* ... */
DBCC_Type * dbcc_namespace_get_char_type              (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_float_type             (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_double_type            (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_long_double_type       (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_long_long_type         (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_long_type              (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_short_type             (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_signed_char_type       (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_unsigned_int_type      (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_unsigned_long_long_type(DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_unsigned_long_type     (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_unsigned_short_type    (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_unsigned_short_type    (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_int_type               (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_size_type              (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_ssize_type             (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_ptrdiff_type           (DBCC_Namespace *);

DBCC_Type * dbcc_namespace_get_imaginary_float_type   (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_imaginary_double_type  (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_imaginary_long_double_type(DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_complex_float_type     (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_complex_double_type    (DBCC_Namespace *);
DBCC_Type * dbcc_namespace_get_complex_long_double_type(DBCC_Namespace *);

// '\0', 'L', 'u', 'U'
DBCC_Type * dbcc_namespace_get_constant_char_type     (char  prefix);

DBCC_Type * dbcc_namespace_get_floating_point_type    (DBCC_Namespace *ns,
                                                       DBCC_FloatType  ftype);
