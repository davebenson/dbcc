#include "../dbcc.h"
#include "../dsk/dsk.h"
#include <stdio.h>
#include <assert.h>

static void
print_c_quoted_with_len (unsigned len, const char *str)
{
  printf ("\"");
  while (len--)
    {
      if (*str == '\n')
        printf("\\n");
      else if (*str == '\t')
        printf("\\t");
      else if (((uint8_t) (*str)) >= 127)
        printf("\\%03o", (uint8_t) *str);

      // Basically, the next statement tries to encode 1- and 2-byte octal sequences
      // as their shorter length - which is only allowed if it is not followed
      // by a digit.
      else if (*str < 32 && (len == 0 || !('0' <= str[1] && str[1] <= '9')))
        printf("\\%o", (uint8_t) *str);

      // the control character is followed by a digit: hence it must be expanded
      // to 3 (octal) digits.
      else if (*str < 32)
        printf("\\%03o", (uint8_t) *str);

      // Many characters are simply backslashed to escape them.
      else if (*str == '"' || *str == '\\')
        printf("\\%c", *str);

      // the remaining characters require no special processing
      else
        printf ("%c", *str);
      str++;
    }

  printf ("\"");
}

static void
print_json_quoted (const char *str)
{
  print_c_quoted_with_len (strlen (str), str);
}

static void
dump_statement_json_or_null (DBCC_Statement *stmt, void *handler_data);

static void
dump_expr_json_or_null (DBCC_Expr *expr, void *handler_data);

static void
dump_expr_json (DBCC_Expr *expr, void *handler_data);

static void
dump_statement_json (DBCC_Statement *stmt, void *handler_data)
{
  assert(handler_data == NULL);

  switch (stmt->type)
    {
      case DBCC_STATEMENT_COMPOUND:
        printf ("{\{\"type\": \"compound\", \"statements\":[");
        for (unsigned i = 0; i < stmt->v_compound.n_statements; i++)
          dump_statement_json(stmt->v_compound.statements[i], handler_data);
        printf("]}");
        break;

      case DBCC_STATEMENT_FOR:
        printf("\{\"type\": \"for\", \"init\":");
        dump_statement_json_or_null (stmt->v_for.init, handler_data);
        printf(",\"condition\":");
        dump_expr_json_or_null (stmt->v_for.condition, handler_data);
        printf(",\"next\":");
        dump_statement_json_or_null (stmt->v_for.advance, handler_data);
        printf(",\"body\":");
        dump_statement_json_or_null (stmt->v_for.body, handler_data);
        printf("}");
        break;
      case DBCC_STATEMENT_WHILE:
        printf("\{\"type\": \"while\", \"condition\":");
        dump_expr_json_or_null (stmt->v_while.condition, handler_data);
        printf(",\"body\":");
        dump_statement_json_or_null (stmt->v_while.body, handler_data);
        printf("}");
        break;
      case DBCC_STATEMENT_DO_WHILE:
        printf("\{\"type\": \"do_while\", \"body\":");
        dump_statement_json_or_null (stmt->v_do_while.body, handler_data);
        printf(",\"condition\":");
        dump_expr_json_or_null (stmt->v_do_while.condition, handler_data);
        printf("}");
        break;
      case DBCC_STATEMENT_SWITCH:
        printf("{\"type\":\"switch\",\"value\":");
        dump_expr_json_or_null (stmt->v_switch.value_expr, handler_data);
        printf(",\"body\":");
        dump_statement_json (stmt->v_switch.body, handler_data);
        printf("}");
        break;
      case DBCC_STATEMENT_IF:
        printf("{\"type\":\"if\",\"condition\":");
        dump_expr_json (stmt->v_if.condition, handler_data);
        printf(",\"body\":");
        dump_statement_json_or_null (stmt->v_if.body, handler_data);
        printf(",\"else\":");
        dump_statement_json_or_null (stmt->v_if.else_body, handler_data);
        printf("}");
        break;
      case DBCC_STATEMENT_GOTO:
        printf("{\"type\":\"goto\",\"label\":\"%s\",\"resolved\":%s}",
                dbcc_symbol_get_string (stmt->v_goto.label),
                stmt->v_goto.target == NULL ? "false" : "true");
        break;
      case DBCC_STATEMENT_LABEL:
        printf("{\"type\":\"goto\",\"label\":\"%s\"}",
                dbcc_symbol_get_string (stmt->v_label.name));
        break;
      case DBCC_STATEMENT_EXPR:
        printf("{\"type\":\"expr\",\"expr\":\"");
        dump_expr_json (stmt->v_expr.expr, handler_data);
        printf("}");
        break;
      default:
        assert(0);
    }
}

static const char *
unary_op_name (DBCC_UnaryOperator op)
{
  switch (op)
    {
    case DBCC_UNARY_OPERATOR_NOOP:        return "noop";
    case DBCC_UNARY_OPERATOR_LOGICAL_NOT: return "logical_not";
    case DBCC_UNARY_OPERATOR_BITWISE_NOT: return "bitwise_not";
    case DBCC_UNARY_OPERATOR_NEGATE:      return "negate";
    case DBCC_UNARY_OPERATOR_REFERENCE:   return "reference";
    case DBCC_UNARY_OPERATOR_DEREFERENCE: return "dereference";
    default: assert(0);
    }
}

static const char *
binary_op_name (DBCC_BinaryOperator op)
{
  switch (op)
    {
    case DBCC_BINARY_OPERATOR_ADD: return "add";
    case DBCC_BINARY_OPERATOR_SUB: return "sub";
    case DBCC_BINARY_OPERATOR_MUL: return "mul";
    case DBCC_BINARY_OPERATOR_DIV: return "div";
    case DBCC_BINARY_OPERATOR_REM: return "remainder";
    case DBCC_BINARY_OPERATOR_LT: return "lt";
    case DBCC_BINARY_OPERATOR_LTEQ: return "lteq";
    case DBCC_BINARY_OPERATOR_GT: return "gt";
    case DBCC_BINARY_OPERATOR_GTEQ: return "gteq";
    case DBCC_BINARY_OPERATOR_EQ: return "eq";
    case DBCC_BINARY_OPERATOR_NE: return "ne";
    case DBCC_BINARY_OPERATOR_SHIFT_LEFT: return "shift_left";
    case DBCC_BINARY_OPERATOR_SHIFT_RIGHT: return "shift_right";
    case DBCC_BINARY_OPERATOR_LOGICAL_AND: return "logical_and";
    case DBCC_BINARY_OPERATOR_LOGICAL_OR: return "logical_or";
    case DBCC_BINARY_OPERATOR_BITWISE_AND: return "bitwise_and";
    case DBCC_BINARY_OPERATOR_BITWISE_OR: return "bitwise_or";
    case DBCC_BINARY_OPERATOR_BITWISE_XOR: return "bitwise_xor";
    case DBCC_BINARY_OPERATOR_COMMA: return "comma";
    default: assert(0); return "";
  }
}

static void
dump_type_json (DBCC_Type *type, void *handler_data)
{
  switch (type->metatype)
  {
  case DBCC_TYPE_METATYPE_VOID:
    printf("\"void\"");
    break;
  case DBCC_TYPE_METATYPE_BOOL:
    printf("\"bool\"");
    break;
  case DBCC_TYPE_METATYPE_INT:
    printf("\"%sint%u\"",
           type->v_int.is_signed ? "" : "u",
           (unsigned)(8 * type->base.sizeof_instance));
    break;
  case DBCC_TYPE_METATYPE_FLOAT:
    switch (type->v_float.float_type)
      {
      case DBCC_FLOAT_TYPE_FLOAT: printf("\"float\""); break;
      case DBCC_FLOAT_TYPE_DOUBLE: printf("\"double\""); break;
      case DBCC_FLOAT_TYPE_LONG_DOUBLE: printf("\"long double\""); break;
      case DBCC_FLOAT_TYPE_COMPLEX_FLOAT: printf("\"complex float\""); break;
      case DBCC_FLOAT_TYPE_COMPLEX_DOUBLE: printf("\"complex double\""); break;
      case DBCC_FLOAT_TYPE_COMPLEX_LONG_DOUBLE: printf("\"complex long double\""); break;
      case DBCC_FLOAT_TYPE_IMAGINARY_FLOAT: printf("\"imaginary float\""); break;
      case DBCC_FLOAT_TYPE_IMAGINARY_DOUBLE: printf("\"imaginary double\""); break;
      case DBCC_FLOAT_TYPE_IMAGINARY_LONG_DOUBLE: printf("\"imaginary long double\""); break;
      }
    break;
  case DBCC_TYPE_METATYPE_ARRAY:
    printf("{\"metatype\":\"array\",\"element_type\":");
    dump_type_json (type->v_array.element_type, handler_data);
    if (type->v_array.n_elements >= 0)
      printf(",\"length\":%llu", (unsigned long long) type->v_array.n_elements);
    printf("}");
    break;
  case DBCC_TYPE_METATYPE_VARIABLE_LENGTH_ARRAY:
    printf("{\"metatype\":\"variable_length_array\",\"element_type\":");
    dump_type_json (type->v_variable_length_array.element_type, handler_data);
    printf("}");
    break;
  case DBCC_TYPE_METATYPE_STRUCT:
    printf("{\"metatype\":\"struct\"");
    if (type->v_struct.tag != NULL)
      printf(",\"tag\":\"%s\"", dbcc_symbol_get_string (type->v_struct.tag));
    printf(",\"members\":[");
    for (unsigned i = 0; i < type->v_struct.n_members; i++)
      {
        if (i > 0)
          printf(",");
        printf("[\"%s\",", dbcc_symbol_get_string (type->v_struct.members[i].name));
        dump_type_json (type->v_struct.members[i].type, handler_data);
        printf("]");
      }
    printf("]}");
    break;
  case DBCC_TYPE_METATYPE_UNION:
    printf("{\"metatype\":\"union\"");
    if (type->v_union.tag != NULL)
      printf(",\"tag\":\"%s\"", dbcc_symbol_get_string (type->v_union.tag));
    printf(",\"branches\":[");
    for (unsigned i = 0; i < type->v_union.n_branches; i++)
      {
        if (i > 0)
          printf(",");
        printf("[\"%s\",", dbcc_symbol_get_string (type->v_union.branches[i].name));
        dump_type_json (type->v_union.branches[i].type, handler_data);
        printf("]");
      }
    printf("]}");
    break;
  case DBCC_TYPE_METATYPE_ENUM:
    printf("{\"metatype\":\"enum\"");
    if (type->v_enum.tag != NULL)
      printf(",\"tag\":\"%s\"", dbcc_symbol_get_string (type->v_enum.tag));
    printf(",\"values\":[");
    for (unsigned i = 0; i < type->v_enum.n_values; i++)
      {
        if (i > 0)
          printf(",");
        printf("[\"%s\",%lld]",
               dbcc_symbol_get_string (type->v_enum.values[i].name),
               (long long) type->v_enum.values[i].value);
      }
    printf("]}");
    break;
  case DBCC_TYPE_METATYPE_POINTER:
    printf("{\"metatype\":\"pointer\",\"subtype\":");
    dump_type_json (type->v_pointer.target_type, handler_data);
    printf("}");
    break;
  case DBCC_TYPE_METATYPE_TYPEDEF:
    printf("{\"metatype\":\"pointer\",\"alias\":\"%s\",\"aliased_type\":",
             dbcc_symbol_get_string (type->base.name));
    dump_type_json (type->v_typedef.underlying_type, handler_data);
    printf("}");
    break;
  case DBCC_TYPE_METATYPE_QUALIFIED:
    printf("{\"metatype\":\"qualfied\",\"restrict\":%s,\"const\":%s,\"atomic\":%s,\"volatile\":%s,\"subtype\":",
           (type->v_qualified.qualifiers & DBCC_TYPE_QUALIFIER_RESTRICT) ? "true" : "false",
           (type->v_qualified.qualifiers & DBCC_TYPE_QUALIFIER_CONST) ? "true" : "false",
           (type->v_qualified.qualifiers & DBCC_TYPE_QUALIFIER_ATOMIC) ? "true" : "false",
           (type->v_qualified.qualifiers & DBCC_TYPE_QUALIFIER_VOLATILE) ? "true" : "false");
    dump_type_json (type->v_qualified.underlying_type, handler_data);
    printf("}");
    break;

  case DBCC_TYPE_METATYPE_FUNCTION:
    printf("{\"metatype\":\"function\",\"return_type\":");
    dump_type_json (type->v_function.return_type, handler_data);
    printf(",arguments:[");
    for (unsigned i = 0; i < type->v_function.n_params; i++)
      {
        if (i > 0)
          printf(",");
        printf("{\"type\":");
        dump_type_json (type->v_function.params[i].type, handler_data);
        printf("{,\"name\":\"%s\"}",
               dbcc_symbol_get_string (type->v_function.params[i].name));
      }
    printf("], has_varargs: %s}",
           type->v_function.has_varargs ? "true" : "false");
    break;

  case DBCC_TYPE_METATYPE_KR_FUNCTION:
    printf("{\"metatype\":\"kr_function\",\"params\":[");
    for (unsigned i = 0; i < type->v_function_kr.n_params; i++)
      {
        if (i > 0)
          printf(",");
        printf("\"%s\"", dbcc_symbol_get_string (type->v_function_kr.params[i]));
      }
    printf("}");
    break;
  }
}

static void
dump_constant_value (DBCC_Type *type, DBCC_Constant *constant)
{
  DskBuffer buffer = DSK_BUFFER_INIT;
  DBCC_Error *error = NULL;
  switch (constant->constant_type)
    {
      case DBCC_CONSTANT_TYPE_VALUE:
        dsk_buffer_append_string(&buffer, "[\"value\",");
        if (!dbcc_type_value_to_json (type, constant->v_value.data, &buffer, &error))
          {
            fprintf(stderr, "dbcc_type_value_to_json: failed: %s\n", error->message);
            exit(1);
          }
        dsk_buffer_append_string(&buffer, "]");
        break;
      case DBCC_CONSTANT_TYPE_LINK_ADDRESS:
        dsk_buffer_printf(&buffer,
                          "[\"link_address\",\"%s\"]",
                          dbcc_symbol_get_string (constant->v_link_address.name));
        break;
      case DBCC_CONSTANT_TYPE_UNIT_ADDRESS:
        dsk_buffer_printf(&buffer, "[\"unit_address\",\"%s\",%llx]",
                          dbcc_symbol_get_string (constant->v_unit_address.name),
                          (long long unsigned) constant->v_unit_address.address);
        break;
      case DBCC_CONSTANT_TYPE_LOCAL_ADDRESS:
        dsk_buffer_printf(&buffer, "[\"local_address\",\"%p\"]", constant->v_local_address.local);
        break;
      case DBCC_CONSTANT_TYPE_OFFSET:
        dsk_buffer_printf(&buffer, "[\"local_address\",\"%p\"]", constant->v_local_address.local);
        break;
    }
      
  for (DskBufferFragment *frag = buffer.first_frag;
       frag != NULL;
       frag = frag->next)
    {
      if (frag->buf_length > 0)
        fwrite (frag->buf + frag->buf_start, frag->buf_length, 1, stdout);
    }
  dsk_buffer_clear (&buffer);
}

static void dump_structured_initializer_json (DBCC_StructuredInitializer *init, void *handler_data);
static void dump_expr_json (DBCC_Expr *expr, void *handler_data);

static void
dump_structured_initializer_piece_json (DBCC_StructuredInitializerPiece *piece, void *handler_data)
{
  printf("{\"designators\":[");
  for (unsigned i = 0; i < piece->n_designators; i++)
    {
      if (i > 0)
        printf(",");
      switch (piece->designators[i].type)
        {
        case DBCC_DESIGNATOR_INDEX:
          {
            DBCC_Expr *index = piece->designators[i].v_index;
            if (index->base.constant != NULL
             && index->base.constant->constant_type == DBCC_CONSTANT_TYPE_VALUE)
              {
                int64_t value = dbcc_typed_value_get_int64 (index->base.value_type, index->base.constant->v_value.data);
                printf("%lld", value);
              }
            else
              {
                dump_expr_json(index, handler_data);
              }
            break;
          }
        case DBCC_DESIGNATOR_MEMBER:
          printf("\"%s\"", dbcc_symbol_get_string (piece->designators[i].v_member));
          break;
        }
    }
  printf("],");
  if (piece->is_expr)
    {
      printf("\"expr\":");
      dump_expr_json(piece->v_expr, handler_data);
    }
  else
    {
      printf("\"init\":");
      dump_structured_initializer_json (&piece->v_structured_initializer, handler_data);
    }
  printf("}");
}
static void
dump_structured_initializer_json (DBCC_StructuredInitializer *init, void *handler_data)
{
  printf("[");
  for (unsigned i = 0; i < init->n_pieces; i++)
    {
      if (i > 0)
        printf(",");
      dump_structured_initializer_piece_json (init->pieces + i, handler_data);
    }
  printf("]");
}

static void
dump_expr_json (DBCC_Expr *expr, void *handler_data)
{
  switch (expr->expr_type)
    {
    case DBCC_EXPR_TYPE_UNARY_OP:
      printf("{\"type\":\"unary\",\"op\":\"%s\",\"subexpr\":",
             unary_op_name (expr->v_unary.op));
      dump_expr_json (expr->v_unary.a, handler_data);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_BINARY_OP:
      printf("{\"type\":\"binary\",\"op\":\"%s\",\"a\":",
             binary_op_name (expr->v_binary.op));
      dump_expr_json (expr->v_binary.a, handler_data);
      printf(",\"b\":");
      dump_expr_json (expr->v_binary.b, handler_data);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_TERNARY_OP:
      printf("{\"type\":\"ternary\",\"condition\":");
      dump_expr_json (expr->v_ternary.condition, handler_data);
      printf(",\"true_expr\":");
      dump_expr_json (expr->v_ternary.true_value, handler_data);
      printf(",\"false_expr\":");
      dump_expr_json (expr->v_ternary.false_value, handler_data);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_CONSTANT:
      printf("{\"type\":\"constant\",\"value_type\":");
      dump_type_json (expr->base.value_type, handler_data);
      printf(",\"constant\":");
      dump_constant_value (expr->base.value_type, expr->base.constant);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_INPLACE_UNARY_OP:
      printf("{\"type\":\"inplace_unary\",\"op\":");
      printf("\"%s\", \"inout\":", dbcc_inplace_unary_operator_name(expr->v_inplace_unary.op));
      dump_expr_json (expr->v_inplace_unary.inout, handler_data);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_INPLACE_BINARY_OP:
      printf("{\"type\":\"inplace_binary\",\"op\":");
      printf("\"%s\", \"inout\":", dbcc_inplace_binary_operator_name(expr->v_inplace_binary.op));
      dump_expr_json (expr->v_inplace_binary.inout, handler_data);
      printf(",\"b\":");
      dump_expr_json (expr->v_inplace_binary.b, handler_data);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_CALL:
      printf("{\"type\":\"call\",\"function\":");
      dump_expr_json (expr->v_call.head, handler_data);
      printf(",\"params\":[");
      for (unsigned i = 0; i < expr->v_call.n_args; i++)
        {
          if (i > 0)
            printf(",");
          dump_expr_json(expr->v_call.args[i], handler_data);
        }
      printf("]}");
      break;
    case DBCC_EXPR_TYPE_CAST:
      printf("{\"type\":\"cast\",\"target_type\":");
      dump_type_json (expr->base.value_type, handler_data);
      printf(",\"sub_expr\":");
      dump_expr_json (expr->v_cast.pre_cast_expr, handler_data);
      printf("}");
      break;
    case DBCC_EXPR_TYPE_ACCESS:
      printf("{\"type\":\"access\",\"op\":\"%s\",\"object\":",
             expr->v_access.is_pointer ? "->" : ".");
      dump_expr_json (expr->v_access.object, handler_data);
      printf(",\"member\":\"%s\"}", dbcc_symbol_get_string (expr->v_access.name));
      break;
    case DBCC_EXPR_TYPE_IDENTIFIER:
      printf("{\"type\":\"identifier\",\"name\":\"%s\"", dbcc_symbol_get_string (expr->v_identifier.name));
      // OTHER STUFF!
      printf("}");
      break;
    case DBCC_EXPR_TYPE_STRUCTURED_INITIALIZER:
      printf("{\"type\":\"structured_initializer\",\"value\":");
      dump_structured_initializer_json(&expr->v_structured_initializer.initializer, handler_data);
      printf(",\"flattened\":[");
      for (unsigned i = 0; i < expr->v_structured_initializer.n_flat_pieces; i++)
        {
          if (i > 0)
            printf(",");
          printf("{\"offset\":%u,\"length\":%u",
                 (unsigned)expr->v_structured_initializer.flat_pieces[i].offset,
                 (unsigned)expr->v_structured_initializer.flat_pieces[i].length);
          if (expr->v_structured_initializer.flat_pieces[i].piece_expr != NULL)
            {
              printf(",\"expr\":");
              dump_expr_json (expr->v_structured_initializer.flat_pieces[i].piece_expr, handler_data);
            }
          printf("}");
        }
      printf("]}");
      break;
#if 0
    case DBCC_EXPR_TYPE_SUBSCRIPT:
      printf("{\"type\":\"subscript\",\"container\":");
      dump_expr_json (expr->v_subscript.ptr, handler_data);
      printf(",\"index\":");
      dump_expr_json (expr->v_subscript.index, handler_data);
      printf("}");
      break;
#endif
    }
}

static void
dump_statement_json_or_null (DBCC_Statement *stmt, void *handler_data)
{
  if (stmt == NULL)
    printf ("null");
  else
    dump_statement_json (stmt, handler_data);
}

static void
dump_expr_json_or_null (DBCC_Expr *expr, void *handler_data)
{
  if (expr == NULL)
    printf ("null");
  else
    dump_expr_json (expr, handler_data);
}

static void
dump_code_position_json (DBCC_CodePosition *cp)
{
  printf("{\"type\":\"pos\",\"filename\":");
  print_json_quoted (dbcc_symbol_get_string (cp->filename));
  printf(",\"line\":%u,\"column\":%u,\"byte_offset\":%u",
         cp->line_no, cp->column, cp->byte_offset);
  if (cp->included_from)
    {
      printf(",\"included_from\":");
      dump_code_position_json(cp->included_from);
    }
  if (cp->expanded_from)
    {
      printf(",\"expanded_from\":");
      dump_code_position_json(cp->expanded_from);
    }
  printf("}");
}

static void
dump_error_json (DBCC_Error *error)
{
  printf("{\"type\":\"error\",\"code\",\"%s\",\"message\":",
          dbcc_error_code_name (error->code));
  print_json_quoted (error->message);
  for (DBCC_ErrorData *d = error->first_data; d != NULL; d = d->next)
    {
    switch (d->type)
      {
      case DBCC_ERROR_DATA_TYPE_CAUSE:
        printf(",\"cause\":");
        dump_error_json(((DBCC_ErrorData_Cause *) d)->error);
        break;
      case DBCC_ERROR_DATA_TYPE_CODE_POSITION:
        printf(",\"code_position\":");
        dump_code_position_json(((DBCC_ErrorData_CodePosition *) d)->code_position);
        break;
      default:
        break;
      }
    }
  printf("}");
}

static void
dump_error_json_newline (DBCC_Error *error, void *handler_data)
{
  assert(handler_data == NULL);
  dump_error_json(error);
  printf("\n");
}

static void
dump_statement_json_newline (DBCC_Statement *stmt, void *handler_data)
{
  dump_statement_json(stmt, handler_data);
  printf("\n");
}

int main(int argc, char **argv)
{
  if (argc < 2)
    {
      fprintf(stderr, "usage: %s FILE\n", argv[0]);
      return 1;
    }
  DBCC_Parser *parser;
  DBCC_Parser_NewOptions options = DBCC_PARSER_NEW_OPTIONS;
  options.handle_statement = dump_statement_json_newline;
  options.handle_error = dump_error_json_newline;
  parser = dbcc_parser_new (&options);
  if (!dbcc_parser_parse_file (parser, argv[1]))
    return 1;
  dbcc_parser_destroy (parser);
  return 0;
}
