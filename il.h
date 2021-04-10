
#ifndef _IL_H
#define _IL_H

int il_stack_calculate_expression(struct tree_node *node);
int il_stack_calculate(struct stack_item *si, int q, int rresult);
int il_compute_stack(struct stack *sta, int count, int rresult);

#endif

