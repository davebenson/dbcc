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

bool
dbcc_common_char_constant_value (DBCC_TargetEnvironment *target_env,
                                 size_t length,
                                 const char *str,
                                 uint32_t   *codepoint_out,
                                 size_t     *sizeof_char,
                                 DBCC_Error **error)
{
  if (length < 3)
    {
      *error = dbcc_error_new (DBCC_ERROR_CHARACTER_CONSTANT_TOO_SHORT,
                               "character constant too short");
      return false;
    }
  if (*str == 'L')
    *sizeof_char = target_env->sizeof_wchar;
  else if (*str == 'u')
    *sizeof_char = 2;
  else if (*str == 'U')
    *sizeof_char = 4;
  else
    {
      if (*str != '\'')
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_CHARACTER_SEQUENCE,
                                   "character constant start with bad char '%c'",
                                   *str);
          return false;
        }
      *sizeof_char = 1;
    }

  if (*str == '\'')
    {
      str++;
      length--;
    }
  else
    {
      str += 2;
      length -= 2;
    }
  if (*str == '\\')
    {
      // \a \b \f \n \r \t \v
      // \' \" \? \\
      // \OOO \xHHH...
      str++;
      length--;
      assert(length > 0);
      switch (*str)
        {
        case 'a': *codepoint_out = '\a'; str++; length--; break;
        case 'b': *codepoint_out = '\b'; str++; length--; break;
        case 'f': *codepoint_out = '\f'; str++; length--; break;
        case 'n': *codepoint_out = '\n'; str++; length--; break;
        case 'r': *codepoint_out = '\r'; str++; length--; break;
        case 't': *codepoint_out = '\t'; str++; length--; break;
        case 'v': *codepoint_out = '\v'; str++; length--; break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          {
            unsigned max_octal_length = *str <= '3' ? 3 : 2;
            unsigned octal_length = 1;
            unsigned cp = *str - '0';
            str++; length--;
            while (octal_length < max_octal_length
                && octal_length < length)
              {
                if ('0' <= *str && *str <= '7')
                  {
                    cp <<= 3;
                    cp |= (*str - '0');
                    octal_length++;
                    str++;
                    length--;
                    if (length == 0) 
                      {
                        *error = dbcc_error_new (DBCC_ERROR_CHARACTER_CONSTANT_TOO_SHORT,
                                                 "missing ' after \\-octal sequence");
                        return false;
                      }
                  }
                else if (*str != '\'')
                  {
                    *error = dbcc_error_new (DBCC_ERROR_BAD_CHARACTER_SEQUENCE,
                                             "missing ' after \\-octal sequence");
                    return false;
                  }
                else
                  {
                    *codepoint_out = cp;
                    break;
                  }
              }
            break;
          }
        case 'u':
        case 'U':
          {
            char type = *str;
            str++;
            length--;
            if (length == 0)
              {
                *error = dbcc_error_new (DBCC_ERROR_CHARACTER_CONSTANT_TOO_SHORT,
                                         "missing ' after \\u-hex sequence");
                return false;
              }
            const int max_hex_length = type == 'u' ? 4 : 8;
            int cur_hex_length = 0;
            unsigned cp = 0;
            while (length > 0 && cur_hex_length < max_hex_length)
              {
                cp <<= 4;
                if ('0' <= *str && *str <= '9')
                  cp += (*str - '0');
                else if ('a' <= *str && *str <= 'f')
                  cp += (*str - 'a' + 10);
                else if ('A' <= *str && *str <= 'F')
                  cp += (*str - 'A' + 10);
                else if (*str == '\'')
                  break;
                else
                  {
                    *error = dbcc_error_new (DBCC_ERROR_BAD_CHARACTER_SEQUENCE,
                                             "unexpected character in hex-sequence");
                    return false;
                  }
              }
            if (cur_hex_length == 0)
              {
                *error = dbcc_error_new (DBCC_ERROR_BAD_CHARACTER_SEQUENCE,
                                         "need hex char after \\%c", type);
                return false;
              }
            *codepoint_out = cp;
            break;
          }
        }
      if (length == 0)
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_CHARACTER_SEQUENCE,
                                   "missing close ' in character constant");
          return false;
        }
      if (*str != '\'')
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_CHARACTER_SEQUENCE,
                                   "bad character in character constant");
          return false;
        }
    }
  else
    {
      /* literal character (may be UTF-8) */
      if ((*str & 0x80) == 0)
        {
          // ascii
          *codepoint_out = (uint8_t) *str;
        }
      else
        {
          // utf8
          unsigned n_used;
          if (!dsk_utf8_decode_unichar (length, str, &n_used, codepoint_out))
            { 
              *error = dbcc_error_new (DBCC_ERROR_BAD_UTF8,
                                       "invalid UTF-8 in character constant");
              return false;
            }
          str += n_used;
          length -= n_used;
        }
    }
  return true;
}

bool
dbcc_common_number_is_integral  (size_t       length,
                                 const char  *str)
{
  while (length--)
    {
      if (*str < '0' || *str > '9')
        return false;
      str++;
    }
  return true;
}

bool
dbcc_common_number_parse_int64  (size_t       length,
                                 const char  *str,
                                 int64_t     *val_out,
                                 DBCC_Error **error)
{
  unsigned L;
  for (L = 0; L < length; L++)
    {
      if (!('0' <= str[L] && str[L] <= '9')
       && !('a' <= str[L] && str[L] <= 'z')
       && !('A' <= str[L] && str[L] <= 'Z')
       && (str[L] != '.')
         )
        break;
    }
  char *val = alloca (L + 1);
  memcpy (val, str, L);
  val[L] = 0;
  char *end;
  *val_out = strtoull (val, &end, 0);
  if (*end != 0)
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                               "integer constant not parsed correctly");
      return false;
    }
  return true;
}

#if 0
bool
dbcc_common_string_literal_value (size_t       length,
                                  const char  *str,
                                  DBCC_String *out,
                                  DBCC_Error **error)
{
  ...
}

bool
dbcc_common_integer_get_info    (DBCC_TargetEnvironment *target_env,
                                 size_t                  length,
                                 const char             *str,
                                 size_t                 *sizeof_int_type_out,
                                 bool                   *is_signed_out,
                                 DBCC_Error            **error)
{
  ...
}

bool
dbcc_common_floating_point_get_info(DBCC_TargetEnvironment *target_env,
                                    size_t          length,
                                    const char     *str,
                                    DBCC_FloatType *float_type_out,
                                    DBCC_Error    **error)
{
...
}
#endif
