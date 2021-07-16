
#ifndef _ARCH_Z80_H
#define _ARCH_Z80_H

int arch_z80_write_system_init(int target, FILE *file_out);
int arch_z80_write_link_file(int target, char *object_file_name, FILE *file_out);
int arch_z80_create_copy_bytes_functions(void);

#endif
