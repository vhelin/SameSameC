#ifndef _REGISTER_ALLOCATOR_H
#define _REGISTER_ALLOCATOR_H

#include <stddef.h>

#define RA_BLOCK_END_NONE              0
#define RA_BLOCK_END_LABEL             1
#define RA_BLOCK_END_JUMP              2
#define RA_BLOCK_END_FUNCTION_CALL     3
#define RA_BLOCK_END_RETURN            4
#define RA_BLOCK_END_INLINE_ASM        5
#define RA_BLOCK_END_UNMIGRATED        6
#define RA_BLOCK_END_FUNCTION_END      7

#define RA_CFG_EDGE_FALLTHROUGH        1
#define RA_CFG_EDGE_JUMP               2
#define RA_CFG_EDGE_BRANCH_TRUE        3
#define RA_CFG_EDGE_BRANCH_FALSE       4

#define RA_RELOAD_RANGE_BLOCK_END      0
#define RA_RELOAD_RANGE_FUNCTION_END   1
#define RA_RELOAD_RANGE_BRANCH         2
#define RA_RELOAD_RANGE_JOIN           3
#define RA_RELOAD_RANGE_CYCLE          4
#define RA_RELOAD_RANGE_NONCONTIGUOUS  5

#define RA_LINEAR_SCAN_ASSIGN          1
#define RA_LINEAR_SCAN_REPLACE_ACTIVE  2
#define RA_LINEAR_SCAN_KEEP_ACTIVE     3

#define RA_CANDIDATE_PLAN_USE_PRIMARY           1
#define RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED  2
#define RA_CANDIDATE_PLAN_FALLBACK_PATH         3
#define RA_CANDIDATE_PLAN_FALLBACK_CONFLICT     4
#define RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED    5
#define RA_CANDIDATE_PLAN_REJECT_PATH           6

#define RA_CANDIDATE_FALLBACK_UNSUPPORTED       1
#define RA_CANDIDATE_FALLBACK_PATH              2
#define RA_CANDIDATE_FALLBACK_CONFLICT          3

#define RA_CANDIDATE_RANGE_WITHIN_BLOCK         1
#define RA_CANDIDATE_RANGE_BEFORE_DECISION      2

#define RA_CANDIDATE_REJECTION_NONE             0
#define RA_CANDIDATE_REJECTION_NOT_FOUND        1
#define RA_CANDIDATE_REJECTION_RANGE            2
#define RA_CANDIDATE_REJECTION_METADATA         3
#define RA_CANDIDATE_REJECTION_INTERVAL         4
#define RA_CANDIDATE_REJECTION_LEGALITY         5
#define RA_CANDIDATE_REJECTION_TRANSPARENCY     6

#define RA_CANDIDATE_DISPATCH_USE_PRIMARY       1
#define RA_CANDIDATE_DISPATCH_USE_ALTERNATE     2
#define RA_CANDIDATE_DISPATCH_REJECT            3

#define RA_TEMP_RETAIN_REGISTER_ONLY   1
#define RA_TEMP_RETAIN_SPILL_BACKED    2

#define RA_SPLIT_ACTION_NONE                 1
#define RA_SPLIT_ACTION_PRESERVE             2
#define RA_SPLIT_ACTION_REJECT_UNSUPPORTED   3
#define RA_SPLIT_ACTION_REJECT_REGISTER      4

#define RA_RELOAD_ACTION_NONE                1
#define RA_RELOAD_ACTION_INSERT              2

#define RA_JOIN_ACTION_RELOAD_ON_DEMAND      1
#define RA_JOIN_ACTION_SPILL_PREDECESSORS     2

#define RA_JOIN_SPILL_BEFORE_ANCHOR           1
#define RA_JOIN_SPILL_AFTER_ANCHOR            2

#define RA_REACHING_DEFINITION_MISSING        1
#define RA_REACHING_DEFINITION_UNIQUE         2
#define RA_REACHING_DEFINITION_AMBIGUOUS      3

#define RA_JOIN_SCHEDULE_INELIGIBLE            1
#define RA_JOIN_SCHEDULE_READY                 2
#define RA_JOIN_SCHEDULE_CONFLICT              3

#define RA_JOIN_RESERVATION_INELIGIBLE          1
#define RA_JOIN_RESERVATION_READY               2
#define RA_JOIN_RESERVATION_CONFLICT            3

#define RA_JOIN_PATH_RESERVATION_INELIGIBLE     1
#define RA_JOIN_PATH_RESERVATION_READY          2

#define RA_JOIN_PATH_APPLICATION_INELIGIBLE     1
#define RA_JOIN_PATH_APPLICATION_APPLIED        2
#define RA_JOIN_PATH_APPLICATION_PARTIAL        3

#define RA_JOIN_PATH_COMMIT_INELIGIBLE           1
#define RA_JOIN_PATH_COMMIT_COMMITTED            2
#define RA_JOIN_PATH_COMMIT_CONFLICT             3

#define RA_JOIN_PATH_STATE_AVAILABLE              1
#define RA_JOIN_PATH_STATE_OWNED                  2
#define RA_JOIN_PATH_STATE_CONFLICT               3

#define RA_BLOCK_EXIT_SPILL_REQUIRED              1
#define RA_BLOCK_EXIT_SPILL_EXEMPT                 2

#define RA_JOIN_ASSIGNMENT_APPLICATION_INELIGIBLE  1
#define RA_JOIN_ASSIGNMENT_APPLICATION_APPLIED     2
#define RA_JOIN_ASSIGNMENT_APPLICATION_FAILED      3

#define RA_JOIN_ASSIGNMENT_PRODUCER            1
#define RA_JOIN_ASSIGNMENT_CONSUMER            2

#define RA_LOOP_JOIN_NOT_LOOP                   1
#define RA_LOOP_JOIN_READY                      2
#define RA_LOOP_JOIN_AMBIGUOUS                  3

#define RA_LOOP_LATCH_SELECTION_INELIGIBLE      1
#define RA_LOOP_LATCH_SELECTION_READY           2
#define RA_LOOP_LATCH_SELECTION_AMBIGUOUS       3

#define RA_LOOP_REGISTER_SELECTION_INELIGIBLE   1
#define RA_LOOP_REGISTER_SELECTION_READY        2

#define RA_LOOP_REGISTER_EXCLUSION_NONE          0
#define RA_LOOP_REGISTER_EXCLUSION_RESERVATION   1

#define RA_LOOP_CANDIDATE_INELIGIBLE            1
#define RA_LOOP_CANDIDATE_READY                 2
#define RA_LOOP_CANDIDATE_AMBIGUOUS             3

#define RA_LOOP_CANDIDATE_REASON_NONE           0
#define RA_LOOP_CANDIDATE_REASON_TOPOLOGY       1
#define RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN     2
#define RA_LOOP_CANDIDATE_REASON_MULTIPLE_LIVE_IN 3

#define RA_LOOP_REPRESENTATION_NOT_APPLICABLE   1
#define RA_LOOP_REPRESENTATION_TEMP_READY       2
#define RA_LOOP_REPRESENTATION_NO_VALUE         3
#define RA_LOOP_REPRESENTATION_UNIQUE_STACK_VALUE 4
#define RA_LOOP_REPRESENTATION_AMBIGUOUS_STACK_VALUES 5

#define RA_LOOP_STACK_ROLE_ENTRY_WRITE           (1 << 0)
#define RA_LOOP_STACK_ROLE_HEADER_READ           (1 << 1)
#define RA_LOOP_STACK_ROLE_LATCH_READ            (1 << 2)
#define RA_LOOP_STACK_ROLE_LATCH_WRITE           (1 << 3)
#define RA_LOOP_STACK_ROLE_EXIT_READ              (1 << 4)
#define RA_LOOP_STACK_ROLE_REQUIRED               31

#define RA_LOOP_STACK_SELECTION_INELIGIBLE        1
#define RA_LOOP_STACK_SELECTION_READY             2
#define RA_LOOP_STACK_SELECTION_AMBIGUOUS         3

#define RA_LOOP_STACK_PROMOTION_INELIGIBLE        1
#define RA_LOOP_STACK_PROMOTION_READY              2

#define RA_LOOP_STACK_PROMOTION_REASON_NONE        0
#define RA_LOOP_STACK_PROMOTION_REASON_SELECTION   1
#define RA_LOOP_STACK_PROMOTION_REASON_SIZE        2
#define RA_LOOP_STACK_PROMOTION_REASON_TEMP_CONFLICT 3

#define RA_LOOP_STACK_REWRITE_INELIGIBLE           1
#define RA_LOOP_STACK_REWRITE_READY                2

#define RA_LOOP_STACK_REWRITE_REASON_NONE          0
#define RA_LOOP_STACK_REWRITE_REASON_PROMOTION     1
#define RA_LOOP_STACK_REWRITE_REASON_ROLES         2

#define RA_LOOP_STACK_APPLICATION_INELIGIBLE       1
#define RA_LOOP_STACK_APPLICATION_APPLIED          2
#define RA_LOOP_STACK_APPLICATION_FAILED           3

#define RA_LOOP_STACK_STORAGE_STACK                 1
#define RA_LOOP_STACK_STORAGE_TEMP                  2

#define RA_LOOP_FACTS_INELIGIBLE                1
#define RA_LOOP_FACTS_READY                     2

#define RA_LOOP_FACTS_REASON_NONE               0
#define RA_LOOP_FACTS_REASON_TOPOLOGY           1
#define RA_LOOP_FACTS_REASON_MISSING_ENTRY      2
#define RA_LOOP_FACTS_REASON_MISSING_LATCH      3
#define RA_LOOP_FACTS_REASON_NO_CONSUMER        4

#define RA_LOOP_DEFINITION_INELIGIBLE           1
#define RA_LOOP_DEFINITION_READY                2

#define RA_LOOP_DEFINITION_REASON_NONE          0
#define RA_LOOP_DEFINITION_REASON_TOPOLOGY      1
#define RA_LOOP_DEFINITION_REASON_NOT_LIVE_IN   2
#define RA_LOOP_DEFINITION_REASON_MISSING_ENTRY 3
#define RA_LOOP_DEFINITION_REASON_MISSING_LATCH 4
#define RA_LOOP_DEFINITION_REASON_NO_CONSUMER   5

#define RA_LOOP_RETENTION_INELIGIBLE            1
#define RA_LOOP_RETENTION_READY                 2

#define RA_LOOP_APPLICATION_INELIGIBLE          1
#define RA_LOOP_APPLICATION_APPLIED             2
#define RA_LOOP_APPLICATION_FAILED              3

#define RA_FIXED_POINT_STEP_REBUILD              1
#define RA_FIXED_POINT_STEP_STABLE               2
#define RA_FIXED_POINT_STEP_ITERATION_LIMIT      3

struct tac;
struct temp_register;

struct register_allocator_target_policy {
  char *name;
  int (*get_physical_register_units)(int physical_register);
  int (*get_active_slot_count)(void);
  int (*get_active_slot_index)(int physical_register);
  char *(*get_active_slot_name)(int physical_register);
  int (*get_candidate_register_count)(int size);
  int (*get_candidate_physical_register)(int size, int candidate_index);
  int (*can_fallback_candidate)(int size, int reason);
  int (*prefer_candidate_on_equal_next_use)(int consumer_op, int candidate_operand, int active_operand, int physical_register);
  int (*is_candidate_allowed_for_physical_register)(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int physical_register, int consumer_arg);
  int (*get_tac_clobbers)(int op);
  int (*is_tac_transparent_for_physical_register)(struct tac *t, int physical_register);
  int (*get_call_result_physical_register)(int size);
  int (*get_call_boundary_spill_reason)(int op);
  int (*get_stack_return_value_end_offset)(int return_value_size);
  int (*get_return_value_byte_offset)(int return_value_size, int byte_index);
  int (*get_call_frame_prefix_value)(int value_kind);
  int (*get_stack_argument_end_offset)(int frame_end_offset, int argument_size, int argument_index);
  int (*get_argument_transport)(int source_var_type, int target_var_type);
  int (*get_argument_byte_offset)(int transport, int access_kind, int byte_index);
  int (*get_location_kind)(int source_kind, int physical_register);
  int (*get_address_materialization_mode)(int location_kind, int address_target, int offset);
  int (*can_preserve_split_spill)(int consumer_op, int consumer_arg, int size, int physical_register);
  int (*get_split_reload_physical_register)(struct tac *consumer, int consumer_arg, int size);
};

struct register_allocator_instruction {
  int active;
  int tac_index;
  int is_label;
  char *label;
  int end_reason;
  int is_jump;
  int is_conditional_jump;
  char *jump_target;
};

struct register_allocator_basic_block {
  int start_tac;
  int end_tac;
  int end_reason;
};

struct register_allocator_cfg_edge {
  int from_block;
  int to_block;
  int kind;
  char *target;
};

struct register_allocator_reload_scan_range {
  int end_block;
  int end_instruction;
  int extended_block_count;
  int stop_reason;
};

struct register_allocator_join_spill_site {
  int predecessor_block;
  int edge_kind;
  int anchor_instruction;
  int placement;
};

struct register_allocator_join_spill_mutation {
  int apply;
  struct register_allocator_join_spill_site site;
  int temp_index;
  int physical_register;
};

struct register_allocator_join_spill_emission {
  int emit;
  struct register_allocator_join_spill_site site;
  int temp_index;
  int physical_register;
  int destination_offset;
  int byte_count;
};

struct register_allocator_join_spill_work_item {
  int join_block_index;
  struct register_allocator_join_spill_emission emission;
};

struct register_allocator_join_retention_path {
  int predecessor_block;
  int definition_instruction;
  int anchor_instruction;
  int physical_register;
  int definition_found;
  int definition_supported;
  int path_transparent;
};

struct register_allocator_reaching_definition_block {
  int definition_instruction;
  int definition_found;
  int path_transparent;
};

struct register_allocator_reaching_definition {
  int status;
  int definition_instruction;
  int path_transparent;
  int iterations;
};

struct register_allocator_join_assignment {
  int role;
  int instruction;
  int operand;
  int physical_register;
};

struct register_allocator_join_schedule {
  int status;
  int assignment_count;
  int conflict_instruction;
};

struct register_allocator_join_assignment_application {
  int status;
  int applied_count;
};

struct register_allocator_loop_join {
  int status;
  int entry_predecessor;
  int latch_predecessor;
  int entry_edge;
  int back_edge;
};

struct register_allocator_loop_flow_profile {
  int status;
  int entry_edge_count;
  int back_edge_count;
  int exit_edge_count;
  int terminal_exit_count;
};

struct register_allocator_loop_latch_selection {
  int status;
  int entry_edge_count;
  int back_edge_count;
  int defining_latch_count;
  int latch_definition;
  struct register_allocator_loop_join loop_join;
};

struct register_allocator_loop_latch_definition {
  int block_index;
  int edge_index;
  int definition_instruction;
};

struct register_allocator_loop_latch_collection {
  int status;
  int entry_edge_count;
  int back_edge_count;
  int definition_count;
  int entry_predecessor;
  int entry_edge;
};

struct register_allocator_loop_register_selection {
  int status;
  int physical_register;
  int candidate_index;
  int bound_assignment_count;
  int conflict_assignment;
};

struct register_allocator_loop_register_path_evaluation {
  int physical_register;
  int path_safe;
};

struct register_allocator_loop_register_exclusion {
  int physical_register;
  int excluded;
  int reason;
  int blocking_block;
  int blocking_instruction;
};

struct register_allocator_loop_candidate {
  int status;
  int reason;
  int candidate_count;
  int temp_index;
};

struct register_allocator_loop_representation {
  int status;
  int stack_value_count;
};

struct register_allocator_loop_stack_value_observation {
  int instruction_index;
  int identity;
};

struct register_allocator_loop_stack_value_roles {
  int identity;
  int role_mask;
};

struct register_allocator_loop_stack_value_selection {
  int status;
  int candidate_count;
  int identity;
};

struct register_allocator_loop_stack_promotion_plan {
  int status;
  int reason;
  int identity;
  int temp_index;
  int size;
};

struct register_allocator_loop_stack_rewrite_observation {
  int instruction_index;
  int operand;
  int identity;
  int role;
};

struct register_allocator_loop_stack_rewrite {
  int instruction_index;
  int operand;
  int temp_index;
  int role;
};

struct register_allocator_loop_stack_rewrite_schedule {
  int status;
  int reason;
  int rewrite_count;
  int role_mask;
};

struct register_allocator_loop_stack_application {
  int status;
  int rewrite_count;
};

struct register_allocator_loop_stack_operand_storage {
  int kind;
  int identity;
  int temp_index;
};

struct register_allocator_loop_stack_temp_storage {
  int temp_index;
  int size;
};

struct register_allocator_loop_facts {
  int status;
  int reason;
  int entry_definition;
  int latch_definition;
  int consumer_instruction;
  int consumer_operand;
};

struct register_allocator_loop_definition {
  int status;
  int reason;
  int entry_definition;
  int latch_definition;
  int consumer_instruction;
  int consumer_operand;
};

struct register_allocator_loop_retention_plan {
  int status;
  int assignment_count;
  int reservation_count;
};

struct register_allocator_loop_retention_application {
  int status;
  int assignment_count;
  int reservation_count;
};

struct register_allocator_join_occupied_interval {
  int temp_index;
  int physical_register;
  int live_start;
  int live_end;
  int overlaps_selected_register;
};

struct register_allocator_join_reservation {
  int status;
  int slot_index;
  int live_start;
  int live_end;
  int conflict_temp;
};

struct register_allocator_join_block_reservation {
  int block_index;
  int start_instruction;
  int end_instruction;
};

struct register_allocator_join_path_reservation {
  int status;
  int block_count;
};

struct register_allocator_join_path_application {
  int status;
  int applied_count;
  int failed_block;
};

struct register_allocator_join_path_state_entry {
  int block_index;
  int slot_index;
  int temp_index;
  int start_instruction;
  int end_instruction;
};

struct register_allocator_join_path_commit {
  int status;
  int committed_count;
  int conflict_entry;
};

struct register_allocator_join_path_state_storage {
  int entry_capacity;
  size_t entry_bytes;
};

struct register_allocator_join_path_state_query {
  int status;
  int entry_index;
  int owner_temp;
};

struct register_allocator_block_exit_spill {
  int status;
  int reservation_entry;
  int slot_index;
};

struct register_allocator_control_flow_storage {
  int max_blocks;
  int max_edges;
  size_t instruction_bytes;
  size_t block_bytes;
  size_t edge_bytes;
};

struct register_allocator_fixed_point_step {
  int status;
  int pass;
  int pass_limit;
};

struct register_allocator_liveness_storage {
  int set_count;
  size_t buffer_bytes;
  size_t total_bytes;
};

struct register_allocator_live_interval {
  int used;
  int size;
  int read_count;
  int write_count;
  int live_start;
  int live_end;
};

struct register_allocator_active_slot {
  int temp_index;
  int next_use;
  int producer_instruction;
  int consumer_operand;
  int interval_retained;
};

struct register_allocator_candidate {
  int found;
  int temp_index;
  int producer_instruction;
  int consumer_instruction;
  int consumer_operand;
  int has_single_read;
};

struct register_allocator_candidate_evaluation {
  int physical_register;
  int allowed;
  int path_safe;
  int conflict_free;
};

struct register_allocator_candidate_selection {
  int physical_register;
  int candidate_index;
};

struct register_allocator_candidate_dispatch {
  int action;
  int physical_register;
  int candidate_index;
  int check_path;
  int check_overlap;
};

struct register_allocator_candidate_qualification {
  int eligible;
  int rejection_reason;
  int deciding_instruction;
};

struct register_allocator_candidate_probe {
  struct register_allocator_candidate candidate;
  struct register_allocator_candidate_qualification qualification;
};

struct register_allocator_primary_candidate_evaluation {
  int physical_register;
  int allowed;
  int path_safe;
  int active;
};

struct register_allocator_primary_candidate_decision {
  struct register_allocator_primary_candidate_evaluation evaluation;
  int fallback_unsupported;
  int fallback_path;
  int fallback_conflict;
  int plan;
};

struct register_allocator_candidate_selection_finalization {
  int proceed;
  struct register_allocator_primary_candidate_decision decision;
  struct register_allocator_candidate_dispatch dispatch;
  int split_action;
};

struct register_allocator_candidate_selection_preparation {
  int proceed;
  int interval_blocked;
  int preservation_queried;
  int preservation_supported;
  int preservation_physical_register;
};

struct register_allocator_candidate_selection_orchestration {
  int proceed;
  struct register_allocator_candidate_selection_preparation preparation;
  struct register_allocator_candidate_selection_finalization finalization;
};

struct register_allocator_slot_transition {
  int decision;
  int clear_displaced_interval;
  int spill_displaced_temp;
  int retain_candidate;
  int assign_candidate;
  int displaced_temp;
  int displaced_producer_instruction;
  int displaced_consumer_instruction;
  int displaced_consumer_operand;
};

struct register_allocator_candidate_transition_plan {
  int active_temp;
  int active_next_use;
  int active_replaceable;
  int prefer_candidate_on_equal;
  int linear_scan_decision;
  struct register_allocator_slot_transition transition;
};

struct register_allocator_candidate_transition_preparation {
  int slot_index;
  char *slot_name;
  struct register_allocator_candidate_transition_plan plan;
};

struct register_allocator_temp_state {
  int spill_required;
  int physical_register;
};

struct register_allocator_reload_site {
  int found;
  int instruction;
};

struct register_allocator_reload_chain {
  int count;
  int truncated;
  int stop_instruction;
};

struct register_allocator_reload_mutation {
  int apply;
  int instruction;
  int temp_index;
  int operand;
  int physical_register;
};

struct register_allocator_reload_execution {
  int action;
  struct register_allocator_reload_mutation mutation;
};

struct register_allocator_reload_chain_execution {
  struct register_allocator_reload_chain chain;
  int processed_count;
  int applied_count;
};

struct register_allocator_reload_chain_storage {
  size_t instruction_bytes;
  size_t execution_bytes;
};

struct register_allocator_reload_emission {
  int emit;
  int temp_index;
  int operand;
  int physical_register;
  int source_offset;
  int byte_count;
};

struct register_allocator_spill_emission {
  int emit;
  int temp_index;
  int physical_register;
  int destination_offset;
  int byte_count;
};

struct register_allocator_spill_mutation {
  int apply;
  int instruction;
  int temp_index;
  int operand;
  int physical_register;
};

struct register_allocator_candidate_transition_application {
  int retained_interval;
  struct register_allocator_spill_mutation spill_mutation;
};

struct register_allocator_candidate_transition_execution {
  struct register_allocator_candidate_transition_preparation preparation;
  struct register_allocator_candidate_transition_application application;
};

typedef int (*register_allocator_instruction_predicate)(void *context, int instruction_index, int temp_index);
typedef int (*register_allocator_instruction_query)(void *context, int instruction_index, int temp_index);
typedef int (*register_allocator_join_predicate)(void *context, int block_index, int predecessor_count, int temp_index);
typedef int (*register_allocator_join_spill_site_approver)(void *context, int join_block_index, struct register_allocator_join_spill_site *site);
typedef int (*register_allocator_loop_retention_transaction_applier)(void *context, int temp_index, int slot_index, struct register_allocator_join_assignment *assignments, int assignment_count, struct register_allocator_join_block_reservation *reservations, int reservation_count);
typedef int (*register_allocator_loop_stack_transaction_applier)(void *context, struct register_allocator_loop_stack_promotion_plan *promotion, struct register_allocator_loop_stack_rewrite *rewrites, int rewrite_count);
typedef int (*register_allocator_join_spill_mutation_applier)(void *context, int join_block_index, struct register_allocator_join_spill_site *site, int temp_index, int physical_register);
typedef int (*register_allocator_join_spill_emission_applier)(void *context, int join_block_index, struct register_allocator_join_spill_site *site, int temp_index, int physical_register, int destination_offset, int byte_count);
typedef int (*register_allocator_join_block_reservation_applier)(void *context, int temp_index, int slot_index, struct register_allocator_join_block_reservation *reservation);
typedef int (*register_allocator_join_assignment_transaction_applier)(void *context, int temp_index, struct register_allocator_join_assignment *assignments, int assignment_count);
typedef int (*register_allocator_candidate_predicate)(void *context, int physical_register);
typedef int (*register_allocator_temp_predicate)(void *context, int temp_index);
typedef int (*register_allocator_alternate_selector)(void *context, char *reason, int check_path, int check_overlap, int *selected_physical_register, int *selected_candidate_index);
typedef int (*register_allocator_discovered_candidate_predicate)(void *context, struct register_allocator_candidate *candidate, int physical_register);
typedef int (*register_allocator_path_predicate)(void *context, int instruction_index, int physical_register);
typedef int (*register_allocator_clear_interval_callback)(void *context, int producer_instruction, int consumer_instruction, int consumer_operand);
typedef int (*register_allocator_spill_temp_callback)(void *context, int temp_index);
typedef int (*register_allocator_retain_candidate_callback)(void *context, int temp_index, int physical_register, int producer_instruction, int consumer_instruction, int consumer_operand, int *retained_interval);
typedef int (*register_allocator_spill_mutation_applier)(void *context, int instruction_index, int temp_index, int operand, int physical_register);
typedef void (*register_allocator_candidate_transition_observer)(void *context, struct register_allocator_candidate_transition_preparation *preparation);
typedef int (*register_allocator_reload_mutation_applier)(void *context, int instruction_index, int temp_index, int operand, int physical_register);
typedef int (*register_allocator_reload_transaction_applier)(void *context, struct register_allocator_reload_mutation *mutations, int mutation_count);
typedef int (*register_allocator_reload_register_selector)(void *context, int instruction_index, int temp_index);
typedef int (*register_allocator_reload_operand_register_selector)(void *context, int instruction_index, int temp_index, int operand);

int register_allocator_validate_target_policy(char *function_name, struct register_allocator_target_policy *policy);
int register_allocator_physical_registers_overlap(char *function_name, struct register_allocator_target_policy *policy, int left_register, int right_register, int *overlap);
int register_allocator_plan_active_slot_storage(char *function_name, int slot_count, size_t *byte_count);
int register_allocator_resolve_active_slot_roles(char *function_name, struct register_allocator_target_policy *policy, int *physical_registers, int role_count, int *slot_indices);
int register_allocator_plan_control_flow_storage(char *function_name, int instruction_capacity, struct register_allocator_control_flow_storage *storage);
int register_allocator_plan_liveness_storage(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage);
int register_allocator_resolve_candidate_registers(char *function_name, struct register_allocator_target_policy *policy, int size, int no_physical_register, int *physical_registers, int max_registers, int *register_count);
int register_allocator_evaluate_candidate_registers(char *function_name, int block_index, char *reason, int *physical_registers, int physical_register_count, int no_physical_register, void *context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_conflict_free, struct register_allocator_candidate_evaluation *evaluations, int evaluation_capacity);
int register_allocator_select_candidate_register(char *function_name, int block_index, char *reason, int *physical_registers, int physical_register_count, int no_physical_register, void *context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_conflict_free, struct register_allocator_candidate_evaluation *evaluations, int evaluation_capacity, struct register_allocator_candidate_selection *selection);
int register_allocator_dispatch_candidate_plan(char *function_name, int block_index, int plan, int primary_physical_register, int no_physical_register, void *context, register_allocator_alternate_selector select_alternate, struct register_allocator_candidate_dispatch *dispatch);
int register_allocator_evaluate_primary_candidate(char *function_name, int block_index, int physical_register, int no_physical_register, void *context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_active, struct register_allocator_primary_candidate_evaluation *evaluation);
int register_allocator_decide_primary_candidate(char *function_name, int block_index, struct register_allocator_target_policy *policy, int size, int candidate_count, int physical_register, int no_physical_register, void *context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_active, struct register_allocator_primary_candidate_decision *decision);
int register_allocator_finalize_candidate_selection(char *function_name, int block_index, struct register_allocator_target_policy *policy, int size, int candidate_count, int primary_physical_register, int no_physical_register, int temp_index, int producer_instruction, int consumer_instruction, int has_single_read, int preservation_supported, int preservation_physical_register, void *evaluation_context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_active, void *dispatch_context, register_allocator_alternate_selector select_alternate, struct register_allocator_candidate_selection_finalization *finalization);
int register_allocator_choose_candidate_register(char *function_name, int block_index, char *reason, struct register_allocator_candidate_evaluation *candidates, int candidate_count, int no_physical_register, int *selected_physical_register, int *selected_candidate_index);
int register_allocator_resolve_candidate_fallbacks(char *function_name, struct register_allocator_target_policy *policy, int size, int candidate_count, int *fallback_unsupported, int *fallback_path, int *fallback_conflict);
int register_allocator_resolve_equal_next_use_preference(char *function_name, int block_index, struct register_allocator_target_policy *policy, int consumer_op, int candidate_operand, int active_operand, int physical_register, int no_physical_register, int candidate_next_use, int active_next_use, int *prefer_candidate);
int register_allocator_plan_candidate_fallback(char *function_name, int block_index, int primary_allowed, int primary_path_safe, int primary_active, int fallback_unsupported, int fallback_path, int fallback_conflict);
int register_allocator_interval_is_blocked(char *function_name, int producer_instruction, int consumer_instruction, int spill_reason, int no_spill_reason, int unconditional_spill_reason, int spill_boundary_instruction, int *blocked);
int register_allocator_prepare_candidate_selection(char *function_name, int block_index, struct register_allocator_target_policy *policy, struct register_allocator_candidate *candidate, int consumer_op, int size, int physical_register, int no_physical_register, int spill_reason, int no_spill_reason, int unconditional_spill_reason, int spill_boundary_instruction, struct register_allocator_candidate_selection_preparation *preparation);
int register_allocator_orchestrate_candidate_selection(char *function_name, int block_index, struct register_allocator_target_policy *policy, struct register_allocator_candidate *candidate, int consumer_op, int size, int candidate_count, int primary_physical_register, int no_physical_register, int spill_reason, int no_spill_reason, int unconditional_spill_reason, int spill_boundary_instruction, void *evaluation_context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_active, void *dispatch_context, register_allocator_alternate_selector select_alternate, struct register_allocator_candidate_selection_orchestration *orchestration);
int register_allocator_is_transparent_range(char *function_name, int start_instruction, int end_instruction, int instruction_count, int physical_register, int no_physical_register, void *context, register_allocator_path_predicate is_transparent, int *transparent, int *blocking_instruction);
int register_allocator_evaluate_candidate_path(char *function_name, int block_index, int start_instruction, int end_instruction, int instruction_count, int physical_register, int no_physical_register, void *context, register_allocator_path_predicate has_nearer_candidate, register_allocator_path_predicate has_preserving_overlap, register_allocator_path_predicate is_special_transparent, register_allocator_path_predicate is_target_transparent, int *path_safe, int *deciding_instruction);
int register_allocator_build_basic_blocks(char *function_name, struct register_allocator_instruction *instructions, int instruction_count, struct register_allocator_basic_block *blocks, int max_blocks, int *block_count);
int register_allocator_build_cfg(char *function_name, struct register_allocator_instruction *instructions, int instruction_count, struct register_allocator_basic_block *blocks, int block_count, struct register_allocator_cfg_edge *edges, int max_edges, int *edge_count);
int register_allocator_plan_reload_scan_range(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, struct register_allocator_cfg_edge *edges, int edge_count, int start_block, struct register_allocator_reload_scan_range *range);
int register_allocator_find_next_use(char *function_name, int temp_index, int start_instruction, int end_instruction, int instruction_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp);
int register_allocator_discover_candidate(char *function_name, int producer_instruction, int block_end_instruction, int instruction_count, void *context, register_allocator_instruction_query get_produced_temp, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_query get_consumer_operand, struct register_allocator_candidate *candidate);
int register_allocator_candidate_meets_range(char *function_name, struct register_allocator_candidate *candidate, int decision_instruction, int range_mode, int *eligible);
int register_allocator_qualify_candidate(char *function_name, int physical_register, int no_physical_register, void *context, struct register_allocator_candidate *candidate, register_allocator_discovered_candidate_predicate has_metadata, register_allocator_discovered_candidate_predicate interval_is_clear, register_allocator_discovered_candidate_predicate is_allowed, register_allocator_discovered_candidate_predicate is_path_transparent, struct register_allocator_candidate_qualification *qualification);
int register_allocator_probe_competing_candidate(char *function_name, int producer_instruction, int block_end_instruction, int instruction_count, int decision_instruction, int range_mode, int physical_register, int no_physical_register, void *context, register_allocator_instruction_query get_produced_temp, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_query get_consumer_operand, register_allocator_discovered_candidate_predicate has_metadata, register_allocator_discovered_candidate_predicate interval_is_clear, register_allocator_discovered_candidate_predicate is_allowed, register_allocator_discovered_candidate_predicate is_path_transparent, struct register_allocator_candidate_probe *probe);
int register_allocator_reset_live_intervals(char *function_name, struct register_allocator_live_interval *intervals, int interval_count);
int register_allocator_note_live_interval(char *function_name, struct register_allocator_live_interval *intervals, int interval_count, int temp_index, int size, int instruction_index, int is_write);
int register_allocator_count_live_intervals(char *function_name, struct register_allocator_live_interval *intervals, int interval_count);
int register_allocator_reset_active_slots(char *function_name, int block_index, struct register_allocator_active_slot *slots, int slot_count);
int register_allocator_expire_active_slots(char *function_name, int block_index, int instruction_index, struct register_allocator_active_slot *slots, int slot_count);
int register_allocator_assign_active_slot(char *function_name, int block_index, int slot_index, struct register_allocator_active_slot *slots, int slot_count, int temp_index, int next_use, int producer_instruction, int consumer_operand, int interval_retained);
int register_allocator_linear_scan_choose(char *function_name, int block_index, char *slot_name, int instruction_index, int candidate_temp, int candidate_next_use, int active_temp, int active_next_use, int active_replaceable, int prefer_candidate_on_equal);
int register_allocator_plan_slot_transition(char *function_name, int block_index, int linear_scan_decision, struct register_allocator_active_slot *active_slot, int candidate_temp, int candidate_next_use, int producer_instruction, int consumer_operand, struct register_allocator_slot_transition *transition);
int register_allocator_resolve_active_replaceability(char *function_name, int block_index, int instruction_index, struct register_allocator_active_slot *active_slot, void *context, register_allocator_temp_predicate has_temp_metadata, int *active_replaceable);
int register_allocator_plan_candidate_transition(char *function_name, int block_index, char *slot_name, int instruction_index, int candidate_temp, int candidate_next_use, int consumer_op, int candidate_operand, int physical_register, int no_physical_register, struct register_allocator_active_slot *active_slot, struct register_allocator_target_policy *policy, void *context, register_allocator_temp_predicate has_temp_metadata, struct register_allocator_candidate_transition_plan *plan);
int register_allocator_prepare_candidate_transition(char *function_name, int block_index, int instruction_index, int candidate_temp, int candidate_next_use, int consumer_op, int candidate_operand, int physical_register, int no_physical_register, struct register_allocator_active_slot *active_slots, int active_slot_count, struct register_allocator_target_policy *policy, void *context, register_allocator_temp_predicate has_temp_metadata, struct register_allocator_candidate_transition_preparation *preparation);
int register_allocator_apply_slot_transition(char *function_name, int block_index, int slot_index, struct register_allocator_active_slot *slots, int slot_count, int physical_register, int no_physical_register, int candidate_temp, int candidate_next_use, int producer_instruction, int consumer_operand, void *context, struct register_allocator_slot_transition *transition, register_allocator_clear_interval_callback clear_interval, register_allocator_spill_temp_callback spill_temp, register_allocator_retain_candidate_callback retain_candidate, int *retained_interval);
int register_allocator_retain_temp_state(char *function_name, int block_index, int temp_index, struct register_allocator_temp_state *state, int no_physical_register, int requested_physical_register, int has_spill_constraint, int read_count, int write_count);
int register_allocator_spill_temp_state(char *function_name, int block_index, int temp_index, struct register_allocator_temp_state *state, int no_physical_register, char *reason);
int register_allocator_classify_join_temp_state(char *function_name, int block_index, int predecessor_count, int temp_index, struct register_allocator_temp_state *state, int no_physical_register, int *stack_resident);
int register_allocator_plan_join_action(char *function_name, int block_index, int predecessor_count, int temp_index, int stack_resident, int *action);
int register_allocator_collect_join_predecessors(char *function_name, int block_count, int join_block_index, struct register_allocator_cfg_edge *edges, int edge_count, int *predecessors, int predecessor_capacity, int *predecessor_count);
int register_allocator_plan_join_spill_sites(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, int join_block_index, struct register_allocator_cfg_edge *edges, int edge_count, struct register_allocator_join_spill_site *sites, int site_capacity, int *site_count);
int register_allocator_order_join_spill_sites(char *function_name, int join_block_index, int instruction_count, struct register_allocator_join_spill_site *sites, int site_count);
int register_allocator_approve_join_spill_sites(char *function_name, int join_block_index, int instruction_count, struct register_allocator_join_spill_site *sites, int site_count, void *context, register_allocator_join_spill_site_approver approve_site, int *approved_count);
int register_allocator_prepare_join_spill_mutation(char *function_name, int join_block_index, int temp_index, int join_action, int no_physical_register, int physical_register, struct register_allocator_join_spill_site *site, struct register_allocator_join_spill_mutation *mutation);
int register_allocator_apply_join_spill_mutation(char *function_name, int join_block_index, int no_physical_register, void *context, struct register_allocator_join_spill_mutation *mutation, register_allocator_join_spill_mutation_applier apply_mutation);
int register_allocator_prepare_join_spill_emission(char *function_name, int join_block_index, int no_physical_register, struct register_allocator_join_spill_mutation *mutation, int spill_available, int destination_offset, int byte_count, struct register_allocator_join_spill_emission *emission);
int register_allocator_order_join_spill_emissions(char *function_name, int block_count, int instruction_count, int no_physical_register, struct register_allocator_join_spill_work_item *work_items, int work_item_count);
int register_allocator_plan_post_mutation_rebuild(char *function_name, int original_instruction_count, int current_instruction_count, int inserted_instruction_count, int rewritten_operand_count, int added_temp_count, int *rebuild_required);
int register_allocator_plan_fixed_point_limit(char *function_name, int instruction_capacity, int *pass_limit);
int register_allocator_plan_fixed_point_step(char *function_name, int pass, int pass_limit, int rebuild_required, struct register_allocator_fixed_point_step *step);
int register_allocator_plan_join_retention(char *function_name, int block_count, int instruction_count, int join_block_index, int temp_index, int no_physical_register, struct register_allocator_join_retention_path *paths, int path_count, int consumer_supported, int *selected_physical_register);
int register_allocator_resolve_reaching_definition(char *function_name, int block_count, int instruction_count, struct register_allocator_cfg_edge *edges, int edge_count, struct register_allocator_reaching_definition_block *block_facts, int predecessor_block, int temp_index, struct register_allocator_reaching_definition *result);
int register_allocator_plan_join_schedule(char *function_name, int instruction_count, int temp_index, int no_physical_register, struct register_allocator_join_retention_path *paths, int path_count, int *producer_physical_registers, int consumer_instruction, int consumer_operand, int consumer_physical_register, int selected_physical_register, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_schedule *schedule);
int register_allocator_classify_loop_join(char *function_name, int block_count, int join_block_index, struct register_allocator_cfg_edge *edges, int edge_count, struct register_allocator_loop_join *loop_join);
int register_allocator_profile_loop_flow(char *function_name, int block_count, struct register_allocator_basic_block *blocks, int join_block_index, struct register_allocator_cfg_edge *edges, int edge_count, struct register_allocator_loop_flow_profile *profile);
int register_allocator_select_loop_latch(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, int join_block_index, struct register_allocator_cfg_edge *edges, int edge_count, int temp_index, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate writes_temp, struct register_allocator_loop_latch_selection *selection);
int register_allocator_collect_loop_latch_definitions(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, int join_block_index, struct register_allocator_cfg_edge *edges, int edge_count, int temp_index, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate writes_temp, struct register_allocator_loop_latch_definition *definitions, int definition_capacity, struct register_allocator_loop_latch_collection *collection);
int register_allocator_evaluate_loop_register_paths(char *function_name, int block_index, int temp_index, int no_physical_register, int *candidate_registers, int candidate_count, void *context, register_allocator_candidate_predicate is_path_safe, struct register_allocator_loop_register_path_evaluation *evaluations, int evaluation_capacity);
int register_allocator_prepare_loop_register_exclusions(char *function_name, int block_index, int temp_index, int no_physical_register, int *candidate_registers, int candidate_count, struct register_allocator_loop_register_exclusion *exclusions, int exclusion_capacity);
int register_allocator_exclude_loop_register_candidate(char *function_name, int block_index, int temp_index, int candidate_index, int reason, int blocking_block, int blocking_instruction, struct register_allocator_loop_register_exclusion *exclusions, int exclusion_count);
int register_allocator_select_loop_retention_register(char *function_name, int block_index, int temp_index, int no_physical_register, int preferred_physical_register, int *candidate_registers, int candidate_count, struct register_allocator_join_assignment *assignments, int assignment_count, struct register_allocator_loop_register_path_evaluation *path_evaluations, int path_evaluation_count, struct register_allocator_loop_register_selection *selection);
int register_allocator_select_loop_retention_register_with_exclusions(char *function_name, int block_index, int temp_index, int no_physical_register, int preferred_physical_register, int *candidate_registers, int candidate_count, struct register_allocator_join_assignment *assignments, int assignment_count, struct register_allocator_loop_register_path_evaluation *path_evaluations, int path_evaluation_count, struct register_allocator_loop_register_exclusion *exclusions, int exclusion_count, struct register_allocator_loop_register_selection *selection);
int register_allocator_plan_loop_join_candidate(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, int join_block_index, struct register_allocator_loop_join *loop_join, char *live_in, struct register_allocator_loop_candidate *candidate);
int register_allocator_classify_loop_representation(char *function_name, int join_block_index, struct register_allocator_loop_candidate *candidate, int stack_value_count, struct register_allocator_loop_representation *representation);
int register_allocator_collect_loop_stack_values(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, int join_block_index, struct register_allocator_loop_join *loop_join, struct register_allocator_loop_stack_value_observation *observations, int observation_count, int *identities, int identity_capacity, int *identity_count);
int register_allocator_select_loop_stack_value(char *function_name, int join_block_index, struct register_allocator_loop_stack_value_roles *values, int value_count, struct register_allocator_loop_stack_value_selection *selection);
int register_allocator_plan_loop_stack_promotion(char *function_name, int join_block_index, struct register_allocator_loop_stack_value_selection *selection, int value_size, int proposed_temp_index, int *existing_temp_indices, int existing_temp_count, struct register_allocator_loop_stack_promotion_plan *plan);
int register_allocator_plan_loop_stack_rewrites(char *function_name, int instruction_count, struct register_allocator_loop_stack_promotion_plan *promotion, struct register_allocator_loop_stack_rewrite_observation *observations, int observation_count, struct register_allocator_loop_stack_rewrite *rewrites, int rewrite_capacity, struct register_allocator_loop_stack_rewrite_schedule *schedule);
int register_allocator_apply_loop_stack_promotion(char *function_name, int instruction_count, struct register_allocator_loop_stack_promotion_plan *promotion, struct register_allocator_loop_stack_rewrite *rewrites, int rewrite_capacity, struct register_allocator_loop_stack_rewrite_schedule *schedule, void *context, register_allocator_loop_stack_transaction_applier apply_transaction, struct register_allocator_loop_stack_application *application);
int register_allocator_commit_loop_stack_promotion_storage(char *function_name, int instruction_count, struct register_allocator_loop_stack_promotion_plan *promotion, struct register_allocator_loop_stack_rewrite *rewrites, int rewrite_count, struct register_allocator_loop_stack_operand_storage *operands, int operand_count, struct register_allocator_loop_stack_temp_storage *temps, int temp_capacity, int *temp_count);
int register_allocator_discover_loop_join_facts(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, int join_block_index, int temp_index, struct register_allocator_loop_join *loop_join, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_query get_consumer_operand, struct register_allocator_loop_facts *facts);
int register_allocator_plan_loop_join_definition(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, int join_block_index, int temp_index, int live_in, struct register_allocator_loop_join *loop_join, int entry_definition, int latch_definition, int consumer_instruction, int consumer_operand, struct register_allocator_loop_definition *definition);
int register_allocator_plan_loop_join_schedule(char *function_name, int instruction_count, int temp_index, int no_physical_register, struct register_allocator_loop_join *loop_join, int entry_definition, int entry_physical_register, int latch_definition, int latch_physical_register, int consumer_instruction, int consumer_operand, int consumer_physical_register, int selected_physical_register, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_schedule *schedule);
int register_allocator_plan_loop_join_path_reservations(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, struct register_allocator_cfg_edge *edges, int edge_count, int join_block_index, int temp_index, int slot_index, struct register_allocator_loop_join *loop_join, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_schedule *schedule, struct register_allocator_join_block_reservation *block_reservations, int block_reservation_capacity, struct register_allocator_join_path_reservation *path_reservation);
int register_allocator_plan_multi_latch_retention(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, struct register_allocator_cfg_edge *edges, int edge_count, int join_block_index, int temp_index, int slot_index, int no_physical_register, int selected_physical_register, int entry_physical_register, int latch_physical_register, int consumer_physical_register, struct register_allocator_loop_join *loop_join, struct register_allocator_loop_definition *definition, struct register_allocator_join_assignment *supplemental_producers, int supplemental_producer_count, struct register_allocator_join_assignment *supplemental_consumers, int supplemental_consumer_count, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_block_reservation *reservations, int reservation_capacity, struct register_allocator_loop_retention_plan *plan);
int register_allocator_plan_loop_retention(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, struct register_allocator_cfg_edge *edges, int edge_count, int join_block_index, int temp_index, int slot_index, int no_physical_register, int selected_physical_register, int entry_physical_register, int latch_physical_register, int consumer_physical_register, struct register_allocator_loop_join *loop_join, struct register_allocator_loop_definition *definition, struct register_allocator_join_assignment *supplemental_consumers, int supplemental_consumer_count, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_block_reservation *reservations, int reservation_capacity, struct register_allocator_loop_retention_plan *plan);
int register_allocator_apply_loop_retention(char *function_name, int temp_index, int slot_index, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_block_reservation *reservations, int reservation_capacity, struct register_allocator_loop_retention_plan *plan, void *context, register_allocator_loop_retention_transaction_applier apply_transaction, struct register_allocator_loop_retention_application *application);
int register_allocator_commit_loop_retention_transaction(char *function_name, int block_count, int instruction_count, int temp_index, int slot_index, struct register_allocator_join_assignment *assignments, int assignment_count, struct register_allocator_join_block_reservation *reservations, int reservation_count, struct register_allocator_join_path_state_entry *state_entries, int state_capacity, int *state_count, void *context, register_allocator_join_assignment_transaction_applier apply_assignments);
int register_allocator_apply_join_assignments(char *function_name, int instruction_count, int temp_index, int no_physical_register, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_schedule *schedule, void *context, register_allocator_join_assignment_transaction_applier apply_transaction, struct register_allocator_join_assignment_application *application);
int register_allocator_plan_join_reservation(char *function_name, int instruction_count, int temp_index, int no_physical_register, int selected_physical_register, int slot_index, int slot_count, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_schedule *schedule, struct register_allocator_join_occupied_interval *occupied_intervals, int occupied_interval_count, struct register_allocator_join_reservation *reservation);
int register_allocator_plan_join_path_reservations(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, struct register_allocator_cfg_edge *edges, int edge_count, int temp_index, struct register_allocator_join_assignment *assignments, int assignment_capacity, struct register_allocator_join_schedule *schedule, struct register_allocator_join_reservation *reservation, struct register_allocator_join_block_reservation *block_reservations, int block_reservation_capacity, struct register_allocator_join_path_reservation *path_reservation);
int register_allocator_apply_join_path_reservations(char *function_name, int block_count, int instruction_count, int temp_index, struct register_allocator_join_reservation *reservation, struct register_allocator_join_block_reservation *block_reservations, int block_reservation_capacity, struct register_allocator_join_path_reservation *path_reservation, void *context, register_allocator_join_block_reservation_applier apply_reservation, struct register_allocator_join_path_application *application);
int register_allocator_commit_join_path_reservations(char *function_name, int block_count, int instruction_count, int temp_index, struct register_allocator_join_reservation *reservation, struct register_allocator_join_block_reservation *block_reservations, int block_reservation_capacity, struct register_allocator_join_path_reservation *path_reservation, struct register_allocator_join_path_state_entry *state_entries, int state_capacity, int *state_count, struct register_allocator_join_path_commit *commit);
int register_allocator_plan_join_path_state_storage(char *function_name, int block_count, int temp_count, struct register_allocator_join_path_state_storage *storage);
int register_allocator_query_join_path_state(char *function_name, int block_count, int instruction_count, int slot_count, int block_index, int slot_index, int temp_index, int start_instruction, int end_instruction, struct register_allocator_join_path_state_entry *state_entries, int state_count, struct register_allocator_join_path_state_query *query);
int register_allocator_plan_block_exit_spill(char *function_name, int block_count, int instruction_count, int slot_count, int block_index, int end_instruction, int temp_index, struct register_allocator_join_path_state_entry *state_entries, int state_count, struct register_allocator_block_exit_spill *spill);
int register_allocator_apply_join_spill_emission(char *function_name, int join_block_index, int no_physical_register, void *context, struct register_allocator_join_spill_emission *emission, register_allocator_join_spill_emission_applier apply_emission);
int register_allocator_plan_split_action(char *function_name, int block_index, int temp_index, int producer_instruction, int consumer_instruction, int has_single_read, int preservation_supported, int no_physical_register, int preservation_physical_register, int selected_physical_register);
int register_allocator_discover_reload_site(char *function_name, int block_index, int temp_index, int start_instruction, int end_instruction, int instruction_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, struct register_allocator_reload_site *reload_site);
int register_allocator_discover_reload_chain(char *function_name, int block_index, int temp_index, int start_instruction, int end_instruction, int instruction_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, int *reload_instructions, int max_reload_instructions, struct register_allocator_reload_chain *reload_chain);
int register_allocator_discover_reload_graph(char *function_name, int block_count, int instruction_count, struct register_allocator_basic_block *blocks, struct register_allocator_cfg_edge *edges, int edge_count, int start_block, int spill_instruction, int temp_index, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, int *reload_instructions, int reload_capacity, struct register_allocator_reload_chain *reload_chain);
int register_allocator_plan_reload_action(char *function_name, int block_index, int temp_index, int spill_instruction, int reload_instruction, int no_physical_register, int reload_physical_register);
int register_allocator_prepare_reload_mutation(char *function_name, int block_index, int temp_index, int reload_instruction, int reload_action, int no_physical_register, int reload_physical_register, struct register_allocator_reload_mutation *mutation);
int register_allocator_prepare_reload_operand_mutation(char *function_name, int block_index, int temp_index, int reload_instruction, int reload_operand, int reload_action, int no_physical_register, int reload_physical_register, struct register_allocator_reload_mutation *mutation);
int register_allocator_prepare_reload_emission(char *function_name, int reload_requested, int temp_index, int no_physical_register, int physical_register, int spill_available, int source_offset, int byte_count, struct register_allocator_reload_emission *emission);
int register_allocator_prepare_reload_operand_emission(char *function_name, int reload_requested, int temp_index, int operand, int no_physical_register, int physical_register, int spill_available, int source_offset, int byte_count, struct register_allocator_reload_emission *emission);
int register_allocator_prepare_spill_emission(char *function_name, int spill_requested, int temp_index, int no_physical_register, int physical_register, int spill_available, int destination_offset, int byte_count, struct register_allocator_spill_emission *emission);
int register_allocator_prepare_spill_mutation(char *function_name, int block_index, int temp_index, int consumer_instruction, int consumer_operand, int split_action, int retained_interval, int no_physical_register, int physical_register, struct register_allocator_spill_mutation *mutation);
int register_allocator_apply_spill_mutation(char *function_name, int block_index, int no_physical_register, void *context, struct register_allocator_spill_mutation *mutation, register_allocator_spill_mutation_applier apply_mutation);
int register_allocator_apply_candidate_transition(char *function_name, int block_index, int slot_index, struct register_allocator_active_slot *slots, int slot_count, int physical_register, int no_physical_register, int candidate_temp, int candidate_next_use, int producer_instruction, int consumer_operand, int split_action, void *transition_context, struct register_allocator_slot_transition *transition, register_allocator_clear_interval_callback clear_interval, register_allocator_spill_temp_callback spill_temp, register_allocator_retain_candidate_callback retain_candidate, void *spill_context, register_allocator_spill_mutation_applier apply_spill_mutation, struct register_allocator_candidate_transition_application *application);
int register_allocator_execute_candidate_transition(char *function_name, int block_index, int instruction_index, int candidate_temp, int candidate_next_use, int consumer_op, int candidate_operand, int physical_register, int no_physical_register, int split_action, struct register_allocator_active_slot *active_slots, int active_slot_count, struct register_allocator_target_policy *policy, void *planning_context, register_allocator_temp_predicate has_temp_metadata, void *observer_context, register_allocator_candidate_transition_observer observe_preparation, void *transition_context, register_allocator_clear_interval_callback clear_interval, register_allocator_spill_temp_callback spill_temp, register_allocator_retain_candidate_callback retain_candidate, void *spill_context, register_allocator_spill_mutation_applier apply_spill_mutation, struct register_allocator_candidate_transition_execution *execution);
int register_allocator_apply_reload_mutation(char *function_name, int block_index, int no_physical_register, void *context, struct register_allocator_reload_mutation *mutation, register_allocator_reload_mutation_applier apply_mutation);
int register_allocator_execute_reload(char *function_name, int block_index, int temp_index, int spill_instruction, int reload_instruction, int no_physical_register, int reload_physical_register, void *context, register_allocator_reload_mutation_applier apply_mutation, struct register_allocator_reload_execution *execution);
int register_allocator_execute_reload_operand(char *function_name, int block_index, int temp_index, int spill_instruction, int reload_instruction, int reload_operand, int no_physical_register, int reload_physical_register, void *context, register_allocator_reload_mutation_applier apply_mutation, struct register_allocator_reload_execution *execution);
int register_allocator_plan_reload_chain_storage(char *function_name, int reload_capacity, struct register_allocator_reload_chain_storage *storage);
int register_allocator_execute_reload_chain(char *function_name, int block_index, int temp_index, int spill_instruction, int start_instruction, int end_instruction, int instruction_count, int no_physical_register, void *discovery_context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, void *selection_context, register_allocator_reload_register_selector select_register, void *mutation_context, register_allocator_reload_mutation_applier apply_mutation, int *reload_instructions, struct register_allocator_reload_execution *reload_executions, int reload_capacity, struct register_allocator_reload_chain_execution *chain_execution);
int register_allocator_execute_reload_operand_chain(char *function_name, int block_index, int temp_index, int spill_instruction, int start_instruction, int end_instruction, int instruction_count, int no_physical_register, void *discovery_context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, void *selection_context, register_allocator_instruction_query get_reload_operand, register_allocator_reload_operand_register_selector select_register, void *mutation_context, register_allocator_reload_mutation_applier apply_mutation, int *reload_instructions, struct register_allocator_reload_execution *reload_executions, int reload_capacity, struct register_allocator_reload_chain_execution *chain_execution);
int register_allocator_execute_reload_operand_chain_atomic(char *function_name, int block_index, int temp_index, int spill_instruction, int start_instruction, int end_instruction, int instruction_count, int no_physical_register, void *discovery_context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, void *selection_context, register_allocator_instruction_query get_reload_operand, register_allocator_reload_operand_register_selector select_register, void *transaction_context, register_allocator_reload_transaction_applier apply_transaction, int *reload_instructions, struct register_allocator_reload_execution *reload_executions, int reload_capacity, struct register_allocator_reload_chain_execution *chain_execution);
int register_allocator_execute_reload_graph_transaction(char *function_name, int block_index, int temp_index, int spill_instruction, int no_physical_register, void *selection_context, register_allocator_instruction_query get_reload_operand, register_allocator_reload_operand_register_selector select_register, void *transaction_context, register_allocator_reload_transaction_applier apply_transaction, int *reload_instructions, int reload_count, struct register_allocator_reload_execution *reload_executions, int reload_capacity, struct register_allocator_reload_chain_execution *chain_execution);
int register_allocator_resolve_liveness_index(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, int block_index, int temp_index, int *set_index);
int register_allocator_collect_liveness_use_def(char *function_name, int block_index, int start_instruction, int end_instruction, int instruction_count, int temp_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, char *use_set, char *def_set);
int register_allocator_reconcile_stack_only_joins(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, struct register_allocator_cfg_edge *edges, int edge_count, char *live_in, void *context, register_allocator_join_predicate is_stack_resident);
int register_allocator_solve_liveness(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, struct register_allocator_cfg_edge *edges, int edge_count, char *live_use, char *live_def, char *live_in, char *live_out, int *iterations);

#endif
