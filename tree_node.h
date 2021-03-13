
#ifndef _TREE_NODE_H
#define _TREE_NODE_H

int tree_node_add_child(struct tree_node *node, struct tree_node *child);
int tree_node_set_string(struct tree_node *node, char *string);
void free_tree_node(struct tree_node *node);
void free_tree_node_children(struct tree_node *node);
struct tree_node *clone_tree_node(struct tree_node *node);
struct tree_node *allocate_tree_node(int type);
struct tree_node *allocate_tree_node_with_children(int type, int child_count);
struct tree_node *allocate_tree_node_variable_type(int type);
struct tree_node *allocate_tree_node_value_int(int value);
struct tree_node *allocate_tree_node_value_double(double value);
struct tree_node *allocate_tree_node_value_string(char *string);
struct tree_node *allocate_tree_node_symbol(int symbol);
struct tree_node *allocate_tree_node_stack(int stack);

#endif
