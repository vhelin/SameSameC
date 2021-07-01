
#ifndef _TREE_NODE_H
#define _TREE_NODE_H

int get_variable_type_size(int type);
int get_max_variable_type_2(int t1, int t2);
int get_max_variable_type_4(int t1, int t2, int t3, int t4);
int get_variable_type_constant(int value);
int tree_node_flatten(struct tree_node *node);
int tree_node_does_contain_expressions(struct tree_node *node);
int tree_node_add_child(struct tree_node *node, struct tree_node *child);
int tree_node_set_string(struct tree_node *node, char *string);
int tree_node_get_max_var_type(struct tree_node *node);
int tree_node_get_create_variable_data_items(struct tree_node *node);
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
struct tree_node *allocate_tree_node_bytes(struct token *t);

#endif
