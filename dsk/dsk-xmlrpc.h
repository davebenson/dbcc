

typedef enum {
  DSK_XMLRPC_INT32,
  DSK_XMLRPC_BOOLEAN,
  DSK_XMLRPC_STRING,
  DSK_XMLRPC_DOUBLE,
  DSK_XMLRPC_TIMESTAMP,
  DSK_XMLRPC_BINARY,
  DSK_XMLRPC_STRUCT,
  DSK_XMLRPC_ARRAY
} DskXmlrpcValueType;

typedef struct DskXmlrpcValue DskXmlrpcValue;
typedef struct DskXmlrpcMember DskXmlrpcMember;

struct DskXmlrpcMember {
  const char *name;
  DskXmlrpcValue *value;
};


struct DskXmlrpcValue {
  DskXmlrpcValueType type;
  union {
    int32_t v_int32;
    dsk_boolean v_boolean;
    char *v_string;
    double v_double;
    struct { uint32_t nanoseconds; uint64_t seconds; } v_timestamp;
    struct {
      size_t n_members;
      DskXmlrpcMember *members;
    } v_struct;
    struct {
      size_t n_values;
      DskXmlrpcValue **values;
    } v_array;
  };
};

DskXml *dsk_xmlrpc_make_method_request (const char *method_name,
                                        size_t      n_params,
                                        const DskXmlrpcValue **params);
DskXml *dsk_xmlrpc_make_method_response (size_t      n_params,
                                        const DskXmlrpcValue **params);
DskXml *dsk_xmlrpc_make_fault_response  (int32_t     fault_code,
                                         const char *fault_string);

// --- parsing XMLRPC ---
typedef struct {
  const char *method;
  size_t n_params;
  DskXmlrpcValue **params;
} DskXmlrpcMethodRequestInfo;
dsk_boolean dsk_xmlrpc_parse_method_request (DskXml *xml, DskXmlrpcMethodRequestInfo *out, DskError **error);
void dsk_xmlrpc_method_request_info_clear (DskXmlrpcMethodRequestInfo *to_clear);


typedef struct {
  dsk_boolean faulted;

  // only for the !faulted case.
  size_t n_params;
  DskXmlrpcValue **params;

  // faulted case
  int32_t fault_code;
  const char *fault_message;
} DskXmlrpcMethodResponseInfo;

dsk_boolean dsk_xmlrpc_parse_method_response  (DskXml *xml, DskXmlrpcMethodResponseInfo *out, DskError **error);
void dsk_xmlrpc_method_response_info_clear (DskXmlrpcMethodResponseInfo *info);

