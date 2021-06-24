
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "parse.h"
#include "main.h"
#include "printf.h"
#include "definitions.h"
#include "stack.h"
#include "include_file.h"
#include "pass_4.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char *g_variable_types[4], *g_two_char_symbols[10], g_label[MAX_NAME_LENGTH + 1];
extern double g_parsed_double;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

static struct breakable_stack_item g_breakable_stack_items[256];
static int g_block_level = 0, g_breakable_stack_items_level = -1;
static struct tree_node *g_current_function = NULL;

int g_temp_r = 0, g_temp_label_id = 0;

/* add_tac() uses this variable to set the statement field */
struct tree_node *g_current_statement = NULL;

static int _generate_il_create_block(struct tree_node *node);
static int _generate_il_create_increment_decrement(struct tree_node *node);


static int _enter_breakable(int label_break, int label_continue) {

  if (g_breakable_stack_items_level >= 255)
    return FAILED;

  g_breakable_stack_items_level++;
  
  g_breakable_stack_items[g_breakable_stack_items_level].label_break = label_break;
  g_breakable_stack_items[g_breakable_stack_items_level].label_continue = label_continue;
  
  return SUCCEEDED;
}


static void _exit_breakable(void) {

  if (g_breakable_stack_items_level < 0)
    fprintf(stderr, "_exit_breakable(): Breakable stack is already empty! Please submit a bug report!\n");
  else
    g_breakable_stack_items_level--;
}


int make_sure_all_tacs_have_definition_nodes(void) {

  int i, ret = SUCCEEDED;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int skip_arg1 = NO, skip_arg2 = NO, skip_result = NO;
    
    if (t->op == TAC_OP_DEAD)
      continue;

    /* jump labels don't have definition nodes as they were generated when IL was generated */
    if (t->op == TAC_OP_JUMP ||
        t->op == TAC_OP_JUMP_EQ ||
        t->op == TAC_OP_JUMP_NEQ ||
        t->op == TAC_OP_JUMP_LT ||
        t->op == TAC_OP_JUMP_GT ||
        t->op == TAC_OP_JUMP_LTE ||
        t->op == TAC_OP_JUMP_GTE)
      skip_result = YES;
    
    if (skip_arg1 == NO && t->arg1_type == TAC_ARG_TYPE_LABEL && t->arg1_node == NULL) {
      g_current_filename_id = -1;
      g_current_line_number = -1;
      snprintf(g_error_message, sizeof(g_error_message), "make_sure_all_tacs_have_definition_nodes(): Missing variable \"%s\"'s node! Please submit a bug report!\n", t->arg1_s);
      print_error(g_error_message, ERROR_ERR);
      ret = FAILED;
    }
    if (skip_arg2 == NO && t->arg2_type == TAC_ARG_TYPE_LABEL && t->arg2_node == NULL) {
      g_current_filename_id = -1;
      g_current_line_number = -1;
      snprintf(g_error_message, sizeof(g_error_message), "make_sure_all_tacs_have_definition_nodes(): Missing variable \"%s\"'s node! Please submit a bug report!\n", t->arg2_s);
      print_error(g_error_message, ERROR_ERR);
      ret = FAILED;
    }
    if (skip_result == NO && t->result_type == TAC_ARG_TYPE_LABEL && t->result_node == NULL) {
      g_current_filename_id = -1;
      g_current_line_number = -1;
      snprintf(g_error_message, sizeof(g_error_message), "make_sure_all_tacs_have_definition_nodes(): Missing variable \"%s\"'s node! Please submit a bug report!\n", t->result_s);
      print_error(g_error_message, ERROR_ERR);
      ret = FAILED;
    }
  }

  return ret;
}


int pass_4(void) {

  if (g_verbose_mode == ON)
    printf("Pass 4...\n");

  if (generate_il() == FAILED)
    return FAILED;
  if (make_sure_all_tacs_have_definition_nodes() == FAILED)
    return FAILED;

  print_tacs();
  
  return SUCCEEDED;
}


static int _generate_il_create_expression(struct tree_node *node) {

  struct tac *t;
  int type;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->type == TREE_NODE_TYPE_EXPRESSION) {
    if (il_stack_calculate_expression(node, YES) == FAILED)
      return FAILED;
  }
  else if (node->type == TREE_NODE_TYPE_CONDITION) {
    if (il_stack_calculate_expression(node->children[0], YES) == FAILED)
      return FAILED;
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_STRING) {
    t = add_tac();
    if (t == NULL)
      return FAILED;

    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);

    /* set promotions */
    type = tree_node_get_max_var_type(node->definition->children[0]);
    tac_promote_argument(t, type, TAC_USE_RESULT);
    tac_promote_argument(t, type, TAC_USE_ARG1);

    /* find the definition */
    if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
      return FAILED;
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_INT) {
    t = add_tac();
    if (t == NULL)
      return FAILED;

    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, node->value, NULL);

    /* set promotions */
    type = get_variable_type_constant(node->value);
    tac_promote_argument(t, type, TAC_USE_ARG1);
    tac_promote_argument(t, type, TAC_USE_RESULT);
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_DOUBLE) {
    t = add_tac();
    if (t == NULL)
      return FAILED;

    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, (int)node->value_double, NULL);

    /* set promotions */
    type = get_variable_type_constant((int)node->value_double);
    tac_promote_argument(t, type, TAC_USE_ARG1);
    tac_promote_argument(t, type, TAC_USE_RESULT);
  }
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL) {
    fprintf(stderr, "_generate_il_create_expression(): IMPLEMENT ME - TREE_NODE_TYPE_FUNCTION_CALL!\n");
  }
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM) {
    int rindex = g_temp_r;

    if (_generate_il_create_expression(node->children[0]) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;

    t->op = TAC_OP_ARRAY_READ;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);
    tac_set_arg2(t, TAC_ARG_TYPE_TEMP, rindex, NULL);

    /* find the definition */
    if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
      return FAILED;
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    int rresult;
      
    if (node->value_double > 0.0) {
      /* post */

      /* get the value before increment/decrement */
      t = add_tac();
      if (t == NULL)
        return FAILED;

      rresult = g_temp_r++;
      
      t->op = TAC_OP_ASSIGNMENT;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, rresult, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);

      /* find the definition */
      if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
        return FAILED;
      
      /* increment/decrement */
      if (_generate_il_create_increment_decrement(node) == FAILED)
        return FAILED;

      /* have the result in the latest register */
      t = add_tac();
      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_ASSIGNMENT;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, rresult, 0);
    }
    else {
      /* pre */

      /* increment/decrement */
      if (_generate_il_create_increment_decrement(node) == FAILED)
        return FAILED;

      /* have the result in the latest register */
      t = add_tac();
      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_ASSIGNMENT;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);

      /* find the definition */
      if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
        return FAILED;
    }
  }
  else {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_expression(): Unsupported tree node %d!\n", node->type);
    print_error(g_error_message, ERROR_ERR);

    return FAILED;
  }

  return SUCCEEDED;
}


static int _generate_il_create_variable(struct tree_node *node) {

  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  /* create variable */
  
  symbol_table_add_symbol(node, node->children[1]->label, g_block_level);

  t = add_tac();
  if (t == NULL)
    return FAILED;

  t->op = TAC_OP_CREATE_VARIABLE;
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[1]->label);
  t->result_node = node;

  /* if function argument, no need to assign anything, it's already done */
  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    return SUCCEEDED;
  
  /* make assignment(s) */

  if (node->value == 0) {
    /* single value assignment, not an array */
    int r1 = g_temp_r, type;

    if (_generate_il_create_expression(node->children[2]) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;

    /* r1 is the value */

    t->op = TAC_OP_ASSIGNMENT;
  
    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[1]->label);
    t->result_node = node;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);

    /* set promotions */
    type = tree_node_get_max_var_type(node->children[0]);
    tac_promote_argument(t, type, TAC_USE_RESULT);
    type = tree_node_get_max_var_type(node->children[2]);
    tac_promote_argument(t, type, TAC_USE_ARG1);
  }
  else {
    /* array copy */
    int i;

    for (i = 0; i < node->value; i++) {
      int r2 = g_temp_r, type;

      if (_generate_il_create_expression(node->children[2 + i]) == FAILED)
        return FAILED;

      t = add_tac();
      if (t == NULL)
        return FAILED;

      /* i is the array index, r2 is the value */

      t->op = TAC_OP_ARRAY_ASSIGNMENT;
    
      tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[1]->label);
      t->result_node = node;
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r2, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, i, NULL);

      /* set promotions */
      type = tree_node_get_max_var_type(node->children[0]);
      tac_promote_argument(t, type, TAC_USE_RESULT);
      type = tree_node_get_max_var_type(node->children[2 + i]);
      tac_promote_argument(t, type, TAC_USE_ARG1);
      if (i < 256)
        tac_promote_argument(t, VARIABLE_TYPE_UINT8, TAC_USE_ARG2);
      else
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);
    }
  }
  
  return SUCCEEDED;
}


static int _generate_il_create_assignment(struct tree_node *node) {

  struct tac *t;
  int r1;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  r1 = g_temp_r;

  if (_generate_il_create_expression(node->children[1]) == FAILED)
    return FAILED;

  if (node->added_children == 3) {
    int r2 = g_temp_r, type;

    if (_generate_il_create_expression(node->children[2]) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;

    /* r1 is the array index, r2 is the value */

    t->op = TAC_OP_ARRAY_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[0]->label);
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r2, NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_TEMP, r1, NULL);

    /* set promotions */
    type = tree_node_get_max_var_type(node->children[1]);
    tac_promote_argument(t, type, TAC_USE_ARG2);
    type = tree_node_get_max_var_type(node->children[2]);
    tac_promote_argument(t, type, TAC_USE_ARG1);
    type = tree_node_get_max_var_type(node->children[0]);
    tac_promote_argument(t, type, TAC_USE_RESULT);
  }
  else {
    int type;
    
    t = add_tac();
    if (t == NULL)
      return FAILED;

    /* r1 is the result */
    
    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[0]->label);
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);

    /* set promotions */
    type = tree_node_get_max_var_type(node->children[1]);
    tac_promote_argument(t, type, TAC_USE_ARG1);
    type = tree_node_get_max_var_type(node->children[0]);
    tac_promote_argument(t, type, TAC_USE_RESULT);
  }

  /* find the definition */
  if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_RESULT) == FAILED)
    return FAILED;

  return SUCCEEDED;
}


struct tac *generate_il_create_get_address_array(struct tree_node *node) {

  struct symbol_table_item *sti;
  struct tac *t;
  int r, type;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  r = g_temp_r;
  if (_generate_il_create_expression(node->children[0]) == FAILED)
    return NULL;

  t = add_tac();
  if (t == NULL)
    return NULL;

  t->op = TAC_OP_GET_ADDRESS_ARRAY;
  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);
  tac_set_arg2(t, TAC_ARG_TYPE_TEMP, r, NULL);

  /* find the function */
  sti = symbol_table_find_symbol(node->label);
  if (sti != NULL)
    t->arg1_node = sti->node;
  else {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_get_address_array(): Cannot find variable \"%s\"! Please submit a bug report!\n", node->label);
    print_error(g_error_message, ERROR_ERR);
    return NULL;
  }

  /* set promotions */
  tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
  type = tree_node_get_max_var_type(node->children[0]);
  tac_promote_argument(t, type, TAC_USE_ARG2);
  
  return t;
}


struct tac *generate_il_create_function_call(struct tree_node *node) {

  struct symbol_table_item *sti;
  int *registers = NULL, i;
  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->added_children - 1 > 0) {
    registers = (int *)calloc(sizeof(int) * node->added_children - 1, 1);
    if (registers == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_function_call(): Out of memory error.\n");
      print_error(g_error_message, ERROR_ERR);
      return NULL;
    }
  }

  for (i = 1; i < node->added_children; i++) {
    registers[i-1] = g_temp_r;
    if (_generate_il_create_expression(node->children[i]) == FAILED)
      return NULL;
  }

  t = add_tac();
  if (t == NULL)
    return NULL;

  t->op = TAC_OP_FUNCTION_CALL;
  t->registers = registers;
  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->children[0]->label);
  tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, node->added_children - 1, NULL);

  /* find the function */
  sti = symbol_table_find_symbol(node->children[0]->label);
  if (sti != NULL)
    t->arg1_node = sti->node;
  else {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_function_call(): Cannot find function \"%s\"! Please submit a bug report!\n", node->label);
    print_error(g_error_message, ERROR_ERR);
    return NULL;
  }

  return t;
}


static int _generate_il_create_return(struct tree_node *node) {

  struct tac *t;
  int r1;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->added_children == 0) {
    /* return */

    if (g_current_function->children[0]->value != VARIABLE_TYPE_VOID) {
      snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_return(): The function \"%s\" needs to return a value.\n", g_current_function->children[1]->label);
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }
    
    t = add_tac();
    if (t == NULL)
      return FAILED;
  
    t->op = TAC_OP_RETURN;
  }
  else {
    /* return {expression} */
    int type;

    if (g_current_function->children[0]->value == VARIABLE_TYPE_VOID) {
      snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_return(): The function \"%s\" doesn't return anything.\n", g_current_function->children[1]->label);
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    /* create expression */

    r1 = g_temp_r;

    if (_generate_il_create_expression(node->children[0]) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;
  
    t->op = TAC_OP_RETURN_VALUE;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);    

    /* set promotions */
    if (g_current_function->children[0]->value_double > 0.0)
      type = VARIABLE_TYPE_UINT16;
    else
      type = g_current_function->children[0]->value;
    tac_promote_argument(t, type, TAC_USE_ARG1);
  }

  return SUCCEEDED;
}


static int _generate_il_create_condition(struct tree_node *node, int false_label_id) {

  struct tac *t;
  int r1, type;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->type != TREE_NODE_TYPE_CONDITION) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_condition(): Was expecting TREE_NODE_TYPE_CONDITION, but got %d instead.\n", node->type);
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  r1 = g_temp_r;

  if (_generate_il_create_expression(node->children[0]) == FAILED)
    return FAILED;

  /* compare with 0 */
  t = add_tac();
  if (t == NULL)
    return FAILED;
  
  t->op = TAC_OP_JUMP_EQ;
  tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
  tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0, NULL);
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(false_label_id));

  /* set promotions */
  type = tree_node_get_max_var_type(node->children[0]);
  tac_promote_argument(t, type, TAC_USE_ARG1);
  tac_promote_argument(t, type, TAC_USE_ARG2);
  
  return SUCCEEDED;
}


static int _generate_il_create_switch(struct tree_node *node) {

  int label_exit, i, j = 0, blocks = 0, *labels, r1, r2, got_default = NO;
  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  label_exit = ++g_temp_label_id;

  blocks = (node->added_children) >> 1;

  labels = (int *)calloc(sizeof(int) * node->added_children, 1);
  if (labels == NULL) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_switch(): Out of memory error.\n");
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }
  
  /* reserve labels */
  for (i = 0; i < blocks + 1; i++)
    labels[i] = ++g_temp_label_id;

  /* create (switch) expression */

  r1 = g_temp_r;

  if (_generate_il_create_expression(node->children[0]) == FAILED)
    return FAILED;
  
  /* create tests */

  for (i = 1; i < node->added_children; i += 2) {
    if (i <= node->added_children - 2) {
      /* case */

      /* create (case) expression */

      r2 = g_temp_r;

      if (_generate_il_create_expression(node->children[i]) == FAILED)
        return FAILED;

      /* compare with r1 */
      t = add_tac();
      if (t == NULL) {
        free(labels);
        return FAILED;
      }
  
      t->op = TAC_OP_JUMP_EQ;
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, r2, NULL);
      tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(labels[j]));
      j++;
    }
    else {
      /* default */
      got_default = YES;

      /* jump to default */
      add_tac_jump(generate_temp_label(labels[j]));
    }
  }

  if (got_default == NO) {
    /* jump to exit */
    add_tac_jump(generate_temp_label(label_exit));
  }

  /* create blocks */
  j = 0;

  /* continue inside switch takes you to the exit */
  _enter_breakable(label_exit, label_exit);
  
  for (i = 1; i < node->added_children; i += 2) {
    if (i <= node->added_children - 2) {
      /* case */

      /* label */
      add_tac_label(generate_temp_label(labels[j]));
      j++;

      /* block */
      _generate_il_create_block(node->children[i+1]);
    }
    else {
      /* default */

      /* label */
      add_tac_label(generate_temp_label(labels[j]));

      /* block */
      _generate_il_create_block(node->children[i]);
    }
  }
  
  _exit_breakable();
  
  /* label of exit */
  add_tac_label(generate_temp_label(label_exit));

  free(labels);

  return SUCCEEDED;
}


static int _generate_il_create_if(struct tree_node *node) {

  int label_exit, i;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  label_exit = ++g_temp_label_id;

  for (i = 0; i < node->added_children; i += 2) {
    int label_next_condition = ++g_temp_label_id;
    
    if (i <= node->added_children - 2) {
      /* if/elseif */

      /* condition */
      if (_generate_il_create_condition(node->children[i], label_next_condition) == FAILED)
        return FAILED;

      /* main block */
      _generate_il_create_block(node->children[i+1]);

      /* jump to exit */
      add_tac_jump(generate_temp_label(label_exit));
  
      /* label of next condition */
      add_tac_label(generate_temp_label(label_next_condition));
    }
    else {
      /* else */
      
      /* main block */
      _generate_il_create_block(node->children[i]);
    }
  }

  /* label of exit */
  add_tac_label(generate_temp_label(label_exit));

  return SUCCEEDED;
}


static int _generate_il_create_while(struct tree_node *node) {

  int label_condition, label_exit;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  label_condition = ++g_temp_label_id;
  label_exit = ++g_temp_label_id;
  
  /* label of condition */
  add_tac_label(generate_temp_label(label_condition));

  /* reset the temp register counter */
  g_temp_r = 0;
  
  /* condition */
  if (_generate_il_create_condition(node->children[0], label_exit) == FAILED)
    return FAILED;

  _enter_breakable(label_exit, label_condition);
  
  /* main block */
  _generate_il_create_block(node->children[1]);

  _exit_breakable();
  
  /* jump back to condition */
  add_tac_jump(generate_temp_label(label_condition));
  
  /* label of exit */
  add_tac_label(generate_temp_label(label_exit));

  return SUCCEEDED;
}


static int _generate_il_create_for(struct tree_node *node) {

  int label_condition, label_increments, label_exit;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  label_condition = ++g_temp_label_id;
  label_increments = ++g_temp_label_id;
  label_exit = ++g_temp_label_id;
  
  /* initialization of for() */
  _generate_il_create_block(node->children[0]);

  /* label of condition */
  add_tac_label(generate_temp_label(label_condition));

  /* reset the temp register counter */
  g_temp_r = 0;
  
  /* condition */
  if (_generate_il_create_condition(node->children[1], label_exit) == FAILED)
    return FAILED;

  _enter_breakable(label_exit, label_increments);
  
  /* main block */
  _generate_il_create_block(node->children[3]);

  _exit_breakable();
  
  /* increments of for() */
  add_tac_label(generate_temp_label(label_increments));
  _generate_il_create_block(node->children[2]);
  
  /* jump back to condition */
  add_tac_jump(generate_temp_label(label_condition));
  
  /* label of exit */
  add_tac_label(generate_temp_label(label_exit));

  return SUCCEEDED;
}


static int _generate_il_create_increment_decrement(struct tree_node *node) {

  struct tac *t;
  int type;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  t = add_tac();
  if (t == NULL)
    return FAILED;

  if ((int)node->value == SYMBOL_INCREMENT)
    t->op = TAC_OP_ADD;
  else
    t->op = TAC_OP_SUB;

  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);
  tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 1, NULL);
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->label);
  
  /* find the definition */
  tac_try_find_definition(t, node->label, NULL, TAC_USE_ARG1);
  tac_try_find_definition(t, node->label, NULL, TAC_USE_RESULT);

  /* set promotions */
  type = tree_node_get_max_var_type(t->result_node);
  tac_promote_argument(t, type, TAC_USE_ARG1);
  tac_promote_argument(t, type, TAC_USE_ARG2);
  tac_promote_argument(t, type, TAC_USE_RESULT);
  
  return SUCCEEDED;
}


static int _generate_il_create_break(struct tree_node *node) {

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (g_breakable_stack_items_level < 0) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_break(): break does nothing here, nothing to break from.\n");
    print_error(g_error_message, ERROR_ERR);

    return FAILED;
  }
  
  /* jump out */
  add_tac_jump(generate_temp_label(g_breakable_stack_items[g_breakable_stack_items_level].label_break));

  return SUCCEEDED;
}


static int _generate_il_create_continue(struct tree_node *node) {

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (g_breakable_stack_items_level < 0) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_continue(): continue does nothing here, nothing to continue.\n");
    print_error(g_error_message, ERROR_ERR);

    return FAILED;
  }

  /* jump to the next iteration */
  add_tac_jump(generate_temp_label(g_breakable_stack_items[g_breakable_stack_items_level].label_continue));

  return SUCCEEDED;
}


static int _generate_il_create_label(struct tree_node *node) {

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (add_tac_label(node->label) == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _generate_il_create_goto(struct tree_node *node) {

  struct tac *t;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  t = add_tac_jump(node->label);
  if (t == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _generate_il_create_statement(struct tree_node *node) {

  int r = SUCCEEDED;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  /* add_tac() uses g_current_statement to make the TAC to remember the statement it was created from */
  g_current_statement = node;
  
  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    r = _generate_il_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_ASSIGNMENT)
    r = _generate_il_create_assignment(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL) {
    if (generate_il_create_function_call(node) == NULL)
      r = FAILED;
    else
      r = SUCCEEDED;
  }
  else if (node->type == TREE_NODE_TYPE_RETURN)
    r = _generate_il_create_return(node);
  else if (node->type == TREE_NODE_TYPE_IF)
    r = _generate_il_create_if(node);
  else if (node->type == TREE_NODE_TYPE_SWITCH)
    r = _generate_il_create_switch(node);
  else if (node->type == TREE_NODE_TYPE_WHILE)
    r = _generate_il_create_while(node);
  else if (node->type == TREE_NODE_TYPE_FOR)
    r = _generate_il_create_for(node);
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT)
    r = _generate_il_create_increment_decrement(node);
  else if (node->type == TREE_NODE_TYPE_BREAK)
    r = _generate_il_create_break(node);
  else if (node->type == TREE_NODE_TYPE_CONTINUE)
    r = _generate_il_create_continue(node);
  else if (node->type == TREE_NODE_TYPE_LABEL)
    r = _generate_il_create_label(node);
  else if (node->type == TREE_NODE_TYPE_GOTO)
    r = _generate_il_create_goto(node);
  else {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_statement(): Unsupported node type %d! Please send a bug report!\n", node->type);
    print_error(g_error_message, ERROR_ERR);

    r = FAILED;
  }

  return r;
}


static int _generate_il_create_block(struct tree_node *node) {

  int i;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  g_block_level++;
  
  for (i = 0; i < node->added_children; i++) {
    if (_generate_il_create_statement(node->children[i]) == FAILED)
      return FAILED;
  }

  free_symbol_table_items(g_block_level);
  
  g_block_level--;

  return SUCCEEDED;
}


static int _generate_il_create_function(struct tree_node *node) {

  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  g_current_function = node;
  
  t = add_tac_label(node->children[1]->label);
  if (t == NULL)
    return FAILED;
  t->function_node = node;
  t->is_function = YES;

  /* reset the temp register counter */
  g_temp_r = 0;
  
  if (_generate_il_create_block(node->children[node->added_children-1]) == FAILED)
    return FAILED;

  if (is_last_tac(TAC_OP_RETURN) || is_last_tac(TAC_OP_RETURN_VALUE))
    return SUCCEEDED;

  /* HACK: the block has eneded without a return, so we'll add one here */

  if (node->children[0]->value != VARIABLE_TYPE_VOID) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_function(): The function \"%s\" doesn't end to a return, but its return type is not void!\n", node->children[1]->label);
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }
  
  t = add_tac();
  if (t == NULL)
    return FAILED;
  
  t->op = TAC_OP_RETURN;

  return SUCCEEDED;
}


int generate_il(void) {

  int i;

  /* create global symbols */
  g_block_level = 0;
  
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
      fprintf(stderr, "ADDED SYMBOL %s\n", node->children[1]->label);
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level) == FAILED)
        return FAILED;
    }
  }

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)) {
      fprintf(stderr, "ADDED SYMBOL %s\n", node->children[1]->label);
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level) == FAILED)
        return FAILED;
    }
  }

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION) {
      fprintf(stderr, "ADDED SYMBOL %s\n", node->children[1]->label);
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level) == FAILED)
        return FAILED;
    }
  }

  /*
  print_symbol_table_items();
  */
  
  /* create il code for functions */

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION) {
      if (_generate_il_create_function(node) == FAILED)
        return FAILED;
    }
  }
  
  return SUCCEEDED;
}