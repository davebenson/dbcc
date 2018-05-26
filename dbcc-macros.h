#define DBCC_UNARY_OPERATOR_FOREACH(macro)   \
  macro(NOOP)                                \
  macro(LOGICAL_NOT)                         \
  macro(BITWISE_NOT)                         \
  macro(NEGATE)                              \
  macro(REFERENCE)                           \
  macro(DEREFERENCE)

#define DBCC_BINARY_OPERATOR_FOREACH(macro)  \
    macro(ADD)                               \
    macro(SUB)                               \
    macro(MUL)                               \
    macro(DIV)                               \
    macro(REM)                               \
    macro(LT)                                \
    macro(LTEQ)                              \
    macro(GT)                                \
    macro(GTEQ)                              \
    macro(EQ)                                \
    macro(NE)                                \
    macro(SHIFT_LEFT)                        \
    macro(SHIFT_RIGHT)                       \
    macro(LOGICAL_AND)                       \
    macro(LOGICAL_OR)                        \
    macro(BITWISE_AND)                       \
    macro(BITWISE_OR)                        \
    macro(BITWISE_XOR)                       \
    macro(COMMA)

#define DBCC_INPLACE_UNARY_OPERATOR_FOREACH(macro) \
    macro(PRE_INCR)                          \
    macro(PRE_DECR)                          \
    macro(POST_INCR)                         \
    macro(POST_DECR)

#define DBCC_INPLACE_BINARY_OPERATOR_FOREACH(macro) \
    macro(ASSIGN)                            \
    macro(MUL_ASSIGN)                        \
    macro(DIV_ASSIGN)                        \
    macro(REM_ASSIGN)                        \
    macro(ADD_ASSIGN)                        \
    macro(SUB_ASSIGN)                        \
    macro(LEFT_SHIFT_ASSIGN)                 \
    macro(RIGHT_SHIFT_ASSIGN)                \
    macro(AND_ASSIGN)                        \
    macro(XOR_ASSIGN)                        \
    macro(OR_ASSIGN)

