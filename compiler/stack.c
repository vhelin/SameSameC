
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "defines.h"

#include "parse.h"
#include "stack.h"
#include "include_file.h"
#include "printf.h"
#include "main.h"
#include "definition.h"


/* define this for DEBUG */
/*
#define DEBUG_STACK 1
*/

extern int g_input_number_error_msg, g_bankheader_status, g_input_float_mode;
extern int g_source_pointer, g_size, g_parsed_int, g_string_size, g_parse_floats;
extern char g_xyz[512], *g_buffer, g_tmp[4096], g_label[MAX_NAME_LENGTH + 1];
extern struct definition *g_tmp_def;
extern double g_parsed_double;
extern int g_current_filename_id, g_current_line_number;
struct stack *g_stacks_first = NULL, *g_stacks_tmp = NULL, *g_stacks_last = NULL;
struct stack *g_stacks_header_first = NULL, *g_stacks_header_last = NULL;
extern int g_stack_inserted;
extern int g_operand_hint, g_operand_hint_type;
extern struct token *g_token_current;

int g_latest_stack = 0, g_stacks_inside = 0, g_stacks_outside = 0, g_stack_id = 0;
struct tree_node *g_label_definition = NULL;


static int _stack_insert(void) {

  if (g_stacks_first == NULL) {
    g_stacks_first = g_stacks_tmp;
    g_stacks_last = g_stacks_tmp;
  }
  else {
    g_stacks_last->next = g_stacks_tmp;
    g_stacks_last = g_stacks_tmp;
  }

  g_stacks_tmp->id = g_stack_id;
  g_stacks_tmp->section_status = OFF;
  g_stacks_tmp->section_id = 0;

  g_latest_stack = g_stack_id;
  g_stack_id++;

  return SUCCEEDED;
}


static int _break_before_value_or_string(int i, struct stack_item *si) {

  /* we use this function to test if the previous item in the stack
     is something that cannot be followed by a value or a string.
     in such a case we'll stop adding items to this stack computation */
  
  if (i <= 0)
    return FAILED;

  si = &si[i-1];
  if (si->type == STACK_ITEM_TYPE_VALUE)
    return SUCCEEDED;
  if (si->type == STACK_ITEM_TYPE_STRING)
    return SUCCEEDED;
  if (si->type == STACK_ITEM_TYPE_OPERATOR && si->value == SI_OP_RIGHT)
    return SUCCEEDED;

  return FAILED;
}


#if DEBUG_STACK

static void _debug_print_stack(int line_number, int stack_id, struct stack_item *ta, int count, int id) {

  int k;
  
  printf("LINE %5d: ID = %d (STACK) CALCULATION ID = %d (c%d): ", line_number, id, stack_id, stack_id);

  for (k = 0; k < count; k++) {
    char ar[] = "+-*()|&/^01%~<>!:";

    if (ta[k].type == STACK_ITEM_TYPE_OPERATOR) {
      int value = (int)ta[k].value;
      char arr = ar[value];

      /* 0 - shift left, 1 - shift right, otherwise it's the operator itself */
      if (arr == '0')
        printf("<<");
      else if (arr == '1')
        printf(">>");
      else
        printf("%c", arr);
    }
    else if (ta[k].type == STACK_ITEM_TYPE_VALUE)
      printf("V(%f)", ta[k].value);
    else if (ta[k].type == STACK_ITEM_TYPE_STACK)
      printf("C(%d)", (int)ta[k].value);
    else
      printf("S(%s)", ta[k].string);

    if (k < count-1)
      printf(", ");
  }
  printf("\n");
}

#endif


int get_label_length(char *l) {

  int length;
  
  length = (int)strlen(l);

  if (l[0] == '"' && l[length-1] == '"')
    length -= 2;

  return length;
}


int stack_calculate_tree_node(struct tree_node *node, int *value) {

  struct stack_item si[256];
  struct tree_node *child;
  int q, b = 0, failed = NO;

  /* only one string? HACK_1 */
  if (node->added_children == 1 && node->children[0]->type == TREE_NODE_TYPE_VALUE_STRING) {
    strcpy(g_label, node->children[0]->label);
    g_label_definition = node->children[0]->definition;
    return INPUT_NUMBER_STRING;
  }

  /* slice the data into infix format */
  for (q = 0; q < node->added_children; q++) {
    /* init the stack item */
    si[q].type = 0x123456;
    si[q].sign = 0x123456;
    si[q].value = 0x123456;
    si[q].string[0] = 0;

    child = node->children[q];
    
    if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '-') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_SUB;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM -\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '+') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_ADD;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM +\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '*') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_MULTIPLY;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM *\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '/') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_DIVIDE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM /\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '|') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_OR;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM |\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '&') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_AND;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM &\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '^') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_POWER;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ^\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '%') {
      if (q == 0) {
        if (g_input_number_error_msg == YES)
          print_error("Syntax error. Invalid use of modulo.\n", ERROR_STC);
        return FAILED;
      }
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_MODULO;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %%\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_LOGICAL_OR) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LOGICAL_OR;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ||\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_LOGICAL_AND) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LOGICAL_AND;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM &&\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_SHIFT_LEFT) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_SHIFT_LEFT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM <<\n");
#endif
    }
    /*
    else if (*in == '<') {
      if (b == 0 && q > 0) {
        if ((si[q-1].type == STACK_ITEM_TYPE_OPERATOR && si[q-1].value == SI_OP_RIGHT) ||
            si[q-1].type == STACK_ITEM_TYPE_VALUE || si[q-1].type == STACK_ITEM_TYPE_STRING)
          break;
      }

      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LOW_BYTE;
      q++;
      in++;
    }
    */
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_SHIFT_RIGHT) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_SHIFT_RIGHT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM >>\n");
#endif
    }
    /*
    else if (*in == '>') {
      if (b == 0 && q > 0) {
        if ((si[q-1].type == STACK_ITEM_TYPE_OPERATOR && si[q-1].value == SI_OP_RIGHT) ||
            si[q-1].type == STACK_ITEM_TYPE_VALUE || si[q-1].type == STACK_ITEM_TYPE_STRING)
          break;
      }

      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_HIGH_BYTE;
      q++;
      in++;
    }
    */
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '!') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_NOT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM !\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '~') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_XOR;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ~\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ':') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_BANK;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM :\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '(') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LEFT;
      b++;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM (\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ')') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_RIGHT;
      /* end of expression? */
      if (b == 0)
        break;
      b--;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM )\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '<') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_LT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM <\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '>') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_GT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM >\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_EQUAL) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_EQ;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ==\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_UNEQUAL) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_NEQ;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM !=\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_LTE) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_LTE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM <=\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_GTE) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_GTE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM >=\n");
#endif
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ',')
      break;
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ']')
      break;
    else if (child->type == TREE_NODE_TYPE_VALUE_INT) {
      /* we'll break if the previous item in the stack was a value or a string */
      if (_break_before_value_or_string(q, &si[0]) == SUCCEEDED)
        break;

      si[q].type = STACK_ITEM_TYPE_VALUE;
      si[q].value = child->value;
      si[q].sign = SI_SIGN_POSITIVE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %d\n", (int)si[q].value);
#endif
    }
    else if (child->type == TREE_NODE_TYPE_VALUE_DOUBLE) {
      /* we'll break if the previous item in the stack was a value or a string */
      if (_break_before_value_or_string(q, &si[0]) == SUCCEEDED)
        break;

      si[q].type = STACK_ITEM_TYPE_VALUE;
      si[q].value = child->value_double;
      si[q].sign = SI_SIGN_POSITIVE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %f\n", si[q].value);
#endif
    }
    else if (child->type == TREE_NODE_TYPE_VALUE_STRING) {
      /* we'll break if the previous item in the stack was a value or a string */
      if (_break_before_value_or_string(q, &si[0]) == SUCCEEDED)
        break;

      si[q].sign = SI_SIGN_POSITIVE;
      strncpy(si[q].string, child->label, MAX_NAME_LENGTH);
      si[q].type = STACK_ITEM_TYPE_STRING;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %s\n", si[q].string);
#endif
    }
    else if (child->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
      /* increment/decrement cannot be calculated here, only on the target machine... */
      failed = YES;
    }
    else if (child->type == TREE_NODE_TYPE_GET_ADDRESS) {
      /* get address cannot be calculated here, only on the target machine... */
      failed = YES;
    }
    else if (child->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY) {
      /* get address array cannot be calculated here, only on the target machine... */
      failed = YES;
    }
    else if (child->type == TREE_NODE_TYPE_FUNCTION_CALL) {
      /* function calls cannot be calculated here, only on the target machine... */
      failed = YES;
    }
    else if (child->type == TREE_NODE_TYPE_ARRAY_ITEM) {
      /* array items cannot be calculated here, only on the target machine... */
      /* TODO: if the array is const and index is a value, then this can be solved here... */
      failed = YES;
    }
    else if (child->type == TREE_NODE_TYPE_BYTES) {
      /* raw bytes in a calculation - doesn't make any sense */
      failed = YES;
    }
    else {
      fprintf(stderr, "STACK_CALCULATE_TREE_NODE: Got an unhandled tree_node of type %d! Please submit a bug report!\n", child->type);
      return FAILED;
    }

    if (q == 255) {
      print_error("Out of stack space.\n", ERROR_STC);
      return FAILED;
    }
  }

  if (failed == YES)
    return FAILED;
  
  if (b != 0) {
    print_error("Unbalanced parentheses.\n", ERROR_STC);
    return FAILED;
  }
  
  return stack_calculate(value, si, q, YES);
}


int stack_calculate_tokens(int *value) {

  struct stack_item si[256];
  int q = 0, b = 0;
  
  /* slice the data into infix format */
  while (1) {
    /* init the stack item */
    si[q].type = 0x123456;
    si[q].sign = 0x123456;
    si[q].value = 0x123456;
    si[q].string[0] = 0;

    if (g_token_current == NULL)
      break;

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '-') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_SUB;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM -\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '+') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_ADD;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM +\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_MULTIPLY;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM *\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '/') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_DIVIDE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM /\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '|') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_OR;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM |\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '&') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_AND;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM &\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '^') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_POWER;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ^\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '%') {
      if (q == 0) {
        if (g_input_number_error_msg == YES)
          print_error("Syntax error. Invalid use of modulo.\n", ERROR_STC);
        return FAILED;
      }
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_MODULO;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %%\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_LOGICAL_OR) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LOGICAL_OR;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ||\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_LOGICAL_AND) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LOGICAL_AND;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM &&\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_SHIFT_LEFT) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_SHIFT_LEFT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM <<\n");
#endif
    }
    /*
    else if (*in == '<') {
      if (b == 0 && q > 0) {
        if ((si[q-1].type == STACK_ITEM_TYPE_OPERATOR && si[q-1].value == SI_OP_RIGHT) ||
            si[q-1].type == STACK_ITEM_TYPE_VALUE || si[q-1].type == STACK_ITEM_TYPE_STRING)
          break;
      }

      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LOW_BYTE;
      q++;
      in++;
    }
    */
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_SHIFT_RIGHT) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_SHIFT_RIGHT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM >>\n");
#endif
    }
    /*
    else if (*in == '>') {
      if (b == 0 && q > 0) {
        if ((si[q-1].type == STACK_ITEM_TYPE_OPERATOR && si[q-1].value == SI_OP_RIGHT) ||
            si[q-1].type == STACK_ITEM_TYPE_VALUE || si[q-1].type == STACK_ITEM_TYPE_STRING)
          break;
      }

      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_HIGH_BYTE;
      q++;
      in++;
    }
    */
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '!') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_NOT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM !\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '~') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_XOR;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ~\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ':') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_BANK;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM :\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '(') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_LEFT;
      b++;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM (\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_RIGHT;
      /* end of expression? */
      if (b == 0)
        break;
      b--;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM )\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '<') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_LT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM <\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '>') {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_GT;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM >\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_EQUAL) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_EQ;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM ==\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_UNEQUAL) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_NEQ;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM !=\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_LTE) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_LTE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM <=\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_GTE) {
      si[q].type = STACK_ITEM_TYPE_OPERATOR;
      si[q].value = SI_OP_COMPARE_GTE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM >=\n");
#endif
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',')
      break;
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']')
      break;
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
      /* next token */
      g_token_current = g_token_current->next;
      break;
    }
    else if (g_token_current->id == TOKEN_ID_VALUE_INT) {
      /* we'll break if the previous item in the stack was a value or a string */
      if (_break_before_value_or_string(q, &si[0]) == SUCCEEDED)
        break;

      si[q].type = STACK_ITEM_TYPE_VALUE;
      si[q].value = g_token_current->value;
      si[q].sign = SI_SIGN_POSITIVE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %d\n", (int)si[q].value);
#endif
    }
    else if (g_token_current->id == TOKEN_ID_VALUE_DOUBLE) {
      /* we'll break if the previous item in the stack was a value or a string */
      if (_break_before_value_or_string(q, &si[0]) == SUCCEEDED)
        break;

      si[q].type = STACK_ITEM_TYPE_VALUE;
      si[q].value = g_token_current->value_double;
      si[q].sign = SI_SIGN_POSITIVE;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %f\n", si[q].value);
#endif
    }
    else if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
      /* we'll break if the previous item in the stack was a value or a string */
      if (_break_before_value_or_string(q, &si[0]) == SUCCEEDED)
        break;

      si[q].sign = SI_SIGN_POSITIVE;
      strncpy(si[q].string, g_token_current->label, MAX_NAME_LENGTH);
      si[q].type = STACK_ITEM_TYPE_STRING;
#if DEBUG_STACK
      fprintf(stderr, "GOT STACK ITEM %s\n", si[q].string);
#endif
    }
    else
      break;

    q++;

    /* next token */
    g_token_current = g_token_current->next;

    if (q == 255) {
      print_error("Out of stack space.\n", ERROR_STC);
      return FAILED;
    }
  }

  if (b != 0) {
    print_error("Unbalanced parentheses.\n", ERROR_STC);
    return FAILED;
  }

  return stack_calculate(value, si, q, YES);
}


static struct stack_item_priority_item g_stack_item_priority_items[] = {
  { SI_OP_LOGICAL_OR, 10 },
  { SI_OP_LOGICAL_AND, 20 },
  { SI_OP_OR, 30 },
  { SI_OP_XOR, 40 },
  { SI_OP_AND, 50 },
  { SI_OP_COMPARE_EQ, 60 },
  { SI_OP_COMPARE_NEQ, 60 },
  { SI_OP_COMPARE_LT, 70 },
  { SI_OP_COMPARE_GT, 70 },
  { SI_OP_COMPARE_LTE, 70 },
  { SI_OP_COMPARE_GTE, 70 },
  { SI_OP_SHIFT_LEFT, 80 },
  { SI_OP_SHIFT_RIGHT, 80 },
  { SI_OP_ADD, 90 },
  { SI_OP_SUB, 90 },
  { SI_OP_MULTIPLY, 100 },
  { SI_OP_DIVIDE, 100 },
  { SI_OP_MODULO, 100 },
  { SI_OP_POWER, 100 },
  { SI_OP_PRE_INC, 110 },
  { SI_OP_PRE_DEC, 110 },
  { SI_OP_POST_INC, 110 },
  { SI_OP_POST_DEC, 110 },
  { SI_OP_LOW_BYTE, 110 },
  { SI_OP_HIGH_BYTE, 110 },
  { SI_OP_BANK, 110 },
  { SI_OP_NOT, 110 },
  { SI_OP_GET_ADDRESS, 110 },
  { SI_OP_USE_REGISTER, 200 },
  { 999, 999 }
};


int get_op_priority(int op) {

  int i = 0;

  while (g_stack_item_priority_items[i].op < 999) {
    if (g_stack_item_priority_items[i].op == op)
      return g_stack_item_priority_items[i].priority;
    i++;
  }

  fprintf(stderr, "get_op_priority(): No priority for OP %d! Please submit a bug report\n", op);

  return 0;
}


int stack_calculate(int *value, struct stack_item *si, int q, int save_if_cannot_resolve) {

  int b, d, k, op[256], o, l;
  struct stack_item ta[256];
  struct stack s;
  double dou = 0.0;

  /* only one item found -> let the item parser handle it */
  /*
  if (q == 1)
    return STACK_CALCULATE_DELAY;
  */

  /* check if there was data before the computation */
  if (q > 1 && (si[0].type == STACK_ITEM_TYPE_STRING || si[0].type == STACK_ITEM_TYPE_VALUE)) {
    if (si[1].type == STACK_ITEM_TYPE_STRING || si[1].type == STACK_ITEM_TYPE_VALUE)
      return STACK_CALCULATE_DELAY;
    if (si[1].type == STACK_ITEM_TYPE_OPERATOR) {
      if (si[1].value == SI_OP_LEFT)
        return STACK_CALCULATE_DELAY;
    }
  }

  /* check if the computation has "--" and turn it into a "+" */
  for (k = 0; k < q-2; k++) {
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_SUB &&
        si[k+1].type == STACK_ITEM_TYPE_OPERATOR && si[k+1].value == SI_OP_SUB) {
      si[k].type = STACK_ITEM_TYPE_DELETED;
      si[k+1].value = SI_OP_ADD;
    }
  }

  /* check if the computation has "+-" or "-+" and remove that "+" */
  for (k = 0; k < q-2; k++) {
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_ADD &&
        si[k+1].type == STACK_ITEM_TYPE_OPERATOR && si[k+1].value == SI_OP_SUB) {
      si[k].type = STACK_ITEM_TYPE_DELETED;
    }
  }

  /* fix the sign in every operand */
  for (b = 1, k = 0; k < q; k++) {
    /* NOT VERY USEFUL IN BILIBALI
    if ((q - k) != 1 && si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].value != SI_OP_BANK
        && si[k + 1].value != SI_OP_HIGH_BYTE && si[k + 1].value != SI_OP_LOW_BYTE) {
      if (si[k].value != SI_OP_LEFT && si[k].value != SI_OP_RIGHT && si[k + 1].value != SI_OP_LEFT && si[k + 1].value != SI_OP_RIGHT) {
        print_error("Error in computation syntax.\n", ERROR_STC);
        return FAILED;
      }
    }
    */
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_SUB && b == 1) {
      if (si[k + 1].type == STACK_ITEM_TYPE_VALUE || si[k + 1].type == STACK_ITEM_TYPE_STRING) {
        if (si[k + 1].sign == SI_SIGN_POSITIVE)
          si[k + 1].sign = SI_SIGN_NEGATIVE;
        else
          si[k + 1].sign = SI_SIGN_POSITIVE;
        /* it wasn't a minus operator, it was a sign */
        si[k].type = STACK_ITEM_TYPE_DELETED;
      }
      else if (si[k + 1].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].value == SI_OP_LEFT) {
        o = 1;
        l = k + 2;
        while (o > 0 && l < q) {
          if (si[l].type == STACK_ITEM_TYPE_VALUE || si[l].type == STACK_ITEM_TYPE_STRING) {
            if (si[l].sign == SI_SIGN_POSITIVE)
              si[l].sign = SI_SIGN_NEGATIVE;
            else
              si[l].sign = SI_SIGN_POSITIVE;
          }
          else if (si[l].type == STACK_ITEM_TYPE_OPERATOR) {
            if (si[l].value == SI_OP_LEFT)
              o++;
            else if (si[l].value == SI_OP_RIGHT)
              o--;
          }
          l++;
        }

        if (o != 0) {
          print_error("Unbalanced parentheses.\n", ERROR_STC);
          return FAILED;
        }

        si[k].type = STACK_ITEM_TYPE_DELETED;
      }
    }
    /* remove unnecessary + */
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_ADD && b == 1) {
      if (si[k + 1].type == STACK_ITEM_TYPE_VALUE || si[k + 1].type == STACK_ITEM_TYPE_STRING)
        si[k].type = STACK_ITEM_TYPE_DELETED;
      else if (si[k + 1].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].value == SI_OP_LEFT)
        si[k].type = STACK_ITEM_TYPE_DELETED;
    }
    else if (si[k].type == STACK_ITEM_TYPE_VALUE || si[k].type == STACK_ITEM_TYPE_STRING)
      b = 0;
    else if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_LEFT)
      b = 1;
  }

  /* turn unary XORs into NOTs */
  for (b = 1, k = 0; k < q; k++) {
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_XOR && b == 1)
      si[k].value = SI_OP_NOT;
    else if (si[k].type == STACK_ITEM_TYPE_VALUE || si[k].type == STACK_ITEM_TYPE_STRING)
      b = 0;
    else if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_LEFT)
      b = 1;
  }

  /* convert infix stack into postfix stack */
  for (b = 0, k = 0, d = 0; k < q; k++) {
    /* operands pass through */
    if (si[k].type == STACK_ITEM_TYPE_VALUE) {
      ta[d].type = si[k].type;
      ta[d].value = si[k].value;
      ta[d].sign = si[k].sign;
      d++;
    }
    else if (si[k].type == STACK_ITEM_TYPE_STRING) {
      ta[d].type = si[k].type;
      strcpy(ta[d].string, si[k].string);
      ta[d].sign = si[k].sign;
      d++;
    }
    /* operators get inspected */
    else if (si[k].type == STACK_ITEM_TYPE_OPERATOR) {
      if (b == 0) {
        op[0] = (int)si[k].value;
        b++;
      }
      else {
        if (si[k].value == SI_OP_LEFT) {
          op[b] = SI_OP_LEFT;
          b++;
        }
        else if (si[k].value == SI_OP_RIGHT) {
          b--;
          while (op[b] != SI_OP_LEFT) {
            ta[d].type = STACK_ITEM_TYPE_OPERATOR;
            ta[d].value = op[b];
            b--;
            d++;
          }
        }
        else if (si[k].value == SI_OP_USE_REGISTER) {
          /* use register, priority over everything else */
          op[b] = (int)si[k].value;
          b++;
        }
        else {
          int priority = get_op_priority(si[k].value);
          
          b--;
          while (b != -1 && op[b] != SI_OP_LEFT && get_op_priority(op[b]) >= priority) {
            ta[d].type = STACK_ITEM_TYPE_OPERATOR;
            ta[d].value = op[b];
            b--;
            d++;
          }
          b++;
          op[b] = (int)si[k].value;
          b++;
        }
      }
    }
  }

  /* empty the operator stack */
  while (b > 0) {
    b--;
    ta[d].type = STACK_ITEM_TYPE_OPERATOR;
    ta[d].value = op[b];
    d++;
  }

  /* are all the symbols known? */
  if (resolve_stack(ta, d) == SUCCEEDED) {
    s.stack = ta;
    s.linenumber = g_current_line_number;
    s.filename_id = g_current_filename_id;

    if (compute_stack(&s, d, &dou) == FAILED)
      return FAILED;
    
    g_parsed_double = dou;

    if (g_input_float_mode == ON)
      return INPUT_NUMBER_FLOAT;

    *value = (int)dou;

    return SUCCEEDED;
  }

  /* only one string? */
  if (d == 1 && ta[0].type == STACK_ITEM_TYPE_STRING) {
    fprintf(stderr, "stack_calculate(): The label \"%s\" cannot be turned into a value.\n", ta[0].string);
    strcpy(g_label, ta[0].string);
    g_label_definition = NULL;
    return INPUT_NUMBER_STRING;
  }

  /*
    printf("%d %d %s\n", d, ta[0].type, ta[0].string);
  */

  if (save_if_cannot_resolve == NO)
    return STACK_CALCULATE_CANNOT_RESOLVE;
  
  /* we have a stack full of computation and we save it for wlalink */
  g_stacks_tmp = calloc(sizeof(struct stack), 1);
  if (g_stacks_tmp == NULL) {
    print_error("Out of memory error while allocating room for a new stack.\n", ERROR_STC);
    return FAILED;
  }
  g_stacks_tmp->next = NULL;
  g_stacks_tmp->type = STACK_TYPE_UNKNOWN;
  g_stacks_tmp->bank = -123456;
  g_stacks_tmp->stacksize = d;
  g_stacks_tmp->relative_references = 0;
  g_stacks_tmp->stack = calloc(sizeof(struct stack_item) * d, 1);
  if (g_stacks_tmp->stack == NULL) {
    free(g_stacks_tmp);
    print_error("Out of memory error while allocating room for a new stack.\n", ERROR_STC);
    return FAILED;
  }

  g_stacks_tmp->linenumber = g_current_line_number;
  g_stacks_tmp->filename_id = g_current_filename_id;
  g_stacks_tmp->special_id = 0;

  /* all stacks will be definition stacks by default. pass_4 will mark
     those that are referenced to be STACK_POSITION_CODE stacks */
  g_stacks_tmp->position = STACK_POSITION_DEFINITION;

  for (q = 0; q < d; q++) {
    if (ta[q].type == STACK_ITEM_TYPE_OPERATOR) {
      g_stacks_tmp->stack[q].type = STACK_ITEM_TYPE_OPERATOR;
      g_stacks_tmp->stack[q].value = ta[q].value;
    }
    else if (ta[q].type == STACK_ITEM_TYPE_VALUE) {
      g_stacks_tmp->stack[q].type = STACK_ITEM_TYPE_VALUE;
      g_stacks_tmp->stack[q].value = ta[q].value;
      g_stacks_tmp->stack[q].sign = ta[q].sign;
    }
    else if (ta[q].type == STACK_ITEM_TYPE_STACK) {
      g_stacks_tmp->stack[q].type = STACK_ITEM_TYPE_STACK;
      g_stacks_tmp->stack[q].value = ta[q].value;
      g_stacks_tmp->stack[q].sign = ta[q].sign;
    }
    else {
      g_stacks_tmp->stack[q].type = STACK_ITEM_TYPE_STRING;
      g_stacks_tmp->stack[q].sign = ta[q].sign;
      strcpy(g_stacks_tmp->stack[q].string, ta[q].string);
    }
  }

#if DEBUG_STACK
  _debug_print_stack(g_stacks_tmp->linenumber, g_stack_id, g_stacks_tmp->stack, d, 0);
#endif

  _stack_insert();

  return INPUT_NUMBER_STACK;
}


static int _resolve_string(struct stack_item *s, int *cannot_resolve) {

  /* is this form "string".length? */
  if (is_string_ending_with(s->string, ".length") > 0 ||
      is_string_ending_with(s->string, ".LENGTH") > 0) {
    /* we have a X.length -> parse */
    s->string[strlen(s->string) - 7] = 0;
    s->value = get_label_length(s->string);
    s->type = STACK_ITEM_TYPE_VALUE;
  }
  
  return SUCCEEDED;
}


int resolve_stack(struct stack_item s[], int x) {

  struct stack_item *st;
  int q = x, cannot_resolve = 0;

  st = s;
  while (x > 0) {
    if (s->type == STACK_ITEM_TYPE_STRING) {
      if (_resolve_string(s, &cannot_resolve) == FAILED)
        return FAILED;
    }
    s++;
    x--;
  }

  if (cannot_resolve != 0)
    return FAILED;

  /* find a string, a stack or a bank and fail */
  while (q > 0) {
    if (st->type == STACK_ITEM_TYPE_STRING || st->type == STACK_ITEM_TYPE_STACK || (st->type == STACK_ITEM_TYPE_OPERATOR && st->value == SI_OP_BANK))
      return FAILED;
    q--;
    st++;
  }

  return SUCCEEDED;
}


int compute_stack(struct stack *sta, int x, double *result) {

  struct stack_item *s;
  double v[256];
  int r, t, z;


  v[0] = 0.0;

  s = sta->stack;
  for (r = 0, t = 0; r < x; r++, s++) {
    if (s->type == STACK_ITEM_TYPE_VALUE) {
      if (s->sign == SI_SIGN_NEGATIVE)
        v[t] = -s->value;
      else
        v[t] = s->value;
      t++;
    }
    else {
      switch ((int)s->value) {
      case SI_OP_ADD:
        v[t - 2] += v[t - 1];
        t--;
        break;
      case SI_OP_SUB:
        v[t - 2] -= v[t - 1];
        t--;
        break;
      case SI_OP_MULTIPLY:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Multiply is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] *= v[t - 1];
        t--;
        break;
      case SI_OP_LOW_BYTE:
        z = (int)v[t - 1];
        v[t - 1] = z & 0xFF;
        break;
      case SI_OP_HIGH_BYTE:
        z = (int)v[t - 1];
        v[t - 1] = (z>>8) & 0xFF;
        break;
      case SI_OP_LOGICAL_OR:
        if (v[t-1] != 0 || v[t-2] != 0)
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_LOGICAL_AND:
        if (v[t-1] != 0 && v[t-2] != 0)
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_COMPARE_LT:
        if (v[t-2] < v[t-1])
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_COMPARE_GT:
        if (v[t-2] > v[t-1])
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_COMPARE_EQ:
        if (v[t-2] == v[t-1])
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_COMPARE_NEQ:
        if (v[t-2] != v[t-1])
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_COMPARE_LTE:
        if (v[t-2] <= v[t-1])
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_COMPARE_GTE:
        if (v[t-2] >= v[t-1])
          v[t-2] = 1;
        else
          v[t-2] = 0;
        t--;
        break;
      case SI_OP_NOT:
        /*
        fprintf(stderr, "%s:%d: COMPUTE_STACK: NOT cannot determine the output size.\n", get_file_name(sta->filename_id), sta->linenumber);
        return FAILED;
        */
        if ((int)v[t - 1] == 0)
          v[t - 1] = 1;
        else
          v[t - 1] = 0;
        break;
      case SI_OP_XOR:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: XOR is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 1] ^ (int)v[t - 2];
        t--;
        break;
      case SI_OP_OR:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: OR is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 1] | (int)v[t - 2];
        t--;
        break;
      case SI_OP_AND:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: AND is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 1] & (int)v[t - 2];
        t--;
        break;
      case SI_OP_MODULO:
        if (((int)v[t - 1]) == 0) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Modulo by zero.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Modulo is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 2] % (int)v[t - 1];
        t--;
        break;
      case SI_OP_DIVIDE:
        if (v[t - 1] == 0.0) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Division by zero.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Division is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] /= v[t - 1];
        t--;
        break;
      case SI_OP_POWER:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Power is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = pow(v[t - 2], v[t - 1]);
        t--;
        break;
      case SI_OP_SHIFT_LEFT:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Shift left is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 2] << (int)v[t - 1];
        t--;
        break;
      case SI_OP_SHIFT_RIGHT:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: COMPUTE_STACK: Shift right is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 2] >> (int)v[t - 1];
        t--;
        break;
      }
    }
  }

  /*
    #ifdef W65816
    if (v[0] < -8388608 || v[0] > 16777215) {
    print_error("Out of 24-bit range.\n", ERROR_STC);
    return FAILED;
    }
    #else
    if (v[0] < -32768 || v[0] > 65536) {
    print_error("Out of 16-bit range.\n", ERROR_STC);
    return FAILED;
    }
    #endif
  */

  *result = v[0];

  return SUCCEEDED;
}


int stack_create_label_stack(char *label) {

  if (label == NULL)
    return FAILED;

  /* we need to create a stack that holds just one label */
  g_stacks_tmp = calloc(sizeof(struct stack), 1);
  if (g_stacks_tmp == NULL) {
    print_error("Out of memory error while allocating room for a new stack.\n", ERROR_STC);
    return FAILED;
  }
  g_stacks_tmp->next = NULL;
  g_stacks_tmp->type = STACK_TYPE_UNKNOWN;
  g_stacks_tmp->bank = -123456;
  g_stacks_tmp->stacksize = 1;
  g_stacks_tmp->relative_references = 0;
  g_stacks_tmp->stack = calloc(sizeof(struct stack_item), 1);
  if (g_stacks_tmp->stack == NULL) {
    free(g_stacks_tmp);
    print_error("Out of memory error while allocating room for a new stack.\n", ERROR_STC);
    return FAILED;
  }

  g_stacks_tmp->linenumber = g_current_line_number;
  g_stacks_tmp->filename_id = g_current_filename_id;
  g_stacks_tmp->special_id = 0;
  
  /* all stacks will be definition stacks by default. pass_4 will mark
     those that are referenced to be STACK_POSITION_CODE stacks */
  g_stacks_tmp->position = STACK_POSITION_DEFINITION;

  g_stacks_tmp->stack[0].type = STACK_ITEM_TYPE_STRING;
  g_stacks_tmp->stack[0].sign = SI_SIGN_POSITIVE;
  strcpy(g_stacks_tmp->stack[0].string, label);

#if DEBUG_STACK
  _debug_print_stack(g_stacks_tmp->linenumber, g_stack_id, g_stacks_tmp->stack, 1, 1);
#endif
  
  _stack_insert();

  return SUCCEEDED;
}


int stack_create_stack_stack(int stack_id) {

  /* we need to create a stack that holds just one computation stack */
  g_stacks_tmp = calloc(sizeof(struct stack), 1);
  if (g_stacks_tmp == NULL) {
    print_error("Out of memory error while allocating room for a new stack.\n", ERROR_STC);
    return FAILED;
  }
  g_stacks_tmp->next = NULL;
  g_stacks_tmp->type = STACK_TYPE_UNKNOWN;
  g_stacks_tmp->bank = -123456;
  g_stacks_tmp->stacksize = 1;
  g_stacks_tmp->relative_references = 0;
  g_stacks_tmp->stack = calloc(sizeof(struct stack_item), 1);
  if (g_stacks_tmp->stack == NULL) {
    free(g_stacks_tmp);
    print_error("Out of memory error while allocating room for a new stack.\n", ERROR_STC);
    return FAILED;
  }

  g_stacks_tmp->linenumber = g_current_line_number;
  g_stacks_tmp->filename_id = g_current_filename_id;
  g_stacks_tmp->special_id = 0;
  
  /* all stacks will be definition stacks by default. pass_4 will mark
     those that are referenced to be STACK_POSITION_CODE stacks */
  g_stacks_tmp->position = STACK_POSITION_DEFINITION;

  g_stacks_tmp->stack[0].type = STACK_ITEM_TYPE_STACK;
  g_stacks_tmp->stack[0].value = stack_id;
  g_stacks_tmp->stack[0].sign = SI_SIGN_POSITIVE;

#if DEBUG_STACK
  _debug_print_stack(g_stacks_tmp->linenumber, stack_id, g_stacks_tmp->stack, 1, 2);
#endif

  _stack_insert();
    
  return SUCCEEDED;
}
