
#ifndef _PARSE_H
#define _PARSE_H

int get_next_token(void);
int compare_next_token(char *token);
int expand_macro_arguments(char *in);
int is_string_ending_with(char *s, char *e);
int is_char_a_symbol(char c);

#endif
