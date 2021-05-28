
#ifndef _PASS_4_H
#define _PASS_4_H

int pass_4(void);
int generate_il(void);
int make_sure_all_tacs_have_definition_nodes(void);

struct tac *generate_il_create_function_call(struct tree_node *node);
struct tac *generate_il_create_get_address_array(struct tree_node *node);

#endif
