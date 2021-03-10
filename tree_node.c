
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


int tree_node_add_child(struct tree_node *node, struct tree_node *child) {

  if (child == NULL) {
    print_error("Trying to add a NULL child to a node!\n", ERROR_DIR);
    return FAILED;
  }

  if (node->added_children >= node->children_max) {
    /* try to increase the child list size */
    int new_children_max = node->children_max + 32;
    int i;

    node->children = realloc(node->children, sizeof(struct tree_node *) * new_children_max);
    if (node->children == NULL) {    
      print_error("Cannot increase node's child list!\n", ERROR_DIR);
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

  free(node);
}


struct tree_node *allocate_tree_node(int type) {

  struct tree_node *node = calloc(sizeof(struct tree_node), 1);

  if (node == NULL) {
    print_error("Out of memory while allocating a new tree node.\n", ERROR_DIR);
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

  return node;
}


struct tree_node *allocate_tree_node_with_children(int type, int children_max) {

  struct tree_node *node = allocate_tree_node(type);
  int i;

  if (node == NULL)
    return NULL;

  node->children = calloc(sizeof(struct tree_node *) * children_max, 1);
  if (node->children == NULL) {
    print_error("Out of memory while allocating a new tree node.\n", ERROR_DIR);
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

  if (string == NULL)
    return FAILED;
  
  node->label = calloc(strlen(string) + 1, 1);
  if (node->label == NULL) {
    print_error("Out of memory while allocating a string for a tree node.\n", ERROR_DIR);
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
