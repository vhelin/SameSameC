
#ifndef _MAIN_H
#define _MAIN_H

void procedures_at_exit(void);
int parse_flags(char **flags, int flagc, int *print_usage);
int parse_and_add_definition(char *c, int contains_flag);
int parse_and_add_incdir(char* c, int contains_flag);
int localize_path(char *path);
int strcaselesscmp(char *s1, char *s2);
void print_error(char *error, int type);
void next_line(void);

#endif
