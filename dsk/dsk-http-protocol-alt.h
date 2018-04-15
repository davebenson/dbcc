
typedef enum
{
  DSK_HTTP_HEADER_KEY_ID_UNKNOWN,
} DskHttpHeaderKeyID;

#define DSK_HTTP_HEADER_VALUE_FLAGS_GET_DATA_FLAG(value) ((flags) & 0xff00)
enum DskHttpHeaderValueFlag {
  DSK_HTTP_HEADER_VALUE_FLAG_DATA_NONE = (0<<8),
  DSK_HTTP_HEADER_VALUE_FLAG_DATA_DATE = (1<<8),
};

struct _DskHttpHeaderEntry {
  uint16_t key_id;
  uint16_t flags;
  uint32_t ref_count;
  char *key;
  char *value;
  char **multivalue;
  union {
    DskDate *v_date;
  } data;
};
#define DSK_HTTP_HEADER_VALUE_GET_DATA_FLAG(value) \
        DSK_HTTP_HEADER_VALUE_FLAGS_GET_DATA_FLAG((value)->flags)

struct _DskHttpHeaderNode {
  uint32_t ref_count : 31;
  uint32_t is_red : 1;
  DskHttpHeaderEntry *entry;
};
struct _DskHttpHeader {
  uint32_t is_request : 1;
  DskHttpHeaderNode *known_header_tree;
  DskHttpHeaderNode *unknown_header_tree;
};
