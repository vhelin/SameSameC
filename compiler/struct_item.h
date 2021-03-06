
#ifndef _STRUCT_ITEM_H
#define _STRUCT_ITEM_H

int free_struct_item(struct struct_item *s);
int free_struct_items(void);
int add_struct_item(struct struct_item *s);
int struct_item_add_child(struct struct_item *s, struct struct_item *child);
int calculate_struct_items(void);
int print_struct_items(void);
struct struct_item *allocate_struct_item(char *name, int type);
struct struct_item *find_struct_item(char *name);
struct struct_item *find_struct_item_child(struct struct_item *s, char *name);

#endif
