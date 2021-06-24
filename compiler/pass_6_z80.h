
#ifndef _PASS_6_Z80_H
#define _PASS_6_Z80_H

int pass_6_z80(char *file_name, FILE *file_out);
int generate_asm_z80(FILE *file_out);
int generate_global_variables_z80(char *file_name, FILE *file_out);

#endif
