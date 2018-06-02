

bool dbcc_is_zero                    (size_t      length,
                                      const void *data);

bool dbcc_common_char_constant_value (DBCC_TargetEnvironment *env,
                                      size_t length,
                                      const char *str,
                                      uint32_t   *codepoint_out,
                                      size_t     *sizeof_char,
                                      DBCC_Error **error);

bool dbcc_common_number_is_integral  (size_t       length,
                                      const char  *str);
bool dbcc_common_number_parse_int64  (size_t       length,
                                      const char  *str,
                                      int64_t     *val_out,
                                      DBCC_Error **error);

bool dbcc_common_string_literal_value (size_t       length,
                                       const char  *str,
                                       DBCC_String *out,
                                       DBCC_Error **error);

bool dbcc_common_integer_get_info    (DBCC_TargetEnvironment *target_env,
                                      size_t       length,
                                      const char  *str,
                                      size_t      *sizeof_int_type_out,
                                      bool        *is_signed_out,
                                      DBCC_Error **error);

bool dbcc_common_floating_point_get_info(DBCC_TargetEnvironment *target_env,
                                         size_t       length,
                                         const char  *str,
                                         DBCC_FloatType *float_type_out,
                                         DBCC_Error **error);
