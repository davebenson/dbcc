/* PRIVATE HEADER:
 *   P_Token is a fully-preprocessed token (with string-literals joined).
 *   P_Context contains references to the namespaces etc.
 */
#ifndef __DBCC_P_TOKEN_H_
#define __DBCC_P_TOKEN_H_

typedef struct P_Token P_Token;
typedef struct P_Context P_Context;


P_Context * p_context_new (DBCC_Namespace *ns);

struct P_Token
{
  DBCC_CodePosition *code_position;

  // this is really an enum - whose values are given by 'lemon',
  // so we don't want to export them.
  int token_type;

  union {
    DBCC_Symbol *v_identifier;
    struct {
      DBCC_Symbol *name;
      DBCC_Type *type;
    } v_typedef_name;
    DBCC_String v_string_literal;
    struct {
      uint8_t sizeof_value;
      bool is_signed;
      union {
        int64_t v_int64;
        uint64_t v_uint64;
      };
    } v_i_constant;
    struct {
      DBCC_FloatType float_type;
      long double v_long_double;
    } v_f_constant;
    struct {
      char prefix;              // '\0', 'L', 'u', 'U'
      uint32_t code;
    } v_char_constant;

    DBCC_Symbol *v_func_name;

    // for ENUMERATION_CONSTANT
    struct {
      DBCC_Type *type;
      DBCC_EnumValue *enum_value;
    } v_enum_value;
  };
};

#define P_TOKEN_INIT(shortname, cp) \
  (P_Token) { (cp), P_TOKEN_##shortname, {0,} }

#endif
