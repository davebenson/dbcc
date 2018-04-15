

typedef enum
{
  DSK_HA_FS_MESSAGE_CODE_LIST_FILES,
  DSK_HA_FS_MESSAGE_CODE_ADD_FILE_ATTRIBUTES,
  DSK_HA_FS_MESSAGE_CODE_ADD_ATTRIBUTES,
  DSK_HA_FS_MESSAGE_CODE_BEGIN_UPLOAD,
  DSK_HA_FS_MESSAGE_CODE_BEGIN_DOWNLOAD,
  DSK_HA_FS_MESSAGE_CODE_UPLOAD_DATA,
  DSK_HA_FS_MESSAGE_CODE_DOWNLOAD_DATA,
  DSK_HA_FS_MESSAGE_CODE_UPLOAD_END,
  DSK_HA_FS_MESSAGE_CODE_DOWNLOAD_END,
} Dsk_HA_FS_MessageCode;

typedef enum
{
  DSK_HA_FS_BODY_TYPE_NONE,             // length must be 0
  DSK_HA_FS_BODY_TYPE_BINARY,           // always used for file transfers
  DSK_HA_FS_BODY_TYPE_JSON,             // body is a single JSON Value

  DSK_HA_FS_BODY_TYPE_FILE_ENTRIES,
} Dsk_HA_FS_BodyType;
typedef enum
{
  DSK_HA_FS_MESSAGE_COMPRESSION_NONE,   // compression not supported
} Dsk_HA_FS_MessageCompression;
typedef struct Dsk_HA_FS_MessageHeader
{
  uint32_t message_code;
  uint32_t body_length;
  uint64_t id;     // request_id -- even if is_response; or 0 for one-shots
  uint8_t body_type;
  uint8_t body_compression;
  uint8_t is_response;
  uint8_t reserved_2;
} Dsk_HA_FS_Header;
#define DSK_HA_FS_MESSAGE_HEADER_LENGTH 20

bool dsk_ha_fs_message_header_parse_buffer (DskBuffer *in,
                                            Dsk_HA_FS_Header *out,
                                            DskError **error);
bool dsk_ha_fs_message_header_parse        (unsigned length,
                                            const uint8_t *data,
                                            Dsk_HA_FS_MessageHeader *out,
                                            DskError **error);
void dsk_ha_fs_message_header_pack         (const Dsk_HA_FS_MessageHeader *in,
                                            uint8_t *out);

