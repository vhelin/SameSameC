
#ifndef _DEFINITION_H
#define _DEFINITION_H

struct definition *define(char *name);
struct definition *definition_get(char *label);
void definition_free(struct definition *d);
void definition_free_arguments_data(struct definition *d);
int undefine(char *name);
int definition_add_token(struct definition *d, struct token *t);

#endif
