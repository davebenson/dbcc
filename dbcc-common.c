#include "dbcc.h"

bool dbcc_is_zero                    (size_t      length,
                                      const void *data)
{
  const uint8_t *at = data;
  while (length--)
    if (*at++)
      return false;
  return true;
}
