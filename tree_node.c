
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
    print_error("Out of memory while flattening an expression tree node.\n", ERROR_DIR);
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
  node->definition = NULL;

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

  for (i = 0; i < node->added_children; i++)
    clone->children[i] = clone_tree_node(node->children[i]);

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

  if (node == NULL)
    return FAILED;
  
  if (string == NULL) {
    free(node->label);
    node->label = NULL;
    return SUCCEEDED;
  }
  
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
