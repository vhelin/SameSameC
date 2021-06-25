
#ifndef _FILE_H
#define _FILE_H

int file_load(char *name);
int file_has_main(void);
int file_write_all_asm_files_into_tmp_file(FILE *fp);
int file_find_global_variable_inits(void);
int file_find_next_section(struct file *f, char *section_name, int length);
int file_find_next_ramsection(struct file *f, char *section_name, int length);

#endif
