
#ifndef _DEFINITIONS_H
#define _DEFINITIONS_H

int add_a_new_definition(char *name, double value, char *string, int type, int size);
void definition_get(char *label, struct definition **definition);
int redefine(char *name, double value, char *string, int type, int size);
int undefine(char *name);

#endif
