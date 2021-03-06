
#ifndef _INCLUDE_FILE_H
#define _INCLUDE_FILE_H

int include_file(char *name, int *include_size, char *namespace);
int preprocess_file(char *c, char *d, char *o, int *s, char *f);
int create_full_name(char *dir, char *name);
int print_file_names(void);
char *get_file_name(int id);

#endif
