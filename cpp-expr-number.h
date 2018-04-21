
typedef enum {
  CPP_EXPR_NUMBER_INT64,
  CPP_EXPR_NUMBER_FAIL   // used to represent the result of dividing by 0
} CPP_Expr_NumberType;
struct CPP_Expr_Result {
  CPP_Expr_NumberType type;
  union {
    int64_t v_int64;
  };
};

struct CPP_Expr_Token
{
  int token_type;
  CPP_Expr_Number num;
};
