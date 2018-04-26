
typedef struct {
  DBCC_Error *error;
  bool bool_result;
  bool finished;
} CPP_EvalParserResult;
#define CPP_EVAL_PARSER_RESULT_INIT (CPP_EvalParserResult) {NULL, false, false}

typedef enum {
  CPP_EXPR_RESULT_INT64,
  CPP_EXPR_RESULT_FAIL   // used to represent the result of dividing by 0
} CPP_Expr_ResultType;
typedef struct CPP_Expr_Result CPP_Expr_Result;
struct CPP_Expr_Result
{
  CPP_Expr_ResultType type;
  union {
    int64_t v_int64;
    DBCC_Error *v_error;
  };
};

void *DBCC_CPPExpr_EvaluatorAlloc(void *(*mallocProc)(size_t size));
void DBCC_CPPExpr_Evaluator(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  CPP_Expr_Result yyminor,      /* The value for the token */
  CPP_EvalParserResult *eval_result
);
void DBCC_CPPExpr_EvaluatorFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
);
