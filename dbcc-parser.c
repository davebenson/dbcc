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
#include "dbcc-parser-p.h"
#include "p-token.h"
#include "dsk/dsk-rbtree-macros.h"
#include "cpp-expr-number.h"
#include "cpp-expr-evaluate-p.h"
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

/* Hackery used to implement lexing two-character combos with switch statements.
 * Combines two ASCII characters into an integer.
 */
#define COMBINE_2_CHARS(a,b)   ((((a) & 0xff) << 8) | ((b) & 0xff))


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
static CPP_Expr *copy_cpp_expr_densely (CPP_Expr *expr);

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
  DBCC_TargetEnvironment *target_environment;

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

#define parser_get_ns(parser)      ((parser)->globals)


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
  if ((t->length == 2 && memcmp (t->str, "if", 2) == 0)
   || (t->length == 4 && memcmp (t->str, "elif", 4) == 0))
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
 *
 * The semantics of these expressions is given in Section 6.6.
 */
static bool
tokens_to_boolean_value (unsigned     n_tokens,
                         CPP_Token   *tokens,
                         bool        *result_out,
                         DBCC_Error **error)
{
  void *lemon_parser = DBCC_CPPExpr_EvaluatorAlloc(malloc);
  unsigned i;
  CPP_EvalParserResult eval_result = CPP_EVAL_PARSER_RESULT_INIT;
  for (i = 0; i < n_tokens; i++)
    {
      CPP_Expr_Result res = { CPP_EXPR_RESULT_INT64, .v_int64 = 0 };
      switch (tokens[i].type)
        {
        case CPP_TOKEN_CHAR:
          {
            uint32_t v;
            size_t sizeof_char;
            if (!dbcc_common_char_constant_value (tokens[i].length,
                                                  tokens[i].str,
                                                  &v,
                                                  &sizeof_char,
                                                  error))
              {
                dbcc_error_add_code_position (*error, tokens[i].code_position);
                return false;
              }
            res.v_int64 = v;
            DBCC_CPPExpr_Evaluator(lemon_parser, CPP_EXPR_NUMBER, res, &eval_result);
          }
          break;
        case CPP_TOKEN_NUMBER:
          {
            int64_t val;
            if (!dbcc_common_number_is_integral (tokens[i].length, tokens[i].str))
              {
                *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_INTEGERS_ONLY,
                                         "preprocessor will not handle floating-point number");
                dbcc_error_add_code_position (*error, tokens[i].code_position);
                return false;
              }
            else if (!dbcc_common_number_parse_int64 (tokens[i].length, tokens[i].str, &val, error))
              {
                dbcc_error_add_code_position (*error, tokens[i].code_position);
                return false;
              }
            else
              {
                res.v_int64 = val;
                DBCC_CPPExpr_Evaluator(lemon_parser, CPP_TOKEN_NUMBER, res, &eval_result);
              }
            break;
          }
        case CPP_TOKEN_OPERATOR:
          {
            int et;
            switch (tokens[i].length)
              {
              case 1:
                switch (tokens[i].str[0])
                  {
                  case '|': et = CPP_EXPR_BITWISE_OR; break;
                  case '&': et = CPP_EXPR_BITWISE_AND; break;
                  case '+': et = CPP_EXPR_PLUS; break;
                  case '-': et = CPP_EXPR_MINUS; break;
                  case '*': et = CPP_EXPR_STAR; break;
                  case '/': et = CPP_EXPR_SLASH; break;
                  case '%': et = CPP_EXPR_PERCENT; break;
                  case '!': et = CPP_EXPR_BANG; break;
                  case '~': et = CPP_EXPR_TILDE; break;
                  case '<': et = CPP_EXPR_LT; break;
                  case '>': et = CPP_EXPR_GT; break;
                  case '(': et = CPP_EXPR_LPAREN; break;
                  case ')': et = CPP_EXPR_RPAREN; break;
                  default: goto invalid_operator;
                  }
                break;
              case 2:
                switch (COMBINE_2_CHARS(tokens[i].str[0], tokens[i].str[1]))
                  {
                  case COMBINE_2_CHARS('<', '<'): et = CPP_EXPR_LTLT; break;
                  case COMBINE_2_CHARS('>', '>'): et = CPP_EXPR_GTGT; break;
                  case COMBINE_2_CHARS('&', '&'): et = CPP_EXPR_LOGICAL_AND; break;
                  case COMBINE_2_CHARS('|', '|'): et = CPP_EXPR_LOGICAL_OR; break;
                  case COMBINE_2_CHARS('<', '='): et = CPP_EXPR_LTEQ; break;
                  case COMBINE_2_CHARS('>', '='): et = CPP_EXPR_GTEQ; break;
                  case COMBINE_2_CHARS('=', '='): et = CPP_EXPR_EQ; break;
                  case COMBINE_2_CHARS('!', '='): et = CPP_EXPR_NEQ; break;
                  default: goto invalid_operator;
                  }
              default: goto invalid_operator;
              }
            break;
          }

        case CPP_TOKEN_OPERATOR_DIGRAPH:
          // None of the allowed operators are digraphs.
          goto invalid_operator;

        case CPP_TOKEN_BAREWORD:
          DBCC_CPPExpr_Evaluator(lemon_parser, CPP_EXPR_IDENTIFIER, res, &eval_result);
          break;

        default:
          *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_INTERNAL,
                                   "unexpected token-type should not have reached here (%u)",
                                   tokens[i].type);
          dbcc_error_add_code_position (*error, tokens[i].code_position);
          return false;
        }
    }

  // end parsing
  CPP_Expr_Result res = { CPP_EXPR_RESULT_INT64, .v_int64 = 0 };
  DBCC_CPPExpr_Evaluator(lemon_parser, 0, res, &eval_result);
  if (res.type == CPP_EXPR_RESULT_FAIL)
    {
      *error = res.v_error;
      return false;
    }
  if (eval_result.error != NULL)
    {
      *error = eval_result.error;
      return false;
    }
  assert(eval_result.finished);
  DBCC_CPPExpr_EvaluatorFree(lemon_parser, free);
  *result_out = eval_result.bool_result;
  return true;


invalid_operator:
  *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_INVALID_OPERATOR,
                           "not a valid operator in a preprocessor subexpression");
  dbcc_error_add_code_position (*error, tokens[i].code_position);
  return false;
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
      kw = dbcc_symbol_space_force_len (parser->symbol_space,
                                    expr->tokens[0].length,
                                    expr->tokens[0].str);
      *result_out = lookup_macro (parser, kw) == NULL;
      return true;

    case CPP_EXPR_IFDEF:
      assert(expr->n_tokens == 1);
      assert(expr->tokens[0].type == CPP_TOKEN_BAREWORD);
      kw = dbcc_symbol_space_force_len (parser->symbol_space,
                                    expr->tokens[0].length,
                                    expr->tokens[0].str);
      *result_out = lookup_macro (parser, kw) != NULL;
      return true;

    case CPP_EXPR_IF:
      break;
    }

  // Handle 'defined(Keyword)' (parens are optional),
  // because we must handle it before macro expansion.
  unsigned i;
  for (i = 0; i < expr->n_tokens; )
    {
      if (expr->tokens[i].type == CPP_TOKEN_BAREWORD
       && expr->tokens[i].length == 7
       && memcmp (expr->tokens[i].str, "defined", 7) == 0)
        {
          if (i + 3 < expr->n_tokens 
           && expr->tokens[i+1].type == CPP_TOKEN_OPERATOR
           && expr->tokens[i+1].length == 1
           && expr->tokens[i+1].str[0] == '('
           && expr->tokens[i+2].type == CPP_TOKEN_BAREWORD
           && expr->tokens[i+3].type == CPP_TOKEN_OPERATOR
           && expr->tokens[i+3].length == 1
           && expr->tokens[i+3].str[0] == ')')
            {
              DBCC_Symbol *s = dbcc_symbol_space_try_len (parser->symbol_space, expr->tokens[i+2].length, expr->tokens[i+2].str);
              bool val = (s != NULL && lookup_macro (parser, s) != NULL);
              CPP_Token replace = {
                CPP_TOKEN_NUMBER,
                expr->tokens[i+2].code_position,
                val ? "1" : "0",
                1,
                0
              };
              dbcc_code_position_unref (expr->tokens[i].code_position);
              dbcc_code_position_unref (expr->tokens[i+1].code_position);
              dbcc_code_position_unref (expr->tokens[i+3].code_position);
              expr->tokens[i] = replace;
              memmove (expr->tokens + i + 1,
                       expr->tokens + i + 4,
                       (expr->n_tokens - i - 4) * sizeof (CPP_Token));
              expr->n_tokens -= 3;
            }
          else if (i + 1 < expr->n_tokens
            && expr->tokens[i+1].type == CPP_TOKEN_BAREWORD)
            {
              DBCC_Symbol *s = dbcc_symbol_space_try_len (parser->symbol_space, expr->tokens[i+1].length, expr->tokens[i+1].str);
              bool val = (s != NULL && lookup_macro (parser, s) != NULL);
              CPP_Token replace = {
                CPP_TOKEN_NUMBER,
                expr->tokens[i+1].code_position,
                val ? "1" : "0",
                1,
                0
              };
              dbcc_code_position_unref (expr->tokens[i].code_position);
              expr->tokens[i] = replace;
              memmove (expr->tokens + i + 1,
                       expr->tokens + i + 2,
                       (expr->n_tokens - i - 2) * sizeof (CPP_Token));
              expr->n_tokens -= 1;
            }
          else
            {
              *error_out = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_SYNTAX,
                                       "expected identifier after 'defined' in preprocessor expression");
              dbcc_error_add_code_position (*error_out, expr->tokens[i].code_position);
              return false;
            }
        }
      else
        i++;
    }

  res = expand_macros (parser, expr->n_tokens, expr->tokens);
  unsigned n_tokens;
  CPP_Token *tokens;
  switch (res.type)
    {
      case CPP_MACRO_EXPANSION_RESULT_NO_CHANGE:
        n_tokens = expr->n_tokens;
        tokens = expr->tokens;
        break;

      case CPP_MACRO_EXPANSION_RESULT_SUCCESS:
        n_tokens = res.v_success.n_expanded_tokens;
        tokens = res.v_success.expanded_tokens;
        break;

      case CPP_MACRO_EXPANSION_RESULT_ERROR:
        *error_out = res.v_error.error;
        return false;
    }
  bool rv = tokens_to_boolean_value (n_tokens, tokens, result_out, error_out);
  return rv;
}


static bool
convert_cpp_token_operator_to_ptokentype (CPP_Token   *token,
                                          int         *token_type_out,
                                          DBCC_Error **error)
{
  switch (token->length)
    {
    case 1:
      switch (token->str[0])
        {
        case '(': *token_type_out = P_TOKEN_LPAREN; return true;
        case ')': *token_type_out = P_TOKEN_RPAREN; return true;
        case '{': *token_type_out = P_TOKEN_LBRACE; return true;
        case '}': *token_type_out = P_TOKEN_RBRACE; return true;
        case '[': *token_type_out = P_TOKEN_LBRACKET; return true;
        case ']': *token_type_out = P_TOKEN_RBRACKET; return true;
        case '<': *token_type_out = P_TOKEN_LESS_THAN; return true;
        case '>': *token_type_out = P_TOKEN_GREATER_THAN; return true;
        case '!': *token_type_out = P_TOKEN_EXCLAMATION_POINT; return true;
        case '%': *token_type_out = P_TOKEN_PERCENT; return true;
        case '^': *token_type_out = P_TOKEN_CARAT; return true;
        case '&': *token_type_out = P_TOKEN_AMPERSAND; return true;
        case '*': *token_type_out = P_TOKEN_ASTERISK; return true;
        case '|': *token_type_out = P_TOKEN_VERTICAL_BAR; return true;
        case ';': *token_type_out = P_TOKEN_SEMICOLON; return true;
        case ',': *token_type_out = P_TOKEN_COLON; return true;
        case '.': *token_type_out = P_TOKEN_PERIOD; return true;
        case '/': *token_type_out = P_TOKEN_SLASH; return true;
        case '?': *token_type_out = P_TOKEN_QUESTION_MARK; return true;
        case '+': *token_type_out = P_TOKEN_PLUS; return true;
        case '-': *token_type_out = P_TOKEN_MINUS; return true;
        case '=': *token_type_out = P_TOKEN_EQUAL_SIGN; return true;
        case '~': *token_type_out = P_TOKEN_TILDE; return true;
        }
      goto not_a_smooth_operator;

    case 2:
      switch (COMBINE_2_CHARS(token->str[0], token->str[1]))
        {
        // comparison operators
        case COMBINE_2_CHARS('<', '<'): *token_type_out = P_TOKEN_SHIFT_LEFT; return true;
        case COMBINE_2_CHARS('>', '>'): *token_type_out = P_TOKEN_SHIFT_RIGHT; return true;
        case COMBINE_2_CHARS('!', '='): *token_type_out = P_TOKEN_NOT_EQUAL_TO; return true;
        case COMBINE_2_CHARS('=', '='): *token_type_out = P_TOKEN_IS_EQUAL_TO; return true;

        // increment/decrement
        case COMBINE_2_CHARS('-', '-'): *token_type_out = P_TOKEN_INCREMENT; return true;
        case COMBINE_2_CHARS('+', '+'): *token_type_out = P_TOKEN_DECREMENT; return true;

        // 2-character assignment operator
        case COMBINE_2_CHARS('*', '='): *token_type_out = P_TOKEN_MULTIPLY_ASSIGN; return true;
        case COMBINE_2_CHARS('/', '='): *token_type_out = P_TOKEN_DIVIDE_ASSIGN; return true;
        case COMBINE_2_CHARS('+', '='): *token_type_out = P_TOKEN_ADD_ASSIGN; return true;
        case COMBINE_2_CHARS('-', '='): *token_type_out = P_TOKEN_SUBTRACT_ASSIGN; return true;
        case COMBINE_2_CHARS('%', '='): *token_type_out = P_TOKEN_REMAINDER_ASSIGN; return true;
        case COMBINE_2_CHARS('^', '='): *token_type_out = P_TOKEN_BITWISE_XOR_ASSIGN; return true;
        case COMBINE_2_CHARS('|', '='): *token_type_out = P_TOKEN_BITWISE_OR_ASSIGN; return true;
        case COMBINE_2_CHARS('&', '='): *token_type_out = P_TOKEN_BITWISE_AND_ASSIGN; return true;
        }
      goto not_a_smooth_operator;

    case 3:
      if (memcmp (token->str, "<<=" , 3) == 0)
        {
          *token_type_out = P_TOKEN_SHIFT_LEFT_ASSIGN;
          return true;
        }
      else if (memcmp (token->str, ">>=" , 3) == 0)
        {
          *token_type_out = P_TOKEN_SHIFT_RIGHT_ASSIGN;
          return true;
        }
      else
        goto not_a_smooth_operator;

    not_a_smooth_operator:              // haha: bad operator error
    default:
      *error = dbcc_error_new (DBCC_ERROR_BAD_OPERATOR,
                               "unrecognized operator '%.*s' found",
                               (int) token->length,
                               token->str);
      dbcc_error_add_code_position (*error, token->code_position);
      return false;
    }
}

static bool
convert_cpp_token_digraph_operator_to_ptokentype (CPP_Token   *token,
                                                  int         *token_type_out,
                                                  DBCC_Error **error)
{
  if (token->length != 2)
    {
      *error = dbcc_error_new (DBCC_ERROR_BAD_OPERATOR,
                               "invalid digraph operator length (must be 2, was %u)",
                               (unsigned)(token->length));
      dbcc_error_add_code_position (*error, token->code_position);
      return false;
    }
  switch (token->str[0])
    {
    case '<':
      if (token->str[1] == ':') { *token_type_out = P_TOKEN_LBRACKET; return true; }
      if (token->str[1] == '%') { *token_type_out = P_TOKEN_LBRACE; return true; }
      break;
    case '%':
      if (token->str[1] == '>') { *token_type_out = P_TOKEN_RBRACE; return true; }
      break;
    case ':':
      if (token->str[1] == '>') { *token_type_out = P_TOKEN_RBRACKET; return true; }
      break;
    }
  *error = dbcc_error_new (DBCC_ERROR_BAD_OPERATOR,
                           "invalid digraph operator: '%.*s'",
                           (int)(token->length), token->str);
  dbcc_error_add_code_position (*error, token->code_position);
  return false;
}

/* probably over-optimized :) */
static bool
is_reserved_word (DBCC_Symbol *symbol,
                  int *p_token_type_out)
{
  size_t length = symbol->length;
  const char *str = dbcc_symbol_get_string (symbol);
#define TEST_AND_RETURN(namestr, shortname)    \
  if (memcmp(str, namestr, length) == 0)       \
    {                                          \
      *p_token_type_out = P_TOKEN_##shortname; \
      return true;                             \
    }
  switch (length)
    {
    case 2:
      switch (str[0])
        {
        case 'd':
          if (str[1] == 'o') { *p_token_type_out = P_TOKEN_DO; return true; }
          return false;
        case 'i':
          if (str[1] == 'f') { *p_token_type_out = P_TOKEN_IF; return true; }
          return false;
        }
      return false;

    case 3:
      switch (str[0])
        {
        case 'f':
          if (str[1] == 'o' && str[2] == 'r') { *p_token_type_out = P_TOKEN_FOR; return true; }
          return false;
        case 'i':
          if (str[1] == 'n' && str[2] == 't') { *p_token_type_out = P_TOKEN_INT; return true; }
          return false;
        }
      return false;

    case 4:
      switch (str[0])
        {
        case 'a':
          TEST_AND_RETURN("auto", AUTO);
          return false;
        case 'c':
          TEST_AND_RETURN("case", CASE);
          TEST_AND_RETURN("char", CHAR);
          return false;
        case 'e':
          TEST_AND_RETURN("else", ELSE);
          TEST_AND_RETURN("enum", ENUM);
          return false;
        case 'g':
          TEST_AND_RETURN("goto", GOTO);
          return false;
        case 'l':
          TEST_AND_RETURN("long", LONG);
          return false;
        case 'v':
          TEST_AND_RETURN("void", VOID);
          return false;
        }
      return false;

    case 5:
      switch (str[0])
        {
        case '_':
          TEST_AND_RETURN("_Bool", BOOL);
          return false;
        case 'b':
          TEST_AND_RETURN("break", BREAK);
          return false;
        case 'c':
          TEST_AND_RETURN("const", CONST);
          return false;
        case 'f':
          TEST_AND_RETURN("float", FLOAT);
          return false;
        case 's':
          TEST_AND_RETURN("short", SHORT);
          return false;
        case 'u':
          TEST_AND_RETURN("union", UNION);
          return false;
        case 'w':
          TEST_AND_RETURN("while", WHILE);
          return false;
        }
      return false;

    case 6:
      switch (str[0])
        {
        case 'd':
          TEST_AND_RETURN("double", DOUBLE);
          return false;
        case 'e':
          TEST_AND_RETURN("extern", EXTERN);
          return false;
        case 'i':
          TEST_AND_RETURN("inline", INLINE);
          return false;
        case 'r':
          TEST_AND_RETURN("return", RETURN);
          return false;
        case 's':
          TEST_AND_RETURN("signed", SIGNED);
          TEST_AND_RETURN("sizeof", SIZEOF);
          TEST_AND_RETURN("static", STATIC);
          TEST_AND_RETURN("struct", STRUCT);
          TEST_AND_RETURN("switch", SWITCH);
          return false;
        }
      return false;

    case 7:
      switch (str[0])
        {
        case '_':
          TEST_AND_RETURN("_Atomic", ATOMIC);
          return false;
        case 'a':
          TEST_AND_RETURN("alignof", ALIGNOF);
          return false;
        case 'd':
          TEST_AND_RETURN("default", DEFAULT);
          return false;
        case 't':
          TEST_AND_RETURN("typedef", TYPEDEF);
          return false;
        }
      return false;

    case 8:
      switch (str[0])
        {
        case '_':
          TEST_AND_RETURN("_Alignas", ALIGNAS);
          TEST_AND_RETURN("_Complex", COMPLEX);
          TEST_AND_RETURN("_Generic", GENERIC);
          return false;
        case 'c':
          TEST_AND_RETURN("continue", CONTINUE);
          return false;
        case 'r':
          TEST_AND_RETURN("register", REGISTER);
          TEST_AND_RETURN("restrict", RESTRICT);
          return false;
        case 'u':
          TEST_AND_RETURN("unsigned", UNSIGNED);
          return false;
        case 'v':
          TEST_AND_RETURN("volatile", VOLATILE);
          return false;
        }
      return false;

    case 9:
      TEST_AND_RETURN ("_Noreturn", NORETURN);
      return false;

    case 10:
      TEST_AND_RETURN ("_Imaginary", IMAGINARY);
      return false;

    case 13:
      TEST_AND_RETURN ("_Thread_local", THREAD_LOCAL);
      return false;

    case 14:
      TEST_AND_RETURN ("_Static_assert", STATIC_ASSERT);
      return false;
    }
#undef TEST_AND_RETURN

  return false;
}

typedef enum
{
  CPP_STACK_INACTIVE_PARENT,

  CPP_STACK_INACTIVE_SO_FAR,           // parent active, child inactive
  CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE,
  CPP_STACK_ACTIVE,

  // in else clause - so another #else is not allowed
  CPP_STACK_ACTIVE_ELSE,
  CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE_ELSE,
} CPP_StackFrameStatus;

static inline bool
is_active_cpp_stack_state (CPP_StackFrameStatus status)
{
  return status == CPP_STACK_ACTIVE || status == CPP_STACK_ACTIVE_ELSE;
}

static bool
is_whitespace (const char *str, const char *end)
{
  const char *at = str;
  while (at < end && isspace (*at))
    at++;
  return at == end;
}

/* Handle #line directive.
 * See 6.10.4.  Line Control.
 */
static void
scan_maybe_hash_line (DBCC_Parser *parser,
                      const char *str,
                      const char *endline,
                      DBCC_Symbol **filename_symbol_inout,
                      unsigned   *lineno_inout,
                      unsigned   *column_inout)
{
  assert(*str == '#');
  str++;
  while (str < endline && isspace (*str))
    str++;
  if (str == endline)
    return;
  if (memcmp (str, "line", 4) != 0)
    return;

  /* Skip "line" and following space */
  str += 4;
  while (str < endline && isspace (*str))
    str++;

  /* Parse line number */
  char *end_number;
  *lineno_inout = strtoul (str, &end_number, 10);
  *column_inout = 1;
  str = end_number;

  /* Optional filename (in quotes) */
  while (str < endline && isspace (*str))
    str++;
  if (*str == '"')
    {
      str++;
      const char *end = memchr (str, '"', endline - str);
      if (end == NULL)
        return;
      DBCC_Symbol *sym = dbcc_symbol_space_force_len (parser->symbol_space,
                                                      end - str, str);
      *filename_symbol_inout = sym;
    }
}

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

  unsigned preproc_conditional_stack_alloced = 32;
  CPP_StackFrameStatus *preproc_conditional_stack = malloc (sizeof (CPP_StackFrameStatus) * preproc_conditional_stack_alloced);
  unsigned preproc_conditional_level = 0;
#define PREPROC_TOP   preproc_conditional_stack[preproc_conditional_level-1]
#define PREPROC_NEXT_TOP   preproc_conditional_stack[preproc_conditional_level]
  
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
  const char *last_newline = NULL;
  while (str < end)
    {
      const char *end_line = memchr (str, '\n', end - str);
      if (end_line == NULL)
        {
          DBCC_Error *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_INCOMPLETE_LINE,
                                              "line without terminating newline");
          parser->handlers.handle_error (error, parser->handler_data);
          return false;
        }
      while (str < end && *str != '\n')
        {
          if (*str == '#')
            {
              if (last_newline == NULL || is_whitespace (last_newline+1, str))
                {
                  scan_maybe_hash_line (parser, str, end_line,
                                        &filename_symbol,
                                        &line_no, &column);
                }
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
          last_newline = str;
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
  CPP_Expr *guard = NULL;
  bool is_first = true;
  bool past_last_endif = false;
  
  unsigned at = 0;

#define EMIT_PTOKEN_TO_PARSER(token) \
  do{ \
    DBCC_Lemon_Parser(parser->lemon_parser, \
                      (token).token_type, token, \
                      parser->context); \
  }while(0)

  while (at < n_cpp_tokens)
    {
      if (cpp_tokens[at].type == CPP_TOKEN_NEWLINE)
        at++;
      /* Some semantics of if-then-else directives is found in 6.10.1p6 */
      if (at_line_start && cpp_tokens[at].type == CPP_TOKEN_HASH
       && at + 1 < n_cpp_tokens
       && cpp_tokens[at+1].type == CPP_TOKEN_BAREWORD)
        {
          if (is_if_like_directive_identifier (cpp_tokens + at + 1))
            {
              CPP_Expr cpp_expr;
              DBCC_Error *error = NULL;
              unsigned n_expr_tokens = parse_cpp_expr (n_cpp_tokens - at - 1,
                                                       cpp_tokens + at + 1,
                                                       &cpp_expr, &error);
              if (n_expr_tokens == 0)
                {
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              at += n_expr_tokens + 2;  /* expression tokens plus '#' and if/ifdef/ifndef */

              if (is_first)
                {
                  // completely densely copy cpp expr
                  guard = copy_cpp_expr_densely (&cpp_expr);
                }

              /* evaluate expr to see if we are live */
              bool result;
              if (!eval_cpp_expr_boolean (parser, &cpp_expr, &result, &error))
                {
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }

              if (preproc_conditional_level > 0 && !is_active_cpp_stack_state(PREPROC_TOP))
                PREPROC_NEXT_TOP = CPP_STACK_INACTIVE_PARENT;
              else if (result)
                PREPROC_NEXT_TOP = CPP_STACK_ACTIVE;
              else
                PREPROC_NEXT_TOP = CPP_STACK_INACTIVE_SO_FAR;
              preproc_conditional_level += 1;
            }
          else if (cpp_tokens[at+1].length == 4
               &&  memcmp (cpp_tokens[at+1].str, "else", 4) == 0)
            {
              if (at + 2 >= n_cpp_tokens)
                {
                  DBCC_Error *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_INCOMPLETE_LINE,
                                           "incomplete line after #else");
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              if (cpp_tokens[at+2].type != CPP_TOKEN_NEWLINE)
                {
                  DBCC_Error *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_SYNTAX,
                                           "token after #else (type %s)",
                                           cpp_token_type_name (cpp_tokens[at+2].type));
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              if (preproc_conditional_level == 0)
                {
                  DBCC_Error *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_UNMATCHED_ELSE,
                                                      "got #else directive without corresponding #if");
                  dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              at += 3;
              switch (PREPROC_TOP)
                {
                case CPP_STACK_INACTIVE_PARENT:
                  break;
                case CPP_STACK_ACTIVE:
                  PREPROC_NEXT_TOP = CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE_ELSE;
                  break;
                case CPP_STACK_INACTIVE_SO_FAR:
                  PREPROC_NEXT_TOP = CPP_STACK_ACTIVE_ELSE;
                  break;
                case CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE:
                  PREPROC_NEXT_TOP = CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE_ELSE;
                  break;

                case CPP_STACK_ACTIVE_ELSE:
                case CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE_ELSE:
                  {
                    DBCC_Error *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_ELSE_NOT_ALLOWED,
                                                        "already had #else directive");
                    dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                    parser->handlers.handle_error (error, parser->handler_data);
                  }
                  return false;
                }
            }
          else if (cpp_tokens[at+1].length == 4
               &&  memcmp (cpp_tokens[at+1].str, "elif", 4) == 0)
            {
              CPP_Expr cpp_expr;
              DBCC_Error *error = NULL;
              unsigned n_expr_tokens = parse_cpp_expr (n_cpp_tokens - at - 1,
                                                       cpp_tokens + at + 1,
                                                       &cpp_expr, &error);
              if (n_expr_tokens == 0)
                {
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              if (preproc_conditional_level == 0)
                {
                  error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_ELSE_NOT_ALLOWED,
                                          "#elif encountered at toplevel");
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              at += n_expr_tokens + 2;  /* expression tokens plus '#' and if/ifdef/ifndef */

              switch (PREPROC_TOP)
                {
                case CPP_STACK_INACTIVE_PARENT:
                  break;
                case CPP_STACK_ACTIVE:
                  PREPROC_TOP = CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE;
                  break;
                case CPP_STACK_INACTIVE_SO_FAR:
                  {
                  bool result;
                  if (!eval_cpp_expr_boolean (parser, &cpp_expr, &result, &error))
                    {
                      parser->handlers.handle_error (error, parser->handler_data);
                      return false;
                    }
                  if (result)
                    PREPROC_TOP = CPP_STACK_ACTIVE;
                  else
                    PREPROC_TOP = CPP_STACK_INACTIVE_SO_FAR;
                  break;
                  }

                case CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE:
                  /* no change */
                  break;

                case CPP_STACK_ACTIVE_ELSE:
                case CPP_STACK_INACTIVE_BUT_HAS_BEEN_ACTIVE_ELSE:
                  error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_ELSE_NOT_ALLOWED,
                                           "already had #else directive");
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
            }
          else if (cpp_tokens[at+1].length == 6
               &&  memcmp (cpp_tokens[at+1].str, "define", 6) == 0)
            {
              if (at + 2 == n_cpp_tokens)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_UNEXPECTED_EOF,
                                           "missing name after #define");
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              if (cpp_tokens[at + 2].type != CPP_TOKEN_BAREWORD)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_UNEXPECTED_EOF,
                                           "missing name after #define, got %s",
                                           cpp_token_type_name (cpp_tokens[at + 2].type));
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
            }
          else if (cpp_tokens[at+1].length == 5
               &&  memcmp (cpp_tokens[at+1].str, "endif", 5) == 0)
            {
              if (preproc_conditional_level == 0)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_UNMATCHED_ENDIF,
                                          "no #if/ifdef/ifndef for #endif");
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              if (cpp_tokens[at+2].type == CPP_TOKEN_NEWLINE)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_SYNTAX,
                                          "extra token after #endif: %s",
                                          cpp_token_type_name (cpp_tokens[at+2].type));
                  dbcc_error_add_code_position (error, cpp_tokens[at+2].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }

              preproc_conditional_level--;
              at += 3;
              if (preproc_conditional_level == 0)
                {
                  past_last_endif = true;                         // delete guard if any more tokens found
                  continue;
                }
            }
          else if (cpp_tokens[at+1].length == 4
               &&  memcmp (cpp_tokens[at+1].str, "line", 4) == 0)
            {
              /* #line is handled earlier */
            }
          else if (cpp_tokens[at+1].length == 5
               &&  memcmp (cpp_tokens[at+1].str, "error", 5) == 0)
            {
              /* Section 6.10.5 Error directive */
              const char *msg = cpp_tokens[at+1].str + 5;
              while (*msg != '\n' && isspace (*msg))
                msg++;
              const char *end_msg = strchr (msg, '\n');
              if (end_msg == NULL)
                end_msg = strchr (msg, 0);
              DBCC_Error *error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_HASH_ERROR,
                                  "#error processed: %.*s",
                                  (int)(end_msg - msg), msg);
              dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
              parser->handlers.handle_error (error, parser->handler_data);
              return false;
            }
          else if (cpp_tokens[at+1].length == 5
               &&  memcmp (cpp_tokens[at+1].str, "endif", 5) == 0)
            {
              if (cpp_tokens[at+2].type != CPP_TOKEN_NEWLINE)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_SYNTAX,
                                          "unexpected identifier after #endif");
                  dbcc_error_add_code_position (error, cpp_tokens[at+2].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              if (preproc_conditional_level == 0)
                {
                  DBCC_Error *error;
                  error = dbcc_error_new (DBCC_ERROR_PREPROCESSOR_UNMATCHED_ENDIF,
                                          "#endif without corresponding #if");
                  dbcc_error_add_code_position (error, cpp_tokens[at+1].code_position);
                  parser->handlers.handle_error (error, parser->handler_data);
                  return false;
                }
              preproc_conditional_level -= 1;
              if (preproc_conditional_level == 0)
                past_last_endif = true;
            }
          is_first = false;
          if (past_last_endif && guard != NULL)
            {
              free (guard);
              guard = NULL;
            }
        }
      else
        {
          if (cpp_tokens[at].type != CPP_TOKEN_NEWLINE)
            {
              if (preproc_conditional_level == 0 || is_active_cpp_stack_state (PREPROC_TOP))
                {
                  DBCC_CodePosition *cp = cpp_tokens[at].code_position;
                  P_Token pt;
                  DBCC_Error *error = NULL;
                  switch (cpp_tokens[at].type)
                    {
                    case CPP_TOKEN_HASH:
                      assert(false);
                    case CPP_TOKEN_CONCATENATE:              /* ## */
                      assert(false);
                    case CPP_TOKEN_MACRO_ARGUMENT:
                    case CPP_TOKEN_NEWLINE:
                      assert(false);
                    case CPP_TOKEN_STRING:
                      pt = P_TOKEN_INIT(STRING_LITERAL, cp);
                      if (!dbcc_common_string_literal_value (cpp_tokens[at].length,
                                                             cpp_tokens[at].str,
                                                             &pt.v_string_literal,
                                                             &error))
                       {
                         dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                         parser->handlers.handle_error (error, parser->handler_data);
                         return false;
                       }
                      EMIT_PTOKEN_TO_PARSER(pt);
                      break;
                    case CPP_TOKEN_CHAR:
                      {
                        uint32_t value;
                        size_t sizeof_char;
                        if (!dbcc_common_char_constant_value (cpp_tokens[at].length,
                                                              cpp_tokens[at].str,
                                                              &value,
                                                              &sizeof_char,
                                                              &error))
                          {
                            dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                            parser->handlers.handle_error (error, parser->handler_data);
                            return false;
                          }
                        P_Token t = {
                          .code_position = cpp_tokens[at].code_position,
                          .token_type = P_TOKEN_I_CONSTANT,
                          .v_i_constant.sizeof_value = sizeof_char,
                          .v_i_constant.is_signed = false,
                          .v_i_constant.v_uint64 = value,
                        };
                        EMIT_PTOKEN_TO_PARSER(t);
                      }
                    case CPP_TOKEN_NUMBER:
                      {
                        if (dbcc_common_number_is_integral (cpp_tokens[at].length,
                                                            cpp_tokens[at].str))
                          {
                            size_t sizeof_int_type;
                            bool is_signed;
                            if (!dbcc_common_integer_get_info (parser->target_environment,
                                                               cpp_tokens[at].length,
                                                               cpp_tokens[at].str,
                                                               &sizeof_int_type,
                                                               &is_signed,
                                                               &error))
                              {
                                dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                                parser->handlers.handle_error (error, parser->handler_data);
                                return false;
                              }

                            if (is_signed)
                              {
                                char *end;
                                int64_t v = strtoll (cpp_tokens[at].str, &end, 0);
                                if (cpp_tokens[at].str == end)
                                  {
                                    error = dbcc_error_new (DBCC_ERROR_PARSING_INTEGER,
                                                            "error parsing signed integer");
                                    dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                                    parser->handlers.handle_error (error, parser->handler_data);
                                    return false;
                                  }
                                P_Token t = {
                                  .code_position = cpp_tokens[at].code_position,
                                  .token_type = P_TOKEN_I_CONSTANT,
                                  .v_i_constant.sizeof_value = sizeof_int_type,
                                  .v_i_constant.is_signed = is_signed,
                                  .v_i_constant.v_int64 = v,
                                };
                                EMIT_PTOKEN_TO_PARSER(t);
                              }
                            else
                              {
                                char *end;
                                uint64_t v = strtoull (cpp_tokens[at].str, &end, 0);
                                if (cpp_tokens[at].str == end)
                                  {
                                    error = dbcc_error_new (DBCC_ERROR_PARSING_INTEGER,
                                                            "error parsing unsigned integer");
                                    dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                                    parser->handlers.handle_error (error, parser->handler_data);
                                    return false;
                                  }
                                P_Token t = {
                                  .code_position = cpp_tokens[at].code_position,
                                  .token_type = P_TOKEN_I_CONSTANT,
                                  .v_i_constant.sizeof_value = sizeof_int_type,
                                  .v_i_constant.is_signed = is_signed,
                                  .v_i_constant.v_uint64 = v,
                                };
                                EMIT_PTOKEN_TO_PARSER(t);
                              }
                          }
                        else
                          {
                            char *end;
                            long double v = strtold(cpp_tokens[at].str, &end);
                            if (cpp_tokens[at].str == end)
                              {
                                error = dbcc_error_new (DBCC_ERROR_PARSING_FLOAT,
                                                        "error parsing floating-pointer number");
                                dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                                parser->handlers.handle_error (error, parser->handler_data);
                                return false;
                              }
                            size_t sizeof_float_type;
                            DBCC_Error *error = NULL;
                            if (!dbcc_common_floating_point_get_info(parser->target_environment,
                                                            cpp_tokens[at].length,
                                                            cpp_tokens[at].str,
                                                            &sizeof_float_type,
                                                            &error))
                              {
                                dbcc_error_add_code_position (error, cpp_tokens[at].code_position);
                                parser->handlers.handle_error (error, parser->handler_data);
                                return false;
                              }
                            P_Token t = {
                              .code_position = cpp_tokens[at].code_position,
                              .token_type = P_TOKEN_F_CONSTANT,
                              .v_f_constant.sizeof_value = sizeof_float_type,
                              .v_f_constant.v_long_double = v,
                            };
                            EMIT_PTOKEN_TO_PARSER(t);
                          }
                      }
                      break;

                    case CPP_TOKEN_OPERATOR:
                      {
                        DBCC_Error *error = NULL;
                        memset (&pt, 0, sizeof (P_Token));
                        if (!convert_cpp_token_operator_to_ptokentype (&cpp_tokens[at], &pt.token_type, &error))
                          {
                            parser->handlers.handle_error (error, parser->handler_data);
                            return false;
                          }
                        pt.code_position = cp;
                        EMIT_PTOKEN_TO_PARSER(pt);
                        break;
                      }

                    case CPP_TOKEN_OPERATOR_DIGRAPH:
                      {
                        DBCC_Error *error = NULL;
                        memset (&pt, 0, sizeof (P_Token));
                        if (!convert_cpp_token_digraph_operator_to_ptokentype (&cpp_tokens[at], &pt.token_type, &error))
                          {
                            parser->handlers.handle_error (error, parser->handler_data);
                            return false;
                          }
                        pt.code_position = cp;
                        EMIT_PTOKEN_TO_PARSER(pt);
                        break;
                      }

                    case CPP_TOKEN_BAREWORD:
                      {
                        int token_type;
                        DBCC_Symbol *symbol = dbcc_symbol_space_force_len (parser->symbol_space,
                                                                           cpp_tokens[at].length,
                                                                           cpp_tokens[at].str);
                        if (is_reserved_word (symbol, &token_type))
                          {
                            // known reserved word
                            pt = (P_Token) {.code_position = cpp_tokens[at].code_position,
                                            .token_type = token_type};
                          }
                        else
                          {
                            DBCC_NamespaceEntry ns_entry;
                            if (!dbcc_namespace_lookup (parser_get_ns (parser),
                                                        symbol,
                                                        &ns_entry))
                              {
                                // fallback to IDENTIFIER
                                pt = (P_Token) {.code_position = cpp_tokens[at].code_position,
                                                .token_type = P_TOKEN_IDENTIFIER,
                                                .v_identifier = symbol};
                              }
                            else
                              switch (ns_entry.entry_type)
                                {
                                case DBCC_NAMESPACE_ENTRY_TYPEDEF:
                                  pt = (P_Token) {.code_position = cpp_tokens[at].code_position,
                                                  .token_type = P_TOKEN_TYPEDEF_NAME,
                                                  .v_typedef_name.type = ns_entry.v_typedef,
                                                  .v_typedef_name.name = symbol };
                                  break;
                                case DBCC_NAMESPACE_ENTRY_GLOBAL:
                                  pt = (P_Token) {.code_position = cpp_tokens[at].code_position,
                                                  .token_type = P_TOKEN_IDENTIFIER,
                                                  .v_identifier = symbol};
                                  break;
                                case DBCC_NAMESPACE_ENTRY_ENUM_VALUE:
                                  pt = (P_Token) {.code_position = cpp_tokens[at].code_position,
                                                  .token_type = P_TOKEN_ENUMERATION_CONSTANT,
                                                  .v_enum_value = ns_entry.v_enum_value};
                                  break;
                                default:
                                  assert(0);
                                }
                          }
                        EMIT_PTOKEN_TO_PARSER(pt);
                        break;
                      }
                    }

                }

              is_first = false;
              if (past_last_endif && guard != NULL)
                {
                  free (guard);
                  guard = NULL;
                }
            }
        }
    }
  return true;

#undef CREATE_CP
#undef APPEND_TOKEN
#undef APPEND_CPP_TOKEN
#undef PREPROC_TOP
#undef PREPROC_NEXT_TOP
#undef EMIT_PTOKEN_TO_PARSER
}

void
dbcc_parser_destroy         (DBCC_Parser   *parser)
{
  DBCC_Lemon_ParserFree(parser->lemon_parser, free);
  //TODO free other stuff
  free (parser);
}
