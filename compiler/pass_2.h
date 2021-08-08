
#ifndef _PASS_2_H
#define _PASS_2_H

int pass_2(void);
int add_a_new_definition(char *name, double value, char *string, int type, int size);
int create_definition(void);
int create_variable_or_function(int is_extern, int is_static);
struct tree_node *create_array(double pointer_depth, int variable_type, char *name, int line_number, int file_id);
int create_expression(void);
int create_term(void);
int create_factor(void);
int create_block(struct tree_node *open_function_definition, int expect_curly_braces, int is_multiline_block);
int create_condition(void);
int create_statement(void);
int create_struct_union(int is_extern, int is_static, int is_const_1, int variable_type, int is_global);
char *get_token_simple(struct token *t);
void check_ast(void);

#endif
