


typedef struct {
  uint64_t max_files;
  uint64_t max_bytes;
  const char *data_dir;
  int port;

  const char *master_name;
  int master_port;
} Dsk_HA_FS_DataNode_Options;


Dsk_HA_FS_DataNode *dsk_ha_fs_data_node_new (Dsk_HA_FS_DataNode_Options *options);
