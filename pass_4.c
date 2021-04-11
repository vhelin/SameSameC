
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

int g_temp_r = 0, g_temp_label_id = 0;

char g_temp_label[32];


static void _generate_il_create_block(struct tree_node *node);
static void _generate_il_create_increment_decrement(struct tree_node *node);


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


static char *_generate_temp_label(int id) {

  snprintf(g_temp_label, sizeof(g_temp_label), "label_%d", id);

  return g_temp_label;
}


int pass_4(void) {

  if (g_verbose_mode == ON)
    printf("Pass 4...\n");

  if (generate_il() == FAILED)
    return FAILED;

  print_tacs();
  
  return SUCCEEDED;
}


static void _generate_il_create_variable(struct tree_node *node) {

  struct tac *t;
  
  symbol_table_add_symbol(node, node->children[1]->label, g_block_level);

  t = add_tac();
  if (t == NULL)
    return;

  t->op = TAC_OP_CREATE_VARIABLE;
  t->node = node;
}


static void _generate_il_create_expression(struct tree_node *node) {

  struct tac *t;
  
  if (node->type == TREE_NODE_TYPE_EXPRESSION)
    il_stack_calculate_expression(node);
  else if (node->type == TREE_NODE_TYPE_CONDITION)
    il_stack_calculate_expression(node->children[0]);
  else if (node->type == TREE_NODE_TYPE_VALUE_STRING) {
    t = add_tac();
    if (t == NULL)
      return;

    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_INT) {
    t = add_tac();
    if (t == NULL)
      return;

    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, node->value, NULL);
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_DOUBLE) {
    t = add_tac();
    if (t == NULL)
      return;

    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, (int)node->value_double, NULL);
  }
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL) {
    fprintf(stderr, "_generate_il_create_expression(): IMPLEMENT ME!\n");
  }
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM) {
    int rindex = g_temp_r;

    _generate_il_create_expression(node->children[0]);

    t = add_tac();
    if (t == NULL)
      return;

    t->op = TAC_OP_ARRAY_READ;

    tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);
    tac_set_arg2(t, TAC_ARG_TYPE_TEMP, rindex, NULL);
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    int rresult;
      
    if (node->value_double > 0.0) {
      /* post */

      /* get the value before increment/decrement */
      t = add_tac();
      if (t == NULL)
        return;

      rresult = g_temp_r++;
      
      t->op = TAC_OP_ASSIGNMENT;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, rresult, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);

      /* increment/decrement */
      _generate_il_create_increment_decrement(node);

      /* have the result in the latest register */
      t = add_tac();
      if (t == NULL)
        return;

      t->op = TAC_OP_ASSIGNMENT;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, rresult, 0);
    }
    else {
      /* pre */

      /* increment/decrement */
      _generate_il_create_increment_decrement(node);

      /* have the result in the latest register */
      t = add_tac();
      if (t == NULL)
        return;

      t->op = TAC_OP_ASSIGNMENT;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);
    }
  }
  else {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_expression(): Unsupported tree node %d!\n", node->type);
    print_error(g_error_message, ERROR_ERR);
  }
}


static void _generate_il_create_assignment(struct tree_node *node) {

  struct tac *t;
  int r1;

  r1 = g_temp_r;

  _generate_il_create_expression(node->children[1]);

  if (node->added_children == 3) {
    int r2 = g_temp_r;

    _generate_il_create_expression(node->children[2]);

    t = add_tac();
    if (t == NULL)
      return;

    /* r1 is the array index, r2 is the result */

    t->op = TAC_OP_ARRAY_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[0]->label);
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r2, NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_TEMP, r1, NULL);
  }
  else {
    t = add_tac();
    if (t == NULL)
      return;

    /* r1 is the result */
    
    t->op = TAC_OP_ASSIGNMENT;

    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->children[0]->label);
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
  }
}


static void _generate_il_create_function_call(struct tree_node *node) {
}


static void _generate_il_create_return(struct tree_node *node) {

  struct tac *t;
  int r1;
  
  if (node->added_children == 0) {
    /* return */

    t = add_tac();
    if (t == NULL)
      return;
  
    t->op = TAC_OP_RETURN;
  }
  else {
    /* return {expression} */

    /* create expression */

    r1 = g_temp_r;

    _generate_il_create_expression(node->children[0]);

    t = add_tac();
    if (t == NULL)
      return;
  
    t->op = TAC_OP_RETURN_VALUE;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
  }
}


static void _generate_il_create_condition(struct tree_node *node, int false_label_id) {

  struct tac *t;
  int r1;

  if (node->type != TREE_NODE_TYPE_CONDITION) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_condition(): Was expecting TREE_NODE_TYPE_CONDITION, but got %d instead.\n", node->type);
    print_error(g_error_message, ERROR_ERR);
    return;
  }

  r1 = g_temp_r;

  _generate_il_create_expression(node->children[0]);

  /* compare with 0 */
  t = add_tac();
  if (t == NULL)
    return;

  t->op = TAC_OP_ASSIGNMENT;
  tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
  tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, 0, NULL);

  t = add_tac();
  if (t == NULL)
    return;
  
  t->op = TAC_OP_JUMP_EQ;
  tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
  tac_set_arg2(t, TAC_ARG_TYPE_TEMP, g_temp_r - 1, NULL);
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, _generate_temp_label(false_label_id));
}


static void _generate_il_create_switch(struct tree_node *node) {

  int label_exit, i, j = 0, blocks = 0, *labels, r1, r2, got_default = NO;
  struct tac *t;

  label_exit = ++g_temp_label_id;

  blocks = (node->added_children) >> 1;

  labels = (int *)calloc(sizeof(int) * node->added_children, 1);
  if (labels == NULL) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_switch(): Out of memory error.\n");
    print_error(g_error_message, ERROR_ERR);
    return;
  }
  
  /* reserve labels */
  for (i = 0; i < blocks + 1; i++)
    labels[i] = ++g_temp_label_id;

  /* create (switch) expression */

  r1 = g_temp_r;

  _generate_il_create_expression(node->children[0]);
  
  /* create tests */

  for (i = 1; i < node->added_children; i += 2) {
    if (i <= node->added_children - 2) {
      /* case */

      /* create (case) expression */

      r2 = g_temp_r;

      _generate_il_create_expression(node->children[i]);

      /* compare with r1 */
      t = add_tac();
      if (t == NULL) {
        free(labels);
        return;
      }
  
      t->op = TAC_OP_JUMP_EQ;
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, r2, NULL);
      tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, _generate_temp_label(labels[j]));
      j++;
    }
    else {
      /* default */
      got_default = YES;

      /* jump to default */
      add_tac_jump(_generate_temp_label(labels[j]));
    }
  }

  if (got_default == NO) {
    /* jump to exit */
    add_tac_jump(_generate_temp_label(label_exit));
  }

  /* create blocks */
  j = 0;

  /* continue inside switch takes you to the exit */
  _enter_breakable(label_exit, label_exit);
  
  for (i = 1; i < node->added_children; i += 2) {
    if (i <= node->added_children - 2) {
      /* case */

      /* label */
      add_tac_label(_generate_temp_label(labels[j]));
      j++;

      /* block */
      _generate_il_create_block(node->children[i+1]);
    }
    else {
      /* default */

      /* label */
      add_tac_label(_generate_temp_label(labels[j]));

      /* block */
      _generate_il_create_block(node->children[i]);
    }
  }
  
  _exit_breakable();
  
  /* label of exit */
  add_tac_label(_generate_temp_label(label_exit));

  free(labels);
}


static void _generate_il_create_if(struct tree_node *node) {

  int label_exit, i;

  label_exit = ++g_temp_label_id;

  for (i = 0; i < node->added_children; i += 2) {
    int label_next_condition = ++g_temp_label_id;
    
    if (i <= node->added_children - 2) {
      /* if/elseif */

      /* condition */
      _generate_il_create_condition(node->children[i], label_next_condition);

      /* main block */
      _generate_il_create_block(node->children[i+1]);

      /* jump to exit */
      add_tac_jump(_generate_temp_label(label_exit));
  
      /* label of next condition */
      add_tac_label(_generate_temp_label(label_next_condition));
    }
    else {
      /* else */
      
      /* main block */
      _generate_il_create_block(node->children[i]);
    }
  }

  /* label of exit */
  add_tac_label(_generate_temp_label(label_exit));
}


static void _generate_il_create_while(struct tree_node *node) {

  int label_condition, label_exit;
  
  label_condition = ++g_temp_label_id;
  label_exit = ++g_temp_label_id;
  
  /* label of condition */
  add_tac_label(_generate_temp_label(label_condition));

  /* reset the temp register counter */
  g_temp_r = 0;
  
  /* condition */
  _generate_il_create_condition(node->children[0], label_exit);

  _enter_breakable(label_exit, label_condition);
  
  /* main block */
  _generate_il_create_block(node->children[1]);

  _exit_breakable();
  
  /* jump back to condition */
  add_tac_jump(_generate_temp_label(label_condition));
  
  /* label of exit */
  add_tac_label(_generate_temp_label(label_exit));
}


static void _generate_il_create_for(struct tree_node *node) {

  int label_condition, label_increments, label_exit;
  
  label_condition = ++g_temp_label_id;
  label_increments = ++g_temp_label_id;
  label_exit = ++g_temp_label_id;
  
  /* initialization of for() */
  _generate_il_create_block(node->children[0]);

  /* label of condition */
  add_tac_label(_generate_temp_label(label_condition));

  /* reset the temp register counter */
  g_temp_r = 0;
  
  /* condition */
  _generate_il_create_condition(node->children[1], label_exit);

  _enter_breakable(label_exit, label_increments);
  
  /* main block */
  _generate_il_create_block(node->children[3]);

  _exit_breakable();
  
  /* increments of for() */
  add_tac_label(_generate_temp_label(label_increments));
  _generate_il_create_block(node->children[2]);
  
  /* jump back to condition */
  add_tac_jump(_generate_temp_label(label_condition));
  
  /* label of exit */
  add_tac_label(_generate_temp_label(label_exit));
}


static void _generate_il_create_increment_decrement(struct tree_node *node) {

  struct tac *t = add_tac();

  if (t == NULL)
    return;

  t->op = TAC_OP_ASSIGNMENT;
  tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, node->label);

  t = add_tac();
  if (t == NULL)
    return;

  t->op = TAC_OP_ASSIGNMENT;
  tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
  tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, 1, NULL);

  t = add_tac();
  if (t == NULL)
    return;

  if ((int)node->value == SYMBOL_INCREMENT)
    t->op = TAC_OP_ADD;
  else
    t->op = TAC_OP_SUB;

  tac_set_arg1(t, TAC_ARG_TYPE_TEMP, g_temp_r - 2, NULL);
  tac_set_arg2(t, TAC_ARG_TYPE_TEMP, g_temp_r - 1, NULL);
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, node->label);
}


static void _generate_il_create_break(struct tree_node *node) {

  if (g_breakable_stack_items_level < 0) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_break(): break does nothing here, nothing to break from.\n");
    print_error(g_error_message, ERROR_ERR);
  }
  else {
    /* jump out */
    add_tac_jump(_generate_temp_label(g_breakable_stack_items[g_breakable_stack_items_level].label_break));
  }
}


static void _generate_il_create_continue(struct tree_node *node) {

  if (g_breakable_stack_items_level < 0) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_break(): continue does nothing here, nothing to continue.\n");
    print_error(g_error_message, ERROR_ERR);
  }
  else {
    /* jump to the next iteration */
    add_tac_jump(_generate_temp_label(g_breakable_stack_items[g_breakable_stack_items_level].label_continue));
  }
}


static void _generate_il_create_statement(struct tree_node *node) {

  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE)
    _generate_il_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_ASSIGNMENT)
    _generate_il_create_assignment(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL)
    _generate_il_create_function_call(node);
  else if (node->type == TREE_NODE_TYPE_RETURN)
    _generate_il_create_return(node);
  else if (node->type == TREE_NODE_TYPE_IF)
    _generate_il_create_if(node);
  else if (node->type == TREE_NODE_TYPE_SWITCH)
    _generate_il_create_switch(node);
  else if (node->type == TREE_NODE_TYPE_WHILE)
    _generate_il_create_while(node);
  else if (node->type == TREE_NODE_TYPE_FOR)
    _generate_il_create_for(node);
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT)
    _generate_il_create_increment_decrement(node);
  else if (node->type == TREE_NODE_TYPE_BREAK)
    _generate_il_create_break(node);
  else if (node->type == TREE_NODE_TYPE_CONTINUE)
    _generate_il_create_continue(node);
  else {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_statement(): Unsupported node type %d! Please send a bug report!\n", node->type);
    print_error(g_error_message, ERROR_ERR);
  }
}


static void _generate_il_create_block(struct tree_node *node) {

  int i;
  
  g_block_level++;
  
  for (i = 0; i < node->added_children; i++)
    _generate_il_create_statement(node->children[i]);

  free_symbol_table_items(g_block_level);
  
  g_block_level--;
}


static void _generate_il_create_function(struct tree_node *node) {

  if (add_tac_label(node->children[1]->label) == NULL)
    return;  

  /* reset the temp register counter */
  g_temp_r = 0;
  
  _generate_il_create_block(node->children[node->added_children-1]);
}


int generate_il(void) {

  int i;

  /* create global symbols */
  g_block_level = 0;
  
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level) == FAILED)
        return FAILED;
    }
  }

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level) == FAILED)
        return FAILED;
    }
  }

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION) {
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
    if (node != NULL && node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION)
      _generate_il_create_function(node);
  }
  
  return SUCCEEDED;
}
