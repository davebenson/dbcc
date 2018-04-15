

typedef struct _DskCritbit DskCritbit;

DskCritbit *dsk_critbit_new    (void);
void        dsk_critbit_destroy(DskCritbit    *critbit);
dsk_boolean dsk_critbit_set    (DskCritbit    *critbit,
                                size_t         len,
                                const uint8_t *data,
                                void          *value);
dsk_boolean dsk_critbit_get    (DskCritbit    *critbit,
                                size_t         len,
                                const uint8_t *data,
                                void         **value_out);
dsk_boolean dsk_critbit_remove (DskCritbit    *critbit,
                                size_t         len,
                                const uint8_t *data);



typedef enum {
  DSK_CRITBIT_ACCESS_DELETE_IF_EXISTS = (1<<0),
  DSK_CRITBIT_ACCESS_CREATE_IF_NOT_EXISTS = (1<<1),
} DskCritbitAccessFlags;
dsk_boolean dsk_critbit_access (DskCritbit            *critbit,
                                size_t                 len,
                                const uint8_t         *data,
                                void                ***pointer_to_value_out,
                                DskCritbitAccessFlags  access_flags);
