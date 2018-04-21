/* This file handles Translation Phases 1-7,
 * leaving part of Translation Phase 8 for
 * the Lemon-Parser; in fact,
 * Translation Phase 8 is where things actually get interesting.
 *
 * See the C11 Spec 5.1.1.2, but here's a
 * brief descriptions of the phases:
 *     1.  Character set handling, newline normalizing, and trigraph sequences.
 *     2.  backslash-newline:  this is handled as a line continuation character.
 *     3.  whitespace normalization and comments.
 *     4.  preprocessing directives, _Pragma expressions, #include.
 *     5.  conversion from source character set to target character set
 *         for character and string literals.
 *         we skip this step, and assume both are ascii/utf8.
 *     6.  adjacent string literals are concatenated.
 *     7.  whitespace tokens are dropped.
 *
 * The last phase, phase 8, is:
 *     8.  all external references are resolved and output is collected into a program image.
 *
 * Optimization:
 *     After Step 3, we nose around and try to determine if the entire file is wrapped
 *     in a #if/ifdef  .... #endif   tag.  If so, we cache the preprocessor expression,
 *     and will skip the file if it passes next time around.
 *
 *     This is poor-man's module support, from a performance perspective.
 *
 * References:
 *   [C11]  Specification http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf
 *
 * For the most part, Section 6.10 "Preprocessing directives" is the
 * reference for all this.
 */


#include "dbcc.h"
#include "dbcc-dsk.h"
#include "dbcc-parser.h"
#include "dbcc-parser-lemon.h"
#include "p-token.h"
#include "dsk/dsk-rbtree-macros.h"
#include <ctype.h>
#include <stdio.h>

#define DBCC_PARSER_MAGIC 0xe2315aaf

/* I have added stubs where trigraph support
 * would need to be added, but I haven't bothered to do
 * so.  Because trigraphs are handled first they are a bit pervasive
 * in this file (they have to be handled in preprocessing directives,
 * operators, quoted strings and characters, etc.)
 */
#define SUPPORT_TRIGRAPHS 0

typedef enum
{
  CPP_TOKEN_HASH,
  CPP_TOKEN_STRING,
  CPP_TOKEN_CHAR,
  CPP_TOKEN_NUMBER,
  CPP_TOKEN_OPERATOR,
  CPP_TOKEN_OPERATOR_DIGRAPH,
  CPP_TOKEN_BAREWORD,
  CPP_TOKEN_NEWLINE,
  CPP_TOKEN_CONCATENATE,              /* ## */

  // only found in macros
  CPP_TOKEN_MACRO_ARGUMENT
} CPP_TokenType;

#define DUMP_CPP_TOKENS 1


#if DUMP_CPP_TOKENS
static const char *cpp_token_type_name (CPP_TokenType type)
{
  switch (type)
    {
    case CPP_TOKEN_HASH:             return "hash (#)";
    case CPP_TOKEN_STRING:           return "string-literal";
    case CPP_TOKEN_CHAR:             return "character-constant";
    case CPP_TOKEN_NUMBER:           return "number-constant";
    case CPP_TOKEN_OPERATOR:         return "operator";
    case CPP_TOKEN_OPERATOR_DIGRAPH: return "digraph-operator";
    case CPP_TOKEN_BAREWORD:         return "bareword";
    case CPP_TOKEN_NEWLINE:          return "newline";
    case CPP_TOKEN_CONCATENATE:      return "concat (##)";
    case CPP_TOKEN_MACRO_ARGUMENT:   return "macro-arg";
    default:  return "*unknown-cpp-token-type*";
    }
}
#endif

typedef struct CPP_Token CPP_Token;
typedef struct CPP_Macro CPP_Macro;

struct CPP_Token
{
  CPP_TokenType type;
  DBCC_CodePosition *code_position;
  const char *str;
  unsigned length;

  /* Meaning:
     for MACRO_ARGUMENT : index of the macro-argument
     for STRING : whether "str" has been unquoted.
                  (hack used to implement stringification)
   */
  int alt_int_value;
};
#define CPP_TOKEN(typeshort, cp, str, length) \
 ((CPP_Token) { CPP_TOKEN_##typeshort, (cp), (str), (length), (0) })


typedef struct CPP_TokenArray CPP_TokenArray;
struct CPP_TokenArray
{
  unsigned n;
  CPP_Token *tokens;
  unsigned tokens_alloced;
};
#define CPP_TOKEN_ARRAY_INIT {0,NULL,0}

static void
cpp_token_array_append (CPP_TokenArray *arr, CPP_Token *token)
{
  if (arr->n == arr->tokens_alloced)
    {
      if (arr->n == 0)
        {
          arr->tokens_alloced = 8;
          arr->tokens = malloc (sizeof (CPP_Token) * arr->tokens_alloced);
        }
      else
        {
          arr->tokens_alloced *= 2;
          arr->tokens = realloc (arr->tokens, sizeof (CPP_Token) * arr->tokens_alloced);
        }
    }
  arr->tokens[arr->n++] = *token;
}

struct CPP_Macro
{
  DBCC_Symbol *name;
  bool function_macro;
  unsigned arity;
  bool has_ellipsis;
  DBCC_Symbol **args;
  unsigned n_tokens;
  CPP_Token *tokens;
  bool is_expanding;

  CPP_Macro *left, *right, *parent;
  bool is_red;
};

typedef struct CPP_Expr CPP_Expr;
typedef enum
{
  CPP_EXPR_IF,
  CPP_EXPR_IFDEF,
  CPP_EXPR_IFNDEF,
} CPP_ExprType;
struct CPP_Expr
{
  CPP_ExprType expr_type;
  size_t n_tokens;
  CPP_Token *tokens;
};

struct DBCC_Parser
{
  unsigned magic;
  DBCC_Parser_Handlers handlers;
  void *handler_data;
  DskBuffer buffer;
  DBCC_Namespace *globals;
  P_Context *context;
  void *lemon_parser;
  DBCC_SymbolSpace *symbol_space;

  size_t n_include_dirs;
  char **include_dirs;
  size_t include_dirs_alloced;

  CPP_Macro *macro_tree;
};
#define COMPARE_CPP_MACROS(a,b, rv) rv = (a<b) ? -1 : (a>b) ? 1 : 0
#define GET_MACRO_TREE(parser) \
 (parser)->macro_tree,         \
 CPP_Macro *,                  \
 DSK_STD_GET_IS_RED,           \
 DSK_STD_SET_IS_RED,           \
 parent, left, right,          \
 COMPARE_CPP_MACROS


DBCC_Parser *
dbcc_parser_new             (DBCC_Parser_Handlers *handlers,
                             void          *handler_data)
{
  DBCC_Parser *rv = malloc (sizeof (DBCC_Parser));
  rv->magic = DBCC_PARSER_MAGIC;
  rv->handlers = *handlers;
  rv->handler_data = handler_data;
  dsk_buffer_init (&rv->buffer);
  rv->globals = dbcc_namespace_new_global ();
  rv->context = p_context_new (rv->globals);
  rv->lemon_parser = DBCC_Lemon_ParserAlloc(malloc);
  rv->n_include_dirs = 0;
  rv->include_dirs = NULL;
  rv->include_dirs_alloced = 0;
  return rv;
}

void
dbcc_parser_add_include_dir (DBCC_Parser   *parser,
                             const char    *dir)
{
  if (parser->n_include_dirs == parser->include_dirs_alloced)
    {
      if (parser->include_dirs_alloced == 0)
        {
          parser->include_dirs_alloced = 8;
          parser->include_dirs = malloc (sizeof(char *) * parser->include_dirs_alloced);
        }
      else 
        {
          parser->include_dirs_alloced *= 2;
          parser->include_dirs = realloc (parser->include_dirs,
                                          sizeof(char *) * parser->include_dirs_alloced);
        }
    }
  parser->include_dirs[parser->n_include_dirs++] = strdup(dir);
}

static bool
scan_multiline_comment_body (const char **str_inout,
                             const char  *end,
                             unsigned    *line_no_out,
                             unsigned    *column_out)
{
  const char *str = *str_inout;
  unsigned line = *line_no_out;
  unsigned col = *column_out;
  assert(str[0] == '/' && str[1] == '*');
  str += 2;
  col += 2;
  while (str < end)
    {
      if (*str == '*' && str + 1 < end && str[1] == '/')
        {
          *str_inout = str;
          *line_no_out = line;
          *column_out = col;
          return true;
        }
    }
  return false;
}

/* Number scanning.
 *
 * See 6.4.4.1 for Integer constants
 * and 6.4.4.2 for Floating constants.
 */
typedef enum
{
  CPP_NUMBER_DECIMAL_CONSTANT,
  CPP_NUMBER_OCTAL_CONSTANT,
  CPP_NUMBER_HEXADECIMAL_CONSTANT,
  CPP_NUMBER_DECIMAL_FLOATING_CONSTANT,
  CPP_NUMBER_HEXADECIMAL_FLOATING_CONSTANT,
} CPP_NumberType;

typedef struct CPP_NumberParseResult CPP_NumberParseResult;
typedef enum
{
  CPP_NUMBER_PARSE_NOTHING,
  CPP_NUMBER_PARSE_GOT_NUMBER,
  CPP_NUMBER_PARSE_BAD_NUMBER,
} CPP_NumberParseResultType;
struct CPP_NumberParseResult
{
  CPP_NumberParseResultType type;
  union {
    struct { 
      CPP_NumberType number_type;
      size_t number_length;
    } v_number;
    struct {
      DBCC_ErrorCode error;
      const char *message;
    } v_bad_number;
  };
};
  
static inline CPP_NumberParseResult
try_number(const char     *str,
           const char     *end)

{
#define RETURN_NUMBER(shortname)                            \
  do{ result.type = CPP_NUMBER_PARSE_GOT_NUMBER;            \
      result.v_number.number_type = CPP_NUMBER_##shortname; \
      result.v_number.number_length = at - str;             \
      return result;                                        \
    }while(0)
#define RETURN_ERROR(code, msg)                             \
  do{ result.type = CPP_NUMBER_PARSE_BAD_NUMBER;            \
      result.v_bad_number.error = DBCC_ERROR_##code;        \
      result.v_bad_number.message = (msg);                  \
      return result;                                        \
    }while(0)
#define GOTO_END_OF_NUMBER(shortname)                       \
  do{ result.type = CPP_NUMBER_PARSE_GOT_NUMBER;            \
      result.v_number.number_type = CPP_NUMBER_##shortname; \
      result.v_number.number_length = at - str;             \
      goto end_number;                                      \
    }while(0)
  const char *at = str;
  CPP_NumberParseResult result;
  if (*str == '0')
    {
      at++;
      if (at == end)
        RETURN_NUMBER(DECIMAL_CONSTANT);
      if (*at == 'x' || *at == 'X')
        {
          at++;
          if (at == end)
            RETURN_ERROR(BAD_HEXIDECIMAL_CONSTANT, "need hex digit after 0x");
          while (('0' <= *at && *at <= '9')
              || ('a' <= *at && *at <= 'f')
              || ('A' <= *at && *at <= 'F'))
            {
              at++;
              if (at == end)
                RETURN_NUMBER(HEXADECIMAL_CONSTANT);
            }
          if (*at == '.')
            goto got_binary_point;
          else if (*at == 'p' || *at == 'P')
            goto got_p;
          GOTO_END_OF_NUMBER(HEXADECIMAL_CONSTANT);
        }
      else if ('0' <= *at && *at <= '7')
        {
          while ('0' <= *at && *at <= '7')
            {
              at++;
              if (at == end)
                RETURN_NUMBER(OCTAL_CONSTANT);
            }
          GOTO_END_OF_NUMBER(OCTAL_CONSTANT);
        }
      else if (*at == '.')
        {
          at++;
          goto got_decimal_point;
        }
      else if (*at == 'e' || *at == 'E')                //???
        {
          at++;
          goto got_e;
        }
      else
        GOTO_END_OF_NUMBER(DECIMAL_CONSTANT);
    }
  else if ('1' <= *at && *at <= '9')
    {
      while ('0' <= *at && *at <= '9')
        {
          at++;
          if (at == end)
            RETURN_NUMBER(DECIMAL_CONSTANT);
        }
      if (*at == '.')
        {
          at++;
          if (at == end)
            RETURN_NUMBER(DECIMAL_FLOATING_CONSTANT);
          goto got_decimal_point;
        }
      else if (*at == 'e' || *at == 'E')
        {
          at++;
          if (at == end)
            RETURN_ERROR(BAD_DECIMAL_EXPONENT, "expecting number after e");
          goto got_e;
        }
      else
        GOTO_END_OF_NUMBER(DECIMAL_CONSTANT);
    }
  else if (*at == '.')
    {
      at++;
      goto got_leading_decimal_point;
    }
  else
    {
      result.type = CPP_NUMBER_PARSE_NOTHING;
      return result;
    }


got_leading_decimal_point:
  if (at == end || !('0' <= *at && *at <= '9'))
    {
      result.type = CPP_NUMBER_PARSE_NOTHING;
      return result;
    }
  at++;
  goto got_decimal_point;

got_decimal_point:
  while (at <  end && '0' <= *at && *at <= '9')
    {
      at++;
    }
  if (at < end && (*at == 'e' || *at == 'E'))
    {
      at++;
      goto got_e;
    }
  RETURN_NUMBER(DECIMAL_FLOATING_CONSTANT);

got_e:
  if (at == end)
    RETURN_ERROR(BAD_DECIMAL_EXPONENT, "missing exponent after 'e'");
  if (*at == '+' || *at == '-')
    {
      at++;
      if (at == end)
        RETURN_ERROR(BAD_DECIMAL_EXPONENT, "missing exponent after +/-");
    }
  if ('0' <= *at && *at <= '9')
    {
      at++;
      while (at < end && '0' <= *at && *at <= '9')
        at++;
    }
  GOTO_END_OF_NUMBER(DECIMAL_FLOATING_CONSTANT);

got_binary_point:
  while (at <  end && isxdigit(*at))
    {
      at++;
    }
  if (at < end && (*at == 'p' || *at == 'P'))
    {
      at++;
      goto got_p;
    }
  GOTO_END_OF_NUMBER(DECIMAL_FLOATING_CONSTANT);

got_p:
  if (at == end)
    RETURN_ERROR(BAD_BINARY_EXPONENT, "need decimal exponent for binary exponent");
  if (*at == '+' || *at == '-')
    {
      at++;
      if (at == end)
        RETURN_ERROR(BAD_BINARY_EXPONENT, "need decimal exponent for binary exponent after +/-");
    }
  if ('0' <= *at && *at <= '9')
    {
      at++;
      while (at < end && '0' <= *at && *at <= '9')
        at++;
    }
  GOTO_END_OF_NUMBER(HEXADECIMAL_FLOATING_CONSTANT);

end_number:
  if (at >= end)
    return result;
  if (isalnum (*at))
    {
      RETURN_ERROR(BAD_NUMBER_CONSTANT, "unexpected character after number");
      ///XXX: error
    }
  return result;
}

static inline bool
is_initial_identifer_char (char c)
{
  return c == '_'
      || ('a' <= c && c <= 'z')
      || ('A' <= c && c <= 'Z');
}

static inline unsigned
scan_bareword (const char *str, const char *end)
{
  const char *at = str;
  // must succeed
  assert(is_initial_identifer_char (*at));
  at++;

  while (at < end &&
      (*at == '_' ||
       ('0' <= *at && *at <= '9') ||
       ('a' <= *at && *at <= 'f') ||
       ('A' <= *at && *at <= 'F')))
    {
      at++;
    }
  return at - str;
}

/* Section 6.4.5 String literals
 *
 */
static bool
scan_string   (const char  **str_inout,
               unsigned     *line_no_inout,
               unsigned     *column_inout,
               const char   *end,
               DBCC_ErrorCode *code_out)
{
  const char *str = *str_inout;
  unsigned line = *line_no_inout;
  unsigned column = *column_inout;

  /* skip presumed quote-mark (plus size prefix) */
  if (*str == 'L')
    {
      str++;
      column++;
    }
  assert(*str == '"');
  str++; column++;

  while (str < end && *str != '"')
    {
      if (*str == '\\')
        {
          if (++str == end)
            {
              *code_out = DBCC_ERROR_UNTERMINATED_BACKSLASH_SEQUENCE;
              goto return_false;
            }
          column++;
          switch (*str)
            {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
              str++;
              column++;
              if (str < end && '0' <= *str && *str <= '7')
                {
                  str++;
                  column++;
                  if (str < end && '0' <= *str && *str <= '7')
                    {
                      str++;
                      column++;
                    }
                }
              break;

            case 'x':
              if (str + 2 >= end)
                {
                  *code_out = DBCC_ERROR_UNTERMINATED_BACKSLASH_SEQUENCE;
                  goto return_false;
                }
              str += 3;
              column++;
              break;

            case 'u': 
            case 'U':
              {
                unsigned ucs_len = *str == 'u' ? 4 : 8;
                if (str + ucs_len >= end)
                  {
                    *code_out = DBCC_ERROR_BAD_UNIVERSAL_CHARACTER_SEQUENCE;
                    goto return_false;
                  }
                for (unsigned i = 0; i < ucs_len; i++)
                  if (!isxdigit(str[i+1]))
                    {
                      *code_out = DBCC_ERROR_BAD_UNIVERSAL_CHARACTER_SEQUENCE;
                      goto return_false;
                    }
                str += 1 + ucs_len;
                column += 1 + ucs_len;
                break;
              }

            case '\n':
              str++;
              column = 1;
              line++;
              break;

            default:
              str++;
              column++;
              break;
            }
        }
      else if (*str == '\n')
        {
          *code_out = DBCC_ERROR_UNTERMINATED_DOUBLEQUOTED_STRING;
          goto return_false;
        }
      else
        {
          str++;
          column++;
        }
    }
  if (str >= end)
    {
      *code_out = DBCC_ERROR_UNTERMINATED_DOUBLEQUOTED_STRING;
      goto return_false;
    }
  assert(*str == '"');
  str++;
  column++;
  *str_inout = str;
  *column_inout = column;
  *line_no_inout = line;
  return true;

return_false:
  *str_inout = str;
  *column_inout = column;
  *line_no_inout = line;
  return false;
}

/* 6.4.4.4. Character constants
 *
 * Character literals include various wide types,
 * so the output is a uint32 and width_specifier will
 * be set to 1,2,4 for each character type's size. */
static bool
scan_character (const char **str_inout,
                unsigned    *column_inout,
                unsigned    *line_inout,
                const char  *end,
                DBCC_ErrorCode *code_out)
{
  const char *str = *str_inout;
  if (*str == 'L' || *str == 'u' || *str == 'U')
    {
      str++;
      *column_inout += 1;
    }
  assert(*str == '\'');
  str++;
  *column_inout += 1;
retry_char:
  if (*str == '\\')
    {
      str++;
      if (str >= end)
        {
          *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
          goto return_false;
        }
      if (*str == '\n')
        {
          str++;
          if (str >= end)
            {
              *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
              goto return_false;
            }
          *line_inout += 1;
          *column_inout = 1;
          goto retry_char;
        }
      if ('0' <= *str && *str <= '7')
        {
          if (str + 1 < end && '0' <= str[1] && str[1] <= '7')
            {
              if (str + 2 < end && '0' <= str[2] && str[2] <= '7')
                {
                  str += 3;
                  *column_inout += 3;
                }
              else
                {
                  str += 2;
                  *column_inout += 2;
                }
            }
          else
            {
              str += 1;
              *column_inout += 1;
            }
        }
      else if (*str == 'x')
        {
          str += 1;
          *column_inout += 1;
          if (str + 1 >= end)
            {
              *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
              goto return_false;
            }
          if (!isxdigit(str[0]) || !isxdigit(str[1]))
            {
              *code_out = DBCC_ERROR_BAD_BACKSLASH_HEX;
              goto return_false;
            }
          str += 2;
        }
      else if (*str == 'u' || *str == 'U')
        {
          unsigned n = *str == 'u' ? 4 : 8;
          if (str + n >= end)
            {
              *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
              return false;
            }
          str++;
          *column_inout += 1;
          for (unsigned i = 0; i < n; i++)
            if (!isxdigit (str[i]))
              {
                *code_out = DBCC_ERROR_BAD_UNIVERSAL_CHARACTER_SEQUENCE;
                return false;
              }
           str += n;
           *column_inout += n;
        }
      else
        {
          str++;
          *column_inout += 1;
        }
    }
  else if (*str == '\n')
    {
      *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
      goto return_false;
    }
  else
    {
      str++;
      if (str >= end)
        {
          *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
          goto return_false;
        }
      *column_inout += 1;
    }
  if (*str != '\'')
    {
      *code_out = DBCC_ERROR_UNTERMINATED_CHARACTER_CONSTANT;
      goto return_false;
    }
  str++;
  *column_inout += 1;
  *str_inout = str;
  return true;

return_false:
  *str_inout = str;
  return false;
}

/* 6.4.6 Punctuators 
 * The following 1-, 2-, and 3-character sequences:
       [ ] ( ) { } . ->
       ++ -- & * + - ~ !
       / % << >> < > <= >= == != ^ | && ||
       ? : ; ...
       = *= /= %= += -= <<= >>= &= ^= |=
       , # ##
  and digraphs:
       <: :> <% %> %: %:%:       ... which are equivalent to:
       [  ]  {  }  #  ##
 */
typedef struct ScanPunctuatorResult ScanPunctuatorResult;
typedef enum
{
  SCAN_PUNCTUATOR_RESULT_NO,
  SCAN_PUNCTUATOR_RESULT_SUCCESS,
  SCAN_PUNCTUATOR_RESULT_SUCCESS_DIGRAPH,
} ScanPunctuatorResultType;
struct ScanPunctuatorResult
{
  ScanPunctuatorResultType type;
  union {
    struct {
      unsigned length;
      CPP_TokenType token_type;
    } v_success, v_success_digraph;
  };
};

static ScanPunctuatorResult
scan_punctuator (const char *str, const char *end)
{
  char next_c = (str + 1 < end) ? str[1] : 0;
  ScanPunctuatorResult res;
#define RETURN(n, tok_shortname) \
  do{ res.type = SCAN_PUNCTUATOR_RESULT_SUCCESS; \
      res.v_success.length = (n); \
      res.v_success.token_type = CPP_TOKEN_##tok_shortname; \
    }while(0)
#define RETURN_DIGRAPH(n, tok_shortname) \
  do{ res.type = SCAN_PUNCTUATOR_RESULT_SUCCESS_DIGRAPH; \
      res.v_success_digraph.length = (n); \
      res.v_success_digraph.token_type = CPP_TOKEN_##tok_shortname; \
    }while(0)
  switch (*str)
    {
    case '!':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }

    case '#':
      switch (next_c)
        {
          case '#': RETURN(2, CONCATENATE);
          default: RETURN(1, HASH);
        }

    case '%':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          case '>': RETURN_DIGRAPH(2, OPERATOR);
          case ':':  // either # or ## ... as digraphs
            if (str + 3 < end && str[2] == '%' && str[2] == ':')
              // %:%:   which is equivalent to ##
              RETURN_DIGRAPH(4, CONCATENATE);
            else
              // %:     which is equivalent to #
              RETURN_DIGRAPH(2, HASH);
          default: RETURN(1, OPERATOR);
        }
    case '&':
      switch (next_c)
        {
          case '&': RETURN(2, OPERATOR);
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }

    case '(': RETURN(1, OPERATOR);
    case ')': RETURN(1, OPERATOR);
    case '*':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case '+':
      switch (next_c)
        {
          case '+': RETURN(2, OPERATOR);
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case ',': RETURN(1, OPERATOR);
    case '-':
      switch (next_c)
        {
          case '-': RETURN(2, OPERATOR);
          case '=': RETURN(2, OPERATOR);
          case '>': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case '.':
      RETURN(1, OPERATOR);
    case '/':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case ':':
      switch (next_c)
        {
          case '>': RETURN_DIGRAPH(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case ';': RETURN(1, OPERATOR);
    case '<':
      switch (next_c)
        {
          case '<':
            if (str + 2 < end && str[2] == '=')
              RETURN(3, OPERATOR);
            else
              RETURN(2, OPERATOR);
         case '=': RETURN(2, OPERATOR);
         case ':': RETURN_DIGRAPH(2, OPERATOR);
         case '%': RETURN_DIGRAPH(2, OPERATOR);
         default: RETURN(1, OPERATOR);
       }
    case '=':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case '>':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          case '>':
            if (str + 2 < end && str[2] == '=')
              RETURN(3, OPERATOR);
            else
              RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case '?': RETURN(1, OPERATOR);
    case '[': RETURN(1, OPERATOR);
    case ']': RETURN(1, OPERATOR);
    case '^':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case '{': RETURN(1, OPERATOR);
    case '|':
      switch (next_c)
        {
          case '=': RETURN(2, OPERATOR);
          case '|': RETURN(2, OPERATOR);
          default: RETURN(1, OPERATOR);
        }
    case '}': RETURN(1, OPERATOR);
    case '~': RETURN(1, OPERATOR);
    default:
      res.type = SCAN_PUNCTUATOR_RESULT_NO;
      return res;
    }
#undef RETURN
#undef RETURN_TYPE
}

static bool
is_if_like_directive_identifier (const CPP_Token *t)
{
  return (t->length == 2 && memcmp (t->str, "if", 2) == 0)
      || (t->length == 5 && memcmp (t->str, "ifdef", 5) == 0)
      || (t->length == 6 && memcmp (t->str, "ifndef", 6) == 0);
}

/* Returns the number of tokens in the expression,
   NOT including if/ifdef/ifndef */
static unsigned
parse_cpp_expr (unsigned n_tokens,
                CPP_Token *tokens,
                CPP_Expr  *expr,
                DBCC_Error **error)
{
  /* look for newline token */
  CPP_Token *t = tokens;
  if (t->length == 2 && memcmp (t->str, "if", 2) == 0)
    expr->expr_type = CPP_EXPR_IF;
  else if (t->length == 5 && memcmp (t->str, "ifdef", 2) == 0)
    expr->expr_type = CPP_EXPR_IFDEF;
  else if (t->length == 6 && memcmp (t->str, "ifndef", 2) == 0)
    expr->expr_type = CPP_EXPR_IFNDEF;
  else
    assert(0);

  unsigned i;
  for (i = 1;
       i < n_tokens && tokens[i].type == CPP_TOKEN_NEWLINE;
       i++)
    {
    }
  if (i == n_tokens)
    {
      *error = dbcc_error_new (DBCC_ERROR_UNTERMINATED_PREPROCESSOR_DIRECTIVE,
                               "unexpected end-of-file, expected newline, after #%.*s directive",
                               (int)t->length, t->str);
      dbcc_error_add_code_position (*error, tokens[i-1].code_position);
      return 0;
    }

  /* Additional error-checking for #ifdef/ifndef */
  if (expr->expr_type == CPP_EXPR_IFDEF
   || expr->expr_type == CPP_EXPR_IFNDEF)
    {
      if (i != 2)
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_PREPROCESSOR_DIRECTIVE,
                                   "#ifdef/#ifndef takes exactly 1 argument");
          dbcc_error_add_code_position (*error, tokens[1].code_position);
          return 0;
        }
      if (tokens[1].type != CPP_TOKEN_BAREWORD)
        {
          *error = dbcc_error_new (DBCC_ERROR_BAD_PREPROCESSOR_DIRECTIVE,
                                   "#ifdef/#ifndef must be followed by an identifier, got %s",
                                   cpp_token_type_name (tokens[1].type));
          dbcc_error_add_code_position (*error, tokens[1].code_position);
          return 0;
        }
    }

  return i - 1;
}

typedef struct CPP_MacroExpansionResult CPP_MacroExpansionResult;

typedef enum {
  CPP_MACRO_EXPANSION_RESULT_SUCCESS,
  CPP_MACRO_EXPANSION_RESULT_NO_CHANGE,
  CPP_MACRO_EXPANSION_RESULT_ERROR
} CPP_MacroExpansionResultType;
struct CPP_MacroExpansionResult
{
  CPP_MacroExpansionResultType type;
  union {
    struct {
      unsigned n_expanded_tokens;
      CPP_Token *expanded_tokens;
      DskMemPool used_memory;
    } v_success;
    struct {
      DBCC_Error *error;
    } v_error;
  };
};

static CPP_Macro *
lookup_macro (DBCC_Parser *parser,
              DBCC_Symbol *symbol)
{ 
#define COMPARE_SYMBOL_TO_NODE(sym, node, rv) \
  rv = (symbol < node->name) ? -1 : (symbol < node->name) ? 1 : 0
  CPP_Macro *rvmacro;
  DSK_RBTREE_LOOKUP_COMPARATOR(GET_MACRO_TREE(parser), symbol, COMPARE_SYMBOL_TO_NODE, rvmacro);
  return rvmacro;
#undef COMPARE_SYMBOL_TO_NODE
}

static void
skip_newline_tokens (unsigned *index_inout,
                     unsigned  n_tokens,
                     CPP_Token *tokens)
{
  unsigned i;
  for (i = *index_inout;
       i < n_tokens && tokens[i].type == CPP_TOKEN_NEWLINE;
       i++)
    ;
  *index_inout = i;
}

typedef struct {
  unsigned start, count;
} ArgSlice;

static bool
scan_one_actual_macro_arg (unsigned       n_tokens,
                           CPP_Token     *tokens,
                           bool           ellipsis,
                           unsigned      *tokens_used,
                           DBCC_Error   **error)
{
  unsigned nesting = 0;
  for (unsigned i = 0; i < n_tokens; i++)
    {
      if (tokens[i].type == CPP_TOKEN_OPERATOR
       || tokens[i].length == 1)
        {
          switch (tokens[i].str[0])
            {
            case '(':
              nesting++;
              break;
            case ')':
              if (nesting == 0)
                { 
                  *tokens_used = i;
                  return true;
                }
              nesting--;
              break;
            case ',':
              if (nesting == 0 && !ellipsis)
                {
                  *tokens_used = i;
                  return true;
                }
              break;
            }
        }
    }
  *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_MACRO_INVOCATION_EOF,
                           "end-of-file in macro invocation");
  if (n_tokens > 0)
    dbcc_error_add_code_position (*error, tokens[n_tokens-1].code_position);
  return false;
}

static bool
expand_1_function_macro (CPP_Macro   *macro,
                         unsigned    *i_inout,
                         unsigned     n_tokens,
                         CPP_Token   *tokens,
                         CPP_TokenArray *out,
                         DBCC_Error **error)
{
  ArgSlice *actual_args = alloca (sizeof (ArgSlice) * macro->arity);
  unsigned i = *i_inout;
  skip_newline_tokens(&i, n_tokens, tokens);
  if (i + 1 >= n_tokens
   || tokens[i+1].type != CPP_TOKEN_OPERATOR
   || tokens[i+1].length != 1
   || tokens[i+1].str[0] != '(')
    {
      
      *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_MISSING_LPAREN,
                               "left-paren neeeded after functional-macro %s",
                               dbcc_symbol_get_string (macro->name));
      dbcc_error_add_code_position (*error, tokens[i+1].code_position);
      return false;
    }
  i += 2;
      
  // parse actual arguments
  unsigned arg_index;
  for (arg_index = 0; arg_index < macro->arity; arg_index++)
    {
      unsigned n_used;
      bool ellipsis = arg_index + 1 == macro->arity && macro->has_ellipsis;
      if (!scan_one_actual_macro_arg (n_tokens - i, tokens + i, ellipsis, &n_used, error))
        return false;
      actual_args[arg_index].start = i;
      actual_args[arg_index].count = n_used;
      i += n_used;

      assert (tokens[i].type == CPP_TOKEN_OPERATOR);
      assert (tokens[i].length == 1);
      if (arg_index + 1 < macro->arity)
        {
          // skip comma
          if (tokens[i].str[0] != ',')
            {
              *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_MACRO_INVOCATION,
                                       "too few arguments to macro %s",
                                       dbcc_symbol_get_string(macro->name));
              dbcc_error_add_code_position (*error, tokens[i].code_position);
              return false;
            }
          i++;
        }
      else
        {
          if (tokens[i].str[0] != ')')
            {
              *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_MACRO_INVOCATION,
                                       "expected ')', got '%c' (non-terminated macro invocation of %s)",
                                       tokens[i].str[0],
                                       dbcc_symbol_get_string(macro->name));
              dbcc_error_add_code_position (*error, tokens[i].code_position);
              return false;
            }
          i++;
        }
    }

  // perform macro expansion
  for (unsigned m_at = 0; m_at < macro->n_tokens; m_at++)
    {
      if (macro->tokens[m_at].type == CPP_TOKEN_MACRO_ARGUMENT)
        {
          ArgSlice *slice = actual_args + macro->tokens[m_at].alt_int_value;
          CPP_Token *tok = tokens + slice->start;
          for (unsigned s_at = 0; s_at < slice->count; s_at++, tok++)
            cpp_token_array_append (out, tok);
        }
      else
        cpp_token_array_append (out, macro->tokens + m_at);
    }
  return true;
}

static inline bool
can_concatenate_token_type (CPP_TokenType tt)
{
  return tt == CPP_TOKEN_BAREWORD
      || tt == CPP_TOKEN_NUMBER;
}

static CPP_MacroExpansionResult
expand_macros (DBCC_Parser *parser,
               unsigned     n_tokens,
               CPP_Token   *tokens)
{
  CPP_MacroExpansionResult res;
  DBCC_Symbol *sym;
  CPP_Macro *macro = NULL;
  unsigned i, j;
  CPP_TokenArray token_array = CPP_TOKEN_ARRAY_INIT;
  for (i = 0; i < n_tokens; i++)
    if (tokens[i].type == CPP_TOKEN_BAREWORD
     && (sym=dbcc_symbol_space_try_len (parser->symbol_space, tokens[i].length, tokens[i].str)) != NULL
     && (macro=lookup_macro (parser, sym)) != NULL
     && !macro->is_expanding)
      break;
  if (i == n_tokens)
    {
      res.type = CPP_MACRO_EXPANSION_RESULT_NO_CHANGE;
      return res;
    }
  assert(macro != NULL);
  for (j = 0; j < i; j++)
    cpp_token_array_append (&token_array, tokens + j);
  for (    ; i < n_tokens; i++)
    {
      if (macro == NULL)
        {
          // is the current token a macro?
          if (tokens[i].type == CPP_TOKEN_BAREWORD
           && (sym=dbcc_symbol_space_try_len (parser->symbol_space, tokens[i].length, tokens[i].str)) != NULL
           && (macro=lookup_macro (parser, sym)) != NULL
           && !macro->is_expanding)
            {
              break;
            }
        }

      if (macro == NULL)
        {
          /* pass through */
          cpp_token_array_append (&token_array, tokens + i);
        }
      else 
        {
          CPP_TokenArray sub = CPP_TOKEN_ARRAY_INIT;
          if (macro->function_macro)
            {
              DBCC_Error *error = NULL;
              if (!expand_1_function_macro (macro, &i, n_tokens, tokens, &sub, &error))
                {
                  res.type = CPP_MACRO_EXPANSION_RESULT_ERROR;
                  res.v_error.error = error;
                  return res;
                }
            }
          else
            {
              // single-token macro
              for (unsigned m_at = 0; m_at < macro->n_tokens; m_at++)
                cpp_token_array_append (&sub, macro->tokens + m_at);
              i++;
            }
          
          // Expand macros recursively,
          // except we deliberately guard against literally
          // recursing on the current macro.
          assert(!macro->is_expanding);
          macro->is_expanding = true;
          res = expand_macros (parser, sub.n, sub.tokens);
          macro->is_expanding = false;
          switch (res.type)
            {
            case CPP_MACRO_EXPANSION_RESULT_SUCCESS:
              for (unsigned t = 0; t < res.v_success.n_expanded_tokens; t++)
                cpp_token_array_append (&token_array, &res.v_success.expanded_tokens[t]);
              free (res.v_success.expanded_tokens);
              break;
            case CPP_MACRO_EXPANSION_RESULT_NO_CHANGE:
              break;
            case CPP_MACRO_EXPANSION_RESULT_ERROR:
              return res;
            }

          unsigned t;
          for (t = 0; t + 1 < sub.n; )
            {
              if (sub.tokens[t].type == CPP_TOKEN_HASH
               && sub.tokens[t+1].type == CPP_TOKEN_BAREWORD)
                {
                  /* convert to literal string */
                  CPP_Token toke = {
                    CPP_TOKEN_STRING,
                    sub.tokens[t+1].code_position,
                    sub.tokens[t+1].str,
                    sub.tokens[t+1].length,
                    1                   // string is unquoted
                  };
                  cpp_token_array_append (&token_array, &toke);
                  t += 2;
                }
              else if (can_concatenate_token_type (sub.tokens[t].type)
                    && sub.tokens[t+1].type == CPP_TOKEN_CONCATENATE)
                {
                  /* concatenate into a single token */
                  bool all_numbers = sub.tokens[t].type == CPP_TOKEN_NUMBER;
                  unsigned total_length = sub.tokens[t].length;
                  unsigned n_parts = 2;
                  for (;;)
                    {
                      unsigned a = t + n_parts * 2 - 2;
                      if (a >= sub.n)
                        {
                          res.type = CPP_MACRO_EXPANSION_RESULT_ERROR;
                          res.v_error.error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_CONCATENATION,
                                                              "'##' cannot appear at end of macro expansion");
                          dbcc_error_add_code_position (res.v_error.error, sub.tokens[a].code_position);
                          return res;
                        }
                      else if (!can_concatenate_token_type (sub.tokens[a].type))
                        {
                          res.type = CPP_MACRO_EXPANSION_RESULT_ERROR;
                          res.v_error.error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_CONCATENATION,
                                                              "invalid token type for ##");
                          dbcc_error_add_code_position (res.v_error.error, sub.tokens[a].code_position);
                          return res;
                        }
                      else
                        {
                          /* is ok */
                          total_length += sub.tokens[a].length;
                          if (all_numbers && sub.tokens[a].type != CPP_TOKEN_NUMBER)
                            all_numbers = false;
                        }

                      if (a + 1 < sub.n
                        && sub.tokens[a + 1].type == CPP_TOKEN_CONCATENATE)
                        n_parts++;
                      else
                        break;
                    }
                  char *concat = malloc (total_length + 1);
                  char *at = concat;
                  unsigned tt;
                  for (tt = 0; tt < n_parts; tt++)
                    {
                      CPP_Token *tok = tokens + t + 2 * tt;
                      memcpy (at, tok->str, tok->length);
                      at += tok->length;
                    }
                  *at = 0;
                  CPP_Token new_token = {
                    all_numbers ? CPP_TOKEN_NUMBER : CPP_TOKEN_BAREWORD,
                    sub.tokens[t].code_position,
                    concat,
                    total_length,
                    0
                  };
                  cpp_token_array_append (&token_array, &new_token);
                  t += n_parts * 2 - 1;
                }
              else
                {
                  cpp_token_array_append (&token_array, sub.tokens+t);
                  t++;
                }
            }
          while (t < sub.n)
            {
              cpp_token_array_append (&token_array, sub.tokens+t);
              t++;
            }
        }
    }
  res.type = CPP_MACRO_EXPANSION_RESULT_SUCCESS;
  res.v_success.n_expanded_tokens = token_array.n;
  res.v_success.expanded_tokens = token_array.tokens;
  return res;
}

/* since all preprocessor tokens have been expanded,
 * the list of tokens should consist of the operators:
 *       * / = % & | && || ! ~ == != < <= > >=
 * as well as numbers, parentheses.
 */
static bool
tokens_to_boolean_value (DBCC_Parser *parser,
                         unsigned     n_tokens,
                         CPP_Token   *tokens)
{
  ...
}

static bool
eval_cpp_expr_boolean (DBCC_Parser *parser,
                       CPP_Expr    *expr,
                       bool        *result_out,
                       DBCC_Error **error_out)
{
  CPP_MacroExpansionResult res;
  DBCC_Symbol *kw;

  switch (expr->expr_type)
    {
    case CPP_EXPR_IFNDEF:
      assert(expr->n_tokens == 1);
      assert(expr->tokens[0].type == CPP_TOKEN_BAREWORD);
      kw = dbcc_symbol_space_force (parser->token_space,
                                    expr->tokens[0].length,
                                    expr->tokens[0].str);
      *result_out = lookup_macro (parser, kw) == NULL;
      return true;

    case CPP_EXPR_IFDEF:
      assert(expr->n_tokens == 1);
      assert(expr->tokens[0].type == CPP_TOKEN_BAREWORD);
      kw = dbcc_symbol_space_force (parser->token_space,
                                    expr->tokens[0].length,
                                    expr->tokens[0].str);
      *result_out = ! lookup_macro (parser, kw) == NULL;
      return true;

    case CPP_EXPR_IF:
      break;
    }

  // handle 'defined(Keyword)', because we must handle it before macro expansion.
  ...

  res = expand_macros (parser, ..., ...);
  switch (res)
    {
      case CPP_MACRO_EXPANSION_RESULT_NO_CHANGE:
        ...
      case CPP_MACRO_EXPANSION_RESULT_SUCCESS:
        ...
      case CPP_MACRO_EXPANSION_RESULT_ERROR:
        ...
    }
}

typedef enum
{
  CPP_STACK_INACTIVE_PARENT,

  CPP_STACK_INACTIVE,           // parent active, child inactive
  CPP_STACK_INACTIVE_FOR_ALL_IFS,
  CPP_STACK_ACTIVE,
} CPP_StackFrameStatus;

bool
dbcc_parser_parse_file      (DBCC_Parser   *parser,
                             const char    *filename)
{
  DskError *error = NULL;
  size_t size;
  DBCC_Symbol *filename_symbol = dbcc_symbol_space_force (parser->symbol_space, filename);
  uint8_t *contents = dsk_file_get_contents (filename, &size, &error);
  if (contents == NULL)
    {
      DBCC_Error *e = dbcc_error_from_dsk_error (error);
      dsk_error_unref (error);
      parser->handlers.handle_error (e, parser->handler_data);
      return false;
    }
  
  /* Step 1: convert file into a sequence of "preprocessor tokens".
   * These tokens "point into" the raw file contents, to minimize extra copies.
   */
  const char *str = (const char *) contents;
  const char *end = str + size;
  unsigned line_no = 1;
  unsigned column = 1;
#define CREATE_CP()                                                     \
  dbcc_code_position_new(NULL, NULL,                                    \
                         filename_symbol,                               \
                         line_no, column, str - (char*)contents + 1)
  size_t cpp_tokens_alloced = 1024;
  CPP_Token *cpp_tokens = malloc (sizeof (CPP_Token) * cpp_tokens_alloced);
  size_t n_cpp_tokens = 0;
#define APPEND_TOKEN(token)                                             \
  do{                                                                   \
    if (n_cpp_tokens == cpp_tokens_alloced)                             \
      {                                                                 \
        cpp_tokens_alloced *= 2;                                        \
        cpp_tokens = realloc (cpp_tokens,                               \
                              sizeof (CPP_Token) * cpp_tokens_alloced); \
        cpp_tokens[n_cpp_tokens++] = (token);                           \
      }                                                                 \
  } while(0)
#define APPEND_CPP_TOKEN(typeshort, cp, str, length)                    \
  APPEND_TOKEN(CPP_TOKEN(typeshort, cp, str, length))
  while (str < end)
    {
      while (str < end && *str != '\n')
        {
          if (*str == '#')
            {
              /* handle preprocessor directive. */
              APPEND_TOKEN(CPP_TOKEN(
                HASH,
                CREATE_CP(),
                str,
                1
              ));
              column++;
            }
          else if (str[0] == '/' && str + 1 < end && str[1] == '*')
            {
              str += 2;
              column += 2;
              if (!scan_multiline_comment_body (&str, end, &line_no, &column))
                {
                  /* unterminated comment (line_no and column should
                     refer to the start of the comment). */
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_UNTERMINATED_MULTILINE_COMMENT,
                                          "multiline-style comment unterminated (missing `*/')");
                  dbcc_error_add_code_position (error, CREATE_CP());
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
            }
          else if (str[0] == '/' && str + 1 < end && str[1] == '/')
            {
              const char *endline = str;
              while (endline < end && *endline != '\n')
                endline++;
              if (endline == end)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_MISSING_LINE_TERMINATOR,
                                          "//-style comment not terminated by a newline");
                  dbcc_error_add_code_position (error, CREATE_CP());
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              str = endline + 1;
              column = 1;
              line_no++;
            }
          else if (*str == ' ' || *str == '\t')
            {
              column++;
            }
          else if (is_initial_identifer_char (*str))
            {
              unsigned bw_len = scan_bareword (str, end);
              if (bw_len == 0)
                {
                  assert(0);
                }
              APPEND_CPP_TOKEN(
                BAREWORD,
                CREATE_CP(),
                str,
                1
              );
            }
          else if (*str == '"'
                || (*str == 'L' && str + 1 < end && str[1] == '"'))
            {
              DBCC_ErrorCode code;
              const char *str_start = str;
              if (!scan_string (&str, &line_no, &column, end, &code))
                {
                  assert(0);
                }
              APPEND_TOKEN(CPP_TOKEN(
                STRING,
                CREATE_CP(),
                str_start,
                str - str_start
              ));
            }
          else if (*str == '\'')
            {
              DBCC_ErrorCode code;
              const char *str_start = str;
              if (!scan_character (&str, &column, &line_no, end, &code))
                {
                  assert(0);
                }
              APPEND_TOKEN(CPP_TOKEN(
                CHAR,
                CREATE_CP(),
                str_start,
                str - str_start
              ));
            }
          else 
            {
              CPP_NumberParseResult res = try_number (str, end);
              switch (res.type)
                {
                case CPP_NUMBER_PARSE_NOTHING:
                  break;
                case CPP_NUMBER_PARSE_BAD_NUMBER:
                  {
                    DBCC_Error *error = dbcc_error_new (res.v_bad_number.error,
                                                        "bad numeric constant: %s",
                                                        res.v_bad_number.message);
                    dbcc_error_add_code_position (error, CREATE_CP());
                    parser->handlers.handle_error(error, parser->handler_data);
                    dbcc_error_unref (error);
                    return false;
                  }
                case CPP_NUMBER_PARSE_GOT_NUMBER:
                  {
                    APPEND_TOKEN(CPP_TOKEN(
                      NUMBER,
                      CREATE_CP(),
                      str,
                      res.v_number.number_length
                    ));
                    str += res.v_number.number_length;
                    column += res.v_number.number_length;
                  }
                }

#if SUPPORT_TRIGRAPHS
              if (str[0] == '?' && str + 2 < end && str[1] == '?')
                {
                  /*
                    ??=      #           
                    ??(      [           
                    ??<      { 
                    ??/      \
                    ??)      ]           
                    ??>      }
                    ??’      ˆ           
                    ??!      |           
                    ??-      ˜
                   */
                  trigraph characters
                }
#endif

              ScanPunctuatorResult punc_res = scan_punctuator (str, end);
              switch (punc_res.type)
                {
                case SCAN_PUNCTUATOR_RESULT_NO:
                  break;
                case SCAN_PUNCTUATOR_RESULT_SUCCESS:
                  {
                    unsigned len = punc_res.v_success.length;
                    CPP_Token token = CPP_TOKEN(
                      OPERATOR,
                      CREATE_CP(),
                      str,
                      len
                    );
                    if (len == 1 && str[0] == '#')
                      token.type = CPP_TOKEN_HASH;
                    else if (len == 2 && str[0] == '#' && str[1] == '#')
                      token.type = CPP_TOKEN_CONCATENATE;
                    APPEND_TOKEN(token);
                    str += punc_res.v_success.length;
                    column += punc_res.v_success.length;
                    break;
                  }
                case SCAN_PUNCTUATOR_RESULT_SUCCESS_DIGRAPH:
                  {
                    unsigned len = punc_res.v_success_digraph.length;
                    CPP_Token token = CPP_TOKEN(
                      OPERATOR_DIGRAPH,
                      CREATE_CP(),
                      str,
                      len
                    );
                    if (len == 2 && str[0] == '%' && str[1] == ':')
                      token.type = CPP_TOKEN_HASH;
                    if (len == 4 && str[0] == '%' && str[1] == ':' 
                                 && str[2] == '%' && str[3] == ':')
                      token.type = CPP_TOKEN_CONCATENATE;
                    APPEND_TOKEN(token);
                    str += len;
                    column += len;
                    continue;             // TODO: replace with goto
                  }
                }
              DBCC_Error *e = dbcc_error_new (DBCC_ERROR_UNEXPECTED_CHARACTER,
                                              "unexpected character/byte 0x%02x (%s)",
                                              *str, dsk_ascii_byte_name (*str));
              dbcc_error_add_code_position (e, CREATE_CP());
              parser->handlers.handle_error(e, parser->handler_data);
              return false;
            }
        }
      if (*str == '\n')
        {
          APPEND_CPP_TOKEN(
            NEWLINE,
            CREATE_CP(),
            str,
            1
          );
          column = 1;
          line_no++;
          str++;
        }
    }

#if DUMP_CPP_TOKENS
  for (unsigned i = 0; i < n_cpp_tokens; i++)
    {
      printf("%s: %s:%u:%u: '%.*s'\n",
             cpp_token_type_name (cpp_tokens[i].type),
             dbcc_symbol_get_string (cpp_tokens[i].code_position->filename),
             cpp_tokens[i].code_position->line_no,
             cpp_tokens[i].code_position->column,
             (int) cpp_tokens[i].length,
             cpp_tokens[i].str);

    }
#endif


  /* Now, do the preprocessing.
   * Resultant tokens are pumped directly into lemon-generated parser
   * (they are actually created on the stack in the same location,
   * but the lemon-based parser will copy the token as needed)
   *   (1) macro expansion is handled here
   *   (2) #if's are handled here
   *   (3) #include directives are handled by recursion
   */
  bool at_line_start = true;
  // This will be set true if there is an #if/#ifdef/#ifndef at start of file,
  // and there are no #else/#elif's at top-level, and there is nothing
  // after the final #endif
  bool guarded = false;
  unsigned at = 0;
  while (at < n_cpp_tokens && cpp_tokens[at].type == CPP_TOKEN_NEWLINE)
    at++;

  CPP_Expr guard_expr;

  if (n_cpp_tokens >= 5
   && cpp_tokens[at].type == CPP_TOKEN_HASH
   && cpp_tokens[at+1].type == CPP_TOKEN_BAREWORD
   && is_if_like_directive_identifier (cpp_tokens + at + 1))
    {
      CPP_Expr cpp_expr;
      DBCC_Error *error = NULL;
      guarded = true;
      unsigned n_expr_tokens = parse_cpp_expr (n_cpp_tokens - at - 1,
                                               cpp_tokens + at + 1,
                                               &guard_expr, &error);
      if (n_expr_tokens == 0)
        {
          parser->handlers.handle_error (error, parser->handler_data);
          return false;
        }
      at += n_expr_tokens + 2;  /* expression tokens plus '#' and if/ifdef/ifndef */

      /* evaluate expr to see if we are live */
      if (!eval_cpp_expr_boolean (parser, &guard_expr, &result, &error))
        {
          parser->handlers.handle_error (error, parser->handler_data);
          return false;
        }

      stack_statuses[0] = result ? CPP_STACK_ACTIVE : CPP_STACK_ALWAYS_INACTIVE;
      level = 1;
    }
  while (at < n_cpp_tokens)
    {
      if (cpp_tokens[at].type == CPP_TOKEN_NEWLINE)
        at++;
      if (guarded && level == 0 && at  )
        {
          /* extra tokens found after last endif */
          guarded = false;
        }
      if (at_line_start && cpp_tokens[at].type == CPP_TOKEN_HASH
       && at + 1 < n_cpp_tokens
       && cpp_tokens[at+1].type == CPP_TOKEN_BAREWORD)
        {
          if (is_if_like_directive_identifier (cpp_tokens + at + 1))
            {
              CPP_Expr cpp_expr;
              DBCC_Error *error = NULL;
              guarded = true;
              unsigned n_expr_tokens = parse_cpp_expr (n_cpp_tokens - at - 1,
                                                       cpp_tokens + at + 1,
                                                       &cpp_expr, &error);
              if (n_expr_tokens == 0)
                {
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              at += n_expr_tokens + 2;  /* expression tokens plus '#' and if/ifdef/ifndef */

              /* evaluate expr to see if we are live */
              if (!eval_cpp_expr_boolean (parser, &cpp_expr, &result, &error))
                {
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }

              if (stack_statuses[level-1] != CPP_STACK_ACTIVE)
                stack_statuses[level] = CPP_STACK_INACTIVE_PARENT;
              else if (result)
                stack_statuses[level] = CPP_STACK_ACTIVE;
              else
                stack_statuses[level] = CPP_STACK_ALWAYS_INACTIVE;
              level = 1;
            }
          else if (cpp_tokens[at+1].length == 4
               &&  memcmp (cpp_tokens[at+1].str, "else", 4) == 0)
            {
              if (at + 2 >= n_cpp_tokens)
                {
                  ...eof
                }
              if (cpp_tokens[at+2].type == CPP_TOKEN_NEWLINE)
                {
                  ...
                }
              if (level == 0)
                {
                  ... no match if
                }
              switch (stack_statuses[level-1])
                {
                case CPP_STACK_INACTIVE_PARENT:
                  break;
                case CPP_STACK_ACTIVE:
                  stack_statuses[level] = CPP_STACK_ELSE_INACTIVE;
                  break;
                case CPP_STACK_ALWAYS_INACTIVE:
                  stack_statuses[level] = CPP_STACK_ELSE_ACTIVE;
                  break;
                case CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE:
                  stack_statuses[level] = CPP_STACK_ELSE_INACTIVE;
                  break;

                case CPP_STACK_ELSE_ACTIVE:
                case CPP_STACK_ELSE_INACTIVE:
                  *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_ELSE_NOT_ALLOWED,
                                           "already had #else directive");
                  dbcc_error_add_code_position (*error, cpp_tokens[at+1].code_position);
                  return false;
                }
            }
          else if (cpp_tokens[at+1].length == 4
               &&  memcmp (cpp_tokens[at+1].str, "elif", 4) == 0)
            {
              ... parse cpp expr
              switch (stack_statuses[level-1])
                {
                case CPP_STACK_INACTIVE_PARENT:
                  break;
                case CPP_STACK_ACTIVE:
                  stack_statuses[level] = CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE;
                  break;
                case CPP_STACK_ALWAYS_INACTIVE:
                  if (!eval_cpp_expr_boolean (parser, &cpp_expr, &result, &error))
                    {
                      parser->handlers.handle_error (error, parser->handler_data);
                      return false;
                    }
                  ACTIVE or ALWAYS_INACTIVE
                  break;
                case CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE:
                  /* no change */
                  break;

                case CPP_STACK_ELSE_ACTIVE:
                case CPP_STACK_ELSE_INACTIVE:
                  *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_ELSE_NOT_ALLOWED,
                                           "already had #else directive");
                  dbcc_error_add_code_position (*error, cpp_tokens[at+1].code_position);
                  return false;
                }
            }
          else if (cpp_tokens[at+1].length == 6
               &&  memcmp (cpp_tokens[at+1].str, "define", 6) == 0)
            {
              if (at + 2 == n_cpp_tokens)
                {
                  *error = dbcc_error_new (DBCC_ERROR_UNEXPECTED_EOF,
                                           "missing name after #define");
                  dbcc_error_add_code_position (*error, cpp_tokens[at+1].code_position);
                  return false;
                }
              if (cpp_tokens[at + 2].type != CPP_TOKEN_BAREWORD)
                {
                  *error = dbcc_error_new (DBCC_ERROR_UNEXPECTED_EOF,
                                           "missing name after #define, got %s",
                                           cpp_token_type_name (cpp_tokens[at + 2].type));
                  dbcc_error_add_code_position (*error, cpp_tokens[at+1].code_position);
                  return false;
                }
            }
          else if (cpp_tokens[at+1].length == 5
               &&  memcmp (cpp_tokens[at+1].str, "endif", 5) == 0)
            {
              if (level == 0)
                {
                  *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_UNMATCHED_ENDIF,
                                           "no #if/ifdef/ifndef for #endif");
                  dbcc_error_add_code_position (*error, cpp_tokens[at+1].code_position);
                  return false;
                }
              ...
            }
          else
            {
              ...
            }
        }
      else
        {
          /* Emit P_Token */
          ...
        }
    }
  return true;

#undef CREATE_CP
#undef APPEND_TOKEN
#undef APPEND_CPP_TOKEN
}

void
dbcc_parser_destroy         (DBCC_Parser   *parser)
{
  ...
}

