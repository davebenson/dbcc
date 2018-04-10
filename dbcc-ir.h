
struct DBCC_BB {
  DBCC_IR *first, *last;
};

struct DBCC_IR {
  DBCC_IR_Type type;
  DBCC_IR *prev, *next;
  DBCC_BB *owner;
};

typedef enum DBCC_Location_Type {
  DBCC_LOCATION_REG,            // register
  DBCC_LOCATION_PTR,            // pointer in a register
  DBCC_LOCATION_IMMED           // constant
} DBCC_Location_Type;

struct DBCC_Location {
  DBCC_Location_Type;
  unsigned width;               //log2(size_bytes)
  union {
    unsigned v_reg;             // value stored in register
    unsigned v_ptr;             // pointer value stored in register
    uint8_t v_immed[8];
  } info;
};

struct DBCC_IR_Unary {
  DBCC_IR base;
  DBCC_UnaryOperator op;
  DBCC_Location source;
  DBCC_Location dest;
};

struct DBCC_IR_Binary {
  DBCC_IR base;
  DBCC_BinaryOperator op;
  DBCC_Location source1;
  DBCC_Location source2;
  DBCC_Location dest;
};

struct DBCC_IR_Jump {
  DBCC_IR base;
  DBCC_BB *dst;
};

struct DBCC_IR_JumpConditional {
  DBCC_IR base;
  unsigned reg;
  DBCC_BB *dst;
};

struct DBCC_IR_CallByName {
  DBCC_IR base;
  DBCC_Symbol name;
};

struct DBCC_IR_CallByPointer {
  DBCC_IR base;
  unsigned reg;
};

struct DBCC_IR_ReturnVoid {
  DBCC_IR base;
};

struct DBCC_IR_ReturnReg {
  DBCC_IR base;
  unsigned reg;
};

struct DBCC_Function {
  DBCC_Symbol name;
  unsigned n_blocks;
  DBCC_BB **blocks;
  DBCC_BB *entry;

  DBCC_Function_Flags flags;
};
