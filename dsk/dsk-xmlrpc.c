  


typedef enum {
  DSK_XMLRPC_INT32,
  DSK_XMLRPC_BOOLEAN
  DSK_XMLRPC_STRING,
  DSK_XMLRPC_DOUBLE,
  DSK_XMLRPC_TIMESTAMP,
  DSK_XMLRPC_BINARY,
  DSK_XMLRPC_STRUCT,
  DSK_XMLRPC_ARRAY
} DskXmlrpcValueType;

typedef struct DskXmlrpcValue DskXmlrpcValue;

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

static DskXml *
dsk_xmlrpc_make_param_xml (const DskXmlrpcValue *param)
{
  switch (param->type)
    {
    }
}

DskXml *dsk_xmlrpc_make_method_request (const char *method_name,
                                        size_t      n_params,
                                        const DskXmlrpcValue **params)
{
  DskXml **param_children = dsk_newa (DskXml *, n_params);
  for (size_t i = 0; i < n_params; i++)
    param_children[i] = dsk_xmlrpc_make_param_xml (params[i]);
  DskXml *params = dsk_xml_new_take_n ("params", n_params, param_children);
  return dsk_xml_new_take_2 ("methodCall",
                             dsk_xml_text_child_new ("methodName", method_name),
                             params);
}


DskXml *dsk_xmlrpc_make_method_response (size_t      n_params,
                                        const DskXmlrpcValue **params)
{
}
DskXml *dsk_xmlrpc_make_fault_response  (int32_t     fault_code,
                                         const char *fault_string)
{
}
