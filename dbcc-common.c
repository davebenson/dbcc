#include "dbcc.h"
#include <ctype.h>

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

static inline bool
is_alphanum  (char c)
{
  return ('0' <= c && c <= '9')
      || ('a' <= c && c <= 'z')
      || ('A' <= c && c <= 'Z');
}

static inline bool
is_hex_digit  (char c)
{
  return ('0' <= c && c <= '9')
      || ('a' <= c && c <= 'f')
      || ('A' <= c && c <= 'F');
}

bool
dbcc_common_number_parse_int64  (size_t       length,
                                 const char  *str,
                                 int64_t     *val_out,
                                 DBCC_Error **error)
{
  unsigned L;
  for (L = 0; L < length; L++)
    if (!is_alphanum (str[L]) && str[L] != '.')
      break;
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

/* 6.4.5 String literals
 *
 * p2  "A sequence of adjacent string literal tokens shall not include
 *      both a wide string literal and a UTF−8 string literal."
 * p3  "A character string literal is a sequence of zero
 *      or more multibyte characters enclosed in
 *      double-quotes, as in "xyz". A UTF−8 string literal is the same,
 *      except prefixed by u8.  A wide string literal is the same,
 *      except prefixed by the letter L, u, or U."
 * p4  "The same considerations apply to each element of the sequence
 *      in a string literal as if it were in an integer character constant
 *      (for a character or UTF−8 string literal) or a wide
 *      character constant (for a wide string literal), except that
 *      the single-quote ' is representable either by itself or
 *      by the escape sequence \', but the double-quote " shall
 *      be represented by the escape sequence \".    "
 */
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
  bool negate = false;
  if (str[0] == '-')
    {
      str++;
      length--;
      if (length == 0)
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                                   "nothing after minus sign ('-')");
          return true;
        }
      negate = true;
    }
  if (str[0] == '0')
    {
      if (str[1] == 'x')
        {
          str += 2;
          length -= 2;
          unsigned L;
          for (L = 0; L < length; L++)
            if (!is_hex_digit(str[L]))
              break;
          if (L == 0)
            {
              *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                                       "must have exactly one hex digit after 0x");
              return false;
            }
          str += L;
          length -= L;
          goto maybe_handle_suffix;
        }
      else
        {
          // octal (or simply 0)
          unsigned L;
          for (L = 1; L < length; L++)
            if (!('0' <= str[L] && str[L] <= '7'))
              break;
        }
      goto maybe_handle_suffix;
    }
  else if ('1' <= str[0] && str[0] <= '9')
    {
      unsigned L;
      for (L = 1; L < length; L++)
        if (!('0' <= str[L] && str[L] <= '9'))
          break;
      str += L;
      length -= L;
      goto maybe_handle_suffix;
    }
  else
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                               "bad character in number '%c'",
                               str[0]);
      return false;
    }

maybe_handle_suffix:
  if (length > 0 && (*str == 'u' || *str == 'U'))
    {
      *is_signed_out = false;
      length--;
      str++;
    }
  else
    *is_signed_out = true;

  if (length >= 2
   && ((str[0] == 'l' && str[1] == 'l')
    || (str[0] == 'L' && str[1] == 'L')))
    {
      length -= 2;
      str += 2;
      *sizeof_int_type_out = target_env->sizeof_long_long_int;
      return true;
    }
  if (length >= 1
   && (str[0] == 'l' || str[0] == 'L'))
    {
      length -= 2;
      str += 2;
      *sizeof_int_type_out = target_env->sizeof_long_int;
      return true;
    }
  if (length > 0)
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                               "integer constant followed by unexpected character %c", str[0]);
       return false;
    }
  *sizeof_int_type_out = target_env->sizeof_int;
  return true;
}

/* 6.4.4.2 Floating constants.
 *
 * p2  "A floating constant has a significand part that may be followed
 *      by an exponent part and a suffix that specifies its type.
 *      The components of the significand part may include a digit
 *      sequence representing the whole-number part,
 *      followed by a period (.), followed by a
 *      digit sequence representing the fraction part.
 *      The components of the exponent part are an e, E, p, or P followed
 *      by an exponent consisting of an optionally signed digit sequence.
 *      Either the whole-number part or the fraction part has to be present;
 *      for decimal floating constants, either the period or the exponent
 *      part has to be present."
 */
bool
dbcc_common_floating_point_get_info(DBCC_TargetEnvironment *target_env,
                                    size_t          length,
                                    const char     *str,
                                    DBCC_FloatType *float_type_out,
                                    DBCC_Error    **error)
{
#define TEST_END() \
  if (skip == length) { \
    *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT, \
                             "unexpected end-of-number"); \
    return false; \
  }
  // is this hex or decimal floating-point?
  unsigned skip = 0;
  if (str[skip] == '+' || str[skip] == '-')
    skip++;
  TEST_END();
  if (2 + skip < length && str[skip] == '0' && str[skip+1] == 'x')
    {
      skip += 2;
      TEST_END();

      // scan hex chars
      while (skip < length && isxdigit (str[skip]))
        skip++;

      // scan optional exponent p/P (exponent is in decimal).
      if (length > 0 && (str[skip] == 'p' || str[skip] == 'P'))
        {
          skip++;
          TEST_END();
          if (str[skip] == '+' || str[skip] == '-')
            {
              skip++;
              TEST_END();
            }
          while (skip < length && isdigit (str[skip]))
            skip++;
        }
    }
  else if (!isdigit(str[skip]) && str[skip] != '.')
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                               "bad start character '%c' for number",
                               str[skip]);
      return false;
    }
  else
    {
      // skip decimal
      while (skip < length && isdigit (str[skip]))
        skip++;

      // skip optional e/E
      if (length > 0 && (str[skip] == 'e' || str[skip] == 'E'))
        {
          skip++;
          TEST_END();
          if (str[skip] == '+' || str[skip] == '-')
            {
              skip++;
              TEST_END();
            }
          while (skip < length && isdigit (str[skip]))
            skip++;
        }
    }
  if (skip < length)
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_NUMBER_CONSTANT,
                               "garbage after number");
      return false;
    }
  return true;
}
