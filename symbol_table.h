
#ifndef _SYMBOL_TABLE_H
#define _SYMBOL_TABLE_H

int is_symbol_table_empty(void);
int symbol_table_add_symbol(struct tree_node *node, char *name, int level);
struct symbol_table_item *symbol_table_find_symbol(char *name);
void free_symbol_table_items(int level);
void free_symbol_table(void);

#endif
