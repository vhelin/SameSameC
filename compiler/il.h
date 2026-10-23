
#ifndef _IL_H
#define _IL_H

int il_stack_calculate_expression(struct tree_node *node, int calculate_max_var_type, int false_label_id, int condition_negated);
int il_stack_calculate(struct stack_item *si, int q, int rresult, int false_label_id, int condition_negated);
int il_compute_stack(struct stack *sta, int count, int rresult, int false_label_id, int condition_negated);

#endif
