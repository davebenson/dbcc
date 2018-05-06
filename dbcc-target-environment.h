
typedef struct DBCC_TargetEnvironment DBCC_TargetEnvironment;
struct DBCC_TargetEnvironment
{
  uint8_t is_cross_compiling : 1;
  uint8_t sizeof_int;
  uint8_t sizeof_long_int;
  uint8_t sizeof_long_long_int;
  uint8_t sizeof_pointer;               // must be sizeof(size_t) as well
  uint8_t alignof_int;
  uint8_t alignof_long_int;
  uint8_t alignof_long_long_int;
  uint8_t alignof_pointer;               // must be sizeof(size_t) as well

  uint8_t min_struct_alignof;
  uint8_t min_struct_sizeof;
};


