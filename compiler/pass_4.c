
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
#include "stack.h"
#include "include_file.h"
#include "pass_4.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"
#include "inline_asm.h"
#include "inline_asm_z80.h"
#include "struct_item.h"


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char *g_variable_types[9], *g_two_char_symbols[17], g_label[MAX_NAME_LENGTH + 1];
extern double g_parsed_double;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max, g_backend;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

static struct breakable_stack_item g_breakable_stack_items[256];
static int g_block_level = 0, g_breakable_stack_items_level = -1;
static struct tree_node *g_current_function = NULL;

int g_temp_r = 0, g_temp_label_id = 0;

/* add_tac() uses this variable to set the statement field */
struct tree_node *g_current_statement = NULL;

static int _generate_il_create_block(struct tree_node *node);
static int _generate_il_create_increment_decrement(struct tree_node *node);
static int _generate_il_create_expression(struct tree_node *node);


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


static int _generate_il_calculate_struct_access_address_add_index(struct tree_node *node, int i, int *cregister, int item_size, struct struct_item *si) {

  struct tac *t;
  
  if (node->children[i]->type == TREE_NODE_TYPE_VALUE_INT) {
    t = add_tac();
    if (t == NULL)
      return FAILED;
    
    t->op = TAC_OP_ADD;
    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)(*cregister), NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (double)(item_size * node->children[i]->value), NULL);

    *cregister = g_temp_r++;

    /* set promotions */
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);
  }
  else {
    int result = g_temp_r;

    if (_generate_il_create_expression(node->children[i]) == FAILED)
      return FAILED;

    if (item_size == 1) {
      /* no need for a mul by the item_size */
      t = add_tac();
      if (t == NULL)
        return FAILED;
        
      t->op = TAC_OP_ADD;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)(*cregister), NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (double)result, NULL);
        
      /* set promotions */
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);

      *cregister = g_temp_r++;
    }
    else {
      /* mul the index by the item_size */
      t = add_tac();
      if (t == NULL)
        return FAILED;
        
      t->op = TAC_OP_MUL;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)result, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (double)item_size, NULL);

      /* set promotions */
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);

      /* add the index */
      t = add_tac();
      if (t == NULL)
        return FAILED;
        
      t->op = TAC_OP_ADD;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)(*cregister), NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
        
      /* set promotions */
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);

      *cregister = g_temp_r++;
    }
  }

  return SUCCEEDED;
}


int generate_il_calculate_struct_access_address(struct tree_node *node, int *final_type) {

  int fregister = g_temp_r++, i, cregister, skip = NO, got_array_access = NO;
  struct tree_node *named_node;
  struct struct_item *si;
  struct tac *t;

  si = find_struct_item(node->children[0]->definition->children[0]->children[0]->label);
  if (si == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): Struct \"%s\"'s definition is missing! Please submit a bug report!\n", node->children[0]->definition->children[0]->children[0]->label);
    return print_error_using_tree_node(g_error_message, ERROR_ERR, node);
  }

  named_node = node->children[0];
  
  t = add_tac();
  if (t == NULL)
    return FAILED;

  cregister = g_temp_r++;

  /* read the variable's starting address */
  if (node->children[1]->type != TREE_NODE_TYPE_SYMBOL) {
    return print_error_using_tree_node("generate_il_calculate_struct_access_address(): Pointer node is corrupted (1)! Please submit a bug report!\n", ERROR_ERR, node->children[1]);
  }

  if (node->children[1]->value == '.') {
    t->op = TAC_OP_GET_ADDRESS;
    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);

    /* set promotions */
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);

    /* find the definition */
    if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_ARG1) == FAILED)
      return FAILED;

    i = 2;
  }
  else if (node->children[1]->value == SYMBOL_POINTER) {
    t->op = TAC_OP_ASSIGNMENT;
    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);

    /* set promotions */
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
    tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);

    /* find the definition */
    if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_ARG1) == FAILED)
      return FAILED;

    i = 2;
  }
  else if (node->children[1]->value == '[') {
    int item_size;
    
    if (node->children[0]->definition->children[0]->value_double <= 0.0) {
      /* the root is an array of structs/unions */
      t->op = TAC_OP_GET_ADDRESS;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);

      /* set promotions */
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);

      /* find the definition */
      if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_ARG1) == FAILED)
        return FAILED;

      item_size = node->children[0]->struct_item->size;
    }
    else {
      /* the root is an array of pointers to structs/unions */
      t->op = TAC_OP_GET_ADDRESS;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);

      /* set promotions */
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);

      /* find the definition */
      if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_ARG1) == FAILED)
        return FAILED;

      /* pointer size is 2 */
      item_size = 2;
    }

    /* add the index */

    if (_generate_il_calculate_struct_access_address_add_index(node, 2, &cregister, item_size, si) == FAILED)
      return FAILED;

    if (!(node->children[3]->type == TREE_NODE_TYPE_SYMBOL && node->children[3]->value == ']'))
      return print_error_using_tree_node("generate_il_calculate_struct_access_address(): Expected ']', got something else.\n", ERROR_ERR, node->children[3]);
    
    skip = YES;
    i = 4;
    got_array_access = YES;
  }
  else {
    snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): Pointer node is corrupted (2)! Please submit a bug report!\n");
    return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[1]);
  }

  /* find the definition */
  tac_try_find_definition(t, node->children[0]->label, NULL, TAC_USE_ARG1);

  while (i < node->added_children) {
    if (skip == NO) {
      if (node->children[i]->type == TREE_NODE_TYPE_VALUE_STRING) {
        /* find the struct/union member */
        struct struct_item *si_old = si;

        if (si->type == STRUCT_ITEM_TYPE_STRUCT || si->type == STRUCT_ITEM_TYPE_UNION) {
        }
        else if (si->type == STRUCT_ITEM_TYPE_ITEM && (si->variable_type == VARIABLE_TYPE_STRUCT || si->variable_type == VARIABLE_TYPE_UNION)) {
          si = find_struct_item(si->struct_name);
          if (si == NULL) {
            snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): Cannot find struct/union \"%s\"!\n", si->struct_name);
            return print_error_using_tree_node(g_error_message, ERROR_ERR, named_node);
          }
        }
        else if (si->type == STRUCT_ITEM_TYPE_ARRAY && got_array_access == YES && (si->variable_type == VARIABLE_TYPE_STRUCT ||
                                                                                   si->variable_type == VARIABLE_TYPE_UNION)) {
        }
        else {
          snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): \"%s\" is not a struct/union!\n", named_node->label);
          return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
        }
        
        si = find_struct_item_child(si, node->children[i]->label);
        if (si == NULL) {
          snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): Cannot find member \"%s\" of struct/union \"%s\"!\n", node->children[i]->label, si_old->name);
          return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
        }

        named_node = node->children[i];

        t = add_tac();
        if (t == NULL)
          return FAILED;
        
        t->op = TAC_OP_ADD;
        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);
        tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (double)(si->offset), NULL);
        
        cregister = g_temp_r++;
        
        i++;

        *final_type = si->variable_type;

        /* set promotions */
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);

        got_array_access = NO;
      }
      else {
        snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): A node is corrupted (3)! Please submit a bug report!\n");
        return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
      }
    }

    skip = NO;
    
    if (i >= node->added_children)
      break;

    if (node->children[i]->type != TREE_NODE_TYPE_SYMBOL) {
      snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): A node is corrupted (4)! Please submit a bug report!\n");
      return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
    }

    /* handle '[', '.' and '->' */
    
    if (node->children[i]->value == '.') {
      /* nothing needs to be done here as in the next step we just add the struct member's offset to the pointer... */
      i++;
    }
    else if (node->children[i]->value == SYMBOL_POINTER) {
      t = add_tac();
      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_ARRAY_READ;
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0.0, NULL);      

      cregister = g_temp_r++;

      i++;

      /* set promotions */
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
      t->arg1_var_type = VARIABLE_TYPE_UINT16;
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);
    }
    else if (node->children[i]->value == '[') {
      int item_size = si->size / si->array_items;

      i++;

      if (i >= node->added_children)
        break;

      *final_type = si->variable_type;
      
      if (_generate_il_calculate_struct_access_address_add_index(node, i, &cregister, item_size, si) == FAILED)
        return FAILED;

      i++;
                                                                 
      if (i >= node->added_children)
        break;

      if (!(node->children[i]->type == TREE_NODE_TYPE_SYMBOL && node->children[i]->value == ']')) {
        snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): Was expecting ']', got something else instead.\n");
        return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
      }

      i++;
      
      skip = YES;
      got_array_access = YES;
    }
    else {
      snprintf(g_error_message, sizeof(g_error_message), "generate_il_calculate_struct_access_address(): Unhandled symbol in struct access! Please submit a bug report!\n");
      return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
    }
  }

  t = add_tac();
  if (t == NULL)
    return FAILED;

  t->op = TAC_OP_ASSIGNMENT;
  tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)fregister, NULL);
  tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)cregister, NULL);

  /* set promotions */
  tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
  tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
  
  return SUCCEEDED;
}


int make_sure_all_tacs_have_definition_nodes(void) {

  int i, ret = SUCCEEDED;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int skip_arg1 = NO, skip_arg2 = NO, skip_result = NO;
    
    if (t->op == TAC_OP_DEAD)
      continue;

    /* jumps don't have definition nodes as they were generated when IL was generated */
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

  /* to have something in the C sense 'static' in the going-to-be-generated-asm,
     we'll rename them so that they'll have unique names within the project */
  if (rename_static_variables_and_functions() == FAILED)
    return FAILED;
  
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

    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);

    /* an array? */
    if (node->definition->value > 0)
      t->op = TAC_OP_GET_ADDRESS;

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

    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, (double)(node->value), NULL);

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

    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, node->value_double, NULL);

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

    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r++, NULL);
    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);
    tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (double)rindex, NULL);

    /* find the definition */
    if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
      return FAILED;
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    int rresult;

    if (node->value_double > 0.0) {
      /* post */

      if (node->added_children > 0) {
        /* struct access */

        /* get the value before increment/decrement */
        fprintf(stderr, "IMPLEMENT ME 2\n");
        exit(0);
      }
      else {
        /* variable access */

        /* get the value before increment/decrement */
        t = add_tac();
        if (t == NULL)
          return FAILED;

        rresult = g_temp_r++;
      
        t->op = TAC_OP_ASSIGNMENT;
        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)rresult, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);

        /* find the definition */
        if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
          return FAILED;
      
        /* set promotions */
        type = tree_node_get_max_var_type(node->definition->children[0]);
        tac_promote_argument(t, type, TAC_USE_ARG1);
        tac_promote_argument(t, type, TAC_USE_RESULT);

        /* increment/decrement */
        if (_generate_il_create_increment_decrement(node) == FAILED)
          return FAILED;

        /* have the result in the latest register */
        t = add_tac();
        if (t == NULL)
          return FAILED;

        t->op = TAC_OP_ASSIGNMENT;
        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r++, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)rresult, 0);
      }
    }
    else {
      /* pre */

      if (node->added_children > 0) {
        /* struct access */

        /* increment/decrement */
        fprintf(stderr, "IMPLEMENT ME 3\n");
        exit(0);
      }
      else {
        /* variable access */
        
        /* increment/decrement */
        if (_generate_il_create_increment_decrement(node) == FAILED)
          return FAILED;

        /* have the result in the latest register */
        t = add_tac();
        if (t == NULL)
          return FAILED;

        t->op = TAC_OP_ASSIGNMENT;
        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)g_temp_r++, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);
        
        /* find the definition */
        if (tac_try_find_definition(t, node->label, node, TAC_USE_ARG1) == FAILED)
          return FAILED;
      }
    }
  }
  else {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_expression(): Unsupported tree node %d!\n", node->type);
    return print_error(g_error_message, ERROR_ERR);
  }

  return SUCCEEDED;
}


static int _generate_il_create_variable(struct tree_node *node) {

  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
    /* is it referenced? */
    if (node->reads == 0 && node->writes == 0) {
      /* no -> kill it! */
      snprintf(g_error_message, sizeof(g_error_message), "Removing unreferenced variable \"%s\".\n", node->children[1]->label);
      print_error(g_error_message, ERROR_WRN);

      node->type = TREE_NODE_TYPE_DEAD;

      return SUCCEEDED;
    }
  }
  
  /* create variable */
  
  symbol_table_add_symbol(node, node->children[1]->label, g_block_level, node->line_number, node->file_id);

  t = add_tac();
  if (t == NULL)
    return FAILED;

  t->op = TAC_OP_CREATE_VARIABLE;
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[1]->label);
  t->result_node = node;

  /* if function argument, no need to assign anything, it's already done */
  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    return SUCCEEDED;

  /* if const, no need to assign anything as the contents are written into the end of the function in ASM */
  if ((node->children[0]->value_double == 0 && (node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
      (node->children[0]->value_double > 0 && (node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)) {
    return SUCCEEDED;
  }
  
  /* make assignment(s) */

  /* no initializations? */
  if (node->added_children - 2 <= 0)
    return SUCCEEDED;
  
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
  
    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[1]->label);
    t->result_node = node;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);

    /* set promotions */
    type = tree_node_get_max_var_type(node->children[0]);
    tac_promote_argument(t, type, TAC_USE_RESULT);
    type = tree_node_get_max_var_type(node->children[2]);
    tac_promote_argument(t, type, TAC_USE_ARG1);
  }
  else {
    /* array copy */
    int i, constants = 0, skip_constants = NO;

    /* calculate how many constants there are in the array */
    for (i = 0; i < node->added_children - 2; i++) {
      if (tree_node_is_expression_just_a_constant(node->children[2 + i]) == YES) {
        if (node->children[2 + i]->type == TREE_NODE_TYPE_BYTES)
          constants += node->value;
        else if (node->children[2 + i]->type != TREE_NODE_TYPE_SYMBOL)
          constants++;
      }
    }

    /* in pass_6 we'll copy these constants when encountering TAC_OP_CREATE_VARIABLE, so the
       TACs we create now here take place later */
    if (constants > 3)
      skip_constants = YES;

    if ((node->children[0]->value == VARIABLE_TYPE_STRUCT || node->children[0]->value == VARIABLE_TYPE_UNION) && node->children[0]->value_double == 0) {
      /* array of structs/unions */
      int offset = 0;
      
      for (i = 0; i < node->added_children - 2; i++) {
        int r_address1, r_address2, r_result, type, size, pointer_depth;

        if (node->children[2 + i]->type == TREE_NODE_TYPE_SYMBOL)
          continue;

        /* calculate item size */
        pointer_depth = node->children[2+i]->struct_item->pointer_depth;
        if (pointer_depth > 0)
          type = VARIABLE_TYPE_UINT16;
        else
          type = node->children[2+i]->struct_item->variable_type;
        size = get_variable_type_size(type) / 8;

        /*
        fprintf(stderr, "%d ---> %d (%d, %d)\n", node->children[2+i]->value, (int)node->children[2+i]->struct_item, offset, size);
        */

        if (skip_constants == YES && tree_node_is_expression_just_a_constant(node->children[2 + i]) == YES) {
          offset += size;
          continue;
        }

        /* get the address of the struct item */
        t = add_tac();
        if (t == NULL)
          return FAILED;

        r_address1 = g_temp_r++;
        
        t->op = TAC_OP_GET_ADDRESS;
        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)r_address1, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[1]->label);

        /* set promotions */
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
        t->arg1_node = node;

        r_address2 = g_temp_r++;

        t = add_tac();
        if (t == NULL)
          return FAILED;
        
        t->op = TAC_OP_ADD;
        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)r_address2, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r_address1, NULL);
        tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (double)offset, NULL);

        /* set promotions */
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_RESULT);
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG1);
        tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);

        /* store the expression into struct's item */
        r_result = g_temp_r;
        
        if (_generate_il_create_expression(node->children[2 + i]) == FAILED)
          return FAILED;

        t = add_tac();
        if (t == NULL)
          return FAILED;
        
        /* arg2 is the array index, arg1 is the value */

        t->op = TAC_OP_ARRAY_WRITE;

        tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)r_address2, NULL);
        tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r_result, NULL);
        tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0.0, NULL);

        /* set promotions */
        t->result_var_type = type;
        tac_promote_argument(t, type, TAC_USE_RESULT);
        type = tree_node_get_max_var_type(node->children[2 + i]);
        tac_promote_argument(t, type, TAC_USE_ARG1);
        tac_promote_argument(t, VARIABLE_TYPE_UINT8, TAC_USE_ARG2);

        offset += size;
      }
    }
    else {
      /* array of single type values */
      for (i = 0; i < node->added_children - 2; i++) {
        int r2 = g_temp_r, type;

        if (skip_constants == YES && tree_node_is_expression_just_a_constant(node->children[2 + i]) == YES)
          continue;

        if (_generate_il_create_expression(node->children[2 + i]) == FAILED)
          return FAILED;

        t = add_tac();
        if (t == NULL)
          return FAILED;

        /* i is the array index, r2 is the value */

        t->op = TAC_OP_ARRAY_WRITE;

        tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[1]->label);
        t->result_node = node;
        tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r2, NULL);
        tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (double)i, NULL);

        /* set promotions */
        /*
          type = tree_node_get_max_var_type(node->children[0]);
        */
        type = get_array_item_variable_type(node); /* here we use the type defined by the variable */
        t->result_var_type = type;
        tac_promote_argument(t, type, TAC_USE_RESULT);
        type = tree_node_get_max_var_type(node->children[2 + i]);
        tac_promote_argument(t, type, TAC_USE_ARG1);
        if (i < 256)
          tac_promote_argument(t, VARIABLE_TYPE_UINT8, TAC_USE_ARG2);
        else
          tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);
      }
    }
  }
  
  return SUCCEEDED;
}


static int _generate_il_create_assignment(struct tree_node *node) {

  struct tac *t;
  int r1, is_array_assignment = NO;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  r1 = g_temp_r;

  if (_generate_il_create_expression(node->children[1]) == FAILED)
    return FAILED;

  if (node->added_children == 3) {
    int r2 = g_temp_r, type;

    is_array_assignment = YES;
    
    if (_generate_il_create_expression(node->children[2]) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;

    /* r1 is the array index, r2 is the value */

    t->op = TAC_OP_ARRAY_WRITE;

    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r2, NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);

    /* find the definition */
    if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_RESULT) == FAILED)
      return FAILED;

    /* set promotions */
    type = tree_node_get_max_var_type(node->children[1]);
    tac_promote_argument(t, type, TAC_USE_ARG2);
    type = tree_node_get_max_var_type(node->children[2]);
    tac_promote_argument(t, type, TAC_USE_ARG1);
    type = get_array_item_variable_type(t->result_node);
    t->result_var_type = type;
    tac_promote_argument(t, type, TAC_USE_RESULT);
  }
  else {
    int type;

    /* r1 is the result */
    
    if (node->children[0]->type == TREE_NODE_TYPE_STRUCT_ACCESS) {
      /* struct access */
      int raddress = g_temp_r, final_type;

      if (generate_il_calculate_struct_access_address(node->children[0], &final_type) == FAILED)
        return FAILED;

      t = add_tac();
      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_ARRAY_WRITE;
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0.0, NULL);
      tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)raddress, NULL);

      /* set the promotions */
      tac_promote_argument(t, final_type, TAC_USE_ARG1);
      tac_promote_argument(t, VARIABLE_TYPE_UINT16, TAC_USE_ARG2);
      t->result_var_type = final_type;
      tac_promote_argument(t, final_type, TAC_USE_RESULT);
    }
    else {
      t = add_tac();
      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_ASSIGNMENT;

      tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);

      /* find the definition */
      if (tac_try_find_definition(t, node->children[0]->label, node, TAC_USE_RESULT) == FAILED)
        return FAILED;

      /* set promotions */
      type = tree_node_get_max_var_type(node->children[1]);
      tac_promote_argument(t, type, TAC_USE_ARG1);
      type = tree_node_get_max_var_type(node->children[0]);
      tac_promote_argument(t, type, TAC_USE_RESULT);
    }
  }

  if ((is_array_assignment == NO && node->children[0]->definition->children[0]->value_double == 0 && (node->children[0]->definition->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
      (is_array_assignment == NO && node->children[0]->definition->children[0]->value_double > 0 && (node->children[0]->definition->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2) ||
      (is_array_assignment == YES && (node->children[0]->definition->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)) {    
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_assignment(): Variable \"%s\" was declared \"const\". Cannot modify.\n", node->children[0]->label);
    return print_error(g_error_message, ERROR_ERR);
  }
  
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
  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);
  tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (double)r, NULL);

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
  struct function_argument *arguments = NULL;
  struct tac *t;
  int i;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->added_children - 1 > 0) {
    arguments = (struct function_argument *)calloc(sizeof(struct function_argument) * node->added_children - 1, 1);
    if (arguments == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_function_call(): Out of memory error.\n");
      print_error(g_error_message, ERROR_ERR);
      return NULL;
    }
  }

  for (i = 1; i < node->added_children; i++) {
    arguments[i-1].type = TAC_ARG_TYPE_TEMP;
    arguments[i-1].value = g_temp_r;
    arguments[i-1].label = NULL;
    arguments[i-1].var_type = VARIABLE_TYPE_NONE;
    arguments[i-1].var_type_promoted = VARIABLE_TYPE_NONE;
    arguments[i-1].node = NULL;
    
    if (_generate_il_create_expression(node->children[i]) == FAILED) {
      free(arguments);
      return NULL;
    }
  }

  t = add_tac();
  if (t == NULL) {
    free(arguments);
    return NULL;
  }

  t->op = TAC_OP_FUNCTION_CALL;
  t->arguments = arguments;
  t->arguments_count = node->added_children - 1;
  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->children[0]->label);
  tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (double)(node->added_children - 1), NULL);

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
      return print_error(g_error_message, ERROR_ERR);
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
      return print_error(g_error_message, ERROR_ERR);
    }

    /* create expression */

    r1 = g_temp_r;

    if (_generate_il_create_expression(node->children[0]) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;
  
    t->op = TAC_OP_RETURN_VALUE;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);    

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
    return print_error(g_error_message, ERROR_ERR);
  }

  r1 = g_temp_r;

  if (_generate_il_create_expression(node->children[0]) == FAILED)
    return FAILED;

  /* compare with 0 */
  t = add_tac();
  if (t == NULL)
    return FAILED;
  
  t->op = TAC_OP_JUMP_EQ;
  tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);
  tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0.0, NULL);
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, generate_temp_label(false_label_id));

  /* set promotions */
  type = tree_node_get_max_var_type(node->children[0]);
  tac_promote_argument(t, type, TAC_USE_ARG1);
  tac_promote_argument(t, type, TAC_USE_ARG2);
  
  return SUCCEEDED;
}


static int _generate_il_create_switch(struct tree_node *node) {

  int label_exit, i, j = 0, blocks, *labels, r1, r2, got_default = NO;
  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  label_exit = ++g_temp_label_id;

  blocks = (node->added_children) >> 1;

  labels = (int *)calloc(sizeof(int) * node->added_children, 1);
  if (labels == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_switch(): Out of memory error.\n");
    return print_error_using_tree_node(g_error_message, ERROR_ERR, node);
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
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)r1, NULL);
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (double)r2, NULL);
      tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, generate_temp_label(labels[j]));
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
      if (_generate_il_create_block(node->children[i+1]) == FAILED)
        return FAILED;
    }
    else {
      /* default */

      /* label */
      add_tac_label(generate_temp_label(labels[j]));

      /* block */
      if (_generate_il_create_block(node->children[i]) == FAILED)
        return FAILED;
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
      if (_generate_il_create_block(node->children[i+1]) == FAILED)
        return FAILED;

      /* jump to exit */
      add_tac_jump(generate_temp_label(label_exit));
  
      /* label of next condition */
      add_tac_label(generate_temp_label(label_next_condition));
    }
    else {
      /* else */
      
      /* main block */
      if (_generate_il_create_block(node->children[i]) == FAILED)
        return FAILED;
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

  /* condition */
  if (_generate_il_create_condition(node->children[0], label_exit) == FAILED)
    return FAILED;

  _enter_breakable(label_exit, label_condition);
  
  /* main block */
  if (_generate_il_create_block(node->children[1]) == FAILED)
    return FAILED;

  _exit_breakable();
  
  /* jump back to condition */
  add_tac_jump(generate_temp_label(label_condition));
  
  /* label of exit */
  add_tac_label(generate_temp_label(label_exit));

  return SUCCEEDED;
}


static int _generate_il_create_do(struct tree_node *node) {

  int label_start, label_exit;
  
  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  label_start = ++g_temp_label_id;
  label_exit = ++g_temp_label_id;

  /* label of start */
  add_tac_label(generate_temp_label(label_start));

  _enter_breakable(label_exit, label_start);

  /* main block */
  if (_generate_il_create_block(node->children[0]) == FAILED)
    return FAILED;

  _exit_breakable();  
  
  /* condition */
  if (_generate_il_create_condition(node->children[1], label_exit) == FAILED)
    return FAILED;
  
  /* jump back to start */
  add_tac_jump(generate_temp_label(label_start));
  
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
  if (_generate_il_create_block(node->children[0]) == FAILED)
    return FAILED;

  /* label of condition */
  add_tac_label(generate_temp_label(label_condition));

  /* condition */
  if (_generate_il_create_condition(node->children[1], label_exit) == FAILED)
    return FAILED;

  _enter_breakable(label_exit, label_increments);
  
  /* main block */
  if (_generate_il_create_block(node->children[3]) == FAILED)
    return FAILED;

  _exit_breakable();
  
  /* increments of for() */
  add_tac_label(generate_temp_label(label_increments));
  if (_generate_il_create_block(node->children[2]) == FAILED)
    return FAILED;
  
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

  if (node->added_children > 0) {
    /* struct access */
    int raddress = g_temp_r, rtemp, final_type;

    if (generate_il_calculate_struct_access_address(node, &final_type) == FAILED)
      return FAILED;

    t = add_tac();
    if (t == NULL)
      return FAILED;

    rtemp = g_temp_r++;

    /* read the value */
    t->op = TAC_OP_ARRAY_READ;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)raddress, NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0.0, NULL);
    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)rtemp, NULL);

    /* set the promotions */
    t->arg1_var_type = final_type;
    tac_promote_argument(t, final_type, TAC_USE_ARG1);
    tac_promote_argument(t, VARIABLE_TYPE_UINT8, TAC_USE_ARG2);
    tac_promote_argument(t, final_type, TAC_USE_RESULT);
    
    /* inc/dec */
    t = add_tac();
    if (t == NULL)
      return FAILED;

    if ((int)node->value == SYMBOL_INCREMENT)
      t->op = TAC_OP_ADD;
    else
      t->op = TAC_OP_SUB;

    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)rtemp, NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 1.0, NULL);
    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)rtemp, NULL);

    /* set the promotions */
    tac_promote_argument(t, final_type, TAC_USE_ARG1);
    tac_promote_argument(t, final_type, TAC_USE_ARG2);
    tac_promote_argument(t, final_type, TAC_USE_RESULT);
    
    /* write back */
    t = add_tac();
    if (t == NULL)
      return FAILED;

    t->op = TAC_OP_ARRAY_WRITE;
    tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (double)rtemp, NULL);
    tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 0.0, NULL);
    tac_set_result(t, TAC_ARG_TYPE_TEMP, (double)raddress, NULL);

    /* set the promotions */
    tac_promote_argument(t, final_type, TAC_USE_ARG1);
    tac_promote_argument(t, VARIABLE_TYPE_UINT8, TAC_USE_ARG2);
    t->result_var_type = final_type;
    tac_promote_argument(t, final_type, TAC_USE_RESULT);
  }
  else {
    /* variable access */
    
    t = add_tac();
    if (t == NULL)
      return FAILED;

    if ((int)node->value == SYMBOL_INCREMENT)
      t->op = TAC_OP_ADD;
    else
      t->op = TAC_OP_SUB;

    tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);
    tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 1.0, NULL);
    tac_set_result(t, TAC_ARG_TYPE_LABEL, 0.0, node->label);
  
    /* find the definition */
    tac_try_find_definition(t, node->label, NULL, TAC_USE_ARG1);
    tac_try_find_definition(t, node->label, NULL, TAC_USE_RESULT);

    if ((node->definition->children[0]->value_double == 0 && (node->definition->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
        (node->definition->children[0]->value_double > 0 && (node->definition->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)) {
      snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_increment_decrement(): Variable \"%s\" was declared \"const\". Cannot modify.\n", node->label);
      return print_error(g_error_message, ERROR_ERR);
    }
  
    /* set promotions */
    type = tree_node_get_max_var_type(t->result_node->children[0]);
    tac_promote_argument(t, type, TAC_USE_ARG1);
    tac_promote_argument(t, type, TAC_USE_ARG2);
    tac_promote_argument(t, type, TAC_USE_RESULT);
  }
  
  return SUCCEEDED;
}


static int _generate_il_create_break(struct tree_node *node) {

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (g_breakable_stack_items_level < 0) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_break(): \"break\" does nothing here, nothing to break from.\n");
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
    return print_error(g_error_message, ERROR_ERR);
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


static int _generate_il_asm(struct tree_node *node) {

  struct inline_asm *ia;
  struct tac *t;

  /* update file id and line number for all TACs and error messages */
  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  ia = inline_asm_find(node->value);
  if (ia == NULL)
    return FAILED;

  t = add_tac();
  if (t == NULL)
    return FAILED;

  t->op = TAC_OP_ASM;
  tac_set_result(t, TAC_ARG_TYPE_CONSTANT, (double)(ia->id), NULL);

  return SUCCEEDED;
}


static int _generate_il_create_statement(struct tree_node *node) {

  int r;

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
  else if (node->type == TREE_NODE_TYPE_DO)
    r = _generate_il_create_do(node);
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
  else if (node->type == TREE_NODE_TYPE_ASM)
    r = _generate_il_asm(node);
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

  /* ... unless it's a function prototype */
  if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE)
    return SUCCEEDED;
  
  /* ... unless it's a __pureasm function! */
  if ((node->flags & TREE_NODE_FLAG_PUREASM) == TREE_NODE_FLAG_PUREASM)
    return SUCCEEDED;
  
  if (node->children[0]->value != VARIABLE_TYPE_VOID || node->children[0]->value_double > 0) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_il_create_function(): The function \"%s\" doesn't end to a return, but its return type is not void!\n", node->children[1]->label);
    return print_error(g_error_message, ERROR_ERR);
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
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, node->line_number, node->file_id) == FAILED)
        return FAILED;
    }
  }

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)) {
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, node->line_number, node->file_id) == FAILED)
        return FAILED;
    }
  }

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION) {
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, node->line_number, node->file_id) == FAILED)
        return FAILED;
    }
  }

  /*
  print_symbol_table_items();
  */
  
  /* create il code for functions */

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL) {
      if (node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION || node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
        if (_generate_il_create_function(node) == FAILED)
          return FAILED;
      }
      else if (node->type == TREE_NODE_TYPE_ASM) {
        if (_generate_il_asm(node) == FAILED)
          return FAILED;
      }
    }
  }
  
  return SUCCEEDED;
}


int rename_static_variables_and_functions(void) {

  char new_name[MAX_NAME_LENGTH+1], source_name[MAX_NAME_LENGTH+1], *s;
  int i, j, length;

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if ((node->flags & TREE_NODE_FLAG_STATIC) == TREE_NODE_FLAG_STATIC) {
      /* give it a new, hopefully unique name, to mimic staticness! */

      /* get the source file name without special characters */
      s = get_file_name(node->file_id);
      length = strlen(s);

      for (j = 0; j < length; j++) {
        if (s[j] >= 'a' && s[j] <= 'z')
          source_name[j] = s[j];
        else if (s[j] >= 'A' && s[j] <= 'Z')
          source_name[j] = s[j];
        else if (s[j] >= '0' && s[j] <= '9')
          source_name[j] = s[j];
        else
          source_name[j] = '_';
      }
      source_name[j] = 0;
      
      snprintf(new_name, sizeof(new_name), "%s__static__%s_%d", node->children[1]->label, source_name, rand());

      /* free the old label */
      free(node->children[1]->label);

      /* allocate new, bigger one */
      node->children[1]->label = calloc(strlen(new_name) + 1, 1);
      if (node->children[1]->label == NULL)
        return print_error("rename_static_variables_and_functions(): Out of memory while allocating a string for a tree node.\n", ERROR_ERR);

      strcpy(node->children[1]->label, new_name);
    }
  }

  return SUCCEEDED;
}
