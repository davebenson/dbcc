
typedef struct {
  DskObjectClass base_class;
} Dsk_HA_FS_DataNode_Class;


typedef struct {
  DskObject base_instance;
  int port;

  char *work_dir;
  int work_dir_fd;
  size_t id_len;
  const uin8t_ id_len;

  DskClientStream *master_stream;
  DskOctetSink *master_sink;
  DskOctetSource *master_source;
  DskBuffer master_outgoing;
  DskBuffer master_incoming;
} Dsk_HA_FS_DataNode;

static Dsk_HA_FS_DataNode_Class dsk_ha_fs_data_node_class = {
  DSK_OBJECT_CLASS_DEFINE(
    Dsk_HA_FS_DataNode,
    &dsk_object_class, 
    NULL,
    dsk_ha_fs_data_node_finalize
  )
};

static void
message_to_buffer (uint32_t code, const DskJsonValue *value)
{
  DskBufferPlaceholder pl;
  dsk_buffer_append_placeholder (output, 8, &pl);
  size_t pre_size = out->size;
  dsk_json_value_to_buffer (value, out);
  size_t json_size = out->size - pre_size;
  uint8_t header[8];
  dsk_uint32le_pack (code, header + 0);
  dsk_uint32le_pack (json_size, header + 4);
  dsk_buffer_placeholder_set (&pl, 8, header);
} 

static void
master_send_handshake (Dsk_HA_FS_DataNode *data_node)
{
  assert(data_node->master_outgoing.size == 0);
  dsk_buffer_append (&data_node->master_outgoing, ...);
  trap_master_writable (data_node);
}

static dsk_boolean
dispatch_master_command (Dsk_HA_FS_DataNode *node,
                         uint32_t            code,
                         const DskJsonValue *value)
{
...
}

struct dsk_boolean
master_stream_is_readable (DskOctetSource *source,
                           Dsk_HA_FS_DataNode *node)
{
  dsk_octet_source_read_buffer (source, &node->master_incoming, &error);
  if (error != NULL)
    {
      master_disconnect (node, error);
      return true;
    }
  while (node->master_incoming.size >= 8)
    {
      uint8_t header[8];
      dsk_buffer_peek (&node->master_incoming, 8, header);
      uint32_t code, length;

      // parse header
      dsk_uint32le_parse (header + 0, &code);
      dsk_uint32le_parse (header + 4, &length);

      // stop if not enough data
      if (node->master_incoming.size < 8 + length)
        return true;

      dsk_buffer_discard (&node->master_incoming, 8);
      if (!parse_json (&node->master_incoming, length, &json_value, &error))
        {
          master_disconnect (node, error);
          return true;
        }
      dispatch_master_command (node, code, json_value);
    }
  return true;
}

Dsk_HA_FS_DataNode *
dsk_ha_fs_data_node_new (Dsk_HA_FS_DataNode_Options *options,
                         DskError **error)
{
  // Load ID from directory, or create ID.
  

  Dsk_HA_FS_DataNode *rv = dsk_object_new (&dsk_ha_fs_data_node_class);
  DskClientStreamOptions cs_options = DSK_CLIENT_STREAM_OPTIONS_INIT;
  cs_options.hostname = options->master_name;
  cs_options.port = options->master_port;
  cs_options.reconnect_time = 500;
  if (!dsk_client_stream_new (&cs_options,
                              &rv->master_stream, 
                              &rv->master_sink, 
                              &rv->master_source, 
                              error))
    {
      dsk_object_unref (rv);
      return NULL;
    }
  dsk_hook_trap (&rv->master_source->connect_hook, handle_master_connected, rv);
  dsk_hook_trap (&rv->master_source->disconnect_hook, handle_master_disconnected, rv);
  if (rv->master_stream->is_connected)
    {
      master_send_handshake (rv);
    }
  return rv;
}
