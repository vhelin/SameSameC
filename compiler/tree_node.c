
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "tree_node.h"
#include "main.h"


extern int g_current_filename_id, g_current_line_number;

static int g_variable_type_priorities[6] = {
  0, /* VARIABLE_TYPE_NONE */
  1, /* VARIABLE_TYPE_VOID */
  2, /* VARIABLE_TYPE_INT8 */
  4, /* VARIABLE_TYPE_INT16 */
  3, /* VARIABLE_TYPE_UINT8 */
  5  /* VARIABLE_TYPE_UINT16 */
};

static int g_variable_type_sizes[6] = {
  0,  /* VARIABLE_TYPE_NONE */
  0,  /* VARIABLE_TYPE_VOID */
  8,  /* VARIABLE_TYPE_INT8 */
  16, /* VARIABLE_TYPE_INT16 */
  8,  /* VARIABLE_TYPE_UINT8 */
  16  /* VARIABLE_TYPE_UINT16 */
};


int tree_node_flatten(struct tree_node *node) {

  int i, j, k, items = 0, got_expression = NO;
  struct tree_node **children, *child;

  /* calculate the total number of items */
  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION) {
      items += node->children[i]->added_children;
      got_expression = YES;
    }
    else
      items++;
  }

  /* no sub-expressions? */
  if (got_expression == NO)
    return SUCCEEDED;

  children = calloc(sizeof(struct tree_node *) * items, 1);
  if (children == NULL) {
    print_error("Out of memory while flattening an expression tree node.\n", ERROR_ERR);
    return FAILED;
  }

  /* copy the items */
  for (i = 0, j = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION) {
      child = node->children[i];
      for (k = 0; k < child->added_children; k++) {
        children[j++] = child->children[k];
        child->children[k] = NULL;
      }
      free_tree_node(child);
      node->children[i] = NULL;
    }
    else
      children[j++] = node->children[i];
  }
  
  /* replace the children array */
  free(node->children);
  node->children = children;
  node->added_children = items;
  node->children_max = items;

  /* flatten again? */
  if (tree_node_does_contain_expressions(node) == YES)
    return tree_node_flatten(node);
  
  return SUCCEEDED;
}


int get_variable_type_size(int type) {

  return g_variable_type_sizes[type];
}


int get_variable_type_constant(int value) {

  if (value >= 0 && value <= 255)
    return VARIABLE_TYPE_UINT8;
  else if (value >= -128 && value <= 127)
    return VARIABLE_TYPE_INT8;
  else if (value >= -32768 && value <= 32767)
    return VARIABLE_TYPE_INT16;
  else
    return VARIABLE_TYPE_UINT16;
}


int get_max_variable_type_2(int t1, int t2) {

  if (g_variable_type_priorities[t1] > g_variable_type_priorities[t2])
    return t1;
  else if (g_variable_type_priorities[t1] < g_variable_type_priorities[t2])
    return t2;
  else
    return t1;
}


int get_max_variable_type_4(int t1, int t2, int t3, int t4) {

  int r1, r2;

  r1 = get_max_variable_type_2(t1, t2);
  r2 = get_max_variable_type_2(t3, t4);

  return get_max_variable_type_2(r1, r2);
}


static int _get_create_variable_variable_type(struct tree_node *node) {

  if (node->value_double > 0.0)
    return VARIABLE_TYPE_UINT16;
  else
    return node->value;
}


int get_array_item_variable_type(struct tree_node *node, char *label) {

  int pointer_depth;

  pointer_depth = ((int)node->value_double) - 1;

  if (pointer_depth > 0)
    return VARIABLE_TYPE_UINT16;
  else
    return node->value;
}


int tree_node_get_max_var_type(struct tree_node *node) {

  if (node->type == TREE_NODE_TYPE_VALUE_INT)
    return get_variable_type_constant(node->value);
  else if (node->type == TREE_NODE_TYPE_VALUE_DOUBLE)
    return get_variable_type_constant((int)node->value_double);
  else if (node->type == TREE_NODE_TYPE_VALUE_STRING)
    return _get_create_variable_variable_type(node->definition->children[0]);
  else if (node->type == TREE_NODE_TYPE_VARIABLE_TYPE)
    return _get_create_variable_variable_type(node);
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM)
    return _get_create_variable_variable_type(node->definition->children[0]);
  else if (node->type == TREE_NODE_TYPE_GET_ADDRESS || node->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY)
    return VARIABLE_TYPE_UINT16;
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT)
    return _get_create_variable_variable_type(node->definition->children[0]);
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM)
    return get_array_item_variable_type(node->definition->children[0], node->definition->children[1]->label);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL)
    return _get_create_variable_variable_type(node->definition->children[0]);
  else if (node->type == TREE_NODE_TYPE_EXPRESSION) {
    int type_max = VARIABLE_TYPE_NONE, i;

    for (i = 0; i < node->added_children; i++) {
      struct tree_node *n = node->children[i];

      if (n->type == TREE_NODE_TYPE_VALUE_INT)
        type_max = get_max_variable_type_2(type_max, get_variable_type_constant(n->value));
      else if (n->type == TREE_NODE_TYPE_VALUE_DOUBLE)
        type_max = get_max_variable_type_2(type_max, get_variable_type_constant((int)n->value_double));
      else if (n->type == TREE_NODE_TYPE_SYMBOL) {
      }
      else if (n->type == TREE_NODE_TYPE_EXPRESSION)
        type_max = get_max_variable_type_2(type_max, tree_node_get_max_var_type(n));
      else if (n->type == TREE_NODE_TYPE_VALUE_STRING)
        type_max = get_max_variable_type_2(type_max, _get_create_variable_variable_type(n->definition->children[0]));
      else if (n->type == TREE_NODE_TYPE_GET_ADDRESS || n->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY)
        type_max = get_max_variable_type_2(type_max, VARIABLE_TYPE_UINT16);
      else if (n->type == TREE_NODE_TYPE_INCREMENT_DECREMENT)
        type_max = get_max_variable_type_2(type_max, _get_create_variable_variable_type(n->definition->children[0]));
      else if (n->type == TREE_NODE_TYPE_ARRAY_ITEM)
        type_max = get_max_variable_type_2(type_max, get_array_item_variable_type(n->definition->children[0], n->definition->children[1]->label));
      else if (n->type == TREE_NODE_TYPE_FUNCTION_CALL)
        type_max = get_max_variable_type_2(type_max, _get_create_variable_variable_type(n->definition->children[0]));
      else
        fprintf(stderr, "tree_node_get_max_var_type(): Unsupported tree_node type %d in an expression! Please submit a bug report!\n", n->type);
    }

    return type_max;
  }
  else {
    fprintf(stderr, "tree_node_get_max_var_type(): Unsupported tree_node type %d! Please submit a bug report!\n", node->type);
    return VARIABLE_TYPE_NONE;
  }  
}


int tree_node_does_contain_expressions(struct tree_node *node) {

  int i;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      return YES;
  }
    
  return NO;
}


int tree_node_add_child(struct tree_node *node, struct tree_node *child) {

  if (child == NULL) {
    print_error("Trying to add a NULL child to a node!\n", ERROR_ERR);
    return FAILED;
  }

  if (node->added_children >= node->children_max) {
    /* try to increase the child list size */
    int new_children_max = node->children_max + 32;
    int i;

    node->children = realloc(node->children, sizeof(struct tree_node *) * new_children_max);
    if (node->children == NULL) {    
      print_error("Cannot increase node's child list!\n", ERROR_ERR);
      return FAILED;
    }

    for (i = node->children_max; i < new_children_max; i++)
      node->children[i] = NULL;

    node->children_max = new_children_max;
  }

  node->children[node->added_children] = child;
  node->added_children++;

  return SUCCEEDED;
}


void free_tree_node_children(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->children_max; i++) {
    if (node->children[i] != NULL) {
      free_tree_node(node->children[i]);
      node->children[i] = NULL;
    }
  }

  if (node->children != NULL) {
    free(node->children);
    node->children = NULL;
  }

  node->children_max = 0;
  node->added_children = 0;
}


void free_tree_node(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->label != NULL) {
    free(node->label);
    node->label = NULL;
  }

  free_tree_node_children(node);

  if (node->local_variables != NULL) {
    free(node->local_variables->local_variables);
    free(node->local_variables);
  }

  free(node);
}


struct tree_node *allocate_tree_node(int type) {

  struct tree_node *node = calloc(sizeof(struct tree_node), 1);

  if (node == NULL) {
    print_error("Out of memory while allocating a new tree node.\n", ERROR_ERR);
    return NULL;
  }

  node->type = type;
  node->value = 0;
  node->value_double = 0.0;
  node->label = NULL;
  node->children = NULL;
  node->children_max = 0;
  node->added_children = 0;
  node->file_id = g_current_filename_id;
  node->line_number = g_current_line_number;
  node->definition = NULL;
  node->local_variables = NULL;
  node->flags = 0;
  node->reads = 0;
  node->writes = 0;

  /* NOTE! */
  node->file_id = g_current_filename_id;
  node->line_number = g_current_line_number;
  
  return node;
}


struct tree_node *clone_tree_node(struct tree_node *node) {

  struct tree_node *clone;
  int i;
  
  if (node == NULL)
    return NULL;

  clone = allocate_tree_node_with_children(node->type, node->children_max);
  if (clone == NULL)
    return NULL;

  if (tree_node_set_string(clone, node->label) == FAILED) {
    free_tree_node(clone);
    return NULL;
  }

  clone->type = node->type;
  clone->value = node->value;
  clone->value_double = node->value_double;
  clone->added_children = node->added_children;
  clone->file_id = node->file_id;
  clone->line_number = node->line_number;
  clone->definition = node->definition;
  clone->flags = node->flags;
  clone->reads = node->reads;
  clone->writes = node->writes;

  for (i = 0; i < node->added_children; i++)
    clone->children[i] = clone_tree_node(node->children[i]);

  if (node->local_variables != NULL)
    fprintf(stderr, "clone_tree_node(): ERROR: local_variables != NULL, but cloning it is not implemented!\n");
  
  return clone;
}


struct tree_node *allocate_tree_node_with_children(int type, int children_max) {

  struct tree_node *node = allocate_tree_node(type);
  int i;

  if (node == NULL)
    return NULL;

  if (children_max <= 0)
    return allocate_tree_node(type);
  
  node->children = calloc(sizeof(struct tree_node *) * children_max, 1);
  if (node->children == NULL) {
    print_error("Out of memory while allocating a new tree node.\n", ERROR_ERR);
    free(node);
    return NULL;
  }

  for (i = 0; i < children_max; i++)
    node->children[i] = NULL;

  node->children_max = children_max;
  
  return node;
}


struct tree_node *allocate_tree_node_variable_type(int type) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_VARIABLE_TYPE);

  if (node == NULL)
    return NULL;

  node->value = type;

  return node;
}


struct tree_node *allocate_tree_node_value_int(int value) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_VALUE_INT);

  if (node == NULL)
    return NULL;

  node->value = value;

  return node;
}


struct tree_node *allocate_tree_node_value_double(double value) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_VALUE_DOUBLE);

  if (node == NULL)
    return NULL;

  node->value_double = value;

  return node;
}


struct tree_node *allocate_tree_node_value_string(char *string) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_VALUE_STRING);

  if (node == NULL)
    return NULL;

  if (tree_node_set_string(node, string) == FAILED) {
    free(node);
    node = NULL;
  }

  return node;
}


int tree_node_set_string(struct tree_node *node, char *string) {

  if (node == NULL)
    return FAILED;
  
  if (string == NULL) {
    free(node->label);
    node->label = NULL;
    return SUCCEEDED;
  }

  if (node->label != NULL)
    free(node->label);
  
  node->label = calloc(strlen(string) + 1, 1);
  if (node->label == NULL) {
    print_error("Out of memory while allocating a string for a tree node.\n", ERROR_ERR);
    return FAILED;
  }

  strcpy(node->label, string);

  return SUCCEEDED;
}


struct tree_node *allocate_tree_node_symbol(int symbol) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_SYMBOL);

  if (node == NULL)
    return NULL;

  node->value = symbol;

  return node;
}


struct tree_node *allocate_tree_node_stack(int stack) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_STACK);

  if (node == NULL)
    return NULL;

  node->value = stack;

  return node;
}


struct tree_node *allocate_tree_node_bytes(struct token *t) {

  struct tree_node *node = allocate_tree_node(TREE_NODE_TYPE_BYTES);

  if (node == NULL)
    return NULL;

  node->label = calloc(t->value, 1);
  if (node->label == NULL) {
    print_error("Out of memory while allocating memory for a tree node.\n", ERROR_ERR);
    return FAILED;
  }

  /* copy data */
  memcpy(node->label, t->label, t->value);

  /* copy the number of bytes */
  node->value = t->value;
  
  return node;
}


int tree_node_get_create_variable_data_items(struct tree_node *node) {

  int items = 0, i;
  
  items = 0;
  for (i = 2; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_BYTES)
      items += node->children[i]->value;
    else
      items++;
  }
  
  return items;
}
  

int tree_node_is_expression_just_a_constant(struct tree_node *node) {

  if (node->type == TREE_NODE_TYPE_VALUE_INT || node->type == TREE_NODE_TYPE_VALUE_DOUBLE)
    return YES;
  
  return NO;
}
