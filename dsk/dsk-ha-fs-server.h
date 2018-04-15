
typedef struct _DskHAFSServer DskHAFSServer;


struct _DskHAFSServerConfigImmutable
{
  unsigned ref_count;

  ... masters
  ... mount points
};

struct _DskHAFSServerConfig
{
  DskObject base_instance;
  DskHAFSServerConfigImmutable *c;
};

DskHAFSServerConfig *dsk_ha_fs_server_config_new        (void);
DskHAFSServerConfig *dsk_ha_fs_server_config_clone      (DskHAFSServerConfig *config);
void                 dsk_ha_fs_server_config_add_master (DskHAFSServerConfig *config,
                                                         DskIpAddress        *address,
                                                         unsigned             port);
void                 dsk_ha_fs_server_config_mount_local(DskHAFSServerConfig *config,
                                                         const char          *mount_point,
                                                         const char          *local_fs_dir);

struct _DskHAFSServerMasterState
{
  STATE_CONNECTING,
  STATE_CONNECTED,

struct _DskHAFSServerMaster
{
  DskIpAddress  address;
  unsigned      port;
};

struct _DskHAFSServer
{
  DskObject base_instance;
  DskHAFSServerConfigImmutable *config;
  ... bind info
};

DskHAFSServer *dsk_ha_fs_server_new   (DskHAFSServerConfig *config);
dsk_boolean    dsk_ha_fs_server_bind  (DskHAFSServer       *server,
                                       DskIpAddress        *address,
                                       unsigned             port);




/* Retrieving statistics about this file-server. */
typedef struct {
  ...
} DskHAFSServerStatsFilter;

DskHAFSServerStats *dsk_ha_fs_server_get_stats (DskHAFSServerStatsFilter *opt_filter);
void    dsk_ha_fs_server_stats_free (DskHAFSServerStats *);
