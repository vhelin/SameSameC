
#ifndef _PASS_2_H
#define _PASS_2_H

int pass_2(void);
int add_a_new_definition(char *name, double value, char *string, int type, int size);
int create_definition(void);
struct tree_node *create_variable_or_function(void);
struct tree_node *create_array(double pointer_depth, int variable_type, char *name);
int create_expression(void);
int create_term(void);
int create_factor(void);
int create_block(void);
int create_condition(void);
int create_statement(void);

#endif
