
typedef struct DBCC_TargetEnvironment DBCC_TargetEnvironment;
struct DBCC_TargetEnvironment
{
  uint8_t is_cross_compiling : 1;
  uint8_t is_char_signed : 1;
  uint8_t is_wchar_signed : 1;
  uint8_t sizeof_int;
  uint8_t sizeof_long_int;
  uint8_t sizeof_long_long_int;
  uint8_t sizeof_pointer;               // must be sizeof(size_t) as well
  uint8_t alignof_int;
  uint8_t alignof_long_int;
  uint8_t alignof_long_long_int;
  uint8_t alignof_pointer;               // must be sizeof(size_t) as well
  uint8_t sizeof_wchar;
  uint8_t alignof_int16;
  uint8_t alignof_int32;
  uint8_t alignof_int64;
  uint8_t alignof_float;
  uint8_t alignof_double;
  uint8_t sizeof_long_double;
  uint8_t alignof_long_double;
  uint8_t sizeof_bool;
  uint8_t alignof_bool;

  uint8_t min_struct_alignof;
  uint8_t min_struct_sizeof;
};


