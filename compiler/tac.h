
#ifndef _TAC_H
#define _TAC_H

char *generate_temp_label(int id);
void print_tac(struct tac *t, int is_comment, FILE *file_out);
void print_tacs(void);
void tac_swap_args(struct tac *t);
void free_tac_contents(struct tac *t);
int tac_set_arg1(struct tac *t, int arg_type, double d, char *s);
int tac_set_arg2(struct tac *t, int arg_type, double d, char *s);
int tac_set_result(struct tac *t, int arg_type, double d, char *s);
int tac_copy_arg(struct tac *t, int source, int destination);
int is_last_tac(int op);
int tac_try_find_definition(struct tac *t, char *label, struct tree_node *node, int tac_use);
int tac_promote_argument(struct tac *t, int type, int tac_use);
struct tac *add_tac(void);
struct tac *insert_tac(int index);
int tac_set_register_spill(struct tac *t, struct tree_node *function_node, int temp_index, int no_physical_register, int physical_register, int destination_offset, int byte_count);
struct tac *add_tac_label(char *label);
struct tac *add_tac_jump(char *label);
struct tac *add_tac_calculation(int op, int r1, int r2, int rresult);

#endif
