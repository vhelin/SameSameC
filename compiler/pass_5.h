
#ifndef _PASS_5_H
#define _PASS_5_H

int pass_5(void);
int optimize_il(void);
int optimize_for_inc(void);
int compress_register_names(void);
int propagate_operand_types(void);
int collect_and_preprocess_local_variables_inside_functions(void);
int reorder_global_variables(void);
int delete_function_prototype_tacs(void);

#endif
