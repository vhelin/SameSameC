
#ifndef _TAC_H
#define _TAC_H

char *generate_temp_label(int id);
void print_tac(struct tac *t);
void print_tacs(void);
int tac_set_arg1(struct tac *t, int arg_type, double d, char *s);
int tac_set_arg2(struct tac *t, int arg_type, double d, char *s);
int tac_set_result(struct tac *t, int arg_type, double d, char *s);
int is_last_tac(int op);
int tac_try_find_definition(struct tac *t, char *label, struct tree_node *node, int tac_use);
struct tac *add_tac(void);
struct tac *add_tac_label(char *label);
struct tac *add_tac_jump(char *label);
struct tac *add_tac_calculation(int op, int r1, int r2, int rresult);

#endif
