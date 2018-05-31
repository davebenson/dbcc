#include "dbcc.h"

DBCC_Constant *
dbcc_constant_new_value (DBCC_Type *type,
                         const void *optional_value)
{
  DBCC_Constant *c = malloc (sizeof (DBCC_Constant));
  c->constant_type = DBCC_CONSTANT_TYPE_VALUE;
  size_t s = type->base.sizeof_instance;
  c->v_value.data = malloc (s);
  if (optional_value != NULL)
    memcpy (c->v_value.data, optional_value, s);
  else
    memset (c->v_value.data, 0, s);
  return c;
}

DBCC_Constant *
dbcc_constant_new_value0(DBCC_Type *type)
{
  return dbcc_constant_new_value (type, NULL);
}

DBCC_Constant *
dbcc_constant_copy      (DBCC_Type *type, 
                         DBCC_Constant *to_copy)
{
  DBCC_Constant *rv = malloc (sizeof (DBCC_Constant));
  rv->constant_type = to_copy->constant_type;
  switch (rv->constant_type)
    {
    case DBCC_CONSTANT_TYPE_VALUE:
      rv->v_value.data = malloc (type->base.sizeof_instance);
      memcpy (rv->v_value.data,
              to_copy->v_value.data,
              type->base.sizeof_instance);
      break;
    case DBCC_CONSTANT_TYPE_LINK_ADDRESS:
      rv->v_link_address.name = to_copy->v_link_address.name;
      break;
    case DBCC_CONSTANT_TYPE_UNIT_ADDRESS:
      rv->v_unit_address.name = to_copy->v_unit_address.name;
      rv->v_unit_address.address = to_copy->v_unit_address.address;
      break;
    case DBCC_CONSTANT_TYPE_LOCAL_ADDRESS:
      break;
    case DBCC_CONSTANT_TYPE_OFFSET:
      rv->v_offset.base = dbcc_constant_copy (type, to_copy->v_offset.base);
      rv->v_offset.offset = to_copy->v_offset.offset;
      break;
    }
  return rv;
}

void
dbcc_constant_free     (DBCC_Type *type, 
                        DBCC_Constant *to_free)
{
  switch (to_free->constant_type)
    {
    case DBCC_CONSTANT_TYPE_VALUE:
      free (to_free->v_value.data);
      break;
    case DBCC_CONSTANT_TYPE_LINK_ADDRESS:
      //dbcc_symbol_unref (to_copy->v_link_address.name);
      break;
    case DBCC_CONSTANT_TYPE_UNIT_ADDRESS:
      //dbcc_symbol_unref (to_copy->v_unit_address.name);
      break;
    case DBCC_CONSTANT_TYPE_LOCAL_ADDRESS:
      break;
    case DBCC_CONSTANT_TYPE_OFFSET:
      dbcc_constant_free (type, to_free->v_offset.base);
      break;
    }
  free(to_free);
}
