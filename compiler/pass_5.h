
#ifndef _PASS_5_H
#define _PASS_5_H

#define RA_ARGUMENT_TRANSPORT_NONE          0
#define RA_ARGUMENT_TRANSPORT_BYTE          1
#define RA_ARGUMENT_TRANSPORT_WORD          2
#define RA_ARGUMENT_TRANSPORT_SIGN_EXTEND   3
#define RA_ARGUMENT_TRANSPORT_ZERO_EXTEND   4
#define RA_ARGUMENT_TRANSPORT_TRUNCATE      5

#define RA_ARGUMENT_BYTE_ACCESS_SOURCE      1
#define RA_ARGUMENT_BYTE_ACCESS_TARGET      2

#define RA_CALL_FRAME_PREFIX_END             1
#define RA_CALL_FRAME_RETURN_SP_OFFSET       2
#define RA_CALL_FRAME_SAVED_FP_LOW_OFFSET    3
#define RA_CALL_FRAME_SAVED_FP_HIGH_OFFSET   4

#define RA_LOCATION_SOURCE_CONSTANT       1
#define RA_LOCATION_SOURCE_GLOBAL         2
#define RA_LOCATION_SOURCE_STACK_LOCAL    3
#define RA_LOCATION_SOURCE_STACK_SPILL    4
#define RA_LOCATION_SOURCE_PHYSICAL       5

#define RA_LOCATION_NONE          0
#define RA_LOCATION_CONST         1
#define RA_LOCATION_GLOBAL        2
#define RA_LOCATION_STACK_LOCAL   3
#define RA_LOCATION_STACK_SPILL   4
#define RA_LOCATION_PHY_A         5
#define RA_LOCATION_PHY_HL        6
#define RA_LOCATION_PHY_BC        7
#define RA_LOCATION_PHY_C         8
#define RA_LOCATION_PHY_B         9

#define RA_ADDRESS_TARGET_IX   1
#define RA_ADDRESS_TARGET_IY   2
#define RA_ADDRESS_TARGET_HL   3
#define RA_ADDRESS_TARGET_IX_UNCACHED   4
#define RA_ADDRESS_TARGET_IY_OLD_FRAME  5

#define RA_ADDRESS_MODE_INVALID              0
#define RA_ADDRESS_MODE_NONE                 1
#define RA_ADDRESS_MODE_GLOBAL_LABEL         2
#define RA_ADDRESS_MODE_FRAME_ADDRESS        3
#define RA_ADDRESS_MODE_FRAME_DISPLACEMENT   4

int pass_5(void);
int optimize_il(void);
int optimize_for_inc(void);
int compress_register_names(void);
int reuse_registers(void);
int propagate_operand_types(void);
int collect_and_preprocess_local_variables_inside_functions(void);
int reorder_global_variables(void);
int delete_function_prototype_tacs(void);
int turn_some_muls_and_divs_into_shifts(void);
int get_register_allocator_argument_transport(int source_var_type, int target_var_type);
int get_register_allocator_argument_byte_offset(int transport, int access_kind, int byte_index);
int get_register_allocator_return_value_byte_offset(int return_value_size, int byte_index);
int get_register_allocator_call_frame_prefix_value(int value_kind);
int get_register_allocator_location_kind(int source_kind, int physical_register);
int get_register_allocator_address_materialization_mode(int location_kind, int address_target, int offset);

#endif
