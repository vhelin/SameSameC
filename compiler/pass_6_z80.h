
#ifndef _PASS_6_Z80_H
#define _PASS_6_Z80_H

int pass_6_z80(char *file_name, FILE *file_out);
int generate_asm_z80(FILE *file_out);
int generate_global_variables_z80(char *file_name, FILE *file_out);
int is_z80_bounded_counter(struct tac *t, struct tree_node *function_node, int maximum, int allow_calls);
int generate_asm_bounded_loop_z80(int *tac_index, FILE *file_out, struct tree_node *function_node, int check_ownership);

#endif
