#ifndef _Z80_REGISTER_SPILL_H
#define _Z80_REGISTER_SPILL_H

int z80_emit_register_spill(struct tac *t, struct tree_node *function_node,
    FILE *file_out);
int z80_validate_array_write_reload_operand(char *function_name, int operand);
int z80_get_split_reload_physical_register(int consumer_op, int operand, int size);

#endif
