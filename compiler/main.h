
#ifndef _MAIN_H
#define _MAIN_H

int parse_flags(char **flags, int flagc, int *print_usage);
int parse_and_add_definition(char *c, int contains_flag);
int parse_and_add_incdir(char* c, int contains_flag);
int localize_path(char *path);
int strcaselesscmp(char *s1, char *s2);
int print_error_using_tree_node(char *error, int type, struct tree_node *node);
int print_error_using_token(char *error, int type, struct token *t);
int print_error_using_tac(char *error, int type, struct tac *t);
int print_error(char *error, int type);
void procedures_at_exit(void);
void next_line(void);

#endif
