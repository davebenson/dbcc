


struct _DskHttpHeaderEntryValueList
{
  unsigned value_length;
  char *value;
  DskHttpHeaderEntry *next_value;    // with same key
};

struct _DskHttpHeaderEntry
{
  unsigned key_length;
  char *key;
  char *_lowercase_key;
  unsigned value_length;
  char *value;
  DskHttpHeaderEntryValueList *next_value;
  DskHttpHeaderEntry *_next_in_bucket;
};

struct _DskHttpHeaderClass
{
  DskObjectClass base_class;
};

struct _DskHttpHeader
{
  DskObject base_object;
  char *header_line;

  DskHttpHeaderValue *header_value;


  ///size_t _n_buckets;
  ///DskHttpHeaderEntry **_buckets;
};



DskHttpHeader *dsk_http_header_new (const char *header_line,
                                    size_t      n_headers,
                                    char      **key_value_pairs);
DskHttpHeader *dsk_http_header_new_from_header (DskHttpHeader *init,
                                    size_t      n_mods,
                                    DskHttpHeaderModication mod[]);

struct _DskHttpFramingWriterStream
{
  DskBuffer buffer;
};
void dsk_http_framing_writer_stream_modified_buffer(DskHttpFramingWriterStream *stream);
void dsk_http_framing_writer_stream_close (DskHttpFramingWriterStream *stream);

struct _DskHttpFramingWriter
{
  DskHttpFramingProtocol protocol;
  DskBuffer *target;

  /*< private >*/
  DskHttpFramingWriterStream *(*create_output_stream) (DskHttpFramingWriter *writer,
                                                       DskHttpHeader        *request,
                                                       DskBuffer            *body,
                                                       dsk_boolean           body_complete);
  void                        (*output_stream_modified_buffer)  (DskHttpFramingWriter *writer,
                                                       DskHttpFramingWriterStream *stream);
  void                        (*output_stream_close)  (DskHttpFramingWriter *writer,
                                                       DskHttpFramingWriterStream *stream);
};

DskHttpFramingWriter       *dsk_http_framing_writer_create_v1_0 (void);
DskHttpFramingWriter       *dsk_http_framing_writer_create_v1_1 (void);
DskHttpFramingWriter       *dsk_http_framing_writer_create_v2 (void);


  
DskHttpFramingWriterStream *dsk_http_framing_writer_create_stream
                                                      (DskHttpFramingWriter *writer,
                                                       DskHttpHeader        *request,
                                                       DskBuffer            *body,
                                                       dsk_boolean           body_complete);
