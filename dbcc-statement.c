#include "dbcc.h"
#include "dsk/dsk-rbtree-macros.h"

static inline DBCC_Statement *
statement_alloc (DBCC_StatementType type)
{
  DBCC_Statement *rv = calloc (sizeof(DBCC_Statement), 1);
  rv->type = type;
  return rv;
}


DBCC_Statement *dbcc_statement_new_empty          (void)
{
  return statement_alloc (DBCC_STATEMENT_COMPOUND);
}

DBCC_Statement *dbcc_statement_new_compound       (size_t n_statements,
                                                   DBCC_Statement **statements,
                                                   bool      defines_scope)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_COMPOUND);
  rv->v_compound.n_statements = n_statements;
  rv->v_compound.statements = statements;
  rv->v_compound.defines_scope = defines_scope;
  return rv;
}

DBCC_Statement *dbcc_statement_new_expr           (DBCC_Expr *expr)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_EXPR);
  rv->v_expr.expr = expr;
  return rv;
}

DBCC_Statement *
dbcc_statement_new_label          (DBCC_Symbol *symbol)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_LABEL);
  rv->v_label.name = symbol;
  return rv;
}

DBCC_Statement *
dbcc_statement_new_case           (DBCC_Expr *value)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_CASE);
  rv->v_case.value_expr = value;
  return rv;
}

DBCC_Statement *
dbcc_statement_new_default        (DBCC_CodePosition  *cp)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_DEFAULT);
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

DBCC_Statement *
dbcc_statement_new_declaration    (DBCC_StorageClassSpecifier storage_specs,
                                   DBCC_Type          *type,
                                   DBCC_Symbol        *name,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_DECLARATION);
  rv->v_declaration.type = type;
  rv->v_declaration.name = name;
  rv->v_declaration.storage_specs = storage_specs;
  if (cp)
    {
      rv->base.code_position = cp;
      dbcc_code_position_ref (cp);
    }
  (void) error;
  return rv;
}

DBCC_Statement *
dbcc_statement_new_if             (DBCC_Expr          *expr,
                                   DBCC_Statement     *body,
                                   DBCC_Statement     *else_body,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  /* 6.8.4.1p1: "The controlling expression of an if statement
                 shall have scalar type." */
  if (expr->base.value_type != NULL 
   && !dbcc_type_is_scalar (expr->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_EXPR_NOT_CONDITION,
                               "expression type is not a boolean");
      dbcc_error_add_code_position (*error, cp);
      return NULL;
    }
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_IF);
  rv->v_if.condition = expr;
  rv->v_if.body = body;
  rv->v_if.else_body = else_body;
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

DBCC_Statement *
dbcc_statement_new_for            (DBCC_Statement     *init,
                                   DBCC_Expr          *condition,
                                   DBCC_Statement     *advance,
                                   DBCC_Statement     *body,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  /* 6.8.5 Iteration statements.
   * 6.8.5p2: "The controlling expression of an iteration shall
   *               have a scalar type."
   */
  if (condition != NULL
   && condition->base.value_type != NULL
   && !dbcc_type_is_scalar (condition->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_EXPR_NOT_CONDITION,
                               "expression type is not a boolean in for-statement condition");
      dbcc_error_add_code_position (*error, cp);
      return NULL;
    }
  /* 6.8.5p3: The declaration part of a for statement shall only
              declare identifiers for objects having
              storage class auto or register. */
 DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_FOR);
 rv->v_for.init = init;
 rv->v_for.condition = condition;
 rv->v_for.advance = advance;
 rv->v_for.body = body;
 rv->base.code_position = dbcc_code_position_ref (cp);
 return rv;
}

/* SwitchMapConstructData: structure to help with recursively locating the
 * case-statements and getting their values (and detecting duplicate
 * values).
 */
typedef struct ValueCasePair ValueCasePair;
struct ValueCasePair {
  int64_t value;
  DBCC_Statement *case_statement;
  bool is_red;
  ValueCasePair *left, *right, *parent;
};

typedef struct {
  DBCC_Error *error;
  ValueCasePair *tree_top;
} SwitchMapConstructData;
#define SWITCH_MAP_CONSTRUCT_DATA_INIT (SwitchMapConstructData){ \
  .error = NULL \
}
#define COMPARE_VALUE_CASE_PAIR_BY_VALUE(a,b, rv) \
  rv = a->value < b->value ? -1 : a->value > b->value ? 1 : 0;
#define GET_VALUE_CASE_PAIR_TREE(cd) \
  (cd)->tree_top, \
  ValueCasePair *, DSK_STD_GET_IS_RED, DSK_STD_SET_IS_RED, \
  parent, left, right, \
  COMPARE_VALUE_CASE_PAIR_BY_VALUE

static void
maybe_free_value_case_pair_tree_recursive (ValueCasePair *p)
{
  if (p)
    {
      maybe_free_value_case_pair_tree_recursive(p->left);
      maybe_free_value_case_pair_tree_recursive(p->right);
      free (p);
    }
}
static void
switch_map_construct_data_clear (SwitchMapConstructData *cd)
{
  maybe_free_value_case_pair_tree_recursive (cd->tree_top);
  cd->tree_top = NULL;
  if (cd->error)
    {
      dbcc_error_unref (cd->error);
      cd->error = NULL;
    }
}

static bool
switch_map_construct_data_process (SwitchMapConstructData *construct_data,
                                   DBCC_Statement         *statement)
{
  switch (statement->type)
    {
    case DBCC_STATEMENT_COMPOUND:
      for (size_t i = 0; i < statement->v_compound.n_statements; i++)
        if (!switch_map_construct_data_process (construct_data,
                                                statement->v_compound.statements[i]))
          return false;
      return true;

    case DBCC_STATEMENT_FOR:
      if (statement->v_for.init != NULL
       && !switch_map_construct_data_process (construct_data,
                                              statement->v_for.init))
        return false;
      if (statement->v_for.advance != NULL
       && !switch_map_construct_data_process (construct_data,
                                              statement->v_for.advance))
        return false;
      if (statement->v_for.body != NULL
       && !switch_map_construct_data_process (construct_data,
                                              statement->v_for.body))
        return false;
      return true;

    case DBCC_STATEMENT_WHILE:
      if (statement->v_while.body != NULL
       && !switch_map_construct_data_process (construct_data,
                                              statement->v_while.body))
        return false;
      return true;

    case DBCC_STATEMENT_DO_WHILE:
      if (statement->v_do_while.body != NULL
       && !switch_map_construct_data_process (construct_data,
                                              statement->v_do_while.body))
        return false;
      return true;

    case DBCC_STATEMENT_SWITCH:
      /* If we find another switch-statement nested within,
       * any 'case' statements within will refer to the inner switch-statement,
       * so don't consider those.
       */
      return true;

    case DBCC_STATEMENT_IF:
      if (!switch_map_construct_data_process (construct_data,
                                              statement->v_if.body))
        return false;
      if (statement->v_if.else_body != NULL
       && !switch_map_construct_data_process (construct_data,
                                              statement->v_if.else_body))
        return false;
      return true;

    case DBCC_STATEMENT_CASE:
      // Ensure that expr is constant, by looking at its value.
      if (statement->v_case.value_expr->base.constant == NULL
       || statement->v_case.value_expr->base.constant->constant_type != DBCC_CONSTANT_TYPE_VALUE)
        {
          construct_data->error = dbcc_error_new (DBCC_ERROR_CASE_EXPR_NONCONSTANT,
                                                  "case-statement value is not a constant");
          dbcc_error_add_code_position (construct_data->error,
                                        statement->base.code_position);
          return false;
        }

      // Add value into switch-table.
      DBCC_Expr *ce = statement->v_case.value_expr;
      uint64_t v = dbcc_typed_value_get_int64 (ce->base.value_type,
                                               ce->base.constant->v_value.data);
      ValueCasePair *pair = malloc (sizeof (ValueCasePair));
      ValueCasePair *conflict;
      pair->value = v;
      pair->case_statement = statement;
      DSK_RBTREE_INSERT (GET_VALUE_CASE_PAIR_TREE (construct_data),
                         pair, conflict);
      if (conflict != NULL)
        {
          construct_data->error = dbcc_error_new (DBCC_ERROR_CASE_DUPLICATE,
                                                  "duplicate value %lld at case statement",
                                                  (long long) v);
          dbcc_error_add_code_position (construct_data->error,
                                        statement->base.code_position);
          return false;
        }
      return true;

    /* These cannot has substatements, so aren't interesting
     * in evaluating the switch: just skip them. */
    case DBCC_STATEMENT_LABEL:
    case DBCC_STATEMENT_GOTO:
    case DBCC_STATEMENT_BREAK:
    case DBCC_STATEMENT_CONTINUE:
    case DBCC_STATEMENT_RETURN:
    case DBCC_STATEMENT_DEFAULT:
    case DBCC_STATEMENT_EXPR:
    case DBCC_STATEMENT_DECLARATION:

      return true;
    }
}

static size_t
value_cases_count (ValueCasePair *tree)
{
  return tree != NULL ? (value_cases_count(tree->left)
                         + 1
                         + value_cases_count(tree->right))
                      : 0;
}

static void
update_switch_cases_recursive (ValueCasePair *tree, DBCC_SwitchStatementCase **p_at)
{
  if (tree)
    {
      update_switch_cases_recursive (tree->left, p_at);
      (*p_at)->value = tree->value;
      *p_at += 1;
      update_switch_cases_recursive (tree->left, p_at);
    }
}

DBCC_Statement *
dbcc_statement_new_switch         (DBCC_Expr          *expr,
                                   DBCC_Statement     *switch_body,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  if (expr != NULL
   && expr->base.value_type != NULL
   && !dbcc_type_is_integer (expr->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_EXPR_NOT_CONDITION,
                               "expression type is not an integer-type/enum in case-statement condition");
      dbcc_error_add_code_position (*error, cp);
      return NULL;
    }

  SwitchMapConstructData smcd = SWITCH_MAP_CONSTRUCT_DATA_INIT;

  /* Instead of checking the return-value, we
   * chekc the error member instead. */
  (void) switch_map_construct_data_process (&smcd, switch_body);
  if (smcd.error)
    {
      *error = smcd.error;
      smcd.error = NULL;
      switch_map_construct_data_clear (&smcd);
      return NULL;
    }
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_SWITCH);
  rv->v_switch.value_expr = expr;
  rv->v_switch.body = switch_body;
  rv->v_switch.n_cases = value_cases_count (smcd.tree_top);
  rv->v_switch.cases = DBCC_NEW_ARRAY (rv->v_switch.n_cases,
                                       DBCC_SwitchStatementCase);
  DBCC_SwitchStatementCase *at = rv->v_switch.cases;
  update_switch_cases_recursive (smcd.tree_top, &at);
  assert(at == rv->v_switch.cases + rv->v_switch.n_cases);
  return rv;
}

DBCC_Statement *
dbcc_statement_new_while          (DBCC_Expr          *condition,
                                   DBCC_Statement     *body,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  /* 6.8.5 Iteration statements.
   * 6.8.5p2: "The controlling expression of an iteration shall
   *           have a scalar type."
   */
  if (condition != NULL
   && condition->base.value_type != NULL
   && !dbcc_type_is_scalar (condition->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_EXPR_NOT_CONDITION,
                               "expression type is not a scalar in while-statement condition");
      dbcc_error_add_code_position (*error, cp);
      return NULL;
    }
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_WHILE);
  rv->v_while.condition = condition;
  rv->v_while.body = body;
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

DBCC_Statement *
dbcc_statement_new_do_while       (DBCC_Statement     *body,
                                   DBCC_Expr          *condition,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  /* 6.8.5 Iteration statements.
   * 6.8.5p2: "The controlling expression of an iteration shall
   *           have a scalar type."
   */
  if (condition != NULL
   && condition->base.value_type != NULL
   && !dbcc_type_is_scalar (condition->base.value_type))
    {
      *error = dbcc_error_new (DBCC_ERROR_EXPR_NOT_CONDITION,
                               "expression type is not a scalar in do-while-statement condition");
      dbcc_error_add_code_position (*error, cp);
      return NULL;
    }
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_DO_WHILE);
  rv->v_do_while.condition = condition;
  rv->v_do_while.body = body;
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

DBCC_Statement *
dbcc_statement_new_goto           (DBCC_Symbol        *target,
                                   DBCC_CodePosition  *cp)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_GOTO);
  rv->v_goto.label = target;
  rv->base.code_position = cp;
  return rv;
}

DBCC_Statement *
dbcc_statement_new_continue       (DBCC_CodePosition  *cp)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_CONTINUE);
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

DBCC_Statement *
dbcc_statement_new_return         (DBCC_Expr          *opt_expr,
                                   DBCC_CodePosition  *cp,
                                   DBCC_Error        **error)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_RETURN);
  rv->base.code_position = dbcc_code_position_ref (cp);
  rv->v_return.return_value = opt_expr;
  (void) error;
  return rv;
}


DBCC_Statement *
dbcc_statement_new_break          (DBCC_CodePosition  *cp)
{
  DBCC_Statement *rv = statement_alloc (DBCC_STATEMENT_BREAK);
  rv->base.code_position = dbcc_code_position_ref (cp);
  return rv;
}

void dbcc_statement_destroy (DBCC_Statement *statement)
{
  switch (statement->type)
    {
    case DBCC_STATEMENT_COMPOUND:
      for (unsigned i = 0; i < statement->v_compound.n_statements; i++)
        dbcc_statement_destroy (statement->v_compound.statements[i]);
      break;

    case DBCC_STATEMENT_FOR:
      if (statement->v_for.init != NULL)
        dbcc_statement_destroy (statement->v_for.init);
      if (statement->v_for.condition != NULL)
        dbcc_expr_destroy (statement->v_for.condition);
      if (statement->v_for.advance != NULL)
        dbcc_statement_destroy (statement->v_for.advance);
      dbcc_statement_destroy (statement->v_for.body);
      break;

    case DBCC_STATEMENT_WHILE:
      dbcc_expr_destroy (statement->v_while.condition);
      dbcc_statement_destroy (statement->v_while.body);
      break;

    case DBCC_STATEMENT_DO_WHILE:
      dbcc_statement_destroy (statement->v_do_while.body);
      dbcc_expr_destroy (statement->v_do_while.condition);
      break;

    case DBCC_STATEMENT_SWITCH:
      dbcc_statement_destroy (statement->v_switch.body);
      dbcc_expr_destroy (statement->v_switch.value_expr);
      break;

    case DBCC_STATEMENT_IF:
      if (statement->v_if.condition != NULL)
        dbcc_expr_destroy (statement->v_if.condition);
      if (statement->v_if.body != NULL)
        dbcc_statement_destroy (statement->v_if.body);
      if (statement->v_if.else_body != NULL)
        dbcc_statement_destroy (statement->v_if.else_body);
      break;

    case DBCC_STATEMENT_GOTO:
      break;

    case DBCC_STATEMENT_LABEL:
      break;

    case DBCC_STATEMENT_CASE:
      if (statement->v_case.value_expr != NULL)
        dbcc_expr_destroy (statement->v_case.value_expr);
      break;

    case DBCC_STATEMENT_DEFAULT:
      break;

    case DBCC_STATEMENT_EXPR:
      dbcc_expr_destroy (statement->v_expr.expr);
      break;

    case DBCC_STATEMENT_DECLARATION:
      dbcc_type_unref (statement->v_declaration.type);
      if (statement->v_declaration.opt_value != NULL)
        dbcc_expr_destroy (statement->v_declaration.opt_value);
      break;
    }
  if (statement->base.code_position != NULL)
    dbcc_code_position_unref (statement->base.code_position);
  free (statement);
}
