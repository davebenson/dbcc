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
 */

#include "dbcc.h"
#include "dsk/dsk.h"
#include "dbcc-parser.h"
#include "dbcc-parser-lemon.h"
#include "p-token.h"

#define DBCC_PARSER_MAGIC 0xe2315aaf

typedef enum
{
  CPP_TOKEN_HASH_DEFINE,
  CPP_TOKEN_HASH_LINE,
  CPP_TOKEN_HASH_IF,
  CPP_TOKEN_HASH_ELSE,
  CPP_TOKEN_HASH_ELIF,
  CPP_TOKEN_HASH_ENDIF,
  CPP_TOKEN_HASH_INCLUDE,
  CPP_TOKEN_STRING_LITERAL,
  CPP_TOKEN_OPERATOR,
  CPP_TOKEN_BAREWORD,
  CPP_TOKEN_NEWLINE,

  // only found in macros
  CPP_TOKEN_MACRO_ARGUMENT
} CPP_TokenType;

typedef struct CPP_Token CPP_Token;
typedef struct CPP_Macro CPP_Macro;

struct CPP_Token
{
  CPP_TokenType type;
  DBCC_CodePosition *code_position;
  unsigned length;
  unsigned ref_count;
  /* string data follows */
};

static CPP_Token *cpp_token_new (CPP_TokenType      ttype,
                                 DBCC_CodePosition *cp,
                                 const DBCC_String *str);

struct CPP_Macro
{
  DBCC_Symbol *name;
  bool function_macro;
  unsigned arity;
  DBCC_Symbol **args;
  unsigned n_tokens;
  CPP_Token **tokens;
};


struct DBCC_Parser
{
  unsigned magic;
  DBCC_Parser_Handlers handlers;
  void *handler_data;
  DskBuffer buffer;
  P_Context *context;
  void *lemon_parser;
};

DBCC_Parser *
dbcc_parser_new             (DBCC_Parser_Handlers *handlers,
                             void          *handler_data)
{
  DBCC_Parser *rv = malloc (sizeof (DBCC_Parser));
  rv->magic = DBCC_PARSER_MAGIC;
  rv->handlers = *handlers;
  rv->handler_data = handler_data;
  dsk_buffer_init (&rv->buffer);
  rv->context = p_context_new ();
  rv->lemon_parser = DBCC_Lem
  return rv;
}

void
dbcc_parser_add_include_dir (DBCC_Parser   *parser,
                             const char    *dir)
{
  ...
}

void
dbcc_parser_parse_file      (DBCC_Parser   *parser,
                             const char    *filename)
{
  ...
}

void
dbcc_parser_destroy         (DBCC_Parser   *parser)
{
  ...
}

