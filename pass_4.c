
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
extern int g_verbose_mode, g_input_float_mode;
extern char *g_variable_types[4], *g_two_char_symbols[10], g_label[MAX_NAME_LENGTH + 1];
extern double g_parsed_double;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max;

static int g_block_level = 0;

int g_temp_r = 0, g_temp_label_id = 0;

char g_temp_label[32];


static void _generate_il_create_block(struct tree_node *node);
static void _generate_il_create_increment_decrement(struct tree_node *node);


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
  else
    fprintf(stderr, "_generate_il_create_expression(): Unsupported tree node %d!\n", node->type);
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
}
static void _generate_il_create_if(struct tree_node *node) {

  /* reset the temp register counter */
  g_temp_r = 0;
}
static void _generate_il_create_while(struct tree_node *node) {

  /* reset the temp register counter */
  g_temp_r = 0;
}


static void _generate_il_create_condition(struct tree_node *node, int false_label_id) {

  struct tac *t;
  int r1;

  if (node->type != TREE_NODE_TYPE_CONDITION) {
    fprintf(stderr, "_generate_il_create_condition(): Was expecting TREE_NODE_TYPE_CONDITION, but got %d instead.\n", node->type);
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


static void _generate_il_create_for(struct tree_node *node) {

  int label_condition, label_exit;
  
  /* initialization of for() */
  _generate_il_create_block(node->children[0]);

  /* label of condition */
  label_condition = ++g_temp_label_id;
  add_tac_label(_generate_temp_label(label_condition));
  label_exit = ++g_temp_label_id;

  /* reset the temp register counter */
  g_temp_r = 0;
  
  /* condition */
  _generate_il_create_condition(node->children[1], label_exit);

  /* main block */
  _generate_il_create_block(node->children[3]);

  /* increments of for() */
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
  else if (node->type == TREE_NODE_TYPE_WHILE)
    _generate_il_create_while(node);
  else if (node->type == TREE_NODE_TYPE_FOR)
    _generate_il_create_for(node);
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT)
    _generate_il_create_increment_decrement(node);
  else
    fprintf(stderr, "_generate_il_create_statement(): Unsupported node type %d! Please send a bug report!\n", node->type);
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
