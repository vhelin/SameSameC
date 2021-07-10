
#ifndef _TOKEN_H
#define _TOKEN_H

int token_add(int id, int value, double value_double, char *label);
int token_clone_and_add(struct token *t);
void token_free(struct token *t);
struct token *token_clone(struct token *t);
char *get_token_simple(struct token *t);

#endif
