
#ifndef _DEFINES_H
#define _DEFINES_H

#define BACKEND_NONE 0
#define BACKEND_Z80  1

#define OUTPUT_NONE  0
#define OUTPUT_ASM   1

#define STACK_RETURN_LABEL             1024
#define STACK_CALCULATE_DELAY          2048
#define STACK_CALCULATE_CANNOT_RESOLVE 4096

#define STACK_NONE    0
#define STACK_INSIDE  1
#define STACK_OUTSIDE 2

#define INPUT_NUMBER_EOL           2
#define INPUT_NUMBER_ADDRESS_LABEL 3
#define INPUT_NUMBER_STRING        4
#define INPUT_NUMBER_STACK         5
#define INPUT_NUMBER_FLOAT         6

#define GET_NEXT_TOKEN_SYMBOL      2
#define GET_NEXT_TOKEN_STRING      3
#define GET_NEXT_TOKEN_INT         4
#define GET_NEXT_TOKEN_DOUBLE      5
#define GET_NEXT_TOKEN_STACK       6
#define GET_NEXT_TOKEN_STRING_DATA 7
#define GET_NEXT_TOKEN_LINEFEED    8

#define EVALUATE_TOKEN_NOT_IDENTIFIED 2
#define EVALUATE_TOKEN_EOP            6
#define DIRECTIVE_NOT_IDENTIFIED      9

#define ERROR_NONE 0
#define ERROR_LOG  1
#define ERROR_DIR  2
#define ERROR_NUM  3
#define ERROR_INC  4
#define ERROR_INB  5
#define ERROR_UNF  6
#define ERROR_INP  7
#define ERROR_STC  8
#define ERROR_WRN  9
#define ERROR_ERR 10

#define DEFINITION_TYPE_VALUE         0
#define DEFINITION_TYPE_STRING        1
#define DEFINITION_TYPE_STACK         2
#define DEFINITION_TYPE_ADDRESS_LABEL 3

/* want to use longer strings and labels? change this - PS. it doesn't contain the null terminator */
#define MAX_NAME_LENGTH 255
#define MAX_FLOAT_DIGITS 25
#define INT_MAX_DECIMAL_DIGITS 10

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef NDEBUG
  #define BILIBALI_DEBUG 1
#endif

#ifdef _CRT_SECURE_NO_WARNINGS
  #pragma warning(disable:4996) /* Just in case */
#endif

#define FAILED      0
#define SUCCEEDED   1
#define FINISHED  100
#define THE_END   200

#define OFF 0
#define ON  1

#define NO  0
#define YES 1

#define TRUE  1
#define FALSE 0

#define STACK_ITEM_TYPE_VALUE    0
#define STACK_ITEM_TYPE_OPERATOR 1
#define STACK_ITEM_TYPE_STRING   2
#define STACK_ITEM_TYPE_DELETED  3
#define STACK_ITEM_TYPE_STACK    4
#define STACK_ITEM_TYPE_REGISTER 5
#define STACK_ITEM_TYPE_INC_DEC  6

#define SI_OP_ADD           0
#define SI_OP_SUB           1
#define SI_OP_MULTIPLY      2
#define SI_OP_LEFT          3
#define SI_OP_RIGHT         4
#define SI_OP_OR            5
#define SI_OP_AND           6
#define SI_OP_DIVIDE        7
#define SI_OP_POWER         8
#define SI_OP_SHIFT_LEFT    9
#define SI_OP_SHIFT_RIGHT  10
#define SI_OP_MODULO       11
#define SI_OP_XOR          12
#define SI_OP_LOW_BYTE     13
#define SI_OP_HIGH_BYTE    14
#define SI_OP_NOT          15
#define SI_OP_BANK         16
#define SI_OP_COMPARE_LT   17
#define SI_OP_COMPARE_GT   18
#define SI_OP_COMPARE_EQ   19
#define SI_OP_COMPARE_NEQ  20
#define SI_OP_COMPARE_LTE  21
#define SI_OP_COMPARE_GTE  22
#define SI_OP_PRE_INC      23
#define SI_OP_PRE_DEC      24
#define SI_OP_POST_INC     25
#define SI_OP_POST_DEC     26
#define SI_OP_USE_REGISTER 27
#define SI_OP_LOGICAL_OR   28
#define SI_OP_LOGICAL_AND  29
#define SI_OP_GET_ADDRESS  30

#define SI_SIGN_POSITIVE 0
#define SI_SIGN_NEGATIVE 1

#define STACK_TYPE_8BIT    0
#define STACK_TYPE_16BIT   1
#define STACK_TYPE_24BIT   2
#define STACK_TYPE_UNKNOWN 3
#define STACK_TYPE_13BIT   4

#define STACK_POSITION_DEFINITION 0
#define STACK_POSITION_CODE       1

struct label {
  char label[MAX_NAME_LENGTH + 1];
  int  references;
  struct label *next;
};

struct definition {
  char   name[MAX_NAME_LENGTH + 1];
  int    parenthesis;
  int    had_parenthesis;
  int    has_arguments;
  int    arguments;
  struct token *tokens_first;
  struct token *tokens_last;
  struct token *arguments_first;
  struct token *arguments_last;
  struct token **arguments_data_first;
  struct token **arguments_data_last;
  struct definition *next;
};

struct ext_include_collection {
  int count;
  int max_name_size_bytes;
  char **names;
};

struct incbin_file_data {
  struct incbin_file_data *next;
  char *data;
  char *name;
  int  size;
};

struct active_file_info {
  int    filename_id;
  int    line_current;
  char   namespace[MAX_NAME_LENGTH + 1];
  struct active_file_info *next;
  struct active_file_info *prev;
};

struct file_name_info {
  char   *name;
  int    id;
  struct file_name_info *next;
};

struct source_file {
  char *file_name;
  int  file_size;
  int  id;
  char *source;
  int  lines_total;
  int  *line_pointers;
  struct source_file *next;
};

struct stack {
  struct stack_item *stack;
  struct stack *next;
  int id;
  int position;
  int filename_id;
  int stacksize;
  int linenumber;
  int type;
  int bank;
  int slot;
  int relative_references;
  int base;
  int section_status;
  int section_id;
  int address;
  int special_id;
};

struct stack_item {
  int type;
  int sign;
  double value;
  char string[MAX_NAME_LENGTH + 1];
};

struct label_def {
  char label[MAX_NAME_LENGTH + 1];
  unsigned char section_status;
  unsigned char alive;
  unsigned char type;
  unsigned char symbol;
  int  section_id;
  int  special_id;
  int  address; /* in bank */
  int  bank;
  int  slot;
  int  base;
  int  filename_id;
  int  linenumber;
  struct section_def *section_struct;
  struct label_def *next;
};

#define TYPE_STRING            0
#define TYPE_VALUE             1
#define TYPE_LABEL             2
#define TYPE_STACK_CALCULATION 3

/********************************************************************/
/* BILIBALI internal tokens */
/********************************************************************/

struct token {
  int id;
  int value;
  double value_double;
  char *label;
  int file_id;
  int line_number;
  struct token *next;
};

#define TOKEN_ID_VARIABLE_TYPE  0
#define TOKEN_ID_SYMBOL         1
#define TOKEN_ID_VALUE_INT      2
#define TOKEN_ID_VALUE_DOUBLE   3
#define TOKEN_ID_VALUE_STRING   4
#define TOKEN_ID_RETURN         5
#define TOKEN_ID_NO_OP          6
#define TOKEN_ID_ELSE           7
#define TOKEN_ID_IF             8
#define TOKEN_ID_WHILE          9
#define TOKEN_ID_FOR           10
#define TOKEN_ID_BREAK         11
#define TOKEN_ID_CONTINUE      12
#define TOKEN_ID_COMPARE       13
#define TOKEN_ID_SWITCH        14
#define TOKEN_ID_CASE          15
#define TOKEN_ID_DEFAULT       16
#define TOKEN_ID_BYTES         17
#define TOKEN_ID_ASM           18
#define TOKEN_ID_HINT          19

#define SYMBOL_LOGICAL_OR   0
#define SYMBOL_LOGICAL_AND  1
#define SYMBOL_LTE          2
#define SYMBOL_GTE          3
#define SYMBOL_EQUAL        4
#define SYMBOL_UNEQUAL      5
#define SYMBOL_SHIFT_LEFT   6
#define SYMBOL_SHIFT_RIGHT  7
#define SYMBOL_INCREMENT    8
#define SYMBOL_DECREMENT    9
#define SYMBOL_EQUAL_SUB   10
#define SYMBOL_EQUAL_ADD   11
#define SYMBOL_EQUAL_MUL   12
#define SYMBOL_EQUAL_DIV   13
#define SYMBOL_EQUAL_OR    14
#define SYMBOL_EQUAL_AND   15

#define VARIABLE_TYPE_NONE   0
#define VARIABLE_TYPE_VOID   1
#define VARIABLE_TYPE_INT8   2
#define VARIABLE_TYPE_INT16  3
#define VARIABLE_TYPE_UINT8  4
#define VARIABLE_TYPE_UINT16 5
#define VARIABLE_TYPE_CONST  6

struct local_variable {
  struct tree_node *node;
  int offset_to_fp;
  int size;
};

struct temp_register {
  int register_index;
  int offset_to_fp;
  int size;
};

struct local_variables {
  int arguments_count;
  int local_variables_count;
  struct local_variable *local_variables;
  int temp_registers_count;
  struct temp_register *temp_registers;
  int offset_to_fp_total;
};

struct tree_node {
  int type;
  int value;
  double value_double;
  char *label;
  int file_id;
  int line_number;
  struct tree_node *definition;
  int added_children;
  int children_max;
  struct tree_node **children;
  struct local_variables *local_variables;
  char flags;
  int  reads;
  int  writes;
};

#define TREE_NODE_FLAG_CONST_1       (1 << 0)
#define TREE_NODE_FLAG_CONST_2       (1 << 1)
#define TREE_NODE_FLAG_DATA_IS_CONST (1 << 2)
#define TREE_NODE_FLAG_PUREASM       (1 << 3)
#define TREE_NODE_FLAG_GLOBAL        (1 << 4)

#define TREE_NODE_TYPE_CREATE_VARIABLE      0
#define TREE_NODE_TYPE_ASSIGNMENT           1
#define TREE_NODE_TYPE_FUNCTION_DEFINITION  2
#define TREE_NODE_TYPE_BLOCK                3
#define TREE_NODE_TYPE_FUNCTION_CALL        4
#define TREE_NODE_TYPE_VARIABLE_TYPE        5
#define TREE_NODE_TYPE_VALUE_INT            6
#define TREE_NODE_TYPE_VALUE_DOUBLE         7
#define TREE_NODE_TYPE_VALUE_STRING         8
#define TREE_NODE_TYPE_RETURN               9
#define TREE_NODE_TYPE_BREAK               10
#define TREE_NODE_TYPE_SYMBOL              11
#define TREE_NODE_TYPE_EXPRESSION          12
#define TREE_NODE_TYPE_GLOBAL_NODES        13
#define TREE_NODE_TYPE_STACK               14
#define TREE_NODE_TYPE_CONDITION           15
#define TREE_NODE_TYPE_IF                  16
#define TREE_NODE_TYPE_INCREMENT_DECREMENT 17
#define TREE_NODE_TYPE_WHILE               18
#define TREE_NODE_TYPE_FOR                 19
#define TREE_NODE_TYPE_ARRAY_ITEM          20
#define TREE_NODE_TYPE_FUNCTION_PROTOTYPE  21
#define TREE_NODE_TYPE_CONTINUE            22
#define TREE_NODE_TYPE_SWITCH              23
#define TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT 24
#define TREE_NODE_TYPE_GET_ADDRESS         25
#define TREE_NODE_TYPE_GET_ADDRESS_ARRAY   26
#define TREE_NODE_TYPE_LABEL               27
#define TREE_NODE_TYPE_GOTO                28
#define TREE_NODE_TYPE_DEAD                29
#define TREE_NODE_TYPE_BYTES               30
#define TREE_NODE_TYPE_ASM                 31

struct symbol_table_item {
  int level;
  char *label;
  struct tree_node *node;
  struct symbol_table_item *next;
};

struct tac {
  unsigned char op;

  unsigned char arg1_type;
  double arg1_d;
  char *arg1_s;
  unsigned char arg1_var_type;
  unsigned char arg1_var_type_promoted;
  struct tree_node *arg1_node;

  unsigned char arg2_type;
  double arg2_d;
  char *arg2_s;
  unsigned char arg2_var_type;
  unsigned char arg2_var_type_promoted;
  struct tree_node *arg2_node;
  
  unsigned char result_type;
  double result_d;
  char *result_s;
  unsigned char result_var_type;
  unsigned char result_var_type_promoted;
  struct tree_node *result_node;

  int *registers;
  int *registers_types;
  
  struct tree_node *function_node;
  char is_function;

  struct tree_node *statement;
  int file_id;
  int line_number;
};

#define TAC_OP_DEAD               0
#define TAC_OP_LABEL              1
#define TAC_OP_ADD                2
#define TAC_OP_SUB                3
#define TAC_OP_ASSIGNMENT         4
#define TAC_OP_ARRAY_ASSIGNMENT   5
#define TAC_OP_CREATE_VARIABLE    6
#define TAC_OP_MUL                7
#define TAC_OP_JUMP               8
#define TAC_OP_JUMP_EQ            9
#define TAC_OP_JUMP_LT           10
#define TAC_OP_JUMP_GT           11
#define TAC_OP_JUMP_NEQ          12
#define TAC_OP_JUMP_LTE          13
#define TAC_OP_JUMP_GTE          14
#define TAC_OP_ARRAY_READ        15
#define TAC_OP_DIV               16
#define TAC_OP_MOD               17
#define TAC_OP_AND               18
#define TAC_OP_OR                19
#define TAC_OP_SHIFT_LEFT        20
#define TAC_OP_SHIFT_RIGHT       21
#define TAC_OP_RETURN            22
#define TAC_OP_RETURN_VALUE      23
#define TAC_OP_FUNCTION_CALL     24
#define TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE 25
#define TAC_OP_GET_ADDRESS       26
#define TAC_OP_GET_ADDRESS_ARRAY 27
#define TAC_OP_ASM               28

#define TAC_ARG_TYPE_NONE     0
#define TAC_ARG_TYPE_CONSTANT 1
#define TAC_ARG_TYPE_LABEL    2
#define TAC_ARG_TYPE_TEMP     3

#define TAC_USE_RESULT 0
#define TAC_USE_ARG1   1
#define TAC_USE_ARG2   2

struct breakable_stack_item {
  int label_break;
  int label_continue;
};

struct stack_item_priority_item {
  int op;
  int priority;
};

#define ASM_LINE_FLAG_READ  (1 << 0)
#define ASM_LINE_FLAG_WRITE (1 << 1)

struct asm_line {
  char *line;
  char flags;
  char cpu_register[3]; /* zero terminated */
  int line_number;
  struct tree_node *variable;
  struct asm_line *next;
};

struct inline_asm {
  int id;
  int file_id;
  int line_number;
  struct asm_line *asm_line_first;
  struct asm_line *asm_line_last;
  struct inline_asm *next;
};

#endif /* _DEFINES_H */
