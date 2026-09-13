#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int storage_is_clear(struct register_allocator_liveness_storage *storage) {

  return storage->set_count == 0 && storage->buffer_bytes == 0 &&
      storage->total_bytes == 0;
}

static int expect_storage(char *function_name, int block_count, int temp_count) {

  struct register_allocator_liveness_storage storage;
  int expected_count;

  expected_count = block_count * temp_count;
  if (register_allocator_plan_liveness_storage(function_name, block_count,
      temp_count, &storage) == FAILED)
    return FAILED;
  if (storage.set_count != expected_count ||
      storage.buffer_bytes != (size_t)expected_count * sizeof(char) ||
      storage.total_bytes != storage.buffer_bytes * 4)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_storage(char *function_name, int block_count,
    int temp_count) {

  struct register_allocator_liveness_storage storage;

  storage.set_count = 1;
  storage.buffer_bytes = 2;
  storage.total_bytes = 3;
  if (register_allocator_plan_liveness_storage(function_name, block_count,
      temp_count, &storage) != FAILED)
    return FAILED;
  return storage_is_clear(&storage) == YES ? SUCCEEDED : FAILED;
}

static int test_storage_lifecycle(void) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_cfg_edge edge;
  char *live_use;
  char *live_def;
  char *live_in;
  char *live_out;
  int iterations;
  int status;

  if (register_allocator_plan_liveness_storage("coreLivenessStorageLifecycle",
      2, 2, &storage) == FAILED)
    return FAILED;
  live_use = (char *)calloc(1, storage.buffer_bytes);
  live_def = (char *)calloc(1, storage.buffer_bytes);
  live_in = (char *)calloc(1, storage.buffer_bytes);
  live_out = (char *)calloc(1, storage.buffer_bytes);
  if (live_use == NULL || live_def == NULL || live_in == NULL || live_out == NULL) {
    free(live_out);
    free(live_in);
    free(live_def);
    free(live_use);
    return FAILED;
  }

  edge.from_block = 0;
  edge.to_block = 1;
  edge.kind = RA_CFG_EDGE_FALLTHROUGH;
  edge.target = NULL;
  live_use[2] = YES;
  live_def[1] = YES;
    status = register_allocator_solve_liveness("coreLivenessStorageLifecycle", 2,
      2, &storage, &edge, 1, live_use, live_def, live_in, live_out, &iterations);
  if (status == SUCCEEDED && (iterations != 2 ||
      live_in[0] != YES || live_out[0] != YES || live_in[2] != YES ||
      live_in[1] != NO || live_out[1] != NO))
    status = FAILED;

  free(live_out);
  free(live_in);
  free(live_def);
  free(live_use);
  return status;
}

static int expect_liveness_index(char *function_name, int block_count,
    int temp_count, int block_index, int temp_index, int expected_index) {

  struct register_allocator_liveness_storage storage;
  int set_index;

  if (register_allocator_plan_liveness_storage(function_name, block_count,
      temp_count, &storage) == FAILED)
    return FAILED;
  set_index = -2;
  if (register_allocator_resolve_liveness_index(function_name, block_count,
      temp_count, &storage, block_index, temp_index, &set_index) == FAILED)
    return FAILED;
  return set_index == expected_index ? SUCCEEDED : FAILED;
}

static int expect_invalid_liveness_index(char *function_name, int block_count,
    int temp_count, struct register_allocator_liveness_storage *storage,
    int block_index, int temp_index, int *set_index) {

  if (set_index != NULL)
    *set_index = 99;
  if (register_allocator_resolve_liveness_index(function_name, block_count,
      temp_count, storage, block_index, temp_index, set_index) != FAILED)
    return FAILED;
  if (set_index != NULL && *set_index != -1)
    return FAILED;
  return SUCCEEDED;
}

static int test_liveness_index_resolution(void) {

  struct register_allocator_liveness_storage storage;
  int set_index;

  if (expect_liveness_index("coreLivenessIndexFirst", 4, 3, 0, 0, 0) == FAILED ||
      expect_liveness_index("coreLivenessIndexMiddle", 4, 3, 2, 1, 7) == FAILED ||
      expect_liveness_index("coreLivenessIndexLast", 4, 3, 3, 2, 11) == FAILED)
    return FAILED;
  if (register_allocator_plan_liveness_storage("coreLivenessIndexInvalid", 4,
      3, &storage) == FAILED)
    return FAILED;
  if (expect_invalid_liveness_index(NULL, 4, 3, &storage, 0, 0,
      &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexBlocks", 0, 3, &storage,
      0, 0, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexTemps", 4, 0, &storage,
      0, 0, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexNullStorage", 4, 3, NULL,
      0, 0, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexNegativeBlock", 4, 3,
      &storage, -1, 0, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexBlockRange", 4, 3,
      &storage, 4, 0, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexNegativeTemp", 4, 3,
      &storage, 0, -1, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexTempRange", 4, 3,
      &storage, 0, 3, &set_index) == FAILED ||
      expect_invalid_liveness_index("coreLivenessIndexNullOutput", 4, 3,
      &storage, 0, 0, NULL) == FAILED)
    return FAILED;

  storage.set_count--;
  if (expect_invalid_liveness_index("coreLivenessIndexCells", 4, 3, &storage,
      0, 0, &set_index) == FAILED)
    return FAILED;
  storage.set_count++;
  storage.buffer_bytes--;
  if (expect_invalid_liveness_index("coreLivenessIndexBytes", 4, 3, &storage,
      0, 0, &set_index) == FAILED)
    return FAILED;
  storage.buffer_bytes++;
  storage.total_bytes--;
  if (expect_invalid_liveness_index("coreLivenessIndexTotal", 4, 3, &storage,
      0, 0, &set_index) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_solve(char *function_name,
    struct register_allocator_liveness_storage *storage,
    struct register_allocator_cfg_edge *edges, int edge_count, char *live_use,
    char *live_def, char *live_in, char *live_out, int *iterations) {

  if (iterations != NULL)
    *iterations = 99;
  if (live_in != NULL)
    live_in[0] = YES;
  if (live_out != NULL)
    live_out[0] = YES;
  if (register_allocator_solve_liveness(function_name, 2, 2, storage, edges,
      edge_count, live_use, live_def, live_in, live_out, iterations) != FAILED)
    return FAILED;
  if (iterations != NULL && *iterations != 0)
    return FAILED;
  if (live_in != NULL && live_in[0] != YES)
    return FAILED;
  if (live_out != NULL && live_out[0] != YES)
    return FAILED;
  return SUCCEEDED;
}

static int test_invalid_solve_inputs(void) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_liveness_storage invalid_storage;
  struct register_allocator_cfg_edge edge;
  char live_use[4];
  char live_def[4];
  char live_in[4];
  char live_out[4];
  int iterations;

  if (register_allocator_plan_liveness_storage("coreLivenessSolveInputs", 2,
      2, &storage) == FAILED)
    return FAILED;
  edge.from_block = 0;
  edge.to_block = 1;
  edge.kind = RA_CFG_EDGE_FALLTHROUGH;
  edge.target = NULL;

  if (expect_invalid_solve(NULL, &storage, &edge, 1, live_use, live_def,
      live_in, live_out, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNullStorage", NULL, &edge, 1,
      live_use, live_def, live_in, live_out, &iterations) == FAILED)
    return FAILED;

  invalid_storage = storage;
  invalid_storage.set_count--;
  if (expect_invalid_solve("coreLivenessSolveCells", &invalid_storage, &edge,
      1, live_use, live_def, live_in, live_out, &iterations) == FAILED)
    return FAILED;
  invalid_storage = storage;
  invalid_storage.buffer_bytes--;
  if (expect_invalid_solve("coreLivenessSolveBuffer", &invalid_storage, &edge,
      1, live_use, live_def, live_in, live_out, &iterations) == FAILED)
    return FAILED;
  invalid_storage = storage;
  invalid_storage.total_bytes--;
  if (expect_invalid_solve("coreLivenessSolveTotal", &invalid_storage, &edge,
      1, live_use, live_def, live_in, live_out, &iterations) == FAILED)
    return FAILED;
  invalid_storage.set_count = INT_MAX;
  invalid_storage.buffer_bytes = (size_t)INT_MAX * sizeof(char);
  invalid_storage.total_bytes = invalid_storage.buffer_bytes * 4;
  iterations = 99;
  live_in[0] = YES;
  live_out[0] = YES;
  if (register_allocator_solve_liveness("coreLivenessSolveIterationOverflow",
      INT_MAX, 1, &invalid_storage, &edge, 1, live_use, live_def, live_in,
      live_out, &iterations) != FAILED || iterations != 0 ||
      live_in[0] != YES || live_out[0] != YES)
    return FAILED;

  if (expect_invalid_solve("coreLivenessSolveNullUse", &storage, &edge, 1,
      NULL, live_def, live_in, live_out, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNullDef", &storage, &edge, 1,
      live_use, NULL, live_in, live_out, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNullIn", &storage, &edge, 1,
      live_use, live_def, NULL, live_out, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNullOut", &storage, &edge, 1,
      live_use, live_def, live_in, NULL, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNegativeEdges", &storage, &edge,
      -1, live_use, live_def, live_in, live_out, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNullEdges", &storage, NULL, 1,
      live_use, live_def, live_in, live_out, &iterations) == FAILED ||
      expect_invalid_solve("coreLivenessSolveNullIterations", &storage, &edge,
      1, live_use, live_def, live_in, live_out, NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct use_def_instruction {
  int active;
  int reads[3];
  int writes[3];
};

static int use_def_is_active(void *context, int instruction_index,
    int temp_index) {

  struct use_def_instruction *instructions;

  (void)temp_index;
  instructions = (struct use_def_instruction *)context;
  return instructions[instruction_index].active;
}

static int use_def_reads_temp(void *context, int instruction_index,
    int temp_index) {

  struct use_def_instruction *instructions;

  instructions = (struct use_def_instruction *)context;
  return instructions[instruction_index].reads[temp_index];
}

static int use_def_writes_temp(void *context, int instruction_index,
    int temp_index) {

  struct use_def_instruction *instructions;

  instructions = (struct use_def_instruction *)context;
  return instructions[instruction_index].writes[temp_index];
}

static int test_use_def_collection(void) {

  struct use_def_instruction instructions[5];
  char use_set[3];
  char def_set[3];
  int instruction_index;
  int temp_index;

  for (instruction_index = 0; instruction_index < 5; instruction_index++) {
    instructions[instruction_index].active = YES;
    for (temp_index = 0; temp_index < 3; temp_index++) {
      instructions[instruction_index].reads[temp_index] = NO;
      instructions[instruction_index].writes[temp_index] = NO;
    }
  }
  instructions[0].reads[0] = YES;
  instructions[0].writes[0] = YES;
  instructions[1].writes[1] = YES;
  instructions[2].reads[1] = YES;
  instructions[3].active = NO;
  instructions[3].reads[2] = YES;
  instructions[3].writes[2] = YES;
  instructions[4].reads[2] = YES;
  use_set[0] = YES;
  use_set[1] = YES;
  use_set[2] = YES;
  def_set[0] = YES;
  def_set[1] = YES;
  def_set[2] = YES;

  if (register_allocator_collect_liveness_use_def("coreLivenessUseDef", 4,
      0, 4, 5, 3, instructions, use_def_is_active, use_def_reads_temp,
      use_def_writes_temp, use_set, def_set) == FAILED)
    return FAILED;
  if (use_set[0] != YES || use_set[1] != NO || use_set[2] != YES ||
      def_set[0] != YES || def_set[1] != YES || def_set[2] != NO)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_use_def(char *function_name, int block_index,
    int start_instruction, int end_instruction, int instruction_count,
    int temp_count, void *context,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp, char *use_set,
    char *def_set) {

  if (use_set != NULL)
    use_set[0] = YES;
  if (def_set != NULL)
    def_set[0] = YES;
  if (register_allocator_collect_liveness_use_def(function_name, block_index,
      start_instruction, end_instruction, instruction_count, temp_count,
      context, is_active, reads_temp, writes_temp, use_set, def_set) != FAILED)
    return FAILED;
  if (use_set != NULL && use_set[0] != YES)
    return FAILED;
  if (def_set != NULL && def_set[0] != YES)
    return FAILED;
  return SUCCEEDED;
}

static int test_invalid_use_def_inputs(void) {

  struct use_def_instruction instruction;
  char use_set[1];
  char def_set[1];

  instruction.active = YES;
  instruction.reads[0] = NO;
  instruction.writes[0] = NO;
  if (expect_invalid_use_def(NULL, 0, 0, 0, 1, 1, &instruction,
      use_def_is_active, use_def_reads_temp, use_def_writes_temp, use_set,
      def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefBlock", -1, 0, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefStart", 0, -1, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefRange", 0, 1, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefEnd", 0, 0, 1, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefInstructions", 0, 0, 0, 0, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefTemps", 0, 0, 0, 1, 0,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefContext", 0, 0, 0, 1, 1,
      NULL, use_def_is_active, use_def_reads_temp, use_def_writes_temp, use_set,
      def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefActive", 0, 0, 0, 1, 1,
      &instruction, NULL, use_def_reads_temp, use_def_writes_temp, use_set,
      def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefReads", 0, 0, 0, 1, 1,
      &instruction, use_def_is_active, NULL, use_def_writes_temp, use_set,
      def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefWrites", 0, 0, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, NULL, use_set,
      def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefUse", 0, 0, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      NULL, def_set) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefDef", 0, 0, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, NULL) == FAILED ||
      expect_invalid_use_def("coreLivenessUseDefAliased", 0, 0, 0, 1, 1,
      &instruction, use_def_is_active, use_def_reads_temp, use_def_writes_temp,
      use_set, use_set) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_invalid_use_def_callbacks(void) {

  struct use_def_instruction instruction;
  char use_set[1];
  char def_set[1];

  instruction.active = 2;
  instruction.reads[0] = NO;
  instruction.writes[0] = NO;
  use_set[0] = YES;
  def_set[0] = YES;
  if (register_allocator_collect_liveness_use_def(
      "coreLivenessUseDefInvalidActive", 0, 0, 0, 1, 1, &instruction,
      use_def_is_active, use_def_reads_temp, use_def_writes_temp, use_set,
      def_set) != FAILED || use_set[0] != NO || def_set[0] != NO)
    return FAILED;

  instruction.active = YES;
  instruction.reads[0] = 2;
  use_set[0] = YES;
  def_set[0] = YES;
  if (register_allocator_collect_liveness_use_def(
      "coreLivenessUseDefInvalidRead", 0, 0, 0, 1, 1, &instruction,
      use_def_is_active, use_def_reads_temp, use_def_writes_temp, use_set,
      def_set) != FAILED || use_set[0] != NO || def_set[0] != NO)
    return FAILED;

  instruction.reads[0] = YES;
  instruction.writes[0] = 2;
  use_set[0] = YES;
  def_set[0] = YES;
  if (register_allocator_collect_liveness_use_def(
      "coreLivenessUseDefInvalidWrite", 0, 0, 0, 1, 1, &instruction,
      use_def_is_active, use_def_reads_temp, use_def_writes_temp, use_set,
      def_set) != FAILED || use_set[0] != NO || def_set[0] != NO)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_join_temp_state(char *function_name, int block_index,
    int predecessor_count, int temp_index,
    struct register_allocator_temp_state *state, int no_physical_register,
    int *stack_resident) {

  int original_physical_register;
  int original_spill_required;

  original_spill_required = state != NULL ? state->spill_required : NO;
  original_physical_register = state != NULL ? state->physical_register : 0;
  if (stack_resident != NULL)
    *stack_resident = YES;
  if (register_allocator_classify_join_temp_state(function_name, block_index,
      predecessor_count, temp_index, state, no_physical_register,
      stack_resident) != FAILED)
    return FAILED;
  if (stack_resident != NULL && *stack_resident != NO)
    return FAILED;
  if (state != NULL && (state->spill_required != original_spill_required ||
      state->physical_register != original_physical_register))
    return FAILED;
  return SUCCEEDED;
}

static int test_join_temp_state_classification(void) {

  struct register_allocator_temp_state state;
  int stack_resident;

  state.spill_required = YES;
  state.physical_register = -1;
  stack_resident = NO;
  if (register_allocator_classify_join_temp_state("coreJoinStateStack", 3, 2,
      0, &state, -1, &stack_resident) == FAILED || stack_resident != YES ||
      state.spill_required != YES || state.physical_register != -1)
    return FAILED;

  state.spill_required = NO;
  state.physical_register = 4;
  stack_resident = YES;
  if (register_allocator_classify_join_temp_state("coreJoinStateRetained", 3,
      2, 1, &state, -1, &stack_resident) == FAILED || stack_resident != NO ||
      state.spill_required != NO || state.physical_register != 4)
    return FAILED;

  if (expect_invalid_join_temp_state(NULL, 3, 2, 0, &state, -1,
      &stack_resident) == FAILED ||
      expect_invalid_join_temp_state("coreJoinStateBlock", -1, 2, 0, &state,
      -1, &stack_resident) == FAILED ||
      expect_invalid_join_temp_state("coreJoinStatePredecessors", 3, 1, 0,
      &state, -1, &stack_resident) == FAILED ||
      expect_invalid_join_temp_state("coreJoinStateTemp", 3, 2, -1, &state,
      -1, &stack_resident) == FAILED ||
      expect_invalid_join_temp_state("coreJoinStateNullState", 3, 2, 0, NULL,
      -1, &stack_resident) == FAILED ||
      expect_invalid_join_temp_state("coreJoinStateNullOutput", 3, 2, 0,
      &state, -1, NULL) == FAILED)
    return FAILED;

  state.spill_required = 2;
  state.physical_register = -1;
  if (expect_invalid_join_temp_state("coreJoinStateInvalidSpill", 3, 2, 0,
      &state, -1, &stack_resident) == FAILED)
    return FAILED;
  state.spill_required = YES;
  state.physical_register = 4;
  if (expect_invalid_join_temp_state("coreJoinStateSpillWithRegister", 3, 2,
      0, &state, -1, &stack_resident) == FAILED)
    return FAILED;
  state.spill_required = NO;
  state.physical_register = -1;
  if (expect_invalid_join_temp_state("coreJoinStateRetainedWithoutRegister", 3,
      2, 0, &state, -1, &stack_resident) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_join_action(char *function_name, int block_index,
    int predecessor_count, int temp_index, int stack_resident, int *action) {

  if (action != NULL)
    *action = RA_JOIN_ACTION_RELOAD_ON_DEMAND;
  if (register_allocator_plan_join_action(function_name, block_index,
      predecessor_count, temp_index, stack_resident, action) != FAILED)
    return FAILED;
  if (action != NULL && *action != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_action_planning(void) {

  int action;

  action = 0;
  if (register_allocator_plan_join_action("coreJoinActionStack", 3, 2, 0,
      YES, &action) == FAILED || action != RA_JOIN_ACTION_RELOAD_ON_DEMAND)
    return FAILED;
  action = 0;
  if (register_allocator_plan_join_action("coreJoinActionRetained", 3, 2, 1,
      NO, &action) == FAILED || action != RA_JOIN_ACTION_SPILL_PREDECESSORS)
    return FAILED;
  if (expect_invalid_join_action(NULL, 3, 2, 0, YES, &action) == FAILED ||
      expect_invalid_join_action("coreJoinActionBlock", -1, 2, 0, YES,
      &action) == FAILED ||
      expect_invalid_join_action("coreJoinActionPredecessors", 3, 1, 0, YES,
      &action) == FAILED ||
      expect_invalid_join_action("coreJoinActionTemp", 3, 2, -1, YES,
      &action) == FAILED ||
      expect_invalid_join_action("coreJoinActionStackInvalid", 3, 2, 0, 2,
      &action) == FAILED ||
      expect_invalid_join_action("coreJoinActionNullOutput", 3, 2, 0, YES,
      NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_join_predecessors(char *function_name,
    int block_count, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int *predecessors, int predecessor_capacity, int *predecessor_count) {

  if (predecessor_count != NULL)
    *predecessor_count = 99;
  if (predecessors != NULL && predecessor_capacity > 0)
    predecessors[0] = 77;
  if (register_allocator_collect_join_predecessors(function_name, block_count,
      join_block_index, edges, edge_count, predecessors, predecessor_capacity,
      predecessor_count) != FAILED)
    return FAILED;
  if (predecessor_count != NULL && *predecessor_count != 0)
    return FAILED;
  if (predecessors != NULL && predecessor_capacity > 0 &&
      predecessors[0] != 77)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_predecessor_collection(void) {

  struct register_allocator_cfg_edge edges[4];
  int predecessors[2];
  int predecessor_count;

  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[1].from_block = 0;
  edges[1].to_block = 2;
  edges[2].from_block = 2;
  edges[2].to_block = 3;
  edges[3].from_block = 1;
  edges[3].to_block = 3;
  predecessor_count = 0;
  if (register_allocator_collect_join_predecessors("coreJoinPredCount", 4, 3,
      edges, 4, NULL, 0, &predecessor_count) == FAILED ||
      predecessor_count != 2)
    return FAILED;
  predecessors[0] = -1;
  predecessors[1] = -1;
  if (register_allocator_collect_join_predecessors("coreJoinPredCollect", 4,
      3, edges, 4, predecessors, 2, &predecessor_count) == FAILED ||
      predecessor_count != 2 || predecessors[0] != 2 || predecessors[1] != 1)
    return FAILED;
  if (register_allocator_collect_join_predecessors("coreJoinPredSingle", 4,
      1, edges, 4, predecessors, 2, &predecessor_count) == FAILED ||
      predecessor_count != 1 || predecessors[0] != 0)
    return FAILED;
  if (register_allocator_collect_join_predecessors("coreJoinPredNone", 4, 0,
      edges, 4, predecessors, 2, &predecessor_count) == FAILED ||
      predecessor_count != 0)
    return FAILED;
  if (register_allocator_collect_join_predecessors("coreJoinPredNoEdges", 1,
      0, NULL, 0, NULL, 0, &predecessor_count) == FAILED ||
      predecessor_count != 0)
    return FAILED;

  if (expect_invalid_join_predecessors(NULL, 4, 3, edges, 4, NULL, 0,
      &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredBlocks", 0, 0, edges, 4,
      NULL, 0, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredBlockNegative", 4, -1,
      edges, 4, NULL, 0, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredBlockRange", 4, 4, edges,
      4, NULL, 0, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredEdgesNegative", 4, 3,
      edges, -1, NULL, 0, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredNullEdges", 4, 3, NULL, 1,
      NULL, 0, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredNullCount", 4, 3, edges,
      4, NULL, 0, NULL) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredCountCapacity", 4, 3,
      edges, 4, NULL, 1, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredCapacityNegative", 4, 3,
      edges, 4, predecessors, -1, &predecessor_count) == FAILED ||
      expect_invalid_join_predecessors("coreJoinPredCapacity", 4, 3, edges, 4,
      predecessors, 1, &predecessor_count) == FAILED)
    return FAILED;
  edges[0].from_block = 4;
  if (expect_invalid_join_predecessors("coreJoinPredFrom", 4, 3, edges, 4,
      predecessors, 2, &predecessor_count) == FAILED)
    return FAILED;
  edges[0].from_block = 0;
  edges[0].to_block = 4;
  if (expect_invalid_join_predecessors("coreJoinPredTo", 4, 3, edges, 4,
      predecessors, 2, &predecessor_count) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_spill_blocks(
    struct register_allocator_basic_block *blocks) {

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 2;
  blocks[0].end_reason = RA_BLOCK_END_JUMP;
  blocks[1].start_tac = 3;
  blocks[1].end_tac = 5;
  blocks[1].end_reason = RA_BLOCK_END_JUMP;
  blocks[2].start_tac = 6;
  blocks[2].end_tac = 8;
  blocks[2].end_reason = RA_BLOCK_END_LABEL;
  blocks[3].start_tac = 9;
  blocks[3].end_tac = 11;
  blocks[3].end_reason = RA_BLOCK_END_FUNCTION_END;
}

static int expect_invalid_join_spill_sites(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_join_spill_site *sites, int site_capacity,
    int *site_count) {

  if (site_count != NULL)
    *site_count = 99;
  if (sites != NULL && site_capacity > 0) {
    sites[0].predecessor_block = 77;
    sites[0].edge_kind = 77;
    sites[0].anchor_instruction = 77;
    sites[0].placement = 77;
  }
  if (register_allocator_plan_join_spill_sites(function_name, block_count,
      instruction_count, blocks, join_block_index, edges, edge_count, sites,
      site_capacity, site_count) != FAILED)
    return FAILED;
  if (site_count != NULL && *site_count != 0)
    return FAILED;
  if (sites != NULL && site_capacity > 0 &&
      (sites[0].predecessor_block != 77 || sites[0].edge_kind != 77 ||
      sites[0].anchor_instruction != 77 || sites[0].placement != 77))
    return FAILED;
  return SUCCEEDED;
}

static int test_join_spill_site_planning(void) {

  struct register_allocator_basic_block blocks[4];
  struct register_allocator_cfg_edge edges[3];
  struct register_allocator_join_spill_site sites[3];
  int site_count;

  initialize_join_spill_blocks(blocks);
  edges[0].from_block = 0;
  edges[0].to_block = 3;
  edges[0].kind = RA_CFG_EDGE_JUMP;
  edges[1].from_block = 1;
  edges[1].to_block = 3;
  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[2].from_block = 2;
  edges[2].to_block = 3;
  edges[2].kind = RA_CFG_EDGE_FALLTHROUGH;
  if (register_allocator_plan_join_spill_sites("coreJoinSpillMixed", 4, 12,
      blocks, 3, edges, 3, sites, 3, &site_count) == FAILED ||
      site_count != 3 || sites[0].predecessor_block != 0 ||
      sites[0].edge_kind != RA_CFG_EDGE_JUMP ||
      sites[0].anchor_instruction != 2 ||
      sites[0].placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      sites[1].predecessor_block != 1 ||
      sites[1].edge_kind != RA_CFG_EDGE_BRANCH_TRUE ||
      sites[1].anchor_instruction != 5 ||
      sites[1].placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      sites[2].predecessor_block != 2 ||
      sites[2].edge_kind != RA_CFG_EDGE_FALLTHROUGH ||
      sites[2].anchor_instruction != 8 ||
      sites[2].placement != RA_JOIN_SPILL_AFTER_ANCHOR)
    return FAILED;
  if (register_allocator_plan_join_spill_sites("coreJoinSpillCount", 4, 12,
      blocks, 3, edges, 3, NULL, 0, &site_count) == FAILED || site_count != 3)
    return FAILED;
  if (register_allocator_plan_join_spill_sites("coreJoinSpillNone", 4, 12,
      blocks, 0, edges, 3, NULL, 0, &site_count) == FAILED || site_count != 0)
    return FAILED;

  edges[0].from_block = 1;
  edges[0].to_block = 2;
  edges[0].kind = RA_CFG_EDGE_BRANCH_FALSE;
  if (register_allocator_plan_join_spill_sites("coreJoinSpillBranchFalse", 4,
      12, blocks, 2, edges, 1, sites, 3, &site_count) == FAILED ||
      site_count != 1 || sites[0].anchor_instruction != 5 ||
      sites[0].placement != RA_JOIN_SPILL_BEFORE_ANCHOR)
    return FAILED;

  edges[0].from_block = 0;
  edges[0].to_block = 3;
  edges[0].kind = RA_CFG_EDGE_JUMP;
  if (expect_invalid_join_spill_sites(NULL, 4, 12, blocks, 3, edges, 1, NULL,
      0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillBlocks", 0, 12, blocks, 0,
      edges, 1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillInstructions", 4, 0,
      blocks, 3, edges, 1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillNullBlocks", 4, 12, NULL,
      3, edges, 1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillBlockNegative", 4, 12,
      blocks, -1, edges, 1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillBlockRange", 4, 12, blocks,
      4, edges, 1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillEdgesNegative", 4, 12,
      blocks, 3, edges, -1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillNullEdges", 4, 12, blocks,
      3, NULL, 1, NULL, 0, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillNullCount", 4, 12, blocks,
      3, edges, 1, NULL, 0, NULL) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillCountCapacity", 4, 12,
      blocks, 3, edges, 1, NULL, 1, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillCapacityNegative", 4, 12,
      blocks, 3, edges, 1, sites, -1, &site_count) == FAILED ||
      expect_invalid_join_spill_sites("coreJoinSpillCapacity", 4, 12, blocks,
      3, edges, 1, sites, 0, &site_count) == FAILED)
    return FAILED;

  edges[0].from_block = 4;
  if (expect_invalid_join_spill_sites("coreJoinSpillFrom", 4, 12, blocks, 3,
      edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  edges[0].from_block = 0;
  edges[0].to_block = 4;
  if (expect_invalid_join_spill_sites("coreJoinSpillTo", 4, 12, blocks, 3,
      edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  edges[0].to_block = 3;
  blocks[0].start_tac = -1;
  if (expect_invalid_join_spill_sites("coreJoinSpillStart", 4, 12, blocks, 3,
      edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  initialize_join_spill_blocks(blocks);
  blocks[0].end_tac = 12;
  if (expect_invalid_join_spill_sites("coreJoinSpillEnd", 4, 12, blocks, 3,
      edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  initialize_join_spill_blocks(blocks);
  blocks[0].end_reason = RA_BLOCK_END_LABEL;
  if (expect_invalid_join_spill_sites("coreJoinSpillJumpReason", 4, 12, blocks,
      3, edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  initialize_join_spill_blocks(blocks);
  edges[0].from_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  if (expect_invalid_join_spill_sites("coreJoinSpillFallAdjacent", 4, 12,
      blocks, 3, edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  edges[0].from_block = 2;
  blocks[2].end_reason = RA_BLOCK_END_JUMP;
  if (expect_invalid_join_spill_sites("coreJoinSpillFallReason", 4, 12, blocks,
      3, edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  initialize_join_spill_blocks(blocks);
  edges[0].from_block = 0;
  edges[0].kind = RA_CFG_EDGE_BRANCH_FALSE;
  if (expect_invalid_join_spill_sites("coreJoinSpillFalseAdjacent", 4, 12,
      blocks, 3, edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  edges[0].kind = 99;
  if (expect_invalid_join_spill_sites("coreJoinSpillKind", 4, 12, blocks, 3,
      edges, 1, sites, 3, &site_count) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct join_spill_approval_context {
  int calls;
  int reject_call;
  int invalid_call;
};

static int approve_join_spill_site(void *context, int join_block_index,
    struct register_allocator_join_spill_site *site) {

  struct join_spill_approval_context *approval_context;

  approval_context = (struct join_spill_approval_context *)context;
  approval_context->calls++;
  if (join_block_index < 0 || site == NULL)
    return 2;
  if (approval_context->calls == approval_context->invalid_call)
    return 2;
  if (approval_context->calls == approval_context->reject_call)
    return NO;
  return YES;
}

static void initialize_join_spill_sites(
    struct register_allocator_join_spill_site *sites) {

  sites[0].predecessor_block = 1;
  sites[0].edge_kind = RA_CFG_EDGE_JUMP;
  sites[0].anchor_instruction = 5;
  sites[0].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  sites[1].predecessor_block = 2;
  sites[1].edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  sites[1].anchor_instruction = 8;
  sites[1].placement = RA_JOIN_SPILL_AFTER_ANCHOR;
}

static int join_spill_site_equals(
    struct register_allocator_join_spill_site *left,
    struct register_allocator_join_spill_site *right) {

  return left->predecessor_block == right->predecessor_block &&
      left->edge_kind == right->edge_kind &&
      left->anchor_instruction == right->anchor_instruction &&
      left->placement == right->placement;
}

static int expect_invalid_join_spill_order(char *function_name,
    int join_block_index, int instruction_count,
    struct register_allocator_join_spill_site *sites, int site_count) {

  struct register_allocator_join_spill_site originals[3];
  int site_index;

  if (sites != NULL && site_count > 0) {
    for (site_index = 0; site_index < site_count && site_index < 3;
        site_index++)
      originals[site_index] = sites[site_index];
  }
  if (register_allocator_order_join_spill_sites(function_name,
      join_block_index, instruction_count, sites, site_count) != FAILED)
    return FAILED;
  if (sites != NULL && site_count > 0) {
    for (site_index = 0; site_index < site_count && site_index < 3;
        site_index++) {
      if (join_spill_site_equals(&sites[site_index],
          &originals[site_index]) == NO)
        return FAILED;
    }
  }
  return SUCCEEDED;
}

static int test_join_spill_site_ordering(void) {

  struct register_allocator_join_spill_site sites[4];

  initialize_join_spill_sites(sites);
  sites[2].predecessor_block = 0;
  sites[2].edge_kind = RA_CFG_EDGE_BRANCH_TRUE;
  sites[2].anchor_instruction = 2;
  sites[2].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  if (register_allocator_order_join_spill_sites("coreJoinOrderMixed", 3, 12,
      sites, 3) == FAILED || sites[0].predecessor_block != 2 ||
      sites[0].anchor_instruction != 8 ||
      sites[0].placement != RA_JOIN_SPILL_AFTER_ANCHOR ||
      sites[1].predecessor_block != 1 ||
      sites[1].anchor_instruction != 5 ||
      sites[2].predecessor_block != 0 ||
      sites[2].anchor_instruction != 2)
    return FAILED;
  if (register_allocator_order_join_spill_sites("coreJoinOrderSorted", 3,
      12, sites, 3) == FAILED || sites[0].anchor_instruction != 8 ||
      sites[1].anchor_instruction != 5 || sites[2].anchor_instruction != 2)
    return FAILED;

  sites[0].predecessor_block = 1;
  sites[0].edge_kind = RA_CFG_EDGE_JUMP;
  sites[0].anchor_instruction = 5;
  sites[0].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  sites[1].predecessor_block = 2;
  sites[1].edge_kind = RA_CFG_EDGE_BRANCH_TRUE;
    sites[1].anchor_instruction = 2;
  sites[1].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
    sites[2].predecessor_block = 2;
    sites[2].edge_kind = RA_CFG_EDGE_BRANCH_TRUE;
    sites[2].anchor_instruction = 5;
    sites[2].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
    sites[3].predecessor_block = 3;
    sites[3].edge_kind = RA_CFG_EDGE_JUMP;
    sites[3].anchor_instruction = 8;
    sites[3].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  if (register_allocator_order_join_spill_sites("coreJoinOrderStable", 3,
      12, sites, 4) == FAILED || sites[0].predecessor_block != 3 ||
      sites[0].anchor_instruction != 8 || sites[1].predecessor_block != 1 ||
      sites[1].anchor_instruction != 5 || sites[2].predecessor_block != 2 ||
      sites[2].anchor_instruction != 5 || sites[3].anchor_instruction != 2)
    return FAILED;
  if (register_allocator_order_join_spill_sites("coreJoinOrderEmpty", 0, 12,
      NULL, 0) == FAILED)
    return FAILED;

  initialize_join_spill_sites(sites);
  if (expect_invalid_join_spill_order(NULL, 3, 12, sites, 2) == FAILED ||
      expect_invalid_join_spill_order("coreJoinOrderBlock", -1, 12, sites,
      2) == FAILED ||
      expect_invalid_join_spill_order("coreJoinOrderInstructions", 3, 0,
      sites, 2) == FAILED ||
      expect_invalid_join_spill_order("coreJoinOrderCount", 3, 12, sites,
      -1) == FAILED ||
      expect_invalid_join_spill_order("coreJoinOrderNull", 3, 12, NULL,
      2) == FAILED)
    return FAILED;

  sites[0].predecessor_block = -1;
  if (expect_invalid_join_spill_order("coreJoinOrderPred", 3, 12, sites,
      2) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].anchor_instruction = -1;
  if (expect_invalid_join_spill_order("coreJoinOrderAnchor", 3, 12, sites,
      2) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].anchor_instruction = 12;
  if (expect_invalid_join_spill_order("coreJoinOrderAnchorRange", 3, 12,
      sites, 2) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].edge_kind = 99;
  if (expect_invalid_join_spill_order("coreJoinOrderEdge", 3, 12, sites,
      2) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].placement = 99;
  if (expect_invalid_join_spill_order("coreJoinOrderPlacement", 3, 12, sites,
      2) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  if (expect_invalid_join_spill_order("coreJoinOrderFallthrough", 3, 12,
      sites, 2) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[1].edge_kind = RA_CFG_EDGE_JUMP;
  if (expect_invalid_join_spill_order("coreJoinOrderJump", 3, 12, sites,
      2) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_ordered_join_spill_emission(
    struct register_allocator_join_spill_emission *emission,
    int predecessor_block, int edge_kind, int anchor_instruction,
    int placement, int temp_index) {

  emission->emit = YES;
  emission->site.predecessor_block = predecessor_block;
  emission->site.edge_kind = edge_kind;
  emission->site.anchor_instruction = anchor_instruction;
  emission->site.placement = placement;
  emission->temp_index = temp_index;
  emission->physical_register = temp_index + 1;
  emission->destination_offset = -2 - temp_index * 2;
  emission->byte_count = 2;
}

static int join_spill_work_item_equals(
    struct register_allocator_join_spill_work_item *left,
    struct register_allocator_join_spill_work_item *right) {

  return left->join_block_index == right->join_block_index &&
      left->emission.emit == right->emission.emit &&
      join_spill_site_equals(&left->emission.site,
      &right->emission.site) == YES &&
      left->emission.temp_index == right->emission.temp_index &&
      left->emission.physical_register == right->emission.physical_register &&
      left->emission.destination_offset == right->emission.destination_offset &&
      left->emission.byte_count == right->emission.byte_count;
}

static int expect_invalid_join_spill_emission_order(char *function_name,
    int block_count, int instruction_count, int no_physical_register,
    struct register_allocator_join_spill_work_item *work_items,
    int work_item_count) {

  struct register_allocator_join_spill_work_item originals[4];
  int work_item_index;

  if (work_items != NULL && work_item_count > 0) {
    for (work_item_index = 0; work_item_index < work_item_count &&
        work_item_index < 4; work_item_index++)
      originals[work_item_index] = work_items[work_item_index];
  }
  if (register_allocator_order_join_spill_emissions(function_name,
      block_count, instruction_count, no_physical_register, work_items,
      work_item_count) != FAILED)
    return FAILED;
  if (work_items != NULL && work_item_count > 0) {
    for (work_item_index = 0; work_item_index < work_item_count &&
        work_item_index < 4; work_item_index++) {
      if (join_spill_work_item_equals(&work_items[work_item_index],
          &originals[work_item_index]) == NO)
        return FAILED;
    }
  }
  return SUCCEEDED;
}

static int test_join_spill_emission_ordering(void) {

  struct register_allocator_join_spill_work_item work_items[5];

  work_items[0].join_block_index = 4;
  work_items[1].join_block_index = 3;
  work_items[2].join_block_index = 5;
  work_items[3].join_block_index = 5;
  work_items[4].join_block_index = 4;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  initialize_ordered_join_spill_emission(&work_items[1].emission, 2,
      RA_CFG_EDGE_BRANCH_TRUE, 2, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  initialize_ordered_join_spill_emission(&work_items[2].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, 1);
  initialize_ordered_join_spill_emission(&work_items[3].emission, 3,
      RA_CFG_EDGE_FALLTHROUGH, 8, RA_JOIN_SPILL_AFTER_ANCHOR, 2);
  initialize_ordered_join_spill_emission(&work_items[4].emission, 4,
      RA_CFG_EDGE_FALLTHROUGH, 5, RA_JOIN_SPILL_AFTER_ANCHOR, 3);
  if (register_allocator_order_join_spill_emissions(
      "coreJoinEmissionOrderMixed", 6, 12, -1, work_items, 5) == FAILED ||
      work_items[0].emission.site.anchor_instruction != 8 ||
      work_items[0].join_block_index != 5 ||
      work_items[1].emission.site.placement !=
      RA_JOIN_SPILL_AFTER_ANCHOR || work_items[1].emission.temp_index != 3 ||
      work_items[1].join_block_index != 4 ||
      work_items[2].emission.temp_index != 0 ||
      work_items[2].join_block_index != 4 ||
      work_items[3].emission.temp_index != 1 ||
      work_items[3].join_block_index != 5 ||
      work_items[4].emission.site.anchor_instruction != 2 ||
      work_items[4].join_block_index != 3)
    return FAILED;
  if (register_allocator_order_join_spill_emissions(
      "coreJoinEmissionOrderSorted", 6, 12, -1, work_items, 5) == FAILED ||
      work_items[0].emission.site.anchor_instruction != 8 ||
      work_items[1].emission.temp_index != 3 ||
      work_items[2].emission.temp_index != 0 ||
      work_items[3].emission.temp_index != 1 ||
      work_items[4].emission.site.anchor_instruction != 2)
    return FAILED;
  if (register_allocator_order_join_spill_emissions(
      "coreJoinEmissionOrderEmpty", 6, 12, -1, NULL, 0) == FAILED)
    return FAILED;

  work_items[0].join_block_index = 4;
  work_items[1].join_block_index = 5;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  initialize_ordered_join_spill_emission(&work_items[1].emission, 2,
      RA_CFG_EDGE_FALLTHROUGH, 8, RA_JOIN_SPILL_AFTER_ANCHOR, 1);
  if (expect_invalid_join_spill_emission_order(NULL, 6, 12, -1,
      work_items, 2) == FAILED || expect_invalid_join_spill_emission_order(
      "coreJoinEmissionOrderBlocks", 0, 12, -1, work_items, 2) == FAILED ||
      expect_invalid_join_spill_emission_order(
      "coreJoinEmissionOrderInstructions", 6, 0, -1, work_items, 2) ==
      FAILED || expect_invalid_join_spill_emission_order(
      "coreJoinEmissionOrderCount", 6, 12, -1, work_items, -1) == FAILED ||
      expect_invalid_join_spill_emission_order(
      "coreJoinEmissionOrderNull", 6, 12, -1, NULL, 2) == FAILED)
    return FAILED;

  work_items[0].join_block_index = 6;
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderBlock",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  work_items[0].join_block_index = 4;
  work_items[0].emission.emit = NO;
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderEmit",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, -1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderPred",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 12, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderAnchor",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1, 99, 5,
      RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderEdge",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_AFTER_ANCHOR, 0);
  if (expect_invalid_join_spill_emission_order(
      "coreJoinEmissionOrderPlacement", 6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, -1);
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderTemp",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  work_items[0].emission.physical_register = -1;
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderPhy",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  initialize_ordered_join_spill_emission(&work_items[0].emission, 1,
      RA_CFG_EDGE_JUMP, 5, RA_JOIN_SPILL_BEFORE_ANCHOR, 0);
  work_items[0].emission.byte_count = 0;
  if (expect_invalid_join_spill_emission_order("coreJoinEmissionOrderBytes",
      6, 12, -1, work_items, 2) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_post_mutation_rebuild(char *function_name,
    int original_instruction_count, int current_instruction_count,
  int inserted_instruction_count, int rewritten_operand_count,
  int added_temp_count, int *rebuild_required) {

  if (rebuild_required != NULL)
    *rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(function_name,
      original_instruction_count, current_instruction_count,
      inserted_instruction_count, rewritten_operand_count, added_temp_count,
      rebuild_required) != FAILED)
    return FAILED;
  if (rebuild_required != NULL && *rebuild_required != NO)
    return FAILED;
  return SUCCEEDED;
}

static int test_post_mutation_rebuild_planning(void) {

  int rebuild_required;

  rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(
      "corePostMutationUnchanged", 12, 12, 0, 0, 0,
      &rebuild_required) == FAILED ||
      rebuild_required != NO)
    return FAILED;
  rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(
      "corePostMutationInserted", 12, 15, 3, 0, 0,
      &rebuild_required) == FAILED ||
      rebuild_required != YES)
    return FAILED;
  rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(
      "corePostMutationRewritten", 12, 12, 0, 5, 0,
      &rebuild_required) == FAILED || rebuild_required != YES)
    return FAILED;
  rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(
      "corePostMutationAddedTemp", 12, 12, 0, 0, 1,
      &rebuild_required) == FAILED || rebuild_required != YES)
    return FAILED;
  rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(
      "corePostMutationPromotion", 12, 12, 0, 5, 1,
      &rebuild_required) == FAILED || rebuild_required != YES)
    return FAILED;
  rebuild_required = 99;
  if (register_allocator_plan_post_mutation_rebuild(
      "corePostMutationMaximum", INT_MAX - 1, INT_MAX, 1, 0, 0,
      &rebuild_required) == FAILED || rebuild_required != YES)
    return FAILED;

  if (expect_invalid_post_mutation_rebuild(NULL, 12, 12, 0, 0, 0,
      &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationOriginalZero",
      0, 1, 1, 0, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationCurrentZero",
      1, 0, 0, 0, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationNegative",
      12, 12, -1, 0, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationNegativeRewrite",
      12, 12, 0, -1, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationNegativeTemp",
      12, 12, 0, 0, -1, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationShrink",
      12, 11, 0, 0, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationMismatch",
      12, 14, 3, 0, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationOverflow",
      INT_MAX, INT_MAX, 1, 0, 0, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationTotalOverflow",
      12, 12, 0, INT_MAX, 1, &rebuild_required) == FAILED ||
      expect_invalid_post_mutation_rebuild("corePostMutationNullOutput",
      12, 12, 0, 0, 0, NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_fixed_point_step(char *function_name, int pass,
    int pass_limit, int rebuild_required,
    struct register_allocator_fixed_point_step *step) {

  if (step != NULL) {
    step->status = 99;
    step->pass = 99;
    step->pass_limit = 99;
  }
  if (register_allocator_plan_fixed_point_step(function_name, pass,
      pass_limit, rebuild_required, step) != FAILED)
    return FAILED;
  if (step != NULL && (step->status != 0 || step->pass != 0 ||
      step->pass_limit != 0))
    return FAILED;
  return SUCCEEDED;
}

static int test_fixed_point_planning(void) {

  struct register_allocator_fixed_point_step step;
  int pass_limit;

  pass_limit = 99;
  if (register_allocator_plan_fixed_point_limit("coreFixedPointLimit", 12,
      &pass_limit) == FAILED || pass_limit != 13)
    return FAILED;
  if (register_allocator_plan_fixed_point_limit(NULL, 12,
      &pass_limit) != FAILED || pass_limit != 0 ||
      register_allocator_plan_fixed_point_limit("coreFixedPointZero", 0,
      &pass_limit) != FAILED || pass_limit != 0 ||
      register_allocator_plan_fixed_point_limit("coreFixedPointOverflow",
      INT_MAX, &pass_limit) != FAILED || pass_limit != 0 ||
      register_allocator_plan_fixed_point_limit("coreFixedPointOutput", 12,
      NULL) != FAILED)
    return FAILED;

  if (register_allocator_plan_fixed_point_step("coreFixedPointRebuild", 1,
      13, YES, &step) == FAILED ||
      step.status != RA_FIXED_POINT_STEP_REBUILD || step.pass != 1 ||
      step.pass_limit != 13)
    return FAILED;
  if (register_allocator_plan_fixed_point_step("coreFixedPointStable", 7,
      13, NO, &step) == FAILED ||
      step.status != RA_FIXED_POINT_STEP_STABLE || step.pass != 7 ||
      step.pass_limit != 13)
    return FAILED;
  if (register_allocator_plan_fixed_point_step("coreFixedPointStableAtLimit",
      13, 13, NO, &step) == FAILED ||
      step.status != RA_FIXED_POINT_STEP_STABLE)
    return FAILED;
  if (register_allocator_plan_fixed_point_step("coreFixedPointLimitReached",
      13, 13, YES, &step) == FAILED ||
      step.status != RA_FIXED_POINT_STEP_ITERATION_LIMIT)
    return FAILED;

  if (expect_invalid_fixed_point_step(NULL, 1, 13, YES, &step) == FAILED ||
      expect_invalid_fixed_point_step("coreFixedPointPass", 0, 13, YES,
      &step) == FAILED ||
      expect_invalid_fixed_point_step("coreFixedPointLimitZero", 1, 0, YES,
      &step) == FAILED ||
      expect_invalid_fixed_point_step("coreFixedPointPastLimit", 14, 13, YES,
      &step) == FAILED ||
      expect_invalid_fixed_point_step("coreFixedPointRebuildValue", 1, 13, 2,
      &step) == FAILED ||
      expect_invalid_fixed_point_step("coreFixedPointStepOutput", 1, 13, YES,
      NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_retention_paths(
    struct register_allocator_join_retention_path *paths) {

  paths[0].predecessor_block = 1;
  paths[0].definition_instruction = 3;
  paths[0].anchor_instruction = 5;
  paths[0].physical_register = 4;
  paths[0].definition_found = YES;
  paths[0].definition_supported = YES;
  paths[0].path_transparent = YES;
  paths[1].predecessor_block = 2;
  paths[1].definition_instruction = 7;
  paths[1].anchor_instruction = 8;
  paths[1].physical_register = 4;
  paths[1].definition_found = YES;
  paths[1].definition_supported = YES;
  paths[1].path_transparent = YES;
}

static int expect_invalid_join_retention(char *function_name,
    int block_count, int instruction_count, int join_block_index,
    int temp_index, struct register_allocator_join_retention_path *paths,
    int path_count, int consumer_supported, int *selected_physical_register) {

  struct register_allocator_join_retention_path originals[2];

  if (paths != NULL && path_count == 2)
    memcpy(originals, paths, sizeof(originals));
  if (selected_physical_register != NULL)
    *selected_physical_register = 99;
  if (register_allocator_plan_join_retention(function_name, block_count,
      instruction_count, join_block_index, temp_index, -1, paths, path_count,
      consumer_supported, selected_physical_register) != FAILED)
    return FAILED;
  if (selected_physical_register != NULL &&
      *selected_physical_register != -1)
    return FAILED;
  if (paths != NULL && path_count == 2 &&
      memcmp(originals, paths, sizeof(originals)) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_retention_planning(void) {

  struct register_allocator_join_retention_path paths[2];
  int selected_physical_register;

  initialize_join_retention_paths(paths);
  selected_physical_register = 99;
  if (register_allocator_plan_join_retention("coreJoinRetentionA", 4, 12,
      3, 2, -1, paths, 2, YES, &selected_physical_register) == FAILED ||
      selected_physical_register != 4)
    return FAILED;
  paths[0].physical_register = 2;
  paths[1].physical_register = 2;
  if (register_allocator_plan_join_retention("coreJoinRetentionHL", 4, 12,
      3, 1, -1, paths, 2, YES, &selected_physical_register) == FAILED ||
      selected_physical_register != 2)
    return FAILED;

  initialize_join_retention_paths(paths);
  paths[0].definition_supported = NO;
  paths[1].definition_instruction = -1;
  paths[1].definition_found = NO;
  paths[1].definition_supported = NO;
  paths[1].path_transparent = NO;
  if (register_allocator_plan_join_retention("coreJoinRetentionMissing", 4,
      12, 3, 2, -1, paths, 2, YES,
      &selected_physical_register) == FAILED ||
      selected_physical_register != -1)
    return FAILED;
  initialize_join_retention_paths(paths);
  if (register_allocator_plan_join_retention(
      "coreJoinRetentionConsumer", 4, 12, 3, 2, -1, paths, 2, NO,
      &selected_physical_register) == FAILED ||
      selected_physical_register != -1)
    return FAILED;
  paths[0].definition_supported = NO;
  if (register_allocator_plan_join_retention(
      "coreJoinRetentionDefinition", 4, 12, 3, 2, -1, paths, 2, YES,
      &selected_physical_register) == FAILED ||
      selected_physical_register != -1)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[1].path_transparent = NO;
  if (register_allocator_plan_join_retention("coreJoinRetentionClobber", 4,
      12, 3, 2, -1, paths, 2, YES,
      &selected_physical_register) == FAILED ||
      selected_physical_register != -1)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[1].physical_register = 5;
  if (register_allocator_plan_join_retention("coreJoinRetentionMismatch", 4,
      12, 3, 2, -1, paths, 2, YES,
      &selected_physical_register) == FAILED ||
      selected_physical_register != -1)
    return FAILED;

  initialize_join_retention_paths(paths);
  if (expect_invalid_join_retention(NULL, 4, 12, 3, 2, paths, 2, YES,
      &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionBlocks", 0, 12, 3, 2,
      paths, 2, YES, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionInstructions", 4, 0,
      3, 2, paths, 2, YES, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionJoin", 4, 12, 4, 2,
      paths, 2, YES, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionTemp", 4, 12, 3, -1,
      paths, 2, YES, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionNullPaths", 4, 12, 3,
      2, NULL, 2, YES, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionCount", 4, 12, 3, 2,
      paths, 1, YES, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionConsumerValue", 4, 12,
      3, 2, paths, 2, 9, &selected_physical_register) == FAILED ||
      expect_invalid_join_retention("coreJoinRetentionNullOutput", 4, 12, 3,
      2, paths, 2, YES, NULL) == FAILED)
    return FAILED;

  paths[0].predecessor_block = -1;
  if (expect_invalid_join_retention("coreJoinRetentionPredecessor", 4, 12, 3,
      2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].predecessor_block = 3;
  if (expect_invalid_join_retention("coreJoinRetentionSelf", 4, 12, 3, 2,
      paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[1].predecessor_block = 1;
  if (expect_invalid_join_retention("coreJoinRetentionDuplicate", 4, 12, 3,
      2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].definition_instruction = -1;
  paths[0].definition_found = YES;
  if (expect_invalid_join_retention("coreJoinRetentionDefinitionIndex", 4,
      12, 3, 2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].anchor_instruction = 2;
  if (expect_invalid_join_retention("coreJoinRetentionAnchorOrder", 4, 12, 3,
      2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].anchor_instruction = 12;
  if (expect_invalid_join_retention("coreJoinRetentionAnchorRange", 4, 12, 3,
      2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].physical_register = -1;
  if (expect_invalid_join_retention("coreJoinRetentionPhysical", 4, 12, 3, 2,
      paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].definition_found = 9;
  if (expect_invalid_join_retention("coreJoinRetentionDefinitionFound", 4,
      12, 3, 2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].definition_supported = 9;
  if (expect_invalid_join_retention("coreJoinRetentionDefinitionValue", 4,
      12, 3, 2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].path_transparent = 9;
  if (expect_invalid_join_retention("coreJoinRetentionPathValue", 4, 12, 3,
      2, paths, 2, YES, &selected_physical_register) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_schedule_outputs(
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule) {

  int assignment_index;

  for (assignment_index = 0; assignment_index < 3; assignment_index++) {
    assignments[assignment_index].role = 99;
    assignments[assignment_index].instruction = 99;
    assignments[assignment_index].operand = 99;
    assignments[assignment_index].physical_register = 99;
  }
  schedule->status = 99;
  schedule->assignment_count = 99;
  schedule->conflict_instruction = 99;
}

static int test_join_schedule_planning(void) {

  struct register_allocator_join_retention_path paths[2];
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_assignment original_assignments[3];
  struct register_allocator_join_schedule schedule;
  int producer_physical_registers[2];

  initialize_join_retention_paths(paths);
  producer_physical_registers[0] = -1;
  producer_physical_registers[1] = -1;
  initialize_join_schedule_outputs(assignments, &schedule);
  if (register_allocator_plan_join_schedule("coreJoinScheduleDistinct", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_READY ||
      schedule.assignment_count != 3 || schedule.conflict_instruction != -1 ||
      assignments[0].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[0].instruction != 3 || assignments[0].operand != 0 ||
      assignments[0].physical_register != 4 ||
      assignments[1].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[1].instruction != 7 || assignments[1].operand != 0 ||
      assignments[1].physical_register != 4 ||
      assignments[2].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      assignments[2].instruction != 10 || assignments[2].operand != 1 ||
      assignments[2].physical_register != 4)
    return FAILED;

  initialize_join_schedule_outputs(assignments, &schedule);
  if (register_allocator_plan_join_schedule("coreJoinScheduleResult", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 0, -1, 4,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_READY ||
      schedule.assignment_count != 3 ||
      assignments[2].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      assignments[2].instruction != 10 || assignments[2].operand != 0 ||
      assignments[2].physical_register != 4)
    return FAILED;

  paths[1].definition_instruction = paths[0].definition_instruction;
  producer_physical_registers[0] = 4;
  producer_physical_registers[1] = -1;
  initialize_join_schedule_outputs(assignments, &schedule);
  if (register_allocator_plan_join_schedule("coreJoinScheduleCommon", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 2, 4, 4,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_READY ||
      schedule.assignment_count != 2 ||
      assignments[0].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[0].instruction != 3 ||
      assignments[1].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      assignments[1].instruction != 10 || assignments[1].operand != 2)
    return FAILED;

  initialize_join_retention_paths(paths);
  producer_physical_registers[0] = 5;
  producer_physical_registers[1] = 2;
  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  if (register_allocator_plan_join_schedule("coreJoinScheduleProducerConflict",
      12, 2, -1, paths, 2, producer_physical_registers, 10, 1, 3, 4,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_CONFLICT ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != 3 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;

  producer_physical_registers[0] = -1;
  producer_physical_registers[1] = 4;
  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  if (register_allocator_plan_join_schedule("coreJoinScheduleConsumerConflict",
      12, 2, -1, paths, 2, producer_physical_registers, 10, 1, 3, 4,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_CONFLICT ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != 10 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;

  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  if (register_allocator_plan_join_schedule("coreJoinScheduleIneligible", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 1, -1, -1,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_INELIGIBLE ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != -1 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;
  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  if (register_allocator_plan_join_schedule(
      "coreJoinScheduleIneligibleNoConsumer", 12, 2, -1, paths, 2,
      producer_physical_registers, -1, 0, -1, -1, assignments, 3,
      &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_INELIGIBLE ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != -1 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;

  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  producer_physical_registers[0] = -1;
  producer_physical_registers[1] = -1;
  if (register_allocator_plan_join_schedule("coreJoinScheduleCapacity", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 2, &schedule) != FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_INELIGIBLE ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != -1 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;

  initialize_join_schedule_outputs(assignments, &schedule);
  if (register_allocator_plan_join_schedule(NULL, 12, 2, -1, paths, 2,
      producer_physical_registers, 10, 1, -1, 4, assignments, 3,
      &schedule) != FAILED || schedule.assignment_count != 0 ||
      register_allocator_plan_join_schedule("coreJoinScheduleInstructions", 0,
      2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleTemp", 12, -1,
      -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinSchedulePaths", 12, 2,
      -1, NULL, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinSchedulePathCount", 12,
      2, -1, paths, 1, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleRegisters", 12,
      2, -1, paths, 2, NULL, 10, 1, -1, 4, assignments, 3,
      &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleConsumer", 12,
      2, -1, paths, 2, producer_physical_registers, 12, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleOperand", 12, 2,
      -1, paths, 2, producer_physical_registers, 10, 3, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleCapacityValue",
      12, 2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, -1, &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleAssignments", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4, NULL, 3,
      &schedule) != FAILED ||
      register_allocator_plan_join_schedule("coreJoinScheduleOutput", 12, 2,
      -1, paths, 2, producer_physical_registers, 10, 1, -1, 4, assignments,
      3, NULL) != FAILED)
    return FAILED;

  initialize_join_retention_paths(paths);
  paths[0].definition_supported = NO;
  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  if (register_allocator_plan_join_schedule("coreJoinScheduleInvalidPath", 12,
      2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      schedule.assignment_count != 0 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;
  initialize_join_retention_paths(paths);
  paths[0].definition_instruction = 10;
  initialize_join_schedule_outputs(assignments, &schedule);
  memcpy(original_assignments, assignments, sizeof(assignments));
  if (register_allocator_plan_join_schedule("coreJoinScheduleProducerOrder",
      12, 2, -1, paths, 2, producer_physical_registers, 10, 1, -1, 4,
      assignments, 3, &schedule) != FAILED ||
      schedule.assignment_count != 0 ||
      memcmp(original_assignments, assignments, sizeof(assignments)) != 0)
    return FAILED;
  return SUCCEEDED;
}

struct join_assignment_application_context {
  int calls;
  int fail;
  int values[12];
};

static int apply_join_assignment_transaction(void *context, int temp_index,
    struct register_allocator_join_assignment *assignments,
    int assignment_count) {

  struct join_assignment_application_context *application_context;
  int assignment_index;

  application_context =
      (struct join_assignment_application_context *)context;
  application_context->calls++;
  if (temp_index != 2 || assignment_count != 3)
    return FAILED;
  for (assignment_index = 0; assignment_index < assignment_count;
      assignment_index++) {
    if (assignments[assignment_index].instruction < 0 ||
        assignments[assignment_index].instruction >= 12 ||
        application_context->values[
        assignments[assignment_index].instruction] != -1)
      return FAILED;
  }
  if (application_context->fail == YES)
    return FAILED;
  for (assignment_index = 0; assignment_index < assignment_count;
      assignment_index++)
    application_context->values[
        assignments[assignment_index].instruction] =
        assignments[assignment_index].physical_register;
  return SUCCEEDED;
}

static void initialize_join_assignment_application(
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule,
    struct join_assignment_application_context *context,
    struct register_allocator_join_assignment_application *application) {

  int instruction_index;

  assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[0].instruction = 3;
  assignments[0].operand = 0;
  assignments[0].physical_register = 4;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].instruction = 7;
  assignments[1].operand = 0;
  assignments[1].physical_register = 4;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[2].instruction = 10;
  assignments[2].operand = 1;
  assignments[2].physical_register = 4;
  schedule->status = RA_JOIN_SCHEDULE_READY;
  schedule->assignment_count = 3;
  schedule->conflict_instruction = -1;
  context->calls = 0;
  context->fail = NO;
  for (instruction_index = 0; instruction_index < 12; instruction_index++)
    context->values[instruction_index] = -1;
  application->status = 99;
  application->applied_count = 99;
}

static int expect_invalid_join_assignment_application(char *function_name,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity, struct register_allocator_join_schedule *schedule,
    struct join_assignment_application_context *context,
    struct register_allocator_join_assignment_application *application) {

  int original_values[12];

  memcpy(original_values, context->values, sizeof(original_values));
  if (register_allocator_apply_join_assignments(function_name, 12, 2, -1,
      assignments, assignment_capacity, schedule, context,
      apply_join_assignment_transaction, application) != FAILED ||
      application->status != RA_JOIN_ASSIGNMENT_APPLICATION_INELIGIBLE ||
      application->applied_count != 0 || context->calls != 0 ||
      memcmp(original_values, context->values, sizeof(original_values)) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_assignment_application(void) {

  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;
  struct join_assignment_application_context context;
  struct register_allocator_join_assignment_application application;
  int original_values[12];

  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  if (register_allocator_apply_join_assignments("coreJoinAssignmentApply", 12,
      2, -1, assignments, 3, &schedule, &context,
      apply_join_assignment_transaction, &application) == FAILED ||
      application.status != RA_JOIN_ASSIGNMENT_APPLICATION_APPLIED ||
      application.applied_count != 3 || context.calls != 1 ||
      context.values[3] != 4 || context.values[7] != 4 ||
      context.values[10] != 4)
    return FAILED;

  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  context.fail = YES;
  memcpy(original_values, context.values, sizeof(original_values));
  if (register_allocator_apply_join_assignments("coreJoinAssignmentFailure",
      12, 2, -1, assignments, 3, &schedule, &context,
      apply_join_assignment_transaction, &application) != FAILED ||
      application.status != RA_JOIN_ASSIGNMENT_APPLICATION_FAILED ||
      application.applied_count != 0 || context.calls != 1 ||
      memcmp(original_values, context.values, sizeof(original_values)) != 0)
    return FAILED;

  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  schedule.assignment_count = 0;
  if (register_allocator_apply_join_assignments(
      "coreJoinAssignmentIneligible", 12, 2, -1, NULL, 0, &schedule,
      &context, NULL, &application) == FAILED ||
      application.status != RA_JOIN_ASSIGNMENT_APPLICATION_INELIGIBLE ||
      application.applied_count != 0 || context.calls != 0)
    return FAILED;

  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  if (register_allocator_apply_join_assignments(NULL, 12, 2, -1,
      assignments, 3, &schedule, &context,
      apply_join_assignment_transaction, &application) != FAILED ||
      application.status != RA_JOIN_ASSIGNMENT_APPLICATION_INELIGIBLE ||
      register_allocator_apply_join_assignments("coreJoinAssignmentCount", 0,
      2, -1, assignments, 3, &schedule, &context,
      apply_join_assignment_transaction, &application) != FAILED ||
      register_allocator_apply_join_assignments("coreJoinAssignmentTemp", 12,
      -1, -1, assignments, 3, &schedule, &context,
      apply_join_assignment_transaction, &application) != FAILED ||
      register_allocator_apply_join_assignments(
      "coreJoinAssignmentCapacity", 12, 2, -1, assignments, -1, &schedule,
      &context, apply_join_assignment_transaction, &application) != FAILED ||
      register_allocator_apply_join_assignments("coreJoinAssignmentSchedule",
      12, 2, -1, assignments, 3, NULL, &context,
      apply_join_assignment_transaction, &application) != FAILED ||
      register_allocator_apply_join_assignments("coreJoinAssignmentOutput",
      12, 2, -1, assignments, 3, &schedule, &context,
      apply_join_assignment_transaction, NULL) != FAILED || context.calls != 0)
    return FAILED;

  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  schedule.assignment_count = 4;
  if (expect_invalid_join_assignment_application(
      "coreJoinAssignmentPlanCapacity", assignments, 3, &schedule, &context,
      &application) == FAILED)
    return FAILED;
  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  assignments[0].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  if (expect_invalid_join_assignment_application(
      "coreJoinAssignmentProducerRole", assignments, 3, &schedule, &context,
      &application) == FAILED)
    return FAILED;
  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  assignments[2].operand = 0;
  if (register_allocator_apply_join_assignments(
      "coreJoinAssignmentConsumerResult", 12, 2, -1, assignments, 3,
      &schedule, &context, apply_join_assignment_transaction,
      &application) == FAILED ||
      application.status != RA_JOIN_ASSIGNMENT_APPLICATION_APPLIED ||
      application.applied_count != 3)
    return FAILED;
  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  assignments[2].operand = 3;
  if (expect_invalid_join_assignment_application(
      "coreJoinAssignmentConsumerOperand", assignments, 3, &schedule,
      &context, &application) == FAILED)
    return FAILED;
  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  assignments[1].physical_register = 5;
  if (expect_invalid_join_assignment_application(
      "coreJoinAssignmentRegister", assignments, 3, &schedule, &context,
      &application) == FAILED)
    return FAILED;
  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  assignments[1].instruction = 12;
  if (expect_invalid_join_assignment_application(
      "coreJoinAssignmentInstruction", assignments, 3, &schedule, &context,
      &application) == FAILED)
    return FAILED;
  initialize_join_assignment_application(assignments, &schedule, &context,
      &application);
  assignments[0].physical_register = -1;
  if (expect_invalid_join_assignment_application(
      "coreJoinAssignmentNoRegister", assignments, 3, &schedule, &context,
      &application) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_reservation_inputs(
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_occupied_interval *intervals) {

  assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[0].instruction = 3;
  assignments[0].operand = 0;
  assignments[0].physical_register = 4;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].instruction = 7;
  assignments[1].operand = 0;
  assignments[1].physical_register = 4;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[2].instruction = 10;
  assignments[2].operand = 1;
  assignments[2].physical_register = 4;
  schedule->status = RA_JOIN_SCHEDULE_READY;
  schedule->assignment_count = 3;
  schedule->conflict_instruction = -1;
  intervals[0].temp_index = 8;
  intervals[0].physical_register = 5;
  intervals[0].live_start = 0;
  intervals[0].live_end = 2;
  intervals[0].overlaps_selected_register = YES;
  intervals[1].temp_index = 9;
  intervals[1].physical_register = 2;
  intervals[1].live_start = 4;
  intervals[1].live_end = 9;
  intervals[1].overlaps_selected_register = NO;
  intervals[2].temp_index = 2;
  intervals[2].physical_register = 4;
  intervals[2].live_start = 0;
  intervals[2].live_end = 11;
  intervals[2].overlaps_selected_register = YES;
}

static int expect_invalid_join_reservation(char *function_name,
    int instruction_count, int temp_index, int selected_physical_register,
    int slot_index, int slot_count,
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_occupied_interval *intervals,
    int interval_count, struct register_allocator_join_reservation *result) {

  struct register_allocator_join_assignment original_assignments[3];
  struct register_allocator_join_occupied_interval original_intervals[3];

  if (assignments != NULL)
    memcpy(original_assignments, assignments, sizeof(original_assignments));
  if (intervals != NULL && interval_count == 3)
    memcpy(original_intervals, intervals, sizeof(original_intervals));
  if (result != NULL) {
    result->status = 99;
    result->slot_index = 99;
    result->live_start = 99;
    result->live_end = 99;
    result->conflict_temp = 99;
  }
    if (register_allocator_plan_join_reservation(function_name,
      instruction_count,
      temp_index, -1, selected_physical_register, slot_index, slot_count,
      assignments, 3, schedule, intervals, interval_count, result) != FAILED)
    return FAILED;
  if (result != NULL &&
      (result->status != RA_JOIN_RESERVATION_INELIGIBLE ||
      result->slot_index != -1 || result->live_start != -1 ||
      result->live_end != -1 || result->conflict_temp != -1))
    return FAILED;
  if (assignments != NULL && memcmp(original_assignments, assignments,
      sizeof(original_assignments)) != 0)
    return FAILED;
  if (intervals != NULL && interval_count == 3 &&
      memcmp(original_intervals, intervals, sizeof(original_intervals)) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_reservation_planning(void) {

  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;
  struct register_allocator_join_occupied_interval intervals[3];
  struct register_allocator_join_reservation reservation;

  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  if (register_allocator_plan_join_reservation("coreJoinReservationReady", 12,
      2, -1, 4, 1, 5, assignments, 3, &schedule, intervals, 3,
      &reservation) == FAILED ||
      reservation.status != RA_JOIN_RESERVATION_READY ||
      reservation.slot_index != 1 || reservation.live_start != 3 ||
      reservation.live_end != 10 || reservation.conflict_temp != -1)
    return FAILED;

  intervals[0].live_end = 3;
  intervals[1].overlaps_selected_register = YES;
  if (register_allocator_plan_join_reservation("coreJoinReservationConflict",
      12, 2, -1, 4, 1, 5, assignments, 3, &schedule, intervals, 3,
      &reservation) == FAILED ||
      reservation.status != RA_JOIN_RESERVATION_CONFLICT ||
      reservation.slot_index != -1 || reservation.live_start != -1 ||
      reservation.live_end != -1 || reservation.conflict_temp != 8)
    return FAILED;

  intervals[0].live_end = 2;
  intervals[1].live_start = 10;
  intervals[1].live_end = 10;
  if (register_allocator_plan_join_reservation(
      "coreJoinReservationEndConflict", 12, 2, -1, 4, 1, 5, assignments,
      3, &schedule, intervals, 3, &reservation) == FAILED ||
      reservation.status != RA_JOIN_RESERVATION_CONFLICT ||
      reservation.conflict_temp != 9)
    return FAILED;

  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  schedule.assignment_count = 0;
  if (register_allocator_plan_join_reservation(
      "coreJoinReservationIneligible", 12, 2, -1, -1, -1, 5, NULL,
      0, &schedule, intervals, 3, &reservation) == FAILED ||
      reservation.status != RA_JOIN_RESERVATION_INELIGIBLE ||
      reservation.slot_index != -1 || reservation.live_start != -1 ||
      reservation.live_end != -1 || reservation.conflict_temp != -1)
    return FAILED;

  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  if (expect_invalid_join_reservation(NULL, 12, 2, 4, 1, 5, assignments,
      &schedule, intervals, 3, &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationInstructions", 0,
      2, 4, 1, 5, assignments, &schedule, intervals, 3,
      &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationTemp", 12, -1, 4,
      1, 5, assignments, &schedule, intervals, 3, &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationSlots", 12, 2, 4,
      1, 0, assignments, &schedule, intervals, 3, &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationIntervals", 12, 2,
      4, 1, 5, assignments, &schedule, NULL, 3, &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationCount", 12, 2, 4,
      1, 5, assignments, &schedule, intervals, -1, &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationSchedule", 12, 2,
      4, 1, 5, assignments, NULL, intervals, 3, &reservation) == FAILED ||
      expect_invalid_join_reservation("coreJoinReservationOutput", 12, 2, 4,
      1, 5, assignments, &schedule, intervals, 3, NULL) == FAILED)
    return FAILED;

  schedule.status = RA_JOIN_SCHEDULE_CONFLICT;
  schedule.assignment_count = 0;
  schedule.conflict_instruction = 3;
  if (expect_invalid_join_reservation("coreJoinReservationConflictSchedule",
      12, 2, 4, 1, 5, assignments, &schedule, intervals, 3,
      &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  if (expect_invalid_join_reservation("coreJoinReservationSlot", 12, 2, 4,
      5, 5, assignments, &schedule, intervals, 3, &reservation) == FAILED)
    return FAILED;
  assignments[0].role = 99;
  if (expect_invalid_join_reservation("coreJoinReservationRole", 12, 2, 4,
      1, 5, assignments, &schedule, intervals, 3, &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  assignments[1].physical_register = 2;
  if (expect_invalid_join_reservation("coreJoinReservationRegister", 12, 2,
      4, 1, 5, assignments, &schedule, intervals, 3,
      &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  assignments[2].instruction = 3;
  if (expect_invalid_join_reservation("coreJoinReservationSpan", 12, 2, 4,
      1, 5, assignments, &schedule, intervals, 3, &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  intervals[0].temp_index = -1;
  if (expect_invalid_join_reservation("coreJoinReservationIntervalTemp", 12,
      2, 4, 1, 5, assignments, &schedule, intervals, 3,
      &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  intervals[1].live_end = 12;
  if (expect_invalid_join_reservation("coreJoinReservationIntervalEnd", 12,
      2, 4, 1, 5, assignments, &schedule, intervals, 3,
      &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  intervals[2].overlaps_selected_register = 9;
  if (expect_invalid_join_reservation("coreJoinReservationIntervalOverlap",
      12, 2, 4, 1, 5, assignments, &schedule, intervals, 3,
      &reservation) == FAILED)
    return FAILED;
  initialize_join_reservation_inputs(assignments, &schedule, intervals);
  reservation.status = 99;
  if (register_allocator_plan_join_reservation(
      "coreJoinReservationCapacity", 12, 2, -1, 4, 1, 5, assignments, 2,
      &schedule, intervals, 3, &reservation) != FAILED ||
      reservation.status != RA_JOIN_RESERVATION_INELIGIBLE)
    return FAILED;
  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  schedule.assignment_count = 0;
  schedule.conflict_instruction = 3;
  reservation.status = 99;
  if (register_allocator_plan_join_reservation(
      "coreJoinReservationIneligibleConflict", 12, 2, -1, -1, -1, 5,
      NULL, 0, &schedule, intervals, 3, &reservation) != FAILED ||
      reservation.status != RA_JOIN_RESERVATION_INELIGIBLE)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_path_reservation_graph(
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges,
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_reservation *reservation) {

  int block_index;

  for (block_index = 0; block_index < 6; block_index++) {
    blocks[block_index].start_tac = block_index * 2;
    blocks[block_index].end_tac = block_index * 2 + 1;
    blocks[block_index].end_reason = RA_BLOCK_END_NONE;
  }
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[1].from_block = 0;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[2].from_block = 1;
  edges[2].to_block = 5;
  edges[2].kind = RA_CFG_EDGE_JUMP;
  edges[3].from_block = 2;
  edges[3].to_block = 3;
  edges[3].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[4].from_block = 3;
  edges[4].to_block = 5;
  edges[4].kind = RA_CFG_EDGE_JUMP;
  for (block_index = 0; block_index < 5; block_index++)
    edges[block_index].target = NULL;
  assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[0].instruction = 3;
  assignments[0].operand = 0;
  assignments[0].physical_register = 4;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].instruction = 7;
  assignments[1].operand = 0;
  assignments[1].physical_register = 4;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[2].instruction = 10;
  assignments[2].operand = 1;
  assignments[2].physical_register = 4;
  schedule->status = RA_JOIN_SCHEDULE_READY;
  schedule->assignment_count = 3;
  schedule->conflict_instruction = -1;
  reservation->status = RA_JOIN_RESERVATION_READY;
  reservation->slot_index = 1;
  reservation->live_start = 3;
  reservation->live_end = 10;
  reservation->conflict_temp = -1;
}

static int expect_failed_join_path_reservation(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity, struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_reservation *reservation,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation) {

  struct register_allocator_join_block_reservation original[6];

  if (block_reservations != NULL)
    memcpy(original, block_reservations, sizeof(original));
  if (path_reservation != NULL) {
    path_reservation->status = 99;
    path_reservation->block_count = 99;
  }
  if (register_allocator_plan_join_path_reservations(function_name,
      block_count, instruction_count, blocks, edges, edge_count, 2,
      assignments, assignment_capacity, schedule, reservation,
      block_reservations, block_reservation_capacity,
      path_reservation) != FAILED)
    return FAILED;
  if (path_reservation != NULL &&
      (path_reservation->status != RA_JOIN_PATH_RESERVATION_INELIGIBLE ||
      path_reservation->block_count != 0))
    return FAILED;
  if (block_reservations != NULL && memcmp(original, block_reservations,
      sizeof(original)) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_path_reservation_planning(void) {

  struct register_allocator_basic_block blocks[6];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;
  struct register_allocator_join_reservation reservation;
  struct register_allocator_join_block_reservation block_reservations[6];
  struct register_allocator_join_path_reservation path_reservation;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  memset(block_reservations, 99, sizeof(block_reservations));
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathReservationDiamond", 6, 12, blocks, edges, 5, 2,
      assignments, 3, &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED ||
      path_reservation.status != RA_JOIN_PATH_RESERVATION_READY ||
      path_reservation.block_count != 3 ||
      block_reservations[0].block_index != 1 ||
      block_reservations[0].start_instruction != 3 ||
      block_reservations[0].end_instruction != 3 ||
      block_reservations[1].block_index != 3 ||
      block_reservations[1].start_instruction != 7 ||
      block_reservations[1].end_instruction != 7 ||
      block_reservations[2].block_index != 5 ||
      block_reservations[2].start_instruction != 10 ||
      block_reservations[2].end_instruction != 10)
    return FAILED;

  assignments[0].instruction = 1;
  assignments[1] = assignments[2];
  schedule.assignment_count = 2;
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathReservationCommon", 6, 12, blocks, edges, 5, 2,
      assignments, 3, &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED || path_reservation.block_count != 5 ||
      block_reservations[0].block_index != 0 ||
      block_reservations[4].block_index != 5)
    return FAILED;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  schedule.assignment_count = 0;
  reservation.status = RA_JOIN_RESERVATION_INELIGIBLE;
  reservation.slot_index = -1;
  reservation.live_start = -1;
  reservation.live_end = -1;
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathReservationIneligible", 6, 12, blocks, edges, 5, 2,
      NULL, 0, &schedule, &reservation, NULL, 0,
      &path_reservation) == FAILED ||
      path_reservation.status != RA_JOIN_PATH_RESERVATION_INELIGIBLE ||
      path_reservation.block_count != 0)
    return FAILED;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  assignments[1].instruction = 9;
  memset(block_reservations, 99, sizeof(block_reservations));
  if (expect_failed_join_path_reservation(
      "coreJoinPathReservationUnreachable", 6, 12, blocks, edges, 5,
      assignments, 3, &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED)
    return FAILED;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  memset(block_reservations, 99, sizeof(block_reservations));
  if (expect_failed_join_path_reservation(NULL, 6, 12, blocks, edges, 5,
      assignments, 3, &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED ||
      expect_failed_join_path_reservation("coreJoinPathReservationBlocks",
      0, 12, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED ||
      expect_failed_join_path_reservation("coreJoinPathReservationInstructions",
      6, 0, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED ||
      expect_failed_join_path_reservation("coreJoinPathReservationNullBlocks",
      6, 12, NULL, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED ||
      expect_failed_join_path_reservation("coreJoinPathReservationNullEdges",
      6, 12, blocks, NULL, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED ||
      expect_failed_join_path_reservation("coreJoinPathReservationOutput",
      6, 12, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, NULL) == FAILED)
    return FAILED;

  blocks[1].start_tac = 1;
  if (expect_failed_join_path_reservation("coreJoinPathReservationOverlap",
      6, 12, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED)
    return FAILED;
  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  edges[4].kind = 99;
  if (expect_failed_join_path_reservation("coreJoinPathReservationEdge",
      6, 12, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED)
    return FAILED;
  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  assignments[1].physical_register = 2;
  if (expect_failed_join_path_reservation("coreJoinPathReservationRegister",
      6, 12, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 6, &path_reservation) == FAILED)
    return FAILED;
  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  if (expect_failed_join_path_reservation("coreJoinPathReservationCapacity",
      6, 12, blocks, edges, 5, assignments, 3, &schedule, &reservation,
      block_reservations, 2, &path_reservation) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct join_path_application_context {
  int calls;
  int fail_call;
  int blocks[6];
  int starts[6];
  int ends[6];
  int temp_index;
  int slot_index;
};

static int apply_join_block_reservation(void *context, int temp_index,
    int slot_index,
    struct register_allocator_join_block_reservation *reservation) {

  struct join_path_application_context *application_context;
  int call_index;

  application_context = (struct join_path_application_context *)context;
  call_index = application_context->calls;
  application_context->calls++;
  if (call_index == application_context->fail_call)
    return FAILED;
  application_context->blocks[call_index] = reservation->block_index;
  application_context->starts[call_index] = reservation->start_instruction;
  application_context->ends[call_index] = reservation->end_instruction;
  application_context->temp_index = temp_index;
  application_context->slot_index = slot_index;
  return SUCCEEDED;
}

static int expect_failed_join_path_application(char *function_name,
    int block_count, int instruction_count, int temp_index,
    struct register_allocator_join_reservation *reservation,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation,
    register_allocator_join_block_reservation_applier callback,
    struct register_allocator_join_path_application *application) {

  struct register_allocator_join_block_reservation original[6];
  struct join_path_application_context context;

  if (block_reservations != NULL)
    memcpy(original, block_reservations, sizeof(original));
  memset(&context, 0, sizeof(context));
  context.fail_call = -1;
  if (application != NULL) {
    application->status = 99;
    application->applied_count = 99;
    application->failed_block = 99;
  }
  if (register_allocator_apply_join_path_reservations(function_name,
      block_count, instruction_count, temp_index, reservation,
      block_reservations, block_reservation_capacity, path_reservation,
      &context, callback, application) != FAILED)
    return FAILED;
  if (context.calls != 0)
    return FAILED;
  if (application != NULL &&
      (application->status != RA_JOIN_PATH_APPLICATION_INELIGIBLE ||
      application->applied_count != 0 || application->failed_block != -1))
    return FAILED;
  if (block_reservations != NULL && memcmp(original, block_reservations,
      sizeof(original)) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_path_reservation_application(void) {

  struct register_allocator_basic_block blocks[6];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;
  struct register_allocator_join_reservation reservation;
  struct register_allocator_join_block_reservation block_reservations[6];
  struct register_allocator_join_path_reservation path_reservation;
  struct register_allocator_join_path_application application;
  struct join_path_application_context context;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathApplyPlan", 6, 12, blocks, edges, 5, 2, assignments, 3,
      &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED)
    return FAILED;
  memset(&context, 0, sizeof(context));
  context.fail_call = -1;
  if (register_allocator_apply_join_path_reservations(
      "coreJoinPathApply", 6, 12, 2, &reservation, block_reservations, 6,
      &path_reservation, &context, apply_join_block_reservation,
      &application) == FAILED ||
      application.status != RA_JOIN_PATH_APPLICATION_APPLIED ||
      application.applied_count != 3 || application.failed_block != -1 ||
      context.calls != 3 || context.temp_index != 2 ||
      context.slot_index != 1 || context.blocks[0] != 1 ||
      context.blocks[1] != 3 || context.blocks[2] != 5 ||
      context.starts[0] != 3 || context.ends[2] != 10)
    return FAILED;

  memset(&context, 0, sizeof(context));
  context.fail_call = 1;
  if (register_allocator_apply_join_path_reservations(
      "coreJoinPathApplyPartial", 6, 12, 2, &reservation,
      block_reservations, 6, &path_reservation, &context,
      apply_join_block_reservation, &application) != FAILED ||
      application.status != RA_JOIN_PATH_APPLICATION_PARTIAL ||
      application.applied_count != 1 || application.failed_block != 3 ||
      context.calls != 2 || context.blocks[0] != 1)
    return FAILED;

  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  reservation.status = RA_JOIN_RESERVATION_INELIGIBLE;
  reservation.slot_index = -1;
  reservation.live_start = -1;
  reservation.live_end = -1;
  path_reservation.status = RA_JOIN_PATH_RESERVATION_INELIGIBLE;
  path_reservation.block_count = 0;
  if (register_allocator_apply_join_path_reservations(
      "coreJoinPathApplyIneligible", 6, 12, 2, &reservation, NULL, 0,
      &path_reservation, NULL, NULL, &application) == FAILED ||
      application.status != RA_JOIN_PATH_APPLICATION_INELIGIBLE ||
      application.applied_count != 0 || application.failed_block != -1)
    return FAILED;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathApplyInvalidPlan", 6, 12, blocks, edges, 5, 2,
      assignments, 3, &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED)
    return FAILED;
  if (expect_failed_join_path_application(NULL, 6, 12, 2, &reservation,
      block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyBlocks", 0, 12,
      2, &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyInstructions", 6,
      0, 2, &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyTemp", 6, 12, -1,
      &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyCapacity", 6, 12,
      2, &reservation, block_reservations, -1, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyNullReservation",
      6, 12, 2, NULL, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyNullPlan", 6, 12,
      2, &reservation, block_reservations, 6, NULL,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyNullOutput", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, NULL) == FAILED)
    return FAILED;

  path_reservation.block_count = 7;
  if (expect_failed_join_path_application("coreJoinPathApplyPlanCapacity", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED)
    return FAILED;
  path_reservation.block_count = 3;
  if (expect_failed_join_path_application("coreJoinPathApplyNullBlocks", 6,
      12, 2, &reservation, NULL, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED ||
      expect_failed_join_path_application("coreJoinPathApplyNullCallback", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation, NULL,
      &application) == FAILED)
    return FAILED;
  block_reservations[1].block_index = 1;
  if (expect_failed_join_path_application("coreJoinPathApplyOrder", 6, 12, 2,
      &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED)
    return FAILED;
  block_reservations[1].block_index = 3;
  block_reservations[1].start_instruction = 2;
  if (expect_failed_join_path_application("coreJoinPathApplyRange", 6, 12, 2,
      &reservation, block_reservations, 6, &path_reservation,
      apply_join_block_reservation, &application) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_uncommitted_join_path_state(char *function_name,
    int block_count, int instruction_count, int temp_index,
    struct register_allocator_join_reservation *reservation,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation,
    struct register_allocator_join_path_state_entry *state_entries,
    int state_capacity, int *state_count,
    struct register_allocator_join_path_commit *commit, int expected_result,
    int expected_status) {

  struct register_allocator_join_path_state_entry original[8];
  int original_count;
  int result;

  if (state_entries != NULL)
    memcpy(original, state_entries, sizeof(original));
  original_count = state_count != NULL ? *state_count : -1;
  if (commit != NULL) {
    commit->status = 99;
    commit->committed_count = 99;
    commit->conflict_entry = 99;
  }
  result = register_allocator_commit_join_path_reservations(function_name,
      block_count, instruction_count, temp_index, reservation,
      block_reservations, block_reservation_capacity, path_reservation,
      state_entries, state_capacity, state_count, commit);
  if (result != expected_result)
    return FAILED;
  if (state_count != NULL && *state_count != original_count)
    return FAILED;
  if (state_entries != NULL && memcmp(original, state_entries,
      sizeof(original)) != 0)
    return FAILED;
  if (commit != NULL && (commit->status != expected_status ||
      commit->committed_count != 0))
    return FAILED;
  return SUCCEEDED;
}

static int test_join_path_reservation_commit(void) {

  struct register_allocator_basic_block blocks[6];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;
  struct register_allocator_join_reservation reservation;
  struct register_allocator_join_block_reservation block_reservations[6];
  struct register_allocator_join_path_reservation path_reservation;
  struct register_allocator_join_path_state_entry state_entries[8];
  struct register_allocator_join_path_commit commit;
  int state_count;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathCommitPlan", 6, 12, blocks, edges, 5, 2, assignments, 3,
      &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED)
    return FAILED;
  memset(state_entries, 99, sizeof(state_entries));
  state_count = 0;
  if (register_allocator_commit_join_path_reservations(
      "coreJoinPathCommit", 6, 12, 2, &reservation, block_reservations, 6,
      &path_reservation, state_entries, 8, &state_count, &commit) == FAILED ||
      commit.status != RA_JOIN_PATH_COMMIT_COMMITTED ||
      commit.committed_count != 3 || commit.conflict_entry != -1 ||
      state_count != 3 || state_entries[0].block_index != 1 ||
      state_entries[0].slot_index != 1 || state_entries[0].temp_index != 2 ||
      state_entries[0].start_instruction != 3 ||
      state_entries[0].end_instruction != 3 ||
      state_entries[1].block_index != 3 ||
      state_entries[2].block_index != 5 ||
      state_entries[2].end_instruction != 10)
    return FAILED;

  memset(state_entries, 99, sizeof(state_entries));
  state_entries[0].block_index = 0;
  state_entries[0].slot_index = 1;
  state_entries[0].temp_index = 8;
  state_entries[0].start_instruction = 0;
  state_entries[0].end_instruction = 1;
  state_count = 1;
  if (register_allocator_commit_join_path_reservations(
      "coreJoinPathCommitAppend", 6, 12, 2, &reservation,
      block_reservations, 6, &path_reservation, state_entries, 8,
      &state_count, &commit) == FAILED || state_count != 4 ||
      commit.committed_count != 3 || state_entries[0].temp_index != 8 ||
      state_entries[1].block_index != 1 || state_entries[3].block_index != 5)
    return FAILED;

  memset(state_entries, 99, sizeof(state_entries));
  state_entries[0].block_index = 3;
  state_entries[0].slot_index = 1;
  state_entries[0].temp_index = 8;
  state_entries[0].start_instruction = 6;
  state_entries[0].end_instruction = 8;
  state_count = 1;
  if (expect_uncommitted_join_path_state("coreJoinPathCommitConflict", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, SUCCEEDED,
      RA_JOIN_PATH_COMMIT_CONFLICT) == FAILED || commit.conflict_entry != 0)
    return FAILED;

  state_entries[0].start_instruction = 4;
  state_entries[0].end_instruction = 5;
  if (register_allocator_commit_join_path_reservations(
      "coreJoinPathCommitNonoverlap", 6, 12, 2, &reservation,
      block_reservations, 6, &path_reservation, state_entries, 8,
      &state_count, &commit) == FAILED || state_count != 4)
    return FAILED;

  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  reservation.status = RA_JOIN_RESERVATION_INELIGIBLE;
  reservation.slot_index = -1;
  reservation.live_start = -1;
  reservation.live_end = -1;
  path_reservation.status = RA_JOIN_PATH_RESERVATION_INELIGIBLE;
  path_reservation.block_count = 0;
  if (expect_uncommitted_join_path_state("coreJoinPathCommitIneligible", 6,
      12, 2, &reservation, NULL, 0, &path_reservation, state_entries, 8,
      &state_count, &commit, SUCCEEDED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED)
    return FAILED;

  initialize_join_path_reservation_graph(blocks, edges, assignments,
      &schedule, &reservation);
  if (register_allocator_plan_join_path_reservations(
      "coreJoinPathCommitInvalidPlan", 6, 12, blocks, edges, 5, 2,
      assignments, 3, &schedule, &reservation, block_reservations, 6,
      &path_reservation) == FAILED)
    return FAILED;
  memset(state_entries, 99, sizeof(state_entries));
  state_count = 0;
  if (expect_uncommitted_join_path_state(NULL, 6, 12, 2, &reservation,
      block_reservations, 6, &path_reservation, state_entries, 8,
      &state_count, &commit, FAILED, RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitBlocks", 0, 12,
      2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitInstructions", 6,
      0, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitTemp", 6, 12, -1,
      &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitCapacity", 6, 12,
      2, &reservation, block_reservations, -1, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitStateCapacity", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, -1, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitNullReservation",
      6, 12, 2, NULL, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitNullPlan", 6, 12,
      2, &reservation, block_reservations, 6, NULL, state_entries, 8,
      &state_count, &commit, FAILED, RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitNullCount", 6, 12,
      2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, NULL, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED ||
      expect_uncommitted_join_path_state("coreJoinPathCommitNullOutput", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, NULL, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED)
    return FAILED;

  path_reservation.block_count = 7;
  if (expect_uncommitted_join_path_state("coreJoinPathCommitPlanCapacity", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED)
    return FAILED;
  path_reservation.block_count = 3;
  state_count = 7;
  if (expect_uncommitted_join_path_state("coreJoinPathCommitStorageCapacity",
      6, 12, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED)
    return FAILED;
  state_count = 1;
  state_entries[0].block_index = 6;
  if (expect_uncommitted_join_path_state("coreJoinPathCommitInvalidState", 6,
      12, 2, &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED)
    return FAILED;
  state_count = 0;
  block_reservations[1].block_index = 1;
  if (expect_uncommitted_join_path_state("coreJoinPathCommitOrder", 6, 12, 2,
      &reservation, block_reservations, 6, &path_reservation,
      state_entries, 8, &state_count, &commit, FAILED,
      RA_JOIN_PATH_COMMIT_INELIGIBLE) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_path_state_storage(void) {

  struct register_allocator_join_path_state_storage storage;

  storage.entry_capacity = 99;
  storage.entry_bytes = 99;
  if (register_allocator_plan_join_path_state_storage(
      "coreJoinPathStateStorage", 6, 4, &storage) == FAILED ||
      storage.entry_capacity != 24 || storage.entry_bytes !=
      24 * sizeof(struct register_allocator_join_path_state_entry))
    return FAILED;
  storage.entry_capacity = 99;
  storage.entry_bytes = 99;
  if (register_allocator_plan_join_path_state_storage(NULL, 6, 4,
      &storage) != FAILED || storage.entry_capacity != 0 ||
      storage.entry_bytes != 0)
    return FAILED;
  storage.entry_capacity = 99;
  storage.entry_bytes = 99;
  if (register_allocator_plan_join_path_state_storage(
      "coreJoinPathStateStorageBlocks", 0, 4, &storage) != FAILED ||
      storage.entry_capacity != 0 || storage.entry_bytes != 0)
    return FAILED;
  storage.entry_capacity = 99;
  storage.entry_bytes = 99;
  if (register_allocator_plan_join_path_state_storage(
      "coreJoinPathStateStorageTemps", 6, 0, &storage) != FAILED ||
      storage.entry_capacity != 0 || storage.entry_bytes != 0)
    return FAILED;
  storage.entry_capacity = 99;
  storage.entry_bytes = 99;
  if (register_allocator_plan_join_path_state_storage(
      "coreJoinPathStateStorageNegative", -1, 4, &storage) != FAILED ||
      storage.entry_capacity != 0 || storage.entry_bytes != 0)
    return FAILED;
  storage.entry_capacity = 99;
  storage.entry_bytes = 99;
  if (register_allocator_plan_join_path_state_storage(
      "coreJoinPathStateStorageOverflow", 46341, 46341,
      &storage) != FAILED || storage.entry_capacity != 0 ||
      storage.entry_bytes != 0)
    return FAILED;
  if (register_allocator_plan_join_path_state_storage(
      "coreJoinPathStateStorageNull", 6, 4, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_path_state_entries(
    struct register_allocator_join_path_state_entry *entries) {

  entries[0].block_index = 1;
  entries[0].slot_index = 1;
  entries[0].temp_index = 2;
  entries[0].start_instruction = 3;
  entries[0].end_instruction = 5;
  entries[1].block_index = 1;
  entries[1].slot_index = 2;
  entries[1].temp_index = 3;
  entries[1].start_instruction = 4;
  entries[1].end_instruction = 6;
  entries[2].block_index = 2;
  entries[2].slot_index = 1;
  entries[2].temp_index = 2;
  entries[2].start_instruction = 7;
  entries[2].end_instruction = 8;
}

static int expect_invalid_join_path_state_query(char *function_name,
    int block_count, int instruction_count, int slot_count, int block_index,
    int slot_index, int temp_index, int start_instruction,
    int end_instruction,
    struct register_allocator_join_path_state_entry *entries,
    int state_count) {

  struct register_allocator_join_path_state_query query;

  query.status = 99;
  query.entry_index = 99;
  query.owner_temp = 99;
  if (register_allocator_query_join_path_state(function_name, block_count,
      instruction_count, slot_count, block_index, slot_index, temp_index,
      start_instruction, end_instruction, entries, state_count,
      &query) != FAILED || query.status != RA_JOIN_PATH_STATE_AVAILABLE ||
      query.entry_index != -1 || query.owner_temp != -1)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_path_state_query(void) {

  struct register_allocator_join_path_state_entry entries[3];
  struct register_allocator_join_path_state_query query;

  initialize_join_path_state_entries(entries);
  if (register_allocator_query_join_path_state("coreJoinPathStateOwned", 4,
      12, 4, 1, 1, 2, 3, 5, entries, 3, &query) == FAILED ||
      query.status != RA_JOIN_PATH_STATE_OWNED || query.entry_index != 0 ||
      query.owner_temp != 2)
    return FAILED;
  if (register_allocator_query_join_path_state("coreJoinPathStateConflict", 4,
      12, 4, 1, 1, 7, 4, 4, entries, 3, &query) == FAILED ||
      query.status != RA_JOIN_PATH_STATE_CONFLICT || query.entry_index != 0 ||
      query.owner_temp != 2)
    return FAILED;
  if (register_allocator_query_join_path_state("coreJoinPathStateDisjoint", 4,
      12, 4, 1, 3, 7, 4, 4, entries, 3, &query) == FAILED ||
      query.status != RA_JOIN_PATH_STATE_AVAILABLE ||
      query.entry_index != -1 || query.owner_temp != -1)
    return FAILED;
  if (register_allocator_query_join_path_state("coreJoinPathStateBefore", 4,
      12, 4, 1, 1, 7, 0, 2, entries, 3, &query) == FAILED ||
      query.status != RA_JOIN_PATH_STATE_AVAILABLE || query.entry_index != -1)
    return FAILED;
  if (register_allocator_query_join_path_state("coreJoinPathStateAfter", 4,
      12, 4, 1, 1, 7, 6, 7, entries, 3, &query) == FAILED ||
      query.status != RA_JOIN_PATH_STATE_AVAILABLE)
    return FAILED;
  if (register_allocator_query_join_path_state("coreJoinPathStateEmpty", 4,
      12, 4, 3, 3, 7, 9, 10, NULL, 0, &query) == FAILED ||
      query.status != RA_JOIN_PATH_STATE_AVAILABLE)
    return FAILED;

  if (expect_invalid_join_path_state_query(NULL, 4, 12, 4, 1, 1, 2, 3, 5,
      entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateBlocks", 0, 12,
      4, 1, 1, 2, 3, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateInstructions", 4,
      0, 4, 1, 1, 2, 3, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateSlots", 4, 12,
      0, 1, 1, 2, 3, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateBlock", 4, 12,
      4, 4, 1, 2, 3, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateSlot", 4, 12,
      4, 1, 4, 2, 3, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateTemp", 4, 12,
      4, 1, 1, -1, 3, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateRange", 4, 12,
      4, 1, 1, 2, 6, 5, entries, 3) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateCount", 4, 12,
      4, 1, 1, 2, 3, 5, entries, -1) == FAILED ||
      expect_invalid_join_path_state_query("coreJoinPathStateNullEntries", 4,
      12, 4, 1, 1, 2, 3, 5, NULL, 3) == FAILED)
    return FAILED;
  query.status = 99;
  if (register_allocator_query_join_path_state("coreJoinPathStateNullOutput",
      4, 12, 4, 1, 1, 2, 3, 5, entries, 3, NULL) != FAILED ||
      query.status != 99)
    return FAILED;
  entries[1].slot_index = 4;
  if (expect_invalid_join_path_state_query("coreJoinPathStateInvalidEntry", 4,
      12, 4, 1, 1, 2, 3, 5, entries, 3) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_block_exit_spill(char *function_name,
    int block_count, int instruction_count, int slot_count, int block_index,
    int end_instruction, int temp_index,
    struct register_allocator_join_path_state_entry *entries,
    int state_count) {

  struct register_allocator_block_exit_spill spill;

  spill.status = 99;
  spill.reservation_entry = 99;
  spill.slot_index = 99;
  if (register_allocator_plan_block_exit_spill(function_name, block_count,
      instruction_count, slot_count, block_index, end_instruction,
      temp_index, entries, state_count, &spill) != FAILED ||
      spill.status != RA_BLOCK_EXIT_SPILL_REQUIRED ||
      spill.reservation_entry != -1 || spill.slot_index != -1)
    return FAILED;
  return SUCCEEDED;
}

static int test_block_exit_spill_planning(void) {

  struct register_allocator_join_path_state_entry entries[3];
  struct register_allocator_block_exit_spill spill;

  initialize_join_path_state_entries(entries);
  if (register_allocator_plan_block_exit_spill("coreBlockExitSpillExempt", 4,
      12, 4, 1, 5, 2, entries, 3, &spill) == FAILED ||
      spill.status != RA_BLOCK_EXIT_SPILL_EXEMPT ||
      spill.reservation_entry != 0 || spill.slot_index != 1)
    return FAILED;
  if (register_allocator_plan_block_exit_spill("coreBlockExitSpillBefore", 4,
      12, 4, 1, 2, 2, entries, 3, &spill) == FAILED ||
      spill.status != RA_BLOCK_EXIT_SPILL_REQUIRED ||
      spill.reservation_entry != -1 || spill.slot_index != -1)
    return FAILED;
  if (register_allocator_plan_block_exit_spill("coreBlockExitSpillOtherTemp",
      4, 12, 4, 1, 5, 7, entries, 3, &spill) == FAILED ||
      spill.status != RA_BLOCK_EXIT_SPILL_REQUIRED)
    return FAILED;
  if (register_allocator_plan_block_exit_spill("coreBlockExitSpillEmpty", 4,
      12, 4, 1, 5, 2, NULL, 0, &spill) == FAILED ||
      spill.status != RA_BLOCK_EXIT_SPILL_REQUIRED)
    return FAILED;

  if (expect_invalid_block_exit_spill(NULL, 4, 12, 4, 1, 5, 2, entries,
      3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillBlocks", 0, 12, 4,
      1, 5, 2, entries, 3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillInstructions", 4, 0,
      4, 1, 5, 2, entries, 3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillSlots", 4, 12, 0,
      1, 5, 2, entries, 3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillBlock", 4, 12, 4,
      4, 5, 2, entries, 3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillEnd", 4, 12, 4, 1,
      12, 2, entries, 3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillTemp", 4, 12, 4, 1,
      5, -1, entries, 3) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillCount", 4, 12, 4,
      1, 5, 2, entries, -1) == FAILED ||
      expect_invalid_block_exit_spill("coreBlockExitSpillNullEntries", 4, 12,
      4, 1, 5, 2, NULL, 3) == FAILED)
    return FAILED;
  spill.status = 99;
  if (register_allocator_plan_block_exit_spill("coreBlockExitSpillNullOutput",
      4, 12, 4, 1, 5, 2, entries, 3, NULL) != FAILED ||
      spill.status != 99)
    return FAILED;
  entries[1].slot_index = 4;
  if (expect_invalid_block_exit_spill("coreBlockExitSpillInvalidEntry", 4, 12,
      4, 1, 5, 2, entries, 3) == FAILED)
    return FAILED;
  initialize_join_path_state_entries(entries);
  entries[1].slot_index = 3;
  entries[1].temp_index = 2;
  if (expect_invalid_block_exit_spill("coreBlockExitSpillAmbiguous", 4, 12,
      4, 1, 5, 2, entries, 3) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_reaching_definition_graph(
    struct register_allocator_cfg_edge *edges,
    struct register_allocator_reaching_definition_block *facts) {

  int block_index;

  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[0].target = NULL;
  edges[1].from_block = 0;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[1].target = NULL;
  edges[2].from_block = 1;
  edges[2].to_block = 3;
  edges[2].kind = RA_CFG_EDGE_JUMP;
  edges[2].target = NULL;
  edges[3].from_block = 2;
  edges[3].to_block = 3;
  edges[3].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[3].target = NULL;
  for (block_index = 0; block_index < 5; block_index++) {
    facts[block_index].definition_instruction = -1;
    facts[block_index].definition_found = NO;
    facts[block_index].path_transparent = YES;
  }
  facts[0].definition_instruction = 2;
  facts[0].definition_found = YES;
}

static int expect_invalid_reaching_definition(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_reaching_definition_block *facts,
    int predecessor_block, int temp_index,
    struct register_allocator_reaching_definition *result) {

  struct register_allocator_cfg_edge original_edges[4];
  struct register_allocator_reaching_definition_block original_facts[5];

  if (edges != NULL && edge_count == 4)
    memcpy(original_edges, edges, sizeof(original_edges));
  if (facts != NULL && block_count == 5)
    memcpy(original_facts, facts, sizeof(original_facts));
  if (result != NULL) {
    result->status = 99;
    result->definition_instruction = 99;
    result->path_transparent = YES;
    result->iterations = 99;
  }
  if (register_allocator_resolve_reaching_definition(function_name,
      block_count, instruction_count, edges, edge_count, facts,
      predecessor_block, temp_index, result) != FAILED)
    return FAILED;
  if (result != NULL &&
      (result->status != RA_REACHING_DEFINITION_MISSING ||
      result->definition_instruction != -1 ||
      result->path_transparent != NO || result->iterations != 0))
    return FAILED;
  if (edges != NULL && edge_count == 4 &&
      memcmp(original_edges, edges, sizeof(original_edges)) != 0)
    return FAILED;
  if (facts != NULL && block_count == 5 &&
      memcmp(original_facts, facts, sizeof(original_facts)) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_reaching_definition_resolution(void) {

  struct register_allocator_cfg_edge edges[4];
  struct register_allocator_reaching_definition_block facts[5];
  struct register_allocator_reaching_definition result;

  initialize_reaching_definition_graph(edges, facts);
  if (register_allocator_resolve_reaching_definition(
      "coreReachingDefinitionUnique", 5, 12, edges, 4, facts, 3, 2,
      &result) == FAILED || result.status != RA_REACHING_DEFINITION_UNIQUE ||
      result.definition_instruction != 2 || result.path_transparent != YES ||
      result.iterations <= 0)
    return FAILED;

  facts[2].path_transparent = NO;
  if (register_allocator_resolve_reaching_definition(
      "coreReachingDefinitionClobber", 5, 12, edges, 4, facts, 3, 2,
      &result) == FAILED || result.status != RA_REACHING_DEFINITION_UNIQUE ||
      result.definition_instruction != 2 || result.path_transparent != NO)
    return FAILED;

  initialize_reaching_definition_graph(edges, facts);
  facts[1].definition_instruction = 3;
  facts[1].definition_found = YES;
  facts[2].definition_instruction = 4;
  facts[2].definition_found = YES;
  if (register_allocator_resolve_reaching_definition(
      "coreReachingDefinitionAmbiguous", 5, 12, edges, 4, facts, 3, 2,
      &result) == FAILED ||
      result.status != RA_REACHING_DEFINITION_AMBIGUOUS ||
      result.definition_instruction != -1 || result.path_transparent != NO)
    return FAILED;

  initialize_reaching_definition_graph(edges, facts);
  facts[0].definition_instruction = -1;
  facts[0].definition_found = NO;
  if (register_allocator_resolve_reaching_definition(
      "coreReachingDefinitionMissing", 5, 12, edges, 4, facts, 3, 2,
      &result) == FAILED || result.status != RA_REACHING_DEFINITION_MISSING ||
      result.definition_instruction != -1 || result.path_transparent != NO)
    return FAILED;
  edges[0].from_block = 1;
  edges[0].to_block = 2;
  edges[1].from_block = 2;
  edges[1].to_block = 1;
  if (register_allocator_resolve_reaching_definition(
      "coreReachingDefinitionCycle", 5, 12, edges, 2, facts, 2, 2,
      &result) == FAILED || result.status != RA_REACHING_DEFINITION_MISSING ||
      result.definition_instruction != -1 || result.path_transparent != NO)
    return FAILED;

  initialize_reaching_definition_graph(edges, facts);
  if (expect_invalid_reaching_definition(NULL, 5, 12, edges, 4, facts, 3,
      2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionBlocks", 0,
      12, edges, 4, facts, 0, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionInstructions",
      5, 0, edges, 4, facts, 3, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionEdges", 5,
      12, edges, -1, facts, 3, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionNullEdges", 5,
      12, NULL, 4, facts, 3, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionNullFacts", 5,
      12, edges, 4, NULL, 3, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionPredecessor",
      5, 12, edges, 4, facts, -1, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionBlockRange", 5,
      12, edges, 4, facts, 5, 2, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionTemp", 5, 12,
      edges, 4, facts, 3, -1, &result) == FAILED ||
      expect_invalid_reaching_definition("coreReachingDefinitionNullOutput", 5,
      12, edges, 4, facts, 3, 2, NULL) == FAILED)
    return FAILED;

  facts[0].definition_found = 9;
  if (expect_invalid_reaching_definition("coreReachingDefinitionFound", 5,
      12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  initialize_reaching_definition_graph(edges, facts);
  facts[0].path_transparent = 9;
  if (expect_invalid_reaching_definition("coreReachingDefinitionTransparent",
      5, 12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  initialize_reaching_definition_graph(edges, facts);
  facts[0].definition_instruction = 12;
  if (expect_invalid_reaching_definition("coreReachingDefinitionIndex", 5,
      12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  initialize_reaching_definition_graph(edges, facts);
  facts[1].definition_instruction = 3;
  if (expect_invalid_reaching_definition("coreReachingDefinitionMissingIndex",
      5, 12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  initialize_reaching_definition_graph(edges, facts);
  edges[0].from_block = -1;
  if (expect_invalid_reaching_definition("coreReachingDefinitionEdgeFrom", 5,
      12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  initialize_reaching_definition_graph(edges, facts);
  edges[0].to_block = 5;
  if (expect_invalid_reaching_definition("coreReachingDefinitionEdgeTo", 5,
      12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  initialize_reaching_definition_graph(edges, facts);
  edges[0].kind = 99;
  if (expect_invalid_reaching_definition("coreReachingDefinitionEdgeKind", 5,
      12, edges, 4, facts, 3, 2, &result) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_join_spill_approval(char *function_name,
    int join_block_index, int instruction_count,
    struct register_allocator_join_spill_site *sites, int site_count,
    void *context, register_allocator_join_spill_site_approver approve_site,
    int *approved_count) {

  if (approved_count != NULL)
    *approved_count = 99;
  if (register_allocator_approve_join_spill_sites(function_name,
      join_block_index, instruction_count, sites, site_count, context,
      approve_site, approved_count) != FAILED)
    return FAILED;
  if (approved_count != NULL && *approved_count != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_spill_site_approval(void) {

  struct join_spill_approval_context context;
  struct register_allocator_join_spill_site sites[2];
  int approved_count;

  initialize_join_spill_sites(sites);
  context.calls = 0;
  context.reject_call = -1;
  context.invalid_call = -1;
  if (register_allocator_approve_join_spill_sites("coreJoinApprovalAll", 3,
      12, sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED || approved_count != 2 || context.calls != 2)
    return FAILED;
  context.calls = 0;
  context.reject_call = 2;
  context.invalid_call = -1;
  if (expect_invalid_join_spill_approval("coreJoinApprovalReject", 3, 12,
      sites, 2, &context, approve_join_spill_site, &approved_count) == FAILED ||
      context.calls != 2)
    return FAILED;
  context.calls = 0;
  context.reject_call = -1;
  context.invalid_call = 2;
  if (expect_invalid_join_spill_approval("coreJoinApprovalCallback", 3, 12,
      sites, 2, &context, approve_join_spill_site, &approved_count) == FAILED ||
      context.calls != 2)
    return FAILED;
  approved_count = 99;
  if (register_allocator_approve_join_spill_sites("coreJoinApprovalNone", 0,
      12, NULL, 0, NULL, NULL, &approved_count) == FAILED ||
      approved_count != 0)
    return FAILED;

  context.calls = 0;
  context.invalid_call = -1;
  if (expect_invalid_join_spill_approval(NULL, 3, 12, sites, 2, &context,
      approve_join_spill_site, &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalBlock", -1, 12,
      sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalInstructions", 3, 0,
      sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalCount", 3, 12, sites,
      -1, &context, approve_join_spill_site, &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalNullSites", 3, 12,
      NULL, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalNullContext", 3, 12,
      sites, 2, NULL, approve_join_spill_site,
      &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalNullCallback", 3, 12,
      sites, 2, &context, NULL, &approved_count) == FAILED ||
      expect_invalid_join_spill_approval("coreJoinApprovalNullOutput", 3, 12,
      sites, 2, &context, approve_join_spill_site, NULL) == FAILED)
    return FAILED;

  initialize_join_spill_sites(sites);
  sites[0].predecessor_block = -1;
  if (expect_invalid_join_spill_approval("coreJoinApprovalPredecessor", 3, 12,
      sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].anchor_instruction = -1;
  if (expect_invalid_join_spill_approval("coreJoinApprovalAnchorNegative", 3,
      12, sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].anchor_instruction = 12;
  if (expect_invalid_join_spill_approval("coreJoinApprovalAnchorRange", 3, 12,
      sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].edge_kind = 99;
  if (expect_invalid_join_spill_approval("coreJoinApprovalEdge", 3, 12, sites,
      2, &context, approve_join_spill_site, &approved_count) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].placement = 99;
  if (expect_invalid_join_spill_approval("coreJoinApprovalPlacement", 3, 12,
      sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[0].placement = RA_JOIN_SPILL_AFTER_ANCHOR;
  if (expect_invalid_join_spill_approval("coreJoinApprovalJumpPlacement", 3,
      12, sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED)
    return FAILED;
  initialize_join_spill_sites(sites);
  sites[1].placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  context.calls = 0;
  if (expect_invalid_join_spill_approval("coreJoinApprovalFallPlacement", 3,
      12, sites, 2, &context, approve_join_spill_site,
      &approved_count) == FAILED || context.calls != 0)
    return FAILED;
  return SUCCEEDED;
}

static int join_spill_mutation_is_empty(
    struct register_allocator_join_spill_mutation *mutation,
    int no_physical_register) {

  return mutation->apply == NO && mutation->site.predecessor_block == -1 &&
      mutation->site.edge_kind == 0 &&
      mutation->site.anchor_instruction == -1 && mutation->site.placement == 0 &&
      mutation->temp_index == -1 &&
      mutation->physical_register == no_physical_register ? YES : NO;
}

static int expect_invalid_join_spill_mutation(char *function_name,
    int join_block_index, int temp_index, int join_action,
    int no_physical_register, int physical_register,
    struct register_allocator_join_spill_site *site,
    struct register_allocator_join_spill_mutation *mutation) {

  if (mutation != NULL) {
    mutation->apply = YES;
    mutation->site.predecessor_block = 77;
    mutation->site.edge_kind = 77;
    mutation->site.anchor_instruction = 77;
    mutation->site.placement = 77;
    mutation->temp_index = 77;
    mutation->physical_register = 77;
  }
  if (register_allocator_prepare_join_spill_mutation(function_name,
      join_block_index, temp_index, join_action, no_physical_register,
      physical_register, site, mutation) != FAILED)
    return FAILED;
  if (mutation != NULL &&
      join_spill_mutation_is_empty(mutation, no_physical_register) == NO)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_spill_mutation_preparation(void) {

  struct register_allocator_join_spill_mutation mutation;
  struct register_allocator_join_spill_site site;

  site.predecessor_block = 1;
  site.edge_kind = RA_CFG_EDGE_JUMP;
  site.anchor_instruction = 5;
  site.placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  if (register_allocator_prepare_join_spill_mutation(
      "coreJoinMutationRetained", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED || mutation.apply != YES ||
      mutation.site.predecessor_block != 1 ||
      mutation.site.edge_kind != RA_CFG_EDGE_JUMP ||
      mutation.site.anchor_instruction != 5 ||
      mutation.site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      mutation.temp_index != 0 || mutation.physical_register != 4)
    return FAILED;

  site.predecessor_block = 2;
  site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  site.anchor_instruction = 8;
  site.placement = RA_JOIN_SPILL_AFTER_ANCHOR;
  if (register_allocator_prepare_join_spill_mutation(
      "coreJoinMutationStack", 3, 1, RA_JOIN_ACTION_RELOAD_ON_DEMAND, -1, -1,
      &site, &mutation) == FAILED ||
      join_spill_mutation_is_empty(&mutation, -1) == NO ||
      site.predecessor_block != 2 ||
      site.edge_kind != RA_CFG_EDGE_FALLTHROUGH ||
      site.anchor_instruction != 8 ||
      site.placement != RA_JOIN_SPILL_AFTER_ANCHOR)
    return FAILED;

  if (expect_invalid_join_spill_mutation(NULL, 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site, &mutation) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationBlock", -1, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site, &mutation) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationTemp", 3, -1,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site, &mutation) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationAction", 3, 0, 99,
      -1, 4, &site, &mutation) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationNullSite", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, NULL,
      &mutation) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationNullOutput", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site, NULL) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationReloadRegister", 3,
      0, RA_JOIN_ACTION_RELOAD_ON_DEMAND, -1, 4, &site,
      &mutation) == FAILED ||
      expect_invalid_join_spill_mutation("coreJoinMutationSpillRegister", 3,
      0, RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, -1, &site,
      &mutation) == FAILED)
    return FAILED;

  site.predecessor_block = -1;
  if (expect_invalid_join_spill_mutation("coreJoinMutationPredecessor", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED)
    return FAILED;
  site.predecessor_block = 2;
  site.anchor_instruction = -1;
  if (expect_invalid_join_spill_mutation("coreJoinMutationAnchor", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED)
    return FAILED;
  site.anchor_instruction = 8;
  site.edge_kind = 99;
  if (expect_invalid_join_spill_mutation("coreJoinMutationEdge", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED)
    return FAILED;
  site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  site.placement = 99;
  if (expect_invalid_join_spill_mutation("coreJoinMutationPlacement", 3, 0,
      RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED)
    return FAILED;
  site.placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  if (expect_invalid_join_spill_mutation("coreJoinMutationFallPlacement", 3,
      0, RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED)
    return FAILED;
  site.edge_kind = RA_CFG_EDGE_JUMP;
  site.placement = RA_JOIN_SPILL_AFTER_ANCHOR;
  if (expect_invalid_join_spill_mutation("coreJoinMutationJumpPlacement", 3,
      0, RA_JOIN_ACTION_SPILL_PREDECESSORS, -1, 4, &site,
      &mutation) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int join_spill_emission_is_empty(
    struct register_allocator_join_spill_emission *emission,
    int no_physical_register) {

  return emission->emit == NO && emission->site.predecessor_block == -1 &&
      emission->site.edge_kind == 0 &&
      emission->site.anchor_instruction == -1 &&
      emission->site.placement == 0 && emission->temp_index == -1 &&
      emission->physical_register == no_physical_register &&
      emission->destination_offset == 0 && emission->byte_count == 0 ?
      YES : NO;
}

static void initialize_join_spill_mutation(
    struct register_allocator_join_spill_mutation *mutation);

static int expect_invalid_join_spill_emission(char *function_name,
    int join_block_index, int no_physical_register,
    struct register_allocator_join_spill_mutation *mutation,
    int spill_available, int destination_offset, int byte_count,
    struct register_allocator_join_spill_emission *emission) {

  if (emission != NULL) {
    emission->emit = YES;
    emission->site.predecessor_block = 77;
    emission->site.edge_kind = 77;
    emission->site.anchor_instruction = 77;
    emission->site.placement = 77;
    emission->temp_index = 77;
    emission->physical_register = 77;
    emission->destination_offset = 77;
    emission->byte_count = 77;
  }
  if (register_allocator_prepare_join_spill_emission(function_name,
      join_block_index, no_physical_register, mutation, spill_available,
      destination_offset, byte_count, emission) != FAILED)
    return FAILED;
  if (emission != NULL &&
      join_spill_emission_is_empty(emission, no_physical_register) == NO)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_spill_emission_preparation(void) {

  struct register_allocator_join_spill_emission emission;
  struct register_allocator_join_spill_mutation mutation;

  initialize_join_spill_mutation(&mutation);
  if (register_allocator_prepare_join_spill_emission("coreJoinEmitJump", 3,
      -1, &mutation, YES, -6, 2, &emission) == FAILED ||
      emission.emit != YES || emission.site.predecessor_block != 1 ||
      emission.site.edge_kind != RA_CFG_EDGE_JUMP ||
      emission.site.anchor_instruction != 5 ||
      emission.site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      emission.temp_index != 2 || emission.physical_register != 4 ||
      emission.destination_offset != -6 || emission.byte_count != 2 ||
      mutation.site.predecessor_block != 1 || mutation.temp_index != 2)
    return FAILED;

  mutation.site.predecessor_block = 2;
  mutation.site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  mutation.site.anchor_instruction = 8;
  mutation.site.placement = RA_JOIN_SPILL_AFTER_ANCHOR;
  if (register_allocator_prepare_join_spill_emission("coreJoinEmitFall", 3,
      -1, &mutation, YES, -7, 1, &emission) == FAILED ||
      emission.emit != YES || emission.site.predecessor_block != 2 ||
      emission.site.edge_kind != RA_CFG_EDGE_FALLTHROUGH ||
      emission.site.anchor_instruction != 8 ||
      emission.site.placement != RA_JOIN_SPILL_AFTER_ANCHOR ||
      emission.destination_offset != -7 || emission.byte_count != 1)
    return FAILED;

  mutation.apply = NO;
  mutation.site.predecessor_block = -1;
  mutation.site.edge_kind = 0;
  mutation.site.anchor_instruction = -1;
  mutation.site.placement = 0;
  mutation.temp_index = -1;
  mutation.physical_register = -1;
  if (register_allocator_prepare_join_spill_emission("coreJoinEmitEmpty", 3,
      -1, &mutation, NO, 0, 0, &emission) == FAILED ||
      join_spill_emission_is_empty(&emission, -1) == NO)
    return FAILED;

  initialize_join_spill_mutation(&mutation);
  if (expect_invalid_join_spill_emission(NULL, 3, -1, &mutation, YES, -6, 2,
      &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitBlock", -1, -1,
      &mutation, YES, -6, 2, &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitNullMutation", 3, -1,
      NULL, YES, -6, 2, &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitNullOutput", 3, -1,
      &mutation, YES, -6, 2, NULL) == FAILED)
    return FAILED;

  mutation.apply = 99;
  if (expect_invalid_join_spill_emission("coreJoinEmitApply", 3, -1,
      &mutation, YES, -6, 2, &emission) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.predecessor_block = -1;
  if (expect_invalid_join_spill_emission("coreJoinEmitPredecessor", 3, -1,
      &mutation, YES, -6, 2, &emission) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  if (expect_invalid_join_spill_emission("coreJoinEmitPlacement", 3, -1,
      &mutation, YES, -6, 2, &emission) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.temp_index = -1;
  if (expect_invalid_join_spill_emission("coreJoinEmitTemp", 3, -1,
      &mutation, YES, -6, 2, &emission) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.physical_register = -1;
  if (expect_invalid_join_spill_emission("coreJoinEmitRegister", 3, -1,
      &mutation, YES, -6, 2, &emission) == FAILED)
    return FAILED;

  initialize_join_spill_mutation(&mutation);
  if (expect_invalid_join_spill_emission("coreJoinEmitNoSlot", 3, -1,
      &mutation, NO, -6, 2, &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitSpillFlag", 3, -1,
      &mutation, 99, -6, 2, &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitBytes", 3, -1,
      &mutation, YES, -6, 0, &emission) == FAILED)
    return FAILED;

  mutation.apply = NO;
  mutation.site.predecessor_block = -1;
  mutation.site.edge_kind = 0;
  mutation.site.anchor_instruction = -1;
  mutation.site.placement = 0;
  mutation.temp_index = -1;
  mutation.physical_register = -1;
  if (expect_invalid_join_spill_emission("coreJoinEmitEmptySlot", 3, -1,
      &mutation, YES, 0, 0, &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitEmptyOffset", 3, -1,
      &mutation, NO, -1, 0, &emission) == FAILED ||
      expect_invalid_join_spill_emission("coreJoinEmitEmptyBytes", 3, -1,
      &mutation, NO, 0, 1, &emission) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct join_spill_emission_apply_context {
  int calls;
  int reject;
  int join_block_index;
  struct register_allocator_join_spill_site site;
  int temp_index;
  int physical_register;
  int destination_offset;
  int byte_count;
};

static int apply_join_spill_emission(void *context, int join_block_index,
    struct register_allocator_join_spill_site *site, int temp_index,
    int physical_register, int destination_offset, int byte_count) {

  struct join_spill_emission_apply_context *apply_context;

  apply_context = (struct join_spill_emission_apply_context *)context;
  apply_context->calls++;
  apply_context->join_block_index = join_block_index;
  apply_context->site = *site;
  apply_context->temp_index = temp_index;
  apply_context->physical_register = physical_register;
  apply_context->destination_offset = destination_offset;
  apply_context->byte_count = byte_count;
  return apply_context->reject == YES ? FAILED : SUCCEEDED;
}

static void initialize_join_spill_emission(
    struct register_allocator_join_spill_emission *emission) {

  emission->emit = YES;
  emission->site.predecessor_block = 1;
  emission->site.edge_kind = RA_CFG_EDGE_JUMP;
  emission->site.anchor_instruction = 5;
  emission->site.placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  emission->temp_index = 2;
  emission->physical_register = 4;
  emission->destination_offset = -6;
  emission->byte_count = 2;
}

static int expect_invalid_join_spill_emission_apply(char *function_name,
    int join_block_index, int no_physical_register,
    struct join_spill_emission_apply_context *context,
    struct register_allocator_join_spill_emission *emission,
    register_allocator_join_spill_emission_applier apply_emission) {

  int calls;

  calls = context != NULL ? context->calls : 0;
  if (register_allocator_apply_join_spill_emission(function_name,
      join_block_index, no_physical_register, context, emission,
      apply_emission) != FAILED)
    return FAILED;
  if (context != NULL && context->calls != calls)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_spill_emission_application(void) {

  struct join_spill_emission_apply_context context;
  struct register_allocator_join_spill_emission emission;

  context.calls = 0;
  context.reject = NO;
  initialize_join_spill_emission(&emission);
  if (register_allocator_apply_join_spill_emission("coreJoinEmitApply", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED ||
      context.calls != 1 || context.join_block_index != 3 ||
      context.site.predecessor_block != 1 ||
      context.site.edge_kind != RA_CFG_EDGE_JUMP ||
      context.site.anchor_instruction != 5 ||
      context.site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      context.temp_index != 2 || context.physical_register != 4 ||
      context.destination_offset != -6 || context.byte_count != 2 ||
      emission.emit != YES || emission.destination_offset != -6 ||
      emission.byte_count != 2)
    return FAILED;

  emission.site.predecessor_block = 2;
  emission.site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  emission.site.anchor_instruction = 8;
  emission.site.placement = RA_JOIN_SPILL_AFTER_ANCHOR;
  emission.destination_offset = -7;
  emission.byte_count = 1;
  if (register_allocator_apply_join_spill_emission("coreJoinEmitApplyFall", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED ||
      context.calls != 2 || context.site.predecessor_block != 2 ||
      context.site.edge_kind != RA_CFG_EDGE_FALLTHROUGH ||
      context.site.anchor_instruction != 8 ||
      context.site.placement != RA_JOIN_SPILL_AFTER_ANCHOR ||
      context.destination_offset != -7 || context.byte_count != 1)
    return FAILED;

  context.reject = YES;
  if (register_allocator_apply_join_spill_emission("coreJoinEmitReject", 3,
      -1, &context, &emission, apply_join_spill_emission) != FAILED ||
      context.calls != 3)
    return FAILED;
  context.reject = NO;

  emission.emit = NO;
  emission.site.predecessor_block = -1;
  emission.site.edge_kind = 0;
  emission.site.anchor_instruction = -1;
  emission.site.placement = 0;
  emission.temp_index = -1;
  emission.physical_register = -1;
  emission.destination_offset = 0;
  emission.byte_count = 0;
  if (register_allocator_apply_join_spill_emission("coreJoinEmitSkip", 3,
      -1, NULL, &emission, NULL) == FAILED || context.calls != 3)
    return FAILED;

  initialize_join_spill_emission(&emission);
  if (expect_invalid_join_spill_emission_apply(NULL, 3, -1, &context,
      &emission, apply_join_spill_emission) == FAILED ||
      expect_invalid_join_spill_emission_apply("coreJoinEmitApplyBlock", -1,
      -1, &context, &emission, apply_join_spill_emission) == FAILED ||
      expect_invalid_join_spill_emission_apply("coreJoinEmitApplyNull", 3,
      -1, &context, NULL, apply_join_spill_emission) == FAILED ||
      expect_invalid_join_spill_emission_apply("coreJoinEmitApplyCallback", 3,
      -1, &context, &emission, NULL) == FAILED)
    return FAILED;

  emission.emit = 99;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyFlag", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED)
    return FAILED;
  initialize_join_spill_emission(&emission);
  emission.site.predecessor_block = -1;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyPred", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED)
    return FAILED;
  initialize_join_spill_emission(&emission);
  emission.site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyPlace", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED)
    return FAILED;
  initialize_join_spill_emission(&emission);
  emission.temp_index = -1;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyTemp", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED)
    return FAILED;
  initialize_join_spill_emission(&emission);
  emission.physical_register = -1;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyPhy", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED)
    return FAILED;
  initialize_join_spill_emission(&emission);
  emission.byte_count = 0;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyBytes", 3,
      -1, &context, &emission, apply_join_spill_emission) == FAILED)
    return FAILED;

  emission.emit = NO;
  emission.site.predecessor_block = -1;
  emission.site.edge_kind = 0;
  emission.site.anchor_instruction = -1;
  emission.site.placement = 0;
  emission.temp_index = -1;
  emission.physical_register = -1;
  emission.destination_offset = 1;
  emission.byte_count = 0;
  if (expect_invalid_join_spill_emission_apply("coreJoinEmitApplyEmpty", 3,
      -1, &context, &emission, NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct join_spill_apply_context {
  int calls;
  int reject;
  int join_block_index;
  struct register_allocator_join_spill_site site;
  int temp_index;
  int physical_register;
};

static int apply_join_spill_mutation(void *context, int join_block_index,
    struct register_allocator_join_spill_site *site, int temp_index,
    int physical_register) {

  struct join_spill_apply_context *apply_context;

  apply_context = (struct join_spill_apply_context *)context;
  apply_context->calls++;
  apply_context->join_block_index = join_block_index;
  apply_context->site = *site;
  apply_context->temp_index = temp_index;
  apply_context->physical_register = physical_register;
  return apply_context->reject == YES ? FAILED : SUCCEEDED;
}

static int expect_invalid_join_spill_mutation_apply(char *function_name,
    int join_block_index, int no_physical_register,
    struct join_spill_apply_context *context,
    struct register_allocator_join_spill_mutation *mutation,
    register_allocator_join_spill_mutation_applier apply_mutation) {

  int calls;

  calls = context != NULL ? context->calls : 0;
  if (register_allocator_apply_join_spill_mutation(function_name,
      join_block_index, no_physical_register, context, mutation,
      apply_mutation) != FAILED)
    return FAILED;
  if (context != NULL && context->calls != calls)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_join_spill_mutation(
    struct register_allocator_join_spill_mutation *mutation) {

  mutation->apply = YES;
  mutation->site.predecessor_block = 1;
  mutation->site.edge_kind = RA_CFG_EDGE_JUMP;
  mutation->site.anchor_instruction = 5;
  mutation->site.placement = RA_JOIN_SPILL_BEFORE_ANCHOR;
  mutation->temp_index = 2;
  mutation->physical_register = 4;
}

static int test_join_spill_mutation_application(void) {

  struct join_spill_apply_context context;
  struct register_allocator_join_spill_mutation mutation;

  context.calls = 0;
  context.reject = NO;
  context.join_block_index = -1;
  context.site.predecessor_block = -1;
  context.site.edge_kind = 0;
  context.site.anchor_instruction = -1;
  context.site.placement = 0;
  context.temp_index = -1;
  context.physical_register = -1;
  initialize_join_spill_mutation(&mutation);
  if (register_allocator_apply_join_spill_mutation("coreJoinApply", 3, -1,
      &context, &mutation, apply_join_spill_mutation) == FAILED ||
      context.calls != 1 || context.join_block_index != 3 ||
      context.site.predecessor_block != 1 ||
      context.site.edge_kind != RA_CFG_EDGE_JUMP ||
      context.site.anchor_instruction != 5 ||
      context.site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      context.temp_index != 2 || context.physical_register != 4 ||
      mutation.apply != YES || mutation.site.predecessor_block != 1 ||
      mutation.site.edge_kind != RA_CFG_EDGE_JUMP ||
      mutation.site.anchor_instruction != 5 ||
      mutation.site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR ||
      mutation.temp_index != 2 || mutation.physical_register != 4)
    return FAILED;

  context.reject = YES;
  if (register_allocator_apply_join_spill_mutation("coreJoinApplyReject", 3,
      -1, &context, &mutation, apply_join_spill_mutation) != FAILED ||
      context.calls != 2)
    return FAILED;
  context.reject = NO;

  mutation.apply = NO;
  mutation.site.predecessor_block = -1;
  mutation.site.edge_kind = 0;
  mutation.site.anchor_instruction = -1;
  mutation.site.placement = 0;
  mutation.temp_index = -1;
  mutation.physical_register = -1;
  if (register_allocator_apply_join_spill_mutation("coreJoinApplyEmpty", 3,
      -1, NULL, &mutation, NULL) == FAILED)
    return FAILED;

  initialize_join_spill_mutation(&mutation);
  if (expect_invalid_join_spill_mutation_apply(NULL, 3, -1, &context,
      &mutation, apply_join_spill_mutation) == FAILED ||
      expect_invalid_join_spill_mutation_apply("coreJoinApplyBlock", -1, -1,
      &context, &mutation, apply_join_spill_mutation) == FAILED ||
      expect_invalid_join_spill_mutation_apply("coreJoinApplyNull", 3, -1,
      &context, NULL, apply_join_spill_mutation) == FAILED ||
      expect_invalid_join_spill_mutation_apply("coreJoinApplyCallback", 3,
      -1, &context, &mutation, NULL) == FAILED)
    return FAILED;

  mutation.apply = 99;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyFlag", 3, -1,
      &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.predecessor_block = -1;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyPredecessor", 3,
      -1, &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.anchor_instruction = -1;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyAnchor", 3, -1,
      &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.edge_kind = 99;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyEdge", 3, -1,
      &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.placement = 99;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyPlacement", 3,
      -1, &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.placement = RA_JOIN_SPILL_AFTER_ANCHOR;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyJumpPlacement",
      3, -1, &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.site.edge_kind = RA_CFG_EDGE_FALLTHROUGH;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyFallPlacement",
      3, -1, &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.temp_index = -1;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyTemp", 3, -1,
      &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;
  initialize_join_spill_mutation(&mutation);
  mutation.physical_register = -1;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyRegister", 3,
      -1, &context, &mutation, apply_join_spill_mutation) == FAILED)
    return FAILED;

  mutation.apply = NO;
  mutation.site.predecessor_block = -1;
  mutation.site.edge_kind = 0;
  mutation.site.anchor_instruction = -1;
  mutation.site.placement = 0;
  mutation.temp_index = 0;
  mutation.physical_register = -1;
  if (expect_invalid_join_spill_mutation_apply("coreJoinApplyNonempty", 3,
      -1, &context, &mutation, NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct join_context {
  int stack_resident[3];
  int join_calls;
  int temp_calls;
  int invalid_join_result;
  int invalid_temp;
};

static int join_is_stack_resident(void *context, int block_index,
    int predecessor_count, int temp_index) {

  struct join_context *join_context;

  join_context = (struct join_context *)context;
  if (temp_index < 0) {
    join_context->join_calls++;
    if (join_context->invalid_join_result == YES)
      return 2;
    return block_index >= 0 && predecessor_count > 1 ? YES : NO;
  }
  join_context->temp_calls++;
  if (temp_index == join_context->invalid_temp)
    return 2;
  return join_context->stack_resident[temp_index];
}

static void initialize_join_context(struct join_context *context) {

  int temp_index;

  for (temp_index = 0; temp_index < 3; temp_index++)
    context->stack_resident[temp_index] = YES;
  context->join_calls = 0;
  context->temp_calls = 0;
  context->invalid_join_result = NO;
  context->invalid_temp = -1;
}

static int test_join_reconciliation(void) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_cfg_edge edges[4];
  struct join_context context;
  char live_in[12];
  int cell_index;

  if (register_allocator_plan_liveness_storage("coreJoinDiamond", 4, 3,
      &storage) == FAILED)
    return FAILED;
  for (cell_index = 0; cell_index < 12; cell_index++)
    live_in[cell_index] = NO;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[1].from_block = 0;
  edges[1].to_block = 2;
  edges[2].from_block = 1;
  edges[2].to_block = 3;
  edges[3].from_block = 2;
  edges[3].to_block = 3;
  live_in[9] = YES;
  live_in[11] = YES;
  initialize_join_context(&context);
  if (register_allocator_reconcile_stack_only_joins("coreJoinDiamond", 4, 3,
      &storage, edges, 4, live_in, &context,
      join_is_stack_resident) == FAILED || context.join_calls != 1 ||
      context.temp_calls != 2)
    return FAILED;

  initialize_join_context(&context);
  context.stack_resident[2] = NO;
  if (register_allocator_reconcile_stack_only_joins("coreJoinRetained", 4, 3,
      &storage, edges, 4, live_in, &context,
      join_is_stack_resident) != FAILED || context.join_calls != 1 ||
      context.temp_calls != 2)
    return FAILED;

  initialize_join_context(&context);
  context.invalid_join_result = YES;
  if (register_allocator_reconcile_stack_only_joins("coreJoinInvalidBlock", 4,
      3, &storage, edges, 4, live_in, &context,
      join_is_stack_resident) != FAILED || context.join_calls != 1 ||
      context.temp_calls != 0)
    return FAILED;

  initialize_join_context(&context);
  context.invalid_temp = 0;
  if (register_allocator_reconcile_stack_only_joins("coreJoinInvalidTemp", 4,
      3, &storage, edges, 4, live_in, &context,
      join_is_stack_resident) != FAILED || context.join_calls != 1 ||
      context.temp_calls != 1)
    return FAILED;

  initialize_join_context(&context);
  live_in[9] = 2;
  if (register_allocator_reconcile_stack_only_joins("coreJoinInvalidLiveIn", 4,
      3, &storage, edges, 4, live_in, &context,
      join_is_stack_resident) != FAILED || context.join_calls != 1 ||
      context.temp_calls != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_join_no_join_graphs(void) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_cfg_edge edges[2];
  struct join_context context;
  char live_in[3];

  if (register_allocator_plan_liveness_storage("coreJoinNoEdges", 1, 1,
      &storage) == FAILED)
    return FAILED;
  live_in[0] = NO;
  initialize_join_context(&context);
  if (register_allocator_reconcile_stack_only_joins("coreJoinNoEdges", 1, 1,
      &storage, NULL, 0, live_in, &context,
      join_is_stack_resident) == FAILED || context.join_calls != 0)
    return FAILED;

  if (register_allocator_plan_liveness_storage("coreJoinLoop", 3, 1,
      &storage) == FAILED)
    return FAILED;
  live_in[0] = YES;
  live_in[1] = YES;
  live_in[2] = NO;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[1].from_block = 1;
  edges[1].to_block = 0;
  initialize_join_context(&context);
  if (register_allocator_reconcile_stack_only_joins("coreJoinLoop", 3, 1,
      &storage, edges, 2, live_in, &context,
      join_is_stack_resident) == FAILED || context.join_calls != 0 ||
      context.temp_calls != 0)
    return FAILED;
  return SUCCEEDED;
}

static int test_invalid_join_inputs(void) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_cfg_edge edge;
  struct join_context context;
  char live_in[2];

  if (register_allocator_plan_liveness_storage("coreJoinInvalidInputs", 2, 1,
      &storage) == FAILED)
    return FAILED;
  edge.from_block = 0;
  edge.to_block = 1;
  live_in[0] = NO;
  live_in[1] = NO;
  initialize_join_context(&context);
  if (register_allocator_reconcile_stack_only_joins(NULL, 2, 1, &storage,
      &edge, 1, live_in, &context, join_is_stack_resident) != FAILED ||
      register_allocator_reconcile_stack_only_joins("coreJoinNullStorage", 2,
      1, NULL, &edge, 1, live_in, &context,
      join_is_stack_resident) != FAILED ||
      register_allocator_reconcile_stack_only_joins("coreJoinNegativeEdges", 2,
      1, &storage, &edge, -1, live_in, &context,
      join_is_stack_resident) != FAILED ||
      register_allocator_reconcile_stack_only_joins("coreJoinNullEdges", 2, 1,
      &storage, NULL, 1, live_in, &context,
      join_is_stack_resident) != FAILED ||
      register_allocator_reconcile_stack_only_joins("coreJoinNullLiveIn", 2, 1,
      &storage, &edge, 1, NULL, &context,
      join_is_stack_resident) != FAILED ||
      register_allocator_reconcile_stack_only_joins("coreJoinNullContext", 2,
      1, &storage, &edge, 1, live_in, NULL,
      join_is_stack_resident) != FAILED ||
      register_allocator_reconcile_stack_only_joins("coreJoinNullCallback", 2,
      1, &storage, &edge, 1, live_in, &context, NULL) != FAILED)
    return FAILED;

  storage.set_count = 1;
  if (register_allocator_reconcile_stack_only_joins("coreJoinBadStorage", 2, 1,
      &storage, &edge, 1, live_in, &context,
      join_is_stack_resident) != FAILED)
    return FAILED;
  storage.set_count = 2;
  edge.to_block = 2;
  if (register_allocator_reconcile_stack_only_joins("coreJoinBadEdge", 2, 1,
      &storage, &edge, 1, live_in, &context,
      join_is_stack_resident) != FAILED)
    return FAILED;
  if (register_allocator_reconcile_stack_only_joins("coreJoinEmpty", 0, 1,
      NULL, NULL, 0, NULL, NULL, NULL) == FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_join(struct register_allocator_loop_join *loop_join) {

  loop_join->status = 99;
  loop_join->entry_predecessor = 99;
  loop_join->latch_predecessor = 99;
  loop_join->entry_edge = 99;
  loop_join->back_edge = 99;
}

static int test_loop_join_classification(void) {

  struct register_allocator_cfg_edge edges[4];
  struct register_allocator_loop_join loop_join;

  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 3;
  edges[1].to_block = 1;
  edges[1].kind = RA_CFG_EDGE_JUMP;
  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join("coreLoopJoinReady", 4, 1,
      edges, 2, &loop_join) == FAILED ||
      loop_join.status != RA_LOOP_JOIN_READY ||
      loop_join.entry_predecessor != 0 ||
      loop_join.latch_predecessor != 3 || loop_join.entry_edge != 0 ||
      loop_join.back_edge != 1)
    return FAILED;

  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join("coreLoopJoinForward", 4, 2,
      edges, 1, &loop_join) == FAILED ||
      loop_join.status != RA_LOOP_JOIN_NOT_LOOP ||
      loop_join.entry_predecessor != -1 ||
      loop_join.latch_predecessor != -1)
    return FAILED;
  edges[0].from_block = 1;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_JUMP;
  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join("coreLoopJoinSelf", 4, 1,
      edges, 1, &loop_join) == FAILED ||
      loop_join.status != RA_LOOP_JOIN_NOT_LOOP ||
      loop_join.entry_predecessor != -1 ||
      loop_join.latch_predecessor != 1)
    return FAILED;

  edges[0].from_block = 0;
  edges[0].to_block = 2;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[2].from_block = 3;
  edges[2].to_block = 2;
  edges[2].kind = RA_CFG_EDGE_JUMP;
  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join("coreLoopJoinEntries", 4, 2,
      edges, 3, &loop_join) == FAILED ||
      loop_join.status != RA_LOOP_JOIN_AMBIGUOUS ||
      loop_join.entry_predecessor != 0 || loop_join.latch_predecessor != 3)
    return FAILED;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[1].from_block = 2;
  edges[1].to_block = 1;
  edges[2].from_block = 3;
  edges[2].to_block = 1;
  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join("coreLoopJoinLatches", 4, 1,
      edges, 3, &loop_join) == FAILED ||
      loop_join.status != RA_LOOP_JOIN_AMBIGUOUS ||
      loop_join.entry_predecessor != 0 || loop_join.latch_predecessor != 2)
    return FAILED;

  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join(NULL, 4, 1, edges, 3,
      &loop_join) != FAILED || loop_join.status != RA_LOOP_JOIN_NOT_LOOP ||
      loop_join.entry_predecessor != -1 || loop_join.back_edge != -1)
    return FAILED;
  if (register_allocator_classify_loop_join("coreLoopJoinBlocks", 0, 1,
      edges, 3, &loop_join) != FAILED ||
      register_allocator_classify_loop_join("coreLoopJoinBlock", 4, 4,
      edges, 3, &loop_join) != FAILED ||
      register_allocator_classify_loop_join("coreLoopJoinCount", 4, 1,
      edges, -1, &loop_join) != FAILED ||
      register_allocator_classify_loop_join("coreLoopJoinEdges", 4, 1,
      NULL, 1, &loop_join) != FAILED ||
      register_allocator_classify_loop_join("coreLoopJoinOutput", 4, 1,
      edges, 3, NULL) != FAILED)
    return FAILED;
  edges[1].from_block = 4;
  initialize_loop_join(&loop_join);
  if (register_allocator_classify_loop_join("coreLoopJoinInvalidEdge", 4, 1,
      edges, 3, &loop_join) != FAILED ||
      loop_join.status != RA_LOOP_JOIN_NOT_LOOP ||
      loop_join.entry_predecessor != 0 || loop_join.latch_predecessor != -1)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_flow_profiling(void) {

  struct register_allocator_basic_block blocks[6];
  struct register_allocator_cfg_edge edges[7];
  struct register_allocator_loop_flow_profile profile;
  int block_index;

  for (block_index = 0; block_index < 6; block_index++) {
    blocks[block_index].start_tac = block_index * 2;
    blocks[block_index].end_tac = block_index * 2 + 1;
    blocks[block_index].end_reason = RA_BLOCK_END_JUMP;
  }
  blocks[5].end_reason = RA_BLOCK_END_RETURN;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 5;
  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[2].from_block = 1;
  edges[2].to_block = 2;
  edges[2].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[3].from_block = 2;
  edges[3].to_block = 4;
  edges[3].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[4].from_block = 2;
  edges[4].to_block = 3;
  edges[4].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[5].from_block = 3;
  edges[5].to_block = 5;
  edges[5].kind = RA_CFG_EDGE_JUMP;
  edges[6].from_block = 4;
  edges[6].to_block = 1;
  edges[6].kind = RA_CFG_EDGE_JUMP;
  if (register_allocator_profile_loop_flow("coreLoopFlowBreak", 6, blocks, 1,
      edges, 7, &profile) == FAILED ||
      profile.status != RA_LOOP_JOIN_READY || profile.entry_edge_count != 1 ||
      profile.back_edge_count != 1 || profile.exit_edge_count != 2 ||
      profile.terminal_exit_count != 1)
    return FAILED;

  blocks[3].end_reason = RA_BLOCK_END_RETURN;
  edges[5].from_block = 4;
  edges[5].to_block = 1;
  if (register_allocator_profile_loop_flow("coreLoopFlowEarlyReturn", 6,
      blocks, 1, edges, 6, &profile) == FAILED ||
      profile.status != RA_LOOP_JOIN_READY || profile.entry_edge_count != 1 ||
      profile.back_edge_count != 1 || profile.exit_edge_count != 2 ||
      profile.terminal_exit_count != 2)
    return FAILED;

  blocks[3].end_reason = RA_BLOCK_END_JUMP;
  edges[5].from_block = 3;
  edges[5].to_block = 1;
  edges[6].from_block = 4;
  edges[6].to_block = 1;
  if (register_allocator_profile_loop_flow("coreLoopFlowContinue", 6, blocks,
      1, edges, 7, &profile) == FAILED ||
      profile.status != RA_LOOP_JOIN_AMBIGUOUS ||
      profile.entry_edge_count != 1 || profile.back_edge_count != 2 ||
      profile.exit_edge_count != 1 || profile.terminal_exit_count != 1)
    return FAILED;

  if (register_allocator_profile_loop_flow(NULL, 6, blocks, 1, edges, 7,
      &profile) != FAILED ||
      register_allocator_profile_loop_flow("coreLoopFlowBlocks", 0, blocks, 1,
      edges, 7, &profile) != FAILED ||
      register_allocator_profile_loop_flow("coreLoopFlowBlock", 6, blocks, 6,
      edges, 7, &profile) != FAILED ||
      register_allocator_profile_loop_flow("coreLoopFlowEdges", 6, blocks, 1,
      NULL, 7, &profile) != FAILED ||
      register_allocator_profile_loop_flow("coreLoopFlowOutput", 6, blocks, 1,
      edges, 7, NULL) != FAILED)
    return FAILED;
  edges[6].to_block = 6;
  if (register_allocator_profile_loop_flow("coreLoopFlowInvalidEdge", 6,
      blocks, 1, edges, 7, &profile) != FAILED)
    return FAILED;
  edges[6].to_block = 1;
  blocks[5].end_reason = 99;
  if (register_allocator_profile_loop_flow("coreLoopFlowInvalidBlock", 6,
      blocks, 1, edges, 7, &profile) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_candidate(
    struct register_allocator_loop_candidate *candidate) {

  candidate->status = 99;
  candidate->reason = 99;
  candidate->candidate_count = 99;
  candidate->temp_index = 99;
}

static int test_loop_join_candidate_planning(void) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_candidate candidate;
  char live_in[8];

  memset(live_in, NO, sizeof(live_in));
  storage.set_count = 8;
  storage.buffer_bytes = 8;
  storage.total_bytes = 32;
  loop_join.status = RA_LOOP_JOIN_READY;
  loop_join.entry_predecessor = 0;
  loop_join.latch_predecessor = 1;
  loop_join.entry_edge = 0;
  loop_join.back_edge = 1;

  live_in[6] = YES;
  initialize_loop_candidate(&candidate);
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateReady", 2,
      4, &storage, 1, &loop_join, live_in, &candidate) == FAILED ||
      candidate.status != RA_LOOP_CANDIDATE_READY ||
      candidate.reason != RA_LOOP_CANDIDATE_REASON_NONE ||
      candidate.candidate_count != 1 || candidate.temp_index != 2)
    return FAILED;

  live_in[6] = NO;
  initialize_loop_candidate(&candidate);
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateEmpty", 2,
      4, &storage, 1, &loop_join, live_in, &candidate) == FAILED ||
      candidate.status != RA_LOOP_CANDIDATE_INELIGIBLE ||
      candidate.reason != RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN ||
      candidate.candidate_count != 0 || candidate.temp_index != -1)
    return FAILED;

  live_in[5] = YES;
  live_in[7] = YES;
  initialize_loop_candidate(&candidate);
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateMultiple",
      2, 4, &storage, 1, &loop_join, live_in, &candidate) == FAILED ||
      candidate.status != RA_LOOP_CANDIDATE_READY ||
      candidate.reason != RA_LOOP_CANDIDATE_REASON_NONE ||
      candidate.candidate_count != 2 || candidate.temp_index != 1)
    return FAILED;

  live_in[7] = NO;
  loop_join.status = RA_LOOP_JOIN_AMBIGUOUS;
  initialize_loop_candidate(&candidate);
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateTopology",
      2, 4, &storage, 1, &loop_join, live_in, &candidate) == FAILED ||
      candidate.status != RA_LOOP_CANDIDATE_AMBIGUOUS ||
      candidate.reason != RA_LOOP_CANDIDATE_REASON_TOPOLOGY ||
      candidate.candidate_count != 1 || candidate.temp_index != -1)
    return FAILED;

  loop_join.status = RA_LOOP_JOIN_NOT_LOOP;
  initialize_loop_candidate(&candidate);
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateIneligible",
      2, 4, &storage, 1, &loop_join, live_in, &candidate) == FAILED ||
      candidate.status != RA_LOOP_CANDIDATE_INELIGIBLE ||
      candidate.reason != RA_LOOP_CANDIDATE_REASON_TOPOLOGY ||
      candidate.candidate_count != 0 || candidate.temp_index != -1)
    return FAILED;

  loop_join.status = RA_LOOP_JOIN_READY;
  live_in[5] = 2;
  initialize_loop_candidate(&candidate);
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateValue", 2,
      4, &storage, 1, &loop_join, live_in, &candidate) != FAILED ||
      candidate.status != RA_LOOP_CANDIDATE_INELIGIBLE ||
      candidate.reason != RA_LOOP_CANDIDATE_REASON_NONE ||
      candidate.candidate_count != 0 || candidate.temp_index != -1)
    return FAILED;
  live_in[5] = NO;

  storage.set_count = 7;
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateStorage",
      2, 4, &storage, 1, &loop_join, live_in, &candidate) != FAILED)
    return FAILED;
  storage.set_count = 8;
  if (register_allocator_plan_loop_join_candidate(NULL, 2, 4, &storage, 1,
      &loop_join, live_in, &candidate) != FAILED ||
      register_allocator_plan_loop_join_candidate("coreLoopCandidateBlocks", 0,
      4, &storage, 1, &loop_join, live_in, &candidate) != FAILED ||
      register_allocator_plan_loop_join_candidate("coreLoopCandidateTemps", 2,
      0, &storage, 1, &loop_join, live_in, &candidate) != FAILED ||
      register_allocator_plan_loop_join_candidate("coreLoopCandidateBlock", 2,
      4, &storage, 2, &loop_join, live_in, &candidate) != FAILED ||
      register_allocator_plan_loop_join_candidate("coreLoopCandidateLive", 2,
      4, &storage, 1, &loop_join, NULL, &candidate) != FAILED ||
      register_allocator_plan_loop_join_candidate("coreLoopCandidateOutput", 2,
      4, &storage, 1, &loop_join, live_in, NULL) != FAILED)
    return FAILED;
  loop_join.status = 99;
  if (register_allocator_plan_loop_join_candidate("coreLoopCandidateStatus", 2,
      4, &storage, 1, &loop_join, live_in, &candidate) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_representation_classification(void) {

  struct register_allocator_loop_candidate candidate;
  struct register_allocator_loop_representation representation;

  candidate.status = RA_LOOP_CANDIDATE_READY;
  candidate.reason = RA_LOOP_CANDIDATE_REASON_NONE;
  candidate.candidate_count = 1;
  candidate.temp_index = 2;
  if (register_allocator_classify_loop_representation(
      "coreLoopRepresentationTemp", 1, &candidate, 2,
      &representation) == FAILED ||
      representation.status != RA_LOOP_REPRESENTATION_TEMP_READY ||
      representation.stack_value_count != 2)
    return FAILED;

  candidate.status = RA_LOOP_CANDIDATE_INELIGIBLE;
  candidate.reason = RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN;
  candidate.candidate_count = 0;
  candidate.temp_index = -1;
  if (register_allocator_classify_loop_representation(
      "coreLoopRepresentationEmpty", 1, &candidate, 0,
      &representation) == FAILED ||
      representation.status != RA_LOOP_REPRESENTATION_NO_VALUE ||
      register_allocator_classify_loop_representation(
      "coreLoopRepresentationUnique", 1, &candidate, 1,
      &representation) == FAILED ||
      representation.status != RA_LOOP_REPRESENTATION_UNIQUE_STACK_VALUE ||
      register_allocator_classify_loop_representation(
      "coreLoopRepresentationMultiple", 1, &candidate, 3,
      &representation) == FAILED ||
      representation.status !=
      RA_LOOP_REPRESENTATION_AMBIGUOUS_STACK_VALUES ||
      representation.stack_value_count != 3)
    return FAILED;

  candidate.status = RA_LOOP_CANDIDATE_AMBIGUOUS;
  candidate.reason = RA_LOOP_CANDIDATE_REASON_TOPOLOGY;
  if (register_allocator_classify_loop_representation(
      "coreLoopRepresentationTopology", 1, &candidate, 1,
      &representation) == FAILED ||
      representation.status != RA_LOOP_REPRESENTATION_NOT_APPLICABLE)
    return FAILED;

  candidate.status = RA_LOOP_CANDIDATE_READY;
  candidate.reason = RA_LOOP_CANDIDATE_REASON_NONE;
  candidate.candidate_count = 0;
  candidate.temp_index = -1;
  representation.status = 99;
  representation.stack_value_count = 99;
  if (register_allocator_classify_loop_representation(
      "coreLoopRepresentationCandidate", 1, &candidate, 1,
      &representation) != FAILED ||
      representation.status != RA_LOOP_REPRESENTATION_NOT_APPLICABLE ||
      representation.stack_value_count != 0)
    return FAILED;
  candidate.status = RA_LOOP_CANDIDATE_INELIGIBLE;
  candidate.reason = RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN;
  candidate.candidate_count = 0;
  candidate.temp_index = -1;
  if (register_allocator_classify_loop_representation(NULL, 1, &candidate, 1,
      &representation) != FAILED ||
      register_allocator_classify_loop_representation(
      "coreLoopRepresentationBlock", -1, &candidate, 1,
      &representation) != FAILED ||
      register_allocator_classify_loop_representation(
      "coreLoopRepresentationNullCandidate", 1, NULL, 1,
      &representation) != FAILED ||
      register_allocator_classify_loop_representation(
      "coreLoopRepresentationCount", 1, &candidate, -1,
      &representation) != FAILED ||
      register_allocator_classify_loop_representation(
      "coreLoopRepresentationOutput", 1, &candidate, 1, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_stack_value_collection(void) {

  struct register_allocator_basic_block blocks[4];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_stack_value_observation observations[7];
  int identities[4];
  int identity_count;

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 2;
  blocks[1].start_tac = 3;
  blocks[1].end_tac = 5;
  blocks[2].start_tac = 6;
  blocks[2].end_tac = 8;
  blocks[3].start_tac = 9;
  blocks[3].end_tac = 11;
  loop_join.status = RA_LOOP_JOIN_READY;
  loop_join.entry_predecessor = 0;
  loop_join.latch_predecessor = 2;
  loop_join.entry_edge = 0;
  loop_join.back_edge = 3;
  observations[0].instruction_index = 1;
  observations[0].identity = 5;
  observations[1].instruction_index = 4;
  observations[1].identity = 7;
  observations[2].instruction_index = 7;
  observations[2].identity = 5;
  observations[3].instruction_index = 10;
  observations[3].identity = 99;
  observations[4].instruction_index = 5;
  observations[4].identity = 9;
  observations[5].instruction_index = 6;
  observations[5].identity = 7;
  observations[6].instruction_index = 0;
  observations[6].identity = 5;
  identities[0] = -1;
  identities[1] = -1;
  identities[2] = -1;
  identities[3] = -1;
  identity_count = -1;
  if (register_allocator_collect_loop_stack_values("coreLoopStackValues", 4,
      12, blocks, 1, &loop_join, observations, 7, identities, 4,
      &identity_count) == FAILED || identity_count != 3 ||
      identities[0] != 5 || identities[1] != 7 || identities[2] != 9 ||
      identities[3] != -1)
    return FAILED;

  identities[0] = 41;
  identities[1] = 42;
  identity_count = 99;
  if (register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesCapacity", 4, 12, blocks, 1, &loop_join,
      observations, 7, identities, 2, &identity_count) != FAILED ||
      identity_count != 0 || identities[0] != 41 || identities[1] != 42)
    return FAILED;

  observations[3].instruction_index = 12;
  identity_count = 99;
  if (register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesObservation", 4, 12, blocks, 1, &loop_join,
      observations, 7, identities, 4, &identity_count) != FAILED ||
      identity_count != 0)
    return FAILED;
  observations[3].instruction_index = 10;

  loop_join.status = RA_LOOP_JOIN_AMBIGUOUS;
  identity_count = 99;
  if (register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesTopology", 4, 12, blocks, 1, &loop_join,
      observations, 7, identities, 4, &identity_count) == FAILED ||
      identity_count != 0)
    return FAILED;
  loop_join.status = 99;
  if (register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesTopologyInvalid", 4, 12, blocks, 1, &loop_join,
      observations, 7, identities, 4, &identity_count) != FAILED)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_READY;

  blocks[2].start_tac = 5;
  if (register_allocator_collect_loop_stack_values("coreLoopStackValuesBlock",
      4, 12, blocks, 1, &loop_join, observations, 7, identities, 4,
      &identity_count) != FAILED)
    return FAILED;
  blocks[2].start_tac = 6;
  if (register_allocator_collect_loop_stack_values(NULL, 4, 12, blocks, 1,
      &loop_join, observations, 7, identities, 4, &identity_count) != FAILED ||
      register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesNullObservations", 4, 12, blocks, 1, &loop_join,
      NULL, 1, identities, 4, &identity_count) != FAILED ||
      register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesNullIdentities", 4, 12, blocks, 1, &loop_join,
      observations, 7, NULL, 4, &identity_count) != FAILED ||
      register_allocator_collect_loop_stack_values(
      "coreLoopStackValuesNullCount", 4, 12, blocks, 1, &loop_join,
      observations, 7, identities, 4, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_stack_value_selection(void) {

  struct register_allocator_loop_stack_value_selection selection;
  struct register_allocator_loop_stack_value_roles values[3];

  values[0].identity = 5;
  values[0].role_mask = RA_LOOP_STACK_ROLE_REQUIRED;
  values[1].identity = 7;
  values[1].role_mask = RA_LOOP_STACK_ROLE_HEADER_READ |
      RA_LOOP_STACK_ROLE_LATCH_READ | RA_LOOP_STACK_ROLE_LATCH_WRITE;
  values[2].identity = 9;
  values[2].role_mask = RA_LOOP_STACK_ROLE_ENTRY_WRITE |
      RA_LOOP_STACK_ROLE_HEADER_READ | RA_LOOP_STACK_ROLE_EXIT_READ;
  if (register_allocator_select_loop_stack_value("coreLoopStackSelectReady", 1,
      values, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_STACK_SELECTION_READY ||
      selection.candidate_count != 1 || selection.identity != 5)
    return FAILED;

  values[0].role_mask &= ~RA_LOOP_STACK_ROLE_EXIT_READ;
  if (register_allocator_select_loop_stack_value("coreLoopStackSelectNone", 1,
      values, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_STACK_SELECTION_INELIGIBLE ||
      selection.candidate_count != 0 || selection.identity != -1)
    return FAILED;
  values[0].role_mask = RA_LOOP_STACK_ROLE_REQUIRED;
  values[1].role_mask = RA_LOOP_STACK_ROLE_REQUIRED;
  if (register_allocator_select_loop_stack_value(
      "coreLoopStackSelectAmbiguous", 1, values, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_STACK_SELECTION_READY ||
      selection.candidate_count != 2 || selection.identity != 5)
    return FAILED;
  if (register_allocator_select_loop_stack_value("coreLoopStackSelectEmpty", 1,
      NULL, 0, &selection) == FAILED ||
      selection.status != RA_LOOP_STACK_SELECTION_INELIGIBLE ||
      selection.candidate_count != 0 || selection.identity != -1)
    return FAILED;

  values[1].identity = 5;
  selection.status = 99;
  selection.candidate_count = 99;
  selection.identity = 99;
  if (register_allocator_select_loop_stack_value(
      "coreLoopStackSelectDuplicate", 1, values, 3, &selection) != FAILED ||
      selection.status != RA_LOOP_STACK_SELECTION_INELIGIBLE ||
      selection.candidate_count != 0 || selection.identity != -1)
    return FAILED;
  values[1].identity = 7;
  values[1].role_mask = RA_LOOP_STACK_ROLE_REQUIRED << 1;
  if (register_allocator_select_loop_stack_value("coreLoopStackSelectRoles", 1,
      values, 3, &selection) != FAILED)
    return FAILED;
  values[1].role_mask = RA_LOOP_STACK_ROLE_REQUIRED;
  values[0].identity = -1;
  if (register_allocator_select_loop_stack_value(
      "coreLoopStackSelectIdentity", 1, values, 3, &selection) != FAILED)
    return FAILED;
  values[0].identity = 5;
  if (register_allocator_select_loop_stack_value(NULL, 1, values, 3,
      &selection) != FAILED ||
      register_allocator_select_loop_stack_value("coreLoopStackSelectBlock",
      -1, values, 3, &selection) != FAILED ||
      register_allocator_select_loop_stack_value("coreLoopStackSelectValues",
      1, NULL, 1, &selection) != FAILED ||
      register_allocator_select_loop_stack_value("coreLoopStackSelectCount",
      1, values, -1, &selection) != FAILED ||
      register_allocator_select_loop_stack_value("coreLoopStackSelectOutput",
      1, values, 3, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_stack_promotion_plan(void) {

  int existing_temps[3];
  struct register_allocator_loop_stack_promotion_plan plan;
  struct register_allocator_loop_stack_value_selection selection;

  existing_temps[0] = 0;
  existing_temps[1] = 2;
  existing_temps[2] = 4;
  selection.status = RA_LOOP_STACK_SELECTION_READY;
  selection.candidate_count = 1;
  selection.identity = 5;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionReady8", 1, &selection, 8, 3,
      existing_temps, 3, &plan) == FAILED ||
      plan.status != RA_LOOP_STACK_PROMOTION_READY ||
      plan.reason != RA_LOOP_STACK_PROMOTION_REASON_NONE ||
      plan.identity != 5 || plan.temp_index != 3 || plan.size != 8 ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionReady16", 1, &selection, 16, 3,
      existing_temps, 3, &plan) == FAILED ||
      plan.status != RA_LOOP_STACK_PROMOTION_READY || plan.size != 16)
    return FAILED;
  selection.candidate_count = 2;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionFirst", 1, &selection, 8, 3,
      existing_temps, 3, &plan) == FAILED ||
      plan.status != RA_LOOP_STACK_PROMOTION_READY ||
      plan.identity != 5 || plan.temp_index != 3)
    return FAILED;

  selection.status = RA_LOOP_STACK_SELECTION_INELIGIBLE;
  selection.candidate_count = 0;
  selection.identity = -1;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionIneligible", 1, &selection, 8, 3,
      existing_temps, 3, &plan) == FAILED ||
      plan.status != RA_LOOP_STACK_PROMOTION_INELIGIBLE ||
      plan.reason != RA_LOOP_STACK_PROMOTION_REASON_SELECTION ||
      plan.identity != -1 || plan.temp_index != -1 || plan.size != 0)
    return FAILED;
  selection.status = RA_LOOP_STACK_SELECTION_AMBIGUOUS;
  selection.candidate_count = 2;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionAmbiguous", 1, &selection, 8, 3,
      existing_temps, 3, &plan) == FAILED ||
      plan.status != RA_LOOP_STACK_PROMOTION_INELIGIBLE)
    return FAILED;

  selection.status = RA_LOOP_STACK_SELECTION_READY;
  selection.candidate_count = 1;
  selection.identity = 5;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionSize", 1, &selection, 32, 3,
      existing_temps, 3, &plan) == FAILED ||
      plan.reason != RA_LOOP_STACK_PROMOTION_REASON_SIZE ||
      plan.identity != 5 || plan.temp_index != -1 || plan.size != 32 ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionConflict", 1, &selection, 8, 2,
      existing_temps, 3, &plan) == FAILED ||
      plan.reason != RA_LOOP_STACK_PROMOTION_REASON_TEMP_CONFLICT ||
      plan.temp_index != -1)
    return FAILED;

  selection.candidate_count = 0;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionSelection", 1, &selection, 8, 3,
      existing_temps, 3, &plan) != FAILED ||
      plan.status != RA_LOOP_STACK_PROMOTION_INELIGIBLE ||
      plan.identity != -1 || plan.temp_index != -1 || plan.size != 0)
    return FAILED;
  selection.candidate_count = 1;
  existing_temps[1] = -1;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionTemp", 1, &selection, 8, 3,
      existing_temps, 3, &plan) != FAILED)
    return FAILED;
  existing_temps[1] = 0;
  if (register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionDuplicate", 1, &selection, 8, 3,
      existing_temps, 3, &plan) != FAILED)
    return FAILED;
  existing_temps[1] = 2;
  if (register_allocator_plan_loop_stack_promotion(NULL, 1, &selection, 8, 3,
      existing_temps, 3, &plan) != FAILED ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionBlock", -1, &selection, 8, 3,
      existing_temps, 3, &plan) != FAILED ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionNullSelection", 1, NULL, 8, 3,
      existing_temps, 3, &plan) != FAILED ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionIndex", 1, &selection, 8, -1,
      existing_temps, 3, &plan) != FAILED ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionTemps", 1, &selection, 8, 3, NULL, 1,
      &plan) != FAILED ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionCount", 1, &selection, 8, 3,
      existing_temps, -1, &plan) != FAILED ||
      register_allocator_plan_loop_stack_promotion(
      "coreLoopStackPromotionOutput", 1, &selection, 8, 3,
      existing_temps, 3, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_stack_rewrite_schedule(void) {

  struct register_allocator_loop_stack_promotion_plan promotion;
  struct register_allocator_loop_stack_rewrite_observation observations[6];
  struct register_allocator_loop_stack_rewrite rewrites[5];
  struct register_allocator_loop_stack_rewrite_schedule schedule;

  promotion.status = RA_LOOP_STACK_PROMOTION_READY;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_NONE;
  promotion.identity = 5;
  promotion.temp_index = 3;
  promotion.size = 8;
  observations[0].instruction_index = 2;
  observations[0].operand = TAC_USE_RESULT;
  observations[0].identity = 5;
  observations[0].role = RA_LOOP_STACK_ROLE_ENTRY_WRITE;
  observations[1].instruction_index = 4;
  observations[1].operand = TAC_USE_ARG1;
  observations[1].identity = 5;
  observations[1].role = RA_LOOP_STACK_ROLE_HEADER_READ;
  observations[2].instruction_index = 7;
  observations[2].operand = TAC_USE_ARG1;
  observations[2].identity = 5;
  observations[2].role = RA_LOOP_STACK_ROLE_LATCH_READ;
  observations[3].instruction_index = 8;
  observations[3].operand = TAC_USE_RESULT;
  observations[3].identity = 5;
  observations[3].role = RA_LOOP_STACK_ROLE_LATCH_WRITE;
  observations[4].instruction_index = 10;
  observations[4].operand = TAC_USE_ARG1;
  observations[4].identity = 5;
  observations[4].role = RA_LOOP_STACK_ROLE_EXIT_READ;
  observations[5].instruction_index = 3;
  observations[5].operand = TAC_USE_ARG2;
  observations[5].identity = 7;
  observations[5].role = RA_LOOP_STACK_ROLE_HEADER_READ;
  rewrites[0].instruction_index = -1;
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteReady", 12, &promotion, observations, 6,
      rewrites, 5, &schedule) == FAILED ||
      schedule.status != RA_LOOP_STACK_REWRITE_READY ||
      schedule.reason != RA_LOOP_STACK_REWRITE_REASON_NONE ||
      schedule.rewrite_count != 5 ||
      schedule.role_mask != RA_LOOP_STACK_ROLE_REQUIRED ||
      rewrites[0].instruction_index != 2 ||
      rewrites[0].operand != TAC_USE_RESULT ||
      rewrites[4].instruction_index != 10 ||
      rewrites[4].temp_index != 3 ||
      rewrites[4].role != RA_LOOP_STACK_ROLE_EXIT_READ)
    return FAILED;

  rewrites[0].instruction_index = 99;
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteCapacity", 12, &promotion, observations, 6,
      rewrites, 4, &schedule) != FAILED || schedule.rewrite_count != 0 ||
      rewrites[0].instruction_index != 99)
    return FAILED;
  observations[4].identity = 7;
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteRoles", 12, &promotion, observations, 6,
      rewrites, 5, &schedule) == FAILED ||
      schedule.status != RA_LOOP_STACK_REWRITE_INELIGIBLE ||
      schedule.reason != RA_LOOP_STACK_REWRITE_REASON_ROLES ||
      schedule.rewrite_count != 0 ||
      schedule.role_mask != (RA_LOOP_STACK_ROLE_REQUIRED &
      ~RA_LOOP_STACK_ROLE_EXIT_READ) || rewrites[0].instruction_index != 99)
    return FAILED;
  observations[4].identity = 5;
  promotion.status = RA_LOOP_STACK_PROMOTION_INELIGIBLE;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_SELECTION;
  promotion.identity = -1;
  promotion.temp_index = -1;
  promotion.size = 0;
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewritePromotion", 12, &promotion, observations, 6,
      rewrites, 5, &schedule) == FAILED ||
      schedule.status != RA_LOOP_STACK_REWRITE_INELIGIBLE ||
      schedule.reason != RA_LOOP_STACK_REWRITE_REASON_PROMOTION)
    return FAILED;

  promotion.status = RA_LOOP_STACK_PROMOTION_READY;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_NONE;
  promotion.identity = 5;
  promotion.temp_index = 3;
  promotion.size = 8;
  observations[5] = observations[1];
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteDuplicate", 12, &promotion, observations, 6,
      rewrites, 5, &schedule) != FAILED)
    return FAILED;
  observations[5].instruction_index = 3;
  observations[5].operand = TAC_USE_ARG2;
  observations[5].identity = 7;
  observations[5].role = RA_LOOP_STACK_ROLE_HEADER_READ;
  observations[0].role = RA_LOOP_STACK_ROLE_REQUIRED;
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteObservation", 12, &promotion, observations, 6,
      rewrites, 5, &schedule) != FAILED)
    return FAILED;
  observations[0].role = RA_LOOP_STACK_ROLE_ENTRY_WRITE;
  promotion.temp_index = -1;
  if (register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewritePlan", 12, &promotion, observations, 6,
      rewrites, 5, &schedule) != FAILED)
    return FAILED;
  promotion.temp_index = 3;
  if (register_allocator_plan_loop_stack_rewrites(NULL, 12, &promotion,
      observations, 6, rewrites, 5, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteInstructions", 0, &promotion, observations, 6,
      rewrites, 5, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewritePromotionNull", 12, NULL, observations, 6,
      rewrites, 5, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteObservations", 12, &promotion, NULL, 1,
      rewrites, 5, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteRewrites", 12, &promotion, observations, 6,
      NULL, 5, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteCount", 12, &promotion, observations, -1,
      rewrites, 5, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteCapacityInput", 12, &promotion, observations, 6,
      rewrites, -1, &schedule) != FAILED ||
      register_allocator_plan_loop_stack_rewrites(
      "coreLoopStackRewriteOutput", 12, &promotion, observations, 6,
      rewrites, 5, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct loop_stack_application_context {
  int calls;
  int result;
  int temp_index;
  int rewrite_count;
};

static int apply_loop_stack_promotion_transaction(void *context,
    struct register_allocator_loop_stack_promotion_plan *promotion,
    struct register_allocator_loop_stack_rewrite *rewrites,
    int rewrite_count) {

  struct loop_stack_application_context *application_context;

  application_context = (struct loop_stack_application_context *)context;
  if (application_context == NULL || promotion == NULL || rewrites == NULL ||
      rewrite_count <= 0)
    return FAILED;
  application_context->calls++;
  application_context->temp_index = promotion->temp_index;
  application_context->rewrite_count = rewrite_count;
  return application_context->result;
}

static int test_loop_stack_promotion_application(void) {

  struct register_allocator_loop_stack_application application;
  struct loop_stack_application_context context;
  struct register_allocator_loop_stack_promotion_plan promotion;
  struct register_allocator_loop_stack_rewrite rewrites[5];
  struct register_allocator_loop_stack_rewrite_schedule schedule;
  int rewrite_index;

  promotion.status = RA_LOOP_STACK_PROMOTION_READY;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_NONE;
  promotion.identity = 5;
  promotion.temp_index = 3;
  promotion.size = 8;
  schedule.status = RA_LOOP_STACK_REWRITE_READY;
  schedule.reason = RA_LOOP_STACK_REWRITE_REASON_NONE;
  schedule.rewrite_count = 5;
  schedule.role_mask = RA_LOOP_STACK_ROLE_REQUIRED;
  for (rewrite_index = 0; rewrite_index < 5; rewrite_index++) {
    rewrites[rewrite_index].instruction_index = rewrite_index + 2;
    rewrites[rewrite_index].operand = rewrite_index == 0 || rewrite_index == 3 ?
        TAC_USE_RESULT : TAC_USE_ARG1;
    rewrites[rewrite_index].temp_index = 3;
    rewrites[rewrite_index].role = 1 << rewrite_index;
  }
  context.calls = 0;
  context.result = SUCCEEDED;
  context.temp_index = -1;
  context.rewrite_count = 0;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyReady", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) == FAILED ||
      context.calls != 1 || context.temp_index != 3 ||
      context.rewrite_count != 5 ||
      application.status != RA_LOOP_STACK_APPLICATION_APPLIED ||
      application.rewrite_count != 5)
    return FAILED;

  context.result = FAILED;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyReject", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) == FAILED ||
      context.calls != 2 ||
      application.status != RA_LOOP_STACK_APPLICATION_FAILED ||
      application.rewrite_count != 0)
    return FAILED;
  context.result = 99;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyCallback", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      context.calls != 3 ||
      application.status != RA_LOOP_STACK_APPLICATION_INELIGIBLE)
    return FAILED;

  context.result = SUCCEEDED;
  promotion.status = RA_LOOP_STACK_PROMOTION_INELIGIBLE;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_SELECTION;
  promotion.identity = -1;
  promotion.temp_index = -1;
  promotion.size = 0;
  schedule.status = RA_LOOP_STACK_REWRITE_INELIGIBLE;
  schedule.reason = RA_LOOP_STACK_REWRITE_REASON_PROMOTION;
  schedule.rewrite_count = 0;
  schedule.role_mask = 0;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyIneligible", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) == FAILED ||
      context.calls != 3 ||
      application.status != RA_LOOP_STACK_APPLICATION_INELIGIBLE)
    return FAILED;

  promotion.status = RA_LOOP_STACK_PROMOTION_READY;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_NONE;
  promotion.identity = 5;
  promotion.temp_index = 3;
  promotion.size = 8;
  schedule.status = RA_LOOP_STACK_REWRITE_READY;
  schedule.reason = RA_LOOP_STACK_REWRITE_REASON_NONE;
  schedule.rewrite_count = 5;
  schedule.role_mask = RA_LOOP_STACK_ROLE_REQUIRED;
  rewrites[4].temp_index = 4;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyRewrite", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      context.calls != 3)
    return FAILED;
  rewrites[4].temp_index = 3;
  rewrites[4].instruction_index = rewrites[3].instruction_index;
  rewrites[4].operand = rewrites[3].operand;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyDuplicate", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      context.calls != 3)
    return FAILED;
  rewrites[4].instruction_index = 6;
  rewrites[4].operand = TAC_USE_ARG1;
  rewrites[4].role = RA_LOOP_STACK_ROLE_LATCH_WRITE;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyRoles", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      context.calls != 3)
    return FAILED;
  rewrites[4].role = RA_LOOP_STACK_ROLE_EXIT_READ;
  schedule.rewrite_count = 6;
  if (register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyCapacity", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      context.calls != 3)
    return FAILED;
  schedule.rewrite_count = 5;
  if (register_allocator_apply_loop_stack_promotion(NULL, 12, &promotion,
      rewrites, 5, &schedule, &context,
      apply_loop_stack_promotion_transaction, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyInstructions", 0, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyPromotion", 12, NULL, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyRewrites", 12, &promotion, NULL, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplySchedule", 12, &promotion, rewrites, 5, NULL,
      &context, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyContext", 12, &promotion, rewrites, 5, &schedule,
      NULL, apply_loop_stack_promotion_transaction, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyFunction", 12, &promotion, rewrites, 5, &schedule,
      &context, NULL, &application) != FAILED ||
      register_allocator_apply_loop_stack_promotion(
      "coreLoopStackApplyOutput", 12, &promotion, rewrites, 5, &schedule,
      &context, apply_loop_stack_promotion_transaction, NULL) != FAILED ||
      context.calls != 3)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_stack_storage(
    struct register_allocator_loop_stack_operand_storage *operands,
    int operand_count, struct register_allocator_loop_stack_rewrite *rewrites,
    int rewrite_count, int identity) {

  int operand_index;
  int rewrite_index;

  for (operand_index = 0; operand_index < operand_count; operand_index++) {
    operands[operand_index].kind = RA_LOOP_STACK_STORAGE_TEMP;
    operands[operand_index].identity = -1;
    operands[operand_index].temp_index = 99;
  }
  for (rewrite_index = 0; rewrite_index < rewrite_count; rewrite_index++) {
    operand_index = rewrites[rewrite_index].instruction_index * 3 +
        rewrites[rewrite_index].operand;
    operands[operand_index].kind = RA_LOOP_STACK_STORAGE_STACK;
    operands[operand_index].identity = identity;
    operands[operand_index].temp_index = -1;
  }
}

static int test_loop_stack_promotion_storage_transaction(void) {

  struct register_allocator_loop_stack_operand_storage operands[36];
  struct register_allocator_loop_stack_operand_storage original_operands[36];
  struct register_allocator_loop_stack_promotion_plan promotion;
  struct register_allocator_loop_stack_rewrite rewrites[5];
  struct register_allocator_loop_stack_temp_storage original_temps[3];
  struct register_allocator_loop_stack_temp_storage temps[3];
  int rewrite_index;
  int temp_count;

  promotion.status = RA_LOOP_STACK_PROMOTION_READY;
  promotion.reason = RA_LOOP_STACK_PROMOTION_REASON_NONE;
  promotion.identity = 5;
  promotion.temp_index = 3;
  promotion.size = 8;
  for (rewrite_index = 0; rewrite_index < 5; rewrite_index++) {
    rewrites[rewrite_index].instruction_index = rewrite_index + 2;
    rewrites[rewrite_index].operand = rewrite_index == 0 || rewrite_index == 3 ?
        TAC_USE_RESULT : TAC_USE_ARG1;
    rewrites[rewrite_index].temp_index = 3;
    rewrites[rewrite_index].role = 1 << rewrite_index;
  }
  temps[0].temp_index = 0;
  temps[0].size = 8;
  temps[1].temp_index = -1;
  temps[1].size = 0;
  temps[2].temp_index = -1;
  temps[2].size = 0;
  temp_count = 1;
  initialize_loop_stack_storage(operands, 36, rewrites, 5, 5);
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageCommit", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) == FAILED || temp_count != 2 ||
      temps[1].temp_index != 3 || temps[1].size != 8)
    return FAILED;
  for (rewrite_index = 0; rewrite_index < 5; rewrite_index++) {
    int operand_index;

    operand_index = rewrites[rewrite_index].instruction_index * 3 +
        rewrites[rewrite_index].operand;
    if (operands[operand_index].kind != RA_LOOP_STACK_STORAGE_TEMP ||
        operands[operand_index].identity != -1 ||
        operands[operand_index].temp_index != 3)
      return FAILED;
  }

  temp_count = 1;
  initialize_loop_stack_storage(operands, 36, rewrites, 5, 5);
  memcpy(original_operands, operands, sizeof(operands));
  memcpy(original_temps, temps, sizeof(temps));
  operands[rewrites[4].instruction_index * 3 + rewrites[4].operand].identity = 6;
  memcpy(original_operands, operands, sizeof(operands));
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageStale", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED || temp_count != 1 ||
      memcmp(operands, original_operands, sizeof(operands)) != 0 ||
      memcmp(temps, original_temps, sizeof(temps)) != 0)
    return FAILED;

  initialize_loop_stack_storage(operands, 36, rewrites, 5, 5);
  memcpy(original_operands, operands, sizeof(operands));
  rewrites[4].instruction_index = rewrites[3].instruction_index;
  rewrites[4].operand = rewrites[3].operand;
  memcpy(original_temps, temps, sizeof(temps));
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageDuplicate", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED || temp_count != 1 ||
      memcmp(operands, original_operands, sizeof(operands)) != 0 ||
      memcmp(temps, original_temps, sizeof(temps)) != 0)
    return FAILED;
  rewrites[4].instruction_index = 6;
  rewrites[4].operand = TAC_USE_ARG1;

  temps[0].temp_index = 3;
  memcpy(original_operands, operands, sizeof(operands));
  memcpy(original_temps, temps, sizeof(temps));
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageConflict", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED || temp_count != 1 ||
      memcmp(operands, original_operands, sizeof(operands)) != 0 ||
      memcmp(temps, original_temps, sizeof(temps)) != 0)
    return FAILED;
  temps[0].temp_index = 0;
  temp_count = 3;
  memcpy(original_operands, operands, sizeof(operands));
  memcpy(original_temps, temps, sizeof(temps));
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageCapacity", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED || temp_count != 3 ||
      memcmp(operands, original_operands, sizeof(operands)) != 0 ||
      memcmp(temps, original_temps, sizeof(temps)) != 0)
    return FAILED;
  temp_count = 1;
  memcpy(original_operands, operands, sizeof(operands));
  memcpy(original_temps, temps, sizeof(temps));
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageOperands", 12, &promotion, rewrites, 5,
      operands, 35, temps, 3, &temp_count) != FAILED || temp_count != 1 ||
      memcmp(operands, original_operands, sizeof(operands)) != 0 ||
      memcmp(temps, original_temps, sizeof(temps)) != 0)
    return FAILED;
  rewrites[4].temp_index = 4;
  if (register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageRewrite", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED || temp_count != 1 ||
      memcmp(operands, original_operands, sizeof(operands)) != 0 ||
      memcmp(temps, original_temps, sizeof(temps)) != 0)
    return FAILED;
  rewrites[4].temp_index = 3;
  if (register_allocator_commit_loop_stack_promotion_storage(NULL, 12,
      &promotion, rewrites, 5, operands, 36, temps, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageInstructions", 0, &promotion, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStoragePromotion", 12, NULL, rewrites, 5,
      operands, 36, temps, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageRewrites", 12, &promotion, NULL, 5,
      operands, 36, temps, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageCount", 12, &promotion, rewrites, 0,
      operands, 36, temps, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageOperandStorage", 12, &promotion, rewrites, 5,
      NULL, 36, temps, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageTempStorage", 12, &promotion, rewrites, 5,
      operands, 36, NULL, 3, &temp_count) != FAILED ||
      register_allocator_commit_loop_stack_promotion_storage(
      "coreLoopStackStorageTempCount", 12, &promotion, rewrites, 5,
      operands, 36, temps, 3, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct loop_fact_instruction {
  int active;
  int reads;
  int writes;
  int operand;
};

static int loop_fact_is_active(void *context, int instruction_index,
    int temp_index) {

  struct loop_fact_instruction *instructions;

  (void)temp_index;
  instructions = (struct loop_fact_instruction *)context;
  return instructions[instruction_index].active;
}

static int loop_fact_reads_temp(void *context, int instruction_index,
    int temp_index) {

  struct loop_fact_instruction *instructions;

  (void)temp_index;
  instructions = (struct loop_fact_instruction *)context;
  return instructions[instruction_index].reads;
}

static int loop_fact_writes_temp(void *context, int instruction_index,
    int temp_index) {

  struct loop_fact_instruction *instructions;

  (void)temp_index;
  instructions = (struct loop_fact_instruction *)context;
  return instructions[instruction_index].writes;
}

static int loop_fact_get_consumer_operand(void *context, int instruction_index,
    int temp_index) {

  struct loop_fact_instruction *instructions;

  (void)temp_index;
  instructions = (struct loop_fact_instruction *)context;
  return instructions[instruction_index].operand;
}

static void initialize_loop_facts(struct register_allocator_loop_facts *facts) {

  facts->status = 99;
  facts->reason = 99;
  facts->entry_definition = 99;
  facts->latch_definition = 99;
  facts->consumer_instruction = 99;
  facts->consumer_operand = 99;
}

static void initialize_loop_latch_selection(
    struct register_allocator_loop_latch_selection *selection) {

  selection->status = 99;
  selection->entry_edge_count = 99;
  selection->back_edge_count = 99;
  selection->defining_latch_count = 99;
  selection->latch_definition = 99;
  initialize_loop_join(&selection->loop_join);
}

static int test_loop_latch_selection(void) {

  struct loop_fact_instruction instructions[10];
  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[6];
  struct register_allocator_loop_latch_selection selection;
  int index;

  memset(instructions, 0, sizeof(instructions));
  for (index = 0; index < 10; index++)
    instructions[index].active = YES;
  for (index = 0; index < 5; index++) {
    blocks[index].start_tac = index * 2;
    blocks[index].end_tac = index * 2 + 1;
    blocks[index].end_reason = RA_BLOCK_END_JUMP;
  }
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 4;
  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[2].from_block = 1;
  edges[2].to_block = 2;
  edges[2].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[3].from_block = 2;
  edges[3].to_block = 1;
  edges[3].kind = RA_CFG_EDGE_JUMP;
  edges[4].from_block = 1;
  edges[4].to_block = 3;
  edges[4].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[5].from_block = 3;
  edges[5].to_block = 1;
  edges[5].kind = RA_CFG_EDGE_JUMP;

  instructions[7].writes = YES;
  initialize_loop_latch_selection(&selection);
  if (register_allocator_select_loop_latch("coreLoopLatchUnique", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) == FAILED ||
      selection.status != RA_LOOP_LATCH_SELECTION_AMBIGUOUS ||
      selection.entry_edge_count != 1 || selection.back_edge_count != 2 ||
      selection.defining_latch_count != 1 ||
      selection.latch_definition != 7 ||
      selection.loop_join.status != RA_LOOP_JOIN_AMBIGUOUS ||
      selection.loop_join.entry_predecessor != 0 ||
      selection.loop_join.latch_predecessor != 3 ||
      selection.loop_join.entry_edge != 0 ||
      selection.loop_join.back_edge != 5)
    return FAILED;

  instructions[7].writes = NO;
  initialize_loop_latch_selection(&selection);
  if (register_allocator_select_loop_latch("coreLoopLatchMissing", 5, 10,
      blocks, 1, edges, 4, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) == FAILED ||
      selection.status != RA_LOOP_LATCH_SELECTION_INELIGIBLE ||
      selection.back_edge_count != 1 ||
      selection.defining_latch_count != 0 ||
      selection.loop_join.status != RA_LOOP_JOIN_NOT_LOOP)
    return FAILED;

  instructions[5].writes = YES;
  instructions[7].writes = YES;
  initialize_loop_latch_selection(&selection);
  if (register_allocator_select_loop_latch("coreLoopLatchAmbiguous", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) == FAILED ||
      selection.status != RA_LOOP_LATCH_SELECTION_AMBIGUOUS ||
      selection.defining_latch_count != 2 ||
      selection.loop_join.status != RA_LOOP_JOIN_AMBIGUOUS)
    return FAILED;

  instructions[5].writes = NO;
  instructions[7].active = 2;
  if (register_allocator_select_loop_latch("coreLoopLatchActive", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) != FAILED)
    return FAILED;
  instructions[7].active = YES;
  instructions[7].writes = 2;
  if (register_allocator_select_loop_latch("coreLoopLatchWrites", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) != FAILED)
    return FAILED;
  instructions[7].writes = YES;

  initialize_loop_latch_selection(&selection);
  if (register_allocator_select_loop_latch(NULL, 5, 10, blocks, 1, edges, 6,
      2, instructions, loop_fact_is_active, loop_fact_writes_temp,
      &selection) != FAILED || selection.entry_edge_count != 0 ||
      selection.latch_definition != -1 ||
      register_allocator_select_loop_latch("coreLoopLatchBlocks", 0, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) != FAILED ||
      register_allocator_select_loop_latch("coreLoopLatchContext", 5, 10,
      blocks, 1, edges, 6, 2, NULL, loop_fact_is_active,
      loop_fact_writes_temp, &selection) != FAILED ||
      register_allocator_select_loop_latch("coreLoopLatchCallback", 5, 10,
      blocks, 1, edges, 6, 2, instructions, NULL,
      loop_fact_writes_temp, &selection) != FAILED ||
      register_allocator_select_loop_latch("coreLoopLatchOutput", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, NULL) != FAILED)
    return FAILED;
  edges[5].to_block = 5;
  if (register_allocator_select_loop_latch("coreLoopLatchEdge", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) != FAILED)
    return FAILED;
  edges[5].to_block = 1;
  blocks[4].start_tac = 7;
  if (register_allocator_select_loop_latch("coreLoopLatchBlock", 5, 10,
      blocks, 1, edges, 6, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, &selection) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_latch_collection(void) {

  struct loop_fact_instruction instructions[10];
  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[7];
  struct register_allocator_loop_latch_definition definitions[2];
  struct register_allocator_loop_latch_collection collection;
  int index;

  memset(instructions, 0, sizeof(instructions));
  for (index = 0; index < 10; index++)
    instructions[index].active = YES;
  for (index = 0; index < 5; index++) {
    blocks[index].start_tac = index * 2;
    blocks[index].end_tac = index * 2 + 1;
    blocks[index].end_reason = RA_BLOCK_END_JUMP;
  }
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 4;
  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[2].from_block = 1;
  edges[2].to_block = 2;
  edges[2].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[3].from_block = 2;
  edges[3].to_block = 1;
  edges[3].kind = RA_CFG_EDGE_JUMP;
  edges[4].from_block = 1;
  edges[4].to_block = 3;
  edges[4].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[5].from_block = 3;
  edges[5].to_block = 1;
  edges[5].kind = RA_CFG_EDGE_JUMP;
  edges[6] = edges[5];
  instructions[5].writes = YES;
  instructions[7].writes = YES;

  if (register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionReady", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      2, &collection) == FAILED ||
      collection.status != RA_LOOP_LATCH_SELECTION_READY ||
      collection.entry_edge_count != 1 || collection.back_edge_count != 3 ||
      collection.definition_count != 2 ||
      collection.entry_predecessor != 0 || collection.entry_edge != 0 ||
      definitions[0].block_index != 2 || definitions[0].edge_index != 3 ||
      definitions[0].definition_instruction != 5 ||
      definitions[1].block_index != 3 || definitions[1].edge_index != 5 ||
      definitions[1].definition_instruction != 7)
    return FAILED;

  definitions[0].block_index = 99;
  definitions[1].block_index = 99;
  if (register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionCapacity", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      1, &collection) != FAILED || definitions[0].block_index != -1 ||
      definitions[1].block_index != 99)
    return FAILED;

  instructions[5].writes = NO;
  if (register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionTransparent", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      2, &collection) == FAILED || collection.definition_count != 1 ||
      definitions[0].block_index != 3 ||
      definitions[0].definition_instruction != 7)
    return FAILED;
  instructions[5].writes = YES;

  instructions[7].active = 2;
  if (register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionActive", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      2, &collection) != FAILED)
    return FAILED;
  instructions[7].active = YES;
  edges[5].to_block = 5;
  if (register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionEdge", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      2, &collection) != FAILED)
    return FAILED;
  edges[5].to_block = 1;
  blocks[4].start_tac = 7;
  if (register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionBlock", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      2, &collection) != FAILED)
    return FAILED;
  blocks[4].start_tac = 8;
  if (register_allocator_collect_loop_latch_definitions(NULL, 5, 10, blocks,
      1, edges, 7, 2, instructions, loop_fact_is_active,
      loop_fact_writes_temp, definitions, 2, &collection) != FAILED ||
      register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionOutput", 5, 10, blocks, 1, edges, 7, 2,
      instructions, loop_fact_is_active, loop_fact_writes_temp, definitions,
      2, NULL) != FAILED ||
      register_allocator_collect_loop_latch_definitions(
      "coreLoopLatchCollectionCallback", 5, 10, blocks, 1, edges, 7, 2,
      instructions, NULL, loop_fact_writes_temp, definitions, 2,
      &collection) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_join_fact_discovery(void) {

  struct loop_fact_instruction instructions[12];
  struct register_allocator_basic_block blocks[4];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_facts facts;
  int instruction_index;

  memset(instructions, 0, sizeof(instructions));
  for (instruction_index = 0; instruction_index < 12; instruction_index++)
    instructions[instruction_index].active = YES;
  blocks[0].start_tac = 0;
  blocks[0].end_tac = 2;
  blocks[1].start_tac = 3;
  blocks[1].end_tac = 5;
  blocks[2].start_tac = 6;
  blocks[2].end_tac = 7;
  blocks[3].start_tac = 8;
  blocks[3].end_tac = 10;
  loop_join.status = RA_LOOP_JOIN_READY;
  loop_join.entry_predecessor = 0;
  loop_join.latch_predecessor = 3;
  loop_join.entry_edge = 0;
  loop_join.back_edge = 1;
  instructions[1].writes = YES;
  instructions[2].writes = YES;
  instructions[4].reads = YES;
  instructions[4].operand = 2;
  instructions[5].reads = YES;
  instructions[5].operand = 1;
  instructions[8].writes = YES;
  instructions[9].writes = YES;

  initialize_loop_facts(&facts);
  if (register_allocator_discover_loop_join_facts("coreLoopFactsReady", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.status != RA_LOOP_FACTS_READY ||
      facts.reason != RA_LOOP_FACTS_REASON_NONE ||
      facts.entry_definition != 2 || facts.latch_definition != 9 ||
      facts.consumer_instruction != 4 || facts.consumer_operand != 2)
    return FAILED;

  instructions[4].operand = 0;
  initialize_loop_facts(&facts);
  if (register_allocator_discover_loop_join_facts("coreLoopFactsResult", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.status != RA_LOOP_FACTS_READY ||
      facts.consumer_instruction != 4 || facts.consumer_operand != 0)
    return FAILED;
  instructions[4].operand = 2;

  instructions[2].active = NO;
  instructions[5].active = NO;
  instructions[9].active = NO;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsInactive", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.entry_definition != 1 || facts.latch_definition != 8 ||
      facts.consumer_instruction != 4)
    return FAILED;
  instructions[2].active = YES;
  instructions[5].active = YES;
  instructions[9].active = YES;

  instructions[1].writes = NO;
  instructions[2].writes = NO;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsEntry", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.reason != RA_LOOP_FACTS_REASON_MISSING_ENTRY)
    return FAILED;
  instructions[2].writes = YES;
  instructions[8].writes = NO;
  instructions[9].writes = NO;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsLatch", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.reason != RA_LOOP_FACTS_REASON_MISSING_LATCH)
    return FAILED;
  instructions[9].writes = YES;
  instructions[4].reads = NO;
  instructions[5].reads = NO;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsConsumer", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.reason != RA_LOOP_FACTS_REASON_NO_CONSUMER)
    return FAILED;
  instructions[4].reads = YES;

  loop_join.status = RA_LOOP_JOIN_AMBIGUOUS;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsTopology", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) == FAILED ||
      facts.reason != RA_LOOP_FACTS_REASON_TOPOLOGY)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_READY;

  instructions[2].active = 2;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsActive", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  instructions[2].active = YES;
  instructions[2].writes = 2;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsWrite", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  instructions[2].writes = YES;
  instructions[4].reads = 2;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsRead", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  instructions[4].reads = YES;
  instructions[4].operand = 3;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsOperand", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  instructions[4].operand = 2;

  loop_join.entry_predecessor = 1;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsShape", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  loop_join.entry_predecessor = 0;
  blocks[2].start_tac = 5;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsBlocks", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  blocks[2].start_tac = 6;

  initialize_loop_facts(&facts);
  if (register_allocator_discover_loop_join_facts(NULL, 4, 12, blocks, 1, 2,
      &loop_join, instructions, loop_fact_is_active, loop_fact_reads_temp,
      loop_fact_writes_temp, loop_fact_get_consumer_operand, &facts) != FAILED ||
      facts.status != RA_LOOP_FACTS_INELIGIBLE ||
      facts.entry_definition != -1 || facts.latch_definition != -1 ||
      facts.consumer_instruction != -1 || facts.consumer_operand != 0 ||
      register_allocator_discover_loop_join_facts("coreLoopFactsCount", 0, 12,
      blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsInstructions",
      4, 0, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsBlock", 4, 12,
      blocks, 4, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsTemp", 4, 12,
      blocks, 1, -1, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsContext", 4,
      12, blocks, 1, 2, &loop_join, NULL, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsActiveNull", 4,
      12, blocks, 1, 2, &loop_join, instructions, NULL,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsReadNull", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active, NULL,
      loop_fact_writes_temp, loop_fact_get_consumer_operand, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsWriteNull", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, NULL, loop_fact_get_consumer_operand,
      &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsOperandNull", 4,
      12, blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp, NULL, &facts) != FAILED ||
      register_allocator_discover_loop_join_facts("coreLoopFactsOutput", 4, 12,
      blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, NULL) != FAILED)
    return FAILED;
  loop_join.status = 99;
  if (register_allocator_discover_loop_join_facts("coreLoopFactsStatus", 4, 12,
      blocks, 1, 2, &loop_join, instructions, loop_fact_is_active,
      loop_fact_reads_temp, loop_fact_writes_temp,
      loop_fact_get_consumer_operand, &facts) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_definition(
    struct register_allocator_loop_definition *definition) {

  definition->status = 99;
  definition->reason = 99;
  definition->entry_definition = 99;
  definition->latch_definition = 99;
  definition->consumer_instruction = 99;
  definition->consumer_operand = 99;
}

static int test_loop_join_definition_planning(void) {

  struct register_allocator_basic_block blocks[4];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 2;
  blocks[1].start_tac = 3;
  blocks[1].end_tac = 5;
  blocks[2].start_tac = 6;
  blocks[2].end_tac = 7;
  blocks[3].start_tac = 8;
  blocks[3].end_tac = 10;
  loop_join.status = RA_LOOP_JOIN_READY;
  loop_join.entry_predecessor = 0;
  loop_join.latch_predecessor = 3;
  loop_join.entry_edge = 0;
  loop_join.back_edge = 1;

  initialize_loop_definition(&definition);
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionReady", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) == FAILED ||
      definition.status != RA_LOOP_DEFINITION_READY ||
      definition.reason != RA_LOOP_DEFINITION_REASON_NONE ||
      definition.entry_definition != 2 || definition.latch_definition != 9 ||
      definition.consumer_instruction != 4 || definition.consumer_operand != 1)
    return FAILED;

  initialize_loop_definition(&definition);
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionResult", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 0,
      &definition) == FAILED ||
      definition.status != RA_LOOP_DEFINITION_READY ||
      definition.consumer_instruction != 4 || definition.consumer_operand != 0)
    return FAILED;

  initialize_loop_definition(&definition);
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionNotLive",
      4, 12, blocks, 1, 2, NO, &loop_join, 2, 9, 4, 1,
      &definition) == FAILED ||
      definition.reason != RA_LOOP_DEFINITION_REASON_NOT_LIVE_IN)
    return FAILED;
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionEntry", 4,
      12, blocks, 1, 2, YES, &loop_join, -1, 9, 4, 1,
      &definition) == FAILED ||
      definition.reason != RA_LOOP_DEFINITION_REASON_MISSING_ENTRY ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionLatch", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, -1, 4, 1,
      &definition) == FAILED ||
      definition.reason != RA_LOOP_DEFINITION_REASON_MISSING_LATCH ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionConsumer",
      4, 12, blocks, 1, 2, YES, &loop_join, 2, 9, -1, 1,
      &definition) == FAILED ||
      definition.reason != RA_LOOP_DEFINITION_REASON_NO_CONSUMER)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_NOT_LOOP;
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionTopology",
      4, 12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) == FAILED ||
      definition.reason != RA_LOOP_DEFINITION_REASON_TOPOLOGY)
    return FAILED;

  loop_join.status = RA_LOOP_JOIN_READY;
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionEntryBlock",
      4, 12, blocks, 1, 2, YES, &loop_join, 3, 9, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionLatchBlock",
      4, 12, blocks, 1, 2, YES, &loop_join, 2, 7, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionConsumerBlock",
      4, 12, blocks, 1, 2, YES, &loop_join, 2, 9, 6, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionOperand", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 3,
      &definition) != FAILED)
    return FAILED;
  loop_join.entry_edge = -1;
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionShape", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED)
    return FAILED;
  loop_join.entry_edge = 0;
  blocks[2].start_tac = 5;
  if (register_allocator_plan_loop_join_definition("coreLoopDefinitionBlocks", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED)
    return FAILED;
  blocks[2].start_tac = 6;

  if (register_allocator_plan_loop_join_definition(NULL, 4, 12, blocks, 1, 2,
      YES, &loop_join, 2, 9, 4, 1, &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionBlockCount",
      0, 12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionInstructions",
      4, 0, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionBlock", 4,
      12, blocks, 4, 2, YES, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionTemp", 4,
      12, blocks, 1, -1, YES, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionLive", 4,
      12, blocks, 1, 2, 2, &loop_join, 2, 9, 4, 1,
      &definition) != FAILED ||
      register_allocator_plan_loop_join_definition("coreLoopDefinitionOutput", 4,
      12, blocks, 1, 2, YES, &loop_join, 2, 9, 4, 1, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_schedule(
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule) {

  int assignment_index;

  for (assignment_index = 0; assignment_index < 3; assignment_index++) {
    assignments[assignment_index].role = 99;
    assignments[assignment_index].instruction = 99;
    assignments[assignment_index].operand = 99;
    assignments[assignment_index].physical_register = 99;
  }
  schedule->status = 99;
  schedule->assignment_count = 99;
  schedule->conflict_instruction = 99;
}

static int test_loop_join_schedule_planning(void) {

  struct register_allocator_loop_join loop_join;
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;

  loop_join.status = RA_LOOP_JOIN_READY;
  loop_join.entry_predecessor = 0;
  loop_join.latch_predecessor = 3;
  loop_join.entry_edge = 0;
  loop_join.back_edge = 1;
  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleReady", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_READY ||
      schedule.assignment_count != 3 || schedule.conflict_instruction != -1 ||
      assignments[0].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[0].instruction != 2 || assignments[0].operand != 0 ||
      assignments[0].physical_register != 5 ||
      assignments[1].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[1].instruction != 9 || assignments[1].operand != 0 ||
      assignments[1].physical_register != 5 ||
      assignments[2].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      assignments[2].instruction != 4 || assignments[2].operand != 1 ||
      assignments[2].physical_register != 5)
    return FAILED;

  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleResult", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 0, 5, 5,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_READY ||
      schedule.assignment_count != 3 ||
      assignments[2].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      assignments[2].operand != 0)
    return FAILED;

  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule(
      "coreLoopScheduleUnassigned", 12, 2, -1, &loop_join, 2, -1, 9, -1,
      4, 1, -1, 5, assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_READY ||
      schedule.assignment_count != 3 || schedule.conflict_instruction != -1 ||
      assignments[0].physical_register != 5 ||
      assignments[1].physical_register != 5 ||
      assignments[2].physical_register != 5)
    return FAILED;

  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleEntryConflict",
      12, 2, -1, &loop_join, 2, 4, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_CONFLICT ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != 2)
    return FAILED;
  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleLatchConflict",
      12, 2, -1, &loop_join, 2, 5, 9, 4, 4, 1, 5, 5,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_CONFLICT ||
      schedule.conflict_instruction != 9)
    return FAILED;
  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleConsumerConflict",
      12, 2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 4, 5,
      assignments, 3, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_CONFLICT ||
      schedule.conflict_instruction != 4)
    return FAILED;

  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleCapacity", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 2, &schedule) != FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_INELIGIBLE ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != -1)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_NOT_LOOP;
  initialize_loop_schedule(assignments, &schedule);
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleIneligible",
      12, 2, -1, &loop_join, 2, -1, 9, -1, 4, 1, -1, -1,
      NULL, 0, &schedule) == FAILED ||
      schedule.status != RA_JOIN_SCHEDULE_INELIGIBLE ||
      schedule.assignment_count != 0 || schedule.conflict_instruction != -1)
    return FAILED;

  loop_join.status = RA_LOOP_JOIN_AMBIGUOUS;
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleAmbiguous", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) != FAILED)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_READY;
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleEntryOrder", 12,
      2, -1, &loop_join, 4, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleLatchOrder", 12,
      2, -1, &loop_join, 2, 5, 4, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) != FAILED)
    return FAILED;
  loop_join.entry_edge = -1;
  if (register_allocator_plan_loop_join_schedule("coreLoopScheduleTopology", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) != FAILED)
    return FAILED;
  loop_join.entry_edge = 0;

  if (register_allocator_plan_loop_join_schedule(NULL, 12, 2, -1,
      &loop_join, 2, 5, 9, 5, 4, 1, 5, 5, assignments, 3,
      &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleInstructions",
      0, 2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleTemp", 12,
      -1, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleConsumer", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 12, 1, 5, 5,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleOperand", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 3, 5, 5,
      assignments, 3, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleCapacityValue",
      12, 2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, -1, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleAssignments",
      12, 2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      NULL, 3, &schedule) != FAILED ||
      register_allocator_plan_loop_join_schedule("coreLoopScheduleOutput", 12,
      2, -1, &loop_join, 2, 5, 9, 5, 4, 1, 5, 5,
      assignments, 3, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_path_fixture(
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_schedule *schedule) {

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 2;
  blocks[1].start_tac = 3;
  blocks[1].end_tac = 5;
  blocks[2].start_tac = 6;
  blocks[2].end_tac = 7;
  blocks[3].start_tac = 8;
  blocks[3].end_tac = 10;
  blocks[4].start_tac = 11;
  blocks[4].end_tac = 13;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 3;
  edges[1].to_block = 1;
  edges[1].kind = RA_CFG_EDGE_JUMP;
  edges[2].from_block = 1;
  edges[2].to_block = 2;
  edges[2].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[3].from_block = 2;
  edges[3].to_block = 3;
  edges[3].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[4].from_block = 1;
  edges[4].to_block = 4;
  edges[4].kind = RA_CFG_EDGE_BRANCH_FALSE;
  loop_join->status = RA_LOOP_JOIN_READY;
  loop_join->entry_predecessor = 0;
  loop_join->latch_predecessor = 3;
  loop_join->entry_edge = 0;
  loop_join->back_edge = 1;
  assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[0].instruction = 2;
  assignments[0].operand = 0;
  assignments[0].physical_register = 5;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].instruction = 9;
  assignments[1].operand = 0;
  assignments[1].physical_register = 5;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[2].instruction = 4;
  assignments[2].operand = 1;
  assignments[2].physical_register = 5;
  schedule->status = RA_JOIN_SCHEDULE_READY;
  schedule->assignment_count = 3;
  schedule->conflict_instruction = -1;
}

static int test_loop_join_path_reservation_planning(void) {

  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_schedule schedule;
  struct register_allocator_join_block_reservation reservations[4];
  struct register_allocator_join_path_reservation path_reservation;

  initialize_loop_path_fixture(blocks, edges, &loop_join, assignments,
      &schedule);
  path_reservation.status = 99;
  path_reservation.block_count = 99;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathReady", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) == FAILED ||
      path_reservation.status != RA_JOIN_PATH_RESERVATION_READY ||
      path_reservation.block_count != 4 ||
      reservations[0].block_index != 0 ||
      reservations[0].start_instruction != 2 ||
      reservations[0].end_instruction != 2 ||
      reservations[1].block_index != 1 ||
      reservations[1].start_instruction != 3 ||
      reservations[1].end_instruction != 5 ||
      reservations[2].block_index != 2 ||
      reservations[2].start_instruction != 6 ||
      reservations[2].end_instruction != 7 ||
      reservations[3].block_index != 3 ||
      reservations[3].start_instruction != 8 ||
      reservations[3].end_instruction != 10)
    return FAILED;

  path_reservation.status = 99;
  path_reservation.block_count = 99;
  reservations[0].block_index = 99;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathCapacity", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 3,
      &path_reservation) != FAILED ||
      path_reservation.status != RA_JOIN_PATH_RESERVATION_INELIGIBLE ||
      path_reservation.block_count != 0 || reservations[0].block_index != 99)
    return FAILED;
  edges[3].to_block = 4;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathUnreachable", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  initialize_loop_path_fixture(blocks, edges, &loop_join, assignments,
      &schedule);

  assignments[1].instruction = 7;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathAssignmentBlock", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  assignments[1].instruction = 9;
  assignments[2].physical_register = 4;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathAssignmentRegister", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  assignments[2].physical_register = 5;
  assignments[2].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathAssignmentRole", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;

  edges[0].from_block = 2;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathTopologyEdge", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  edges[0].from_block = 0;
  edges[4].kind = 99;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathInvalidEdge", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  edges[4].kind = RA_CFG_EDGE_BRANCH_FALSE;
  blocks[2].start_tac = 5;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathOverlappingBlock", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  blocks[2].start_tac = 6;
  blocks[4].end_tac = 14;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathInvalidBlock", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  blocks[4].end_tac = 13;

  loop_join.status = RA_LOOP_JOIN_AMBIGUOUS;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathAmbiguous", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_READY;
  schedule.status = RA_JOIN_SCHEDULE_CONFLICT;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathSchedule", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, assignments, 3, &schedule, reservations, 4,
      &path_reservation) != FAILED)
    return FAILED;
  loop_join.status = RA_LOOP_JOIN_NOT_LOOP;
  schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
  schedule.assignment_count = 0;
  if (register_allocator_plan_loop_join_path_reservations(
      "coreLoopPathIneligible", 5, 14, blocks, edges, 5, 1, 2, 7,
      &loop_join, NULL, 0, &schedule, NULL, 0,
      &path_reservation) == FAILED ||
      path_reservation.status != RA_JOIN_PATH_RESERVATION_INELIGIBLE ||
      path_reservation.block_count != 0)
    return FAILED;

  initialize_loop_path_fixture(blocks, edges, &loop_join, assignments,
      &schedule);
  if (register_allocator_plan_loop_join_path_reservations(NULL, 5, 14,
      blocks, edges, 5, 1, 2, 7, &loop_join, assignments, 3, &schedule,
      reservations, 4, &path_reservation) != FAILED ||
      register_allocator_plan_loop_join_path_reservations("coreLoopPathBlocks",
      0, 14, blocks, edges, 5, 1, 2, 7, &loop_join, assignments, 3,
      &schedule, reservations, 4, &path_reservation) != FAILED ||
      register_allocator_plan_loop_join_path_reservations("coreLoopPathEdges",
      5, 14, blocks, NULL, 5, 1, 2, 7, &loop_join, assignments, 3,
      &schedule, reservations, 4, &path_reservation) != FAILED ||
      register_allocator_plan_loop_join_path_reservations("coreLoopPathJoin",
      5, 14, blocks, edges, 5, 5, 2, 7, &loop_join, assignments, 3,
      &schedule, reservations, 4, &path_reservation) != FAILED ||
      register_allocator_plan_loop_join_path_reservations("coreLoopPathTemp",
      5, 14, blocks, edges, 5, 1, -1, 7, &loop_join, assignments, 3,
      &schedule, reservations, 4, &path_reservation) != FAILED ||
      register_allocator_plan_loop_join_path_reservations("coreLoopPathOutput",
      5, 14, blocks, edges, 5, 1, 2, 7, &loop_join, assignments, 3,
      &schedule, reservations, 4, NULL) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

struct loop_retention_application_context {
  int calls;
  int result;
  int temp_index;
  int slot_index;
  int assignment_count;
  int reservation_count;
};

static int apply_loop_retention_transaction(void *context, int temp_index,
    int slot_index, struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_count) {

  struct loop_retention_application_context *application_context;

  application_context = (struct loop_retention_application_context *)context;
  application_context->calls++;
  application_context->temp_index = temp_index;
  application_context->slot_index = slot_index;
  application_context->assignment_count = assignment_count;
  application_context->reservation_count = reservation_count;
  if (assignments == NULL || reservations == NULL)
    return FAILED;
  return application_context->result;
}

static void initialize_loop_retention_fixture(
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_loop_definition *definition,
    struct register_allocator_join_assignment *assignments,
    struct register_allocator_join_block_reservation *reservations,
    struct register_allocator_loop_retention_plan *plan) {

  int index;
  struct register_allocator_join_schedule schedule;

  initialize_loop_path_fixture(blocks, edges, loop_join, assignments,
      &schedule);
  definition->status = RA_LOOP_DEFINITION_READY;
  definition->reason = RA_LOOP_DEFINITION_REASON_NONE;
  definition->entry_definition = 2;
  definition->latch_definition = 9;
  definition->consumer_instruction = 4;
  definition->consumer_operand = 1;
  for (index = 0; index < 3; index++) {
    assignments[index].role = 99;
    assignments[index].instruction = 99;
    assignments[index].operand = 99;
    assignments[index].physical_register = 99;
  }
  for (index = 0; index < 4; index++) {
    reservations[index].block_index = 99;
    reservations[index].start_instruction = 99;
    reservations[index].end_instruction = 99;
  }
  plan->status = 99;
  plan->assignment_count = 99;
  plan->reservation_count = 99;
}

static int test_loop_retention_planning(void) {

  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_assignment original_assignments[3];
  struct register_allocator_join_block_reservation reservations[4];
  struct register_allocator_join_block_reservation original_reservations[4];
  struct register_allocator_loop_retention_plan plan;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  if (register_allocator_plan_loop_retention("coreLoopRetentionReady", 5, 14,
      blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join, &definition,
      NULL, 0, assignments, 3, reservations, 4, &plan) == FAILED ||
      plan.status != RA_LOOP_RETENTION_READY || plan.assignment_count != 3 ||
      plan.reservation_count != 4 || assignments[0].instruction != 2 ||
      assignments[1].instruction != 9 || assignments[2].instruction != 4 ||
      reservations[0].block_index != 0 ||
      reservations[3].block_index != 3)
    return FAILED;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  definition.status = RA_LOOP_DEFINITION_INELIGIBLE;
  if (register_allocator_plan_loop_retention("coreLoopRetentionIneligible", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, NULL, 0, NULL, 0, NULL, 0, &plan) == FAILED ||
      plan.status != RA_LOOP_RETENTION_INELIGIBLE ||
      plan.assignment_count != 0 || plan.reservation_count != 0)
    return FAILED;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  memcpy(original_assignments, assignments, sizeof(assignments));
  memcpy(original_reservations, reservations, sizeof(reservations));
  if (register_allocator_plan_loop_retention("coreLoopRetentionConflict", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 4, 5, 5, &loop_join,
      &definition, NULL, 0, assignments, 3, reservations, 4, &plan) == FAILED ||
      plan.status != RA_LOOP_RETENTION_INELIGIBLE ||
      memcmp(assignments, original_assignments, sizeof(assignments)) != 0 ||
      memcmp(reservations, original_reservations, sizeof(reservations)) != 0)
    return FAILED;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  memcpy(original_assignments, assignments, sizeof(assignments));
  memcpy(original_reservations, reservations, sizeof(reservations));
  if (register_allocator_plan_loop_retention("coreLoopRetentionReservation", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, NULL, 0, assignments, 3, reservations, 3, &plan) != FAILED ||
      plan.status != RA_LOOP_RETENTION_INELIGIBLE ||
      memcmp(assignments, original_assignments, sizeof(assignments)) != 0 ||
      memcmp(reservations, original_reservations, sizeof(reservations)) != 0)
    return FAILED;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  if (register_allocator_plan_loop_retention("coreLoopRetentionCapacity", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, NULL, 0, assignments, 2, reservations, 4, &plan) != FAILED ||
      register_allocator_plan_loop_retention(NULL, 5, 14, blocks, edges, 5,
      1, 2, 7, -1, 5, 5, 5, 5, &loop_join, &definition, assignments, 3,
      NULL, 0, reservations, 4, &plan) != FAILED)
    return FAILED;
  definition.status = 99;
  if (register_allocator_plan_loop_retention("coreLoopRetentionDefinition", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, NULL, 0, assignments, 3, reservations, 4, &plan) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static int test_multi_latch_retention_planning(void) {

  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[6];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;
  struct register_allocator_join_assignment assignments[4];
  struct register_allocator_join_assignment supplemental_producer;
  struct register_allocator_join_block_reservation reservations[5];
  struct register_allocator_loop_retention_application application;
  struct register_allocator_loop_retention_plan plan;
  struct loop_retention_application_context context;
  int index;

  for (index = 0; index < 5; index++) {
    blocks[index].start_tac = index * 3;
    blocks[index].end_tac = index * 3 + 2;
    blocks[index].end_reason = RA_BLOCK_END_JUMP;
  }
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[2].from_block = 2;
  edges[2].to_block = 1;
  edges[2].kind = RA_CFG_EDGE_JUMP;
  edges[3].from_block = 1;
  edges[3].to_block = 3;
  edges[3].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[4].from_block = 3;
  edges[4].to_block = 1;
  edges[4].kind = RA_CFG_EDGE_JUMP;
  edges[5].from_block = 1;
  edges[5].to_block = 4;
  edges[5].kind = RA_CFG_EDGE_BRANCH_FALSE;
  loop_join.status = RA_LOOP_JOIN_READY;
  loop_join.entry_predecessor = 0;
  loop_join.latch_predecessor = 2;
  loop_join.entry_edge = 0;
  loop_join.back_edge = 2;
  definition.status = RA_LOOP_DEFINITION_READY;
  definition.reason = RA_LOOP_DEFINITION_REASON_NONE;
  definition.entry_definition = 2;
  definition.latch_definition = 8;
  definition.consumer_instruction = 4;
  definition.consumer_operand = 1;
  supplemental_producer.role = RA_JOIN_ASSIGNMENT_PRODUCER;
  supplemental_producer.instruction = 11;
  supplemental_producer.operand = 0;
  supplemental_producer.physical_register = -1;

  if (register_allocator_plan_multi_latch_retention(
      "coreMultiLatchRetentionReady", 5, 15, blocks, edges, 6, 1, 2, 7,
      -1, 5, -1, -1, -1, &loop_join, &definition,
      &supplemental_producer, 1, NULL, 0, assignments, 4, reservations, 5,
      &plan) == FAILED || plan.status != RA_LOOP_RETENTION_READY ||
      plan.assignment_count != 4 || plan.reservation_count != 4 ||
      assignments[0].instruction != 2 || assignments[1].instruction != 8 ||
      assignments[2].instruction != 11 || assignments[3].instruction != 4 ||
      assignments[0].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[1].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[2].role != RA_JOIN_ASSIGNMENT_PRODUCER ||
      assignments[3].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      reservations[0].block_index != 0 ||
      reservations[1].block_index != 1 ||
      reservations[2].block_index != 2 ||
      reservations[3].block_index != 3)
    return FAILED;

  memset(&context, 0, sizeof(context));
  context.result = SUCCEEDED;
  if (register_allocator_apply_loop_retention(
      "coreMultiLatchRetentionApply", 2, 7, assignments, 4, reservations, 5,
      &plan, &context, apply_loop_retention_transaction,
      &application) == FAILED || context.calls != 1 ||
      context.assignment_count != 4 || context.reservation_count != 4 ||
      application.status != RA_LOOP_APPLICATION_APPLIED ||
      application.assignment_count != 4 || application.reservation_count != 4)
    return FAILED;

  supplemental_producer.physical_register = 4;
  if (register_allocator_plan_multi_latch_retention(
      "coreMultiLatchRetentionConflict", 5, 15, blocks, edges, 6, 1, 2, 7,
      -1, 5, -1, -1, -1, &loop_join, &definition,
      &supplemental_producer, 1, NULL, 0, assignments, 4, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  supplemental_producer.physical_register = -1;
  supplemental_producer.instruction = 8;
  if (register_allocator_plan_multi_latch_retention(
      "coreMultiLatchRetentionDuplicate", 5, 15, blocks, edges, 6, 1, 2, 7,
      -1, 5, -1, -1, -1, &loop_join, &definition,
      &supplemental_producer, 1, NULL, 0, assignments, 4, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  supplemental_producer.instruction = 11;
  edges[4].to_block = 4;
  if (register_allocator_plan_multi_latch_retention(
      "coreMultiLatchRetentionBackEdge", 5, 15, blocks, edges, 6, 1, 2, 7,
      -1, 5, -1, -1, -1, &loop_join, &definition,
      &supplemental_producer, 1, NULL, 0, assignments, 4, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  edges[4].to_block = 1;
  if (register_allocator_plan_multi_latch_retention(
      "coreMultiLatchRetentionCapacity", 5, 15, blocks, edges, 6, 1, 2, 7,
      -1, 5, -1, -1, -1, &loop_join, &definition,
      &supplemental_producer, 1, NULL, 0, assignments, 3, reservations, 5,
      &plan) != FAILED ||
      register_allocator_plan_multi_latch_retention(
      "coreMultiLatchRetentionInput", 5, 15, blocks, edges, 6, 1, 2, 7,
      -1, 5, -1, -1, -1, &loop_join, &definition,
      NULL, 1, NULL, 0, assignments, 4, reservations, 5, &plan) != FAILED)
    return FAILED;
  return SUCCEEDED;
}


struct loop_register_path_test_context {
  int blocked_physical_register;
  int invalid_physical_register;
};

static int loop_register_path_is_safe(void *context, int physical_register) {

  struct loop_register_path_test_context *path_context;

  path_context = (struct loop_register_path_test_context *)context;
  if (physical_register == path_context->invalid_physical_register)
    return 2;
  return physical_register == path_context->blocked_physical_register ? NO : YES;
}

static int test_loop_register_path_evaluation(void) {

  struct register_allocator_loop_register_path_evaluation evaluations[3];
  struct loop_register_path_test_context context;
  int candidates[3];

  candidates[0] = 4;
  candidates[1] = 5;
  candidates[2] = 6;
  context.blocked_physical_register = 5;
  context.invalid_physical_register = -1;
  if (register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPaths", 1, 2, -1, candidates, 3, &context,
      loop_register_path_is_safe, evaluations, 3) == FAILED ||
      evaluations[0].physical_register != 4 ||
      evaluations[0].path_safe != YES ||
      evaluations[1].physical_register != 5 ||
      evaluations[1].path_safe != NO ||
      evaluations[2].physical_register != 6 ||
      evaluations[2].path_safe != YES)
    return FAILED;
  context.invalid_physical_register = 6;
  if (register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPathsCallback", 1, 2, -1, candidates, 3, &context,
      loop_register_path_is_safe, evaluations, 3) != FAILED)
    return FAILED;
  context.invalid_physical_register = -1;
  if (register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPathsCapacity", 1, 2, -1, candidates, 3, &context,
      loop_register_path_is_safe, evaluations, 2) != FAILED ||
      register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPathsCandidates", 1, 2, -1, NULL, 3, &context,
      loop_register_path_is_safe, evaluations, 3) != FAILED ||
      register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPathsContext", 1, 2, -1, candidates, 3, NULL,
      loop_register_path_is_safe, evaluations, 3) != FAILED ||
      register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPathsPredicate", 1, 2, -1, candidates, 3, &context,
      NULL, evaluations, 3) != FAILED ||
      register_allocator_evaluate_loop_register_paths(
      "coreLoopRegisterPathsOutput", 1, 2, -1, candidates, 3, &context,
      loop_register_path_is_safe, NULL, 3) != FAILED)
    return FAILED;
  return SUCCEEDED;
}


static int test_loop_register_exclusions(void) {

  struct register_allocator_loop_register_exclusion exclusions[3];
  int candidates[3];

  candidates[0] = 4;
  candidates[1] = 5;
  candidates[2] = 6;
  if (register_allocator_prepare_loop_register_exclusions(
      "coreLoopRegisterExclusions", 1, 2, -1, candidates, 3,
      exclusions, 3) == FAILED || exclusions[0].physical_register != 4 ||
      exclusions[0].excluded != NO ||
      exclusions[0].reason != RA_LOOP_REGISTER_EXCLUSION_NONE ||
      exclusions[0].blocking_block != -1 ||
      exclusions[0].blocking_instruction != -1 ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusions", 1, 2, 1,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 3, 11, exclusions, 3) == FAILED ||
      exclusions[1].physical_register != 5 || exclusions[1].excluded != YES ||
      exclusions[1].reason != RA_LOOP_REGISTER_EXCLUSION_RESERVATION ||
      exclusions[1].blocking_block != 3 ||
      exclusions[1].blocking_instruction != 11 ||
      exclusions[0].excluded != NO || exclusions[2].excluded != NO)
    return FAILED;
  if (register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionsDuplicate", 1, 2, 1,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 3, 11, exclusions, 3) != FAILED ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionsIndex", 1, 2, 3,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 3, 11, exclusions, 3) != FAILED ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionsReason", 1, 2, 0,
      RA_LOOP_REGISTER_EXCLUSION_NONE, 3, 11, exclusions, 3) != FAILED ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionsBlock", 1, 2, 0,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, -1, 11, exclusions, 3) != FAILED ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionsInstruction", 1, 2, 0,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 3, -1, exclusions, 3) != FAILED ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionsOutput", 1, 2, 0,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 3, 11, NULL, 3) != FAILED)
    return FAILED;
  candidates[2] = -1;
  if (register_allocator_prepare_loop_register_exclusions(
      "coreLoopRegisterExclusionsCandidate", 1, 2, -1, candidates, 3,
      exclusions, 3) != FAILED)
    return FAILED;
  candidates[2] = 6;
  if (register_allocator_prepare_loop_register_exclusions(
      "coreLoopRegisterExclusionsCapacity", 1, 2, -1, candidates, 3,
      exclusions, 2) != FAILED ||
      register_allocator_prepare_loop_register_exclusions(
      "coreLoopRegisterExclusionsCandidates", 1, 2, -1, NULL, 3,
      exclusions, 3) != FAILED ||
      register_allocator_prepare_loop_register_exclusions(
      "coreLoopRegisterExclusionsOutput", 1, 2, -1, candidates, 3,
      NULL, 3) != FAILED)
    return FAILED;
  return SUCCEEDED;
}


static int test_loop_retention_register_selection(void) {

  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_loop_register_path_evaluation evaluations[3];
  struct register_allocator_loop_register_selection selection;
  int candidates[3];

  candidates[0] = 4;
  candidates[1] = 5;
  candidates[2] = 6;
  evaluations[0].physical_register = 4;
  evaluations[0].path_safe = YES;
  evaluations[1].physical_register = 5;
  evaluations[1].path_safe = YES;
  evaluations[2].physical_register = 6;
  evaluations[2].path_safe = YES;
  assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[0].instruction = 2;
  assignments[0].operand = 0;
  assignments[0].physical_register = -1;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].instruction = 8;
  assignments[1].operand = 0;
  assignments[1].physical_register = -1;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[2].instruction = 4;
  assignments[2].operand = 1;
  assignments[2].physical_register = -1;

  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterPreferred", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_READY ||
      selection.physical_register != 5 || selection.candidate_index != 1 ||
      selection.bound_assignment_count != 0 ||
      selection.conflict_assignment != -1)
    return FAILED;

  assignments[0].physical_register = 6;
  assignments[1].physical_register = 6;
  assignments[2].physical_register = 6;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterPinnedAlternate", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_READY ||
      selection.physical_register != 6 || selection.candidate_index != 2 ||
      selection.bound_assignment_count != 3)
    return FAILED;

  assignments[1].physical_register = 5;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterConflict", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_INELIGIBLE ||
      selection.physical_register != -1 ||
      selection.bound_assignment_count != 3 ||
      selection.conflict_assignment != 0)
    return FAILED;
  assignments[0].physical_register = 7;
  assignments[1].physical_register = 7;
  assignments[2].physical_register = 7;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterUnsupported", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_INELIGIBLE ||
      selection.physical_register != -1)
    return FAILED;

  assignments[0].physical_register = -1;
  assignments[1].physical_register = -1;
  assignments[2].physical_register = -1;
  candidates[2] = 5;
  evaluations[2].physical_register = 5;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterDuplicate", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_INELIGIBLE)
    return FAILED;
  candidates[2] = 6;
  evaluations[2].physical_register = 6;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterPreference", 1, 2, -1, 7, candidates, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED)
    return FAILED;
  assignments[1].role = 99;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterRole", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED)
    return FAILED;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].operand = 1;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterProducer", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED)
    return FAILED;
  assignments[1].operand = 0;
  assignments[2].operand = 3;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterConsumer", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED)
    return FAILED;
  assignments[2].operand = 0;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterConsumerResult", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_READY)
    return FAILED;
  assignments[2].operand = 1;
  assignments[2].physical_register = -2;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterPhysical", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED)
    return FAILED;
  assignments[2].physical_register = -1;
  evaluations[1].path_safe = NO;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterPathAlternate", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_READY ||
      selection.physical_register != 4 || selection.candidate_index != 0)
    return FAILED;
  evaluations[0].path_safe = NO;
  evaluations[2].path_safe = NO;
  if (register_allocator_select_loop_retention_register(
      "coreLoopRegisterPathBlocked", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_INELIGIBLE)
    return FAILED;
  evaluations[0].path_safe = YES;
  evaluations[1].path_safe = YES;
  evaluations[2].path_safe = YES;
  if (register_allocator_select_loop_retention_register(NULL, 1, 2, -1, 5,
      candidates, 3, assignments, 3, evaluations, 3, &selection) != FAILED ||
      register_allocator_select_loop_retention_register(
      "coreLoopRegisterCandidates", 1, 2, -1, 5, NULL, 3,
      assignments, 3, evaluations, 3, &selection) != FAILED ||
      register_allocator_select_loop_retention_register(
      "coreLoopRegisterAssignments", 1, 2, -1, 5, candidates, 3,
      NULL, 3, evaluations, 3, &selection) != FAILED ||
      register_allocator_select_loop_retention_register(
      "coreLoopRegisterOutput", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, NULL) != FAILED ||
      register_allocator_select_loop_retention_register(
      "coreLoopRegisterPaths", 1, 2, -1, 5, candidates, 3,
      assignments, 3, NULL, 3, &selection) != FAILED ||
      register_allocator_select_loop_retention_register(
      "coreLoopRegisterPathCount", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 2, &selection) != FAILED)
    return FAILED;
  return SUCCEEDED;
}


static int test_loop_retention_register_exclusion_selection(void) {

  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_loop_register_exclusion exclusions[3];
  struct register_allocator_loop_register_path_evaluation evaluations[3];
  struct register_allocator_loop_register_selection selection;
  int candidates[3];
  int index;

  candidates[0] = 4;
  candidates[1] = 5;
  candidates[2] = 6;
  for (index = 0; index < 3; index++) {
    evaluations[index].physical_register = candidates[index];
    evaluations[index].path_safe = YES;
    assignments[index].role = index < 2 ? RA_JOIN_ASSIGNMENT_PRODUCER :
        RA_JOIN_ASSIGNMENT_CONSUMER;
    assignments[index].instruction = index + 2;
    assignments[index].operand = index < 2 ? 0 : 1;
    assignments[index].physical_register = -1;
  }
  if (register_allocator_prepare_loop_register_exclusions(
      "coreLoopRegisterExclusionSelection", 1, 2, -1, candidates, 3,
      exclusions, 3) == FAILED ||
      register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionSelection", 1, 2, 1,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 3, 11, exclusions, 3) == FAILED ||
      register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionSelection", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, exclusions, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_READY ||
      selection.physical_register != 4 || selection.candidate_index != 0)
    return FAILED;
  if (register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionChain", 1, 2, 0,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 4, 12, exclusions, 3) == FAILED ||
      register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionChain", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, exclusions, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_READY ||
      selection.physical_register != 6 || selection.candidate_index != 2)
    return FAILED;
  if (register_allocator_exclude_loop_register_candidate(
      "coreLoopRegisterExclusionAll", 1, 2, 2,
      RA_LOOP_REGISTER_EXCLUSION_RESERVATION, 5, 13, exclusions, 3) == FAILED ||
      register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionAll", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, exclusions, 3, &selection) == FAILED ||
      selection.status != RA_LOOP_REGISTER_SELECTION_INELIGIBLE ||
      selection.physical_register != -1 || selection.candidate_index != -1)
    return FAILED;
  if (register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionNull", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, NULL, 3, &selection) != FAILED ||
      register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionCount", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, exclusions, 2, &selection) != FAILED)
    return FAILED;
  exclusions[0].physical_register = 7;
  if (register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionAlignment", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, exclusions, 3, &selection) != FAILED)
    return FAILED;
  exclusions[0].physical_register = 4;
  exclusions[0].excluded = 2;
  if (register_allocator_select_loop_retention_register_with_exclusions(
      "coreLoopRegisterExclusionState", 1, 2, -1, 5, candidates, 3,
      assignments, 3, evaluations, 3, exclusions, 3, &selection) != FAILED)
    return FAILED;
  return SUCCEEDED;
}


static int test_loop_retention_application(void) {

  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_block_reservation reservations[4];
  struct register_allocator_loop_retention_plan plan;
  struct register_allocator_loop_retention_application application;
  struct loop_retention_application_context context;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  if (register_allocator_plan_loop_retention("coreLoopApplyPlan", 5, 14,
      blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join, &definition,
      NULL, 0, assignments, 3, reservations, 4, &plan) == FAILED)
    return FAILED;
  memset(&context, 0, sizeof(context));
  context.result = SUCCEEDED;
  if (register_allocator_apply_loop_retention("coreLoopApplyReady", 2, 7,
      assignments, 3, reservations, 4, &plan, &context,
      apply_loop_retention_transaction, &application) == FAILED ||
      context.calls != 1 || context.temp_index != 2 || context.slot_index != 7 ||
      context.assignment_count != 3 || context.reservation_count != 4 ||
      application.status != RA_LOOP_APPLICATION_APPLIED ||
      application.assignment_count != 3 || application.reservation_count != 4)
    return FAILED;

  context.calls = 0;
  context.result = FAILED;
  if (register_allocator_apply_loop_retention("coreLoopApplyFailed", 2, 7,
      assignments, 3, reservations, 4, &plan, &context,
      apply_loop_retention_transaction, &application) == FAILED ||
      context.calls != 1 || application.status != RA_LOOP_APPLICATION_FAILED ||
      application.assignment_count != 0 || application.reservation_count != 0)
    return FAILED;
  context.calls = 0;
  context.result = 99;
  if (register_allocator_apply_loop_retention("coreLoopApplyCallback", 2, 7,
      assignments, 3, reservations, 4, &plan, &context,
      apply_loop_retention_transaction, &application) != FAILED ||
      context.calls != 1 || application.status != RA_LOOP_APPLICATION_INELIGIBLE)
    return FAILED;

  context.calls = 0;
  assignments[2].physical_register = 4;
  if (register_allocator_apply_loop_retention("coreLoopApplyAssignment", 2, 7,
      assignments, 3, reservations, 4, &plan, &context,
      apply_loop_retention_transaction, &application) != FAILED ||
      context.calls != 0)
    return FAILED;
  assignments[2].physical_register = 5;
  reservations[1].end_instruction = 1;
  if (register_allocator_apply_loop_retention("coreLoopApplyReservation", 2, 7,
      assignments, 3, reservations, 4, &plan, &context,
      apply_loop_retention_transaction, &application) != FAILED ||
      context.calls != 0)
    return FAILED;
  reservations[1].end_instruction = 5;

  plan.status = RA_LOOP_RETENTION_INELIGIBLE;
  plan.assignment_count = 0;
  plan.reservation_count = 0;
  if (register_allocator_apply_loop_retention("coreLoopApplyIneligible", 2, 7,
      NULL, 0, NULL, 0, &plan, &context, apply_loop_retention_transaction,
      &application) == FAILED || context.calls != 0 ||
      application.status != RA_LOOP_APPLICATION_INELIGIBLE)
    return FAILED;
  plan.status = RA_LOOP_RETENTION_READY;
  plan.assignment_count = 2;
  plan.reservation_count = 4;
  if (register_allocator_apply_loop_retention("coreLoopApplyPlanInvalid", 2, 7,
      assignments, 3, reservations, 4, &plan, &context,
      apply_loop_retention_transaction, &application) != FAILED ||
      register_allocator_apply_loop_retention("coreLoopApplyContext", 2, 7,
      assignments, 3, reservations, 4, &plan, NULL,
      apply_loop_retention_transaction, &application) != FAILED ||
      register_allocator_apply_loop_retention("coreLoopApplyFunction", 2, 7,
      assignments, 3, reservations, 4, &plan, &context, NULL,
      &application) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_transaction_context(
    struct join_assignment_application_context *context) {

  int instruction_index;

  context->calls = 0;
  context->fail = NO;
  for (instruction_index = 0; instruction_index < 12; instruction_index++)
    context->values[instruction_index] = -1;
}

static int test_loop_retention_transaction(void) {

  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;
  struct register_allocator_join_assignment assignments[3];
  struct register_allocator_join_block_reservation reservations[4];
  struct register_allocator_loop_retention_plan plan;
  struct register_allocator_join_path_state_entry state_entries[8];
  struct register_allocator_join_path_state_entry original_entries[8];
  struct join_assignment_application_context context;
  int state_count;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  if (register_allocator_plan_loop_retention("coreLoopTransactionPlan", 5, 14,
      blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join, &definition,
      NULL, 0, assignments, 3, reservations, 4, &plan) == FAILED)
    return FAILED;
  memset(state_entries, 0, sizeof(state_entries));
  state_count = 0;
  initialize_loop_transaction_context(&context);
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopTransactionReady", 5, 14, 2, 7, assignments, 3,
      reservations, 4, state_entries, 8, &state_count, &context,
      apply_join_assignment_transaction) == FAILED || context.calls != 1 ||
      state_count != 4 || context.values[2] != 5 ||
      context.values[9] != 5 || context.values[4] != 5 ||
      state_entries[0].block_index != 0 ||
      state_entries[3].block_index != 3)
    return FAILED;

  memset(state_entries, 0, sizeof(state_entries));
  state_entries[0].block_index = 0;
  state_entries[0].slot_index = 7;
  state_entries[0].temp_index = 8;
  state_entries[0].start_instruction = 1;
  state_entries[0].end_instruction = 3;
  state_count = 1;
  memcpy(original_entries, state_entries, sizeof(state_entries));
  initialize_loop_transaction_context(&context);
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopTransactionConflict", 5, 14, 2, 7, assignments, 3,
      reservations, 4, state_entries, 8, &state_count, &context,
      apply_join_assignment_transaction) != FAILED || context.calls != 0 ||
      state_count != 1 ||
      memcmp(state_entries, original_entries, sizeof(state_entries)) != 0)
    return FAILED;

  memset(state_entries, 0, sizeof(state_entries));
  memcpy(original_entries, state_entries, sizeof(state_entries));
  state_count = 0;
  initialize_loop_transaction_context(&context);
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopTransactionCapacity", 5, 14, 2, 7, assignments, 3,
      reservations, 4, state_entries, 3, &state_count, &context,
      apply_join_assignment_transaction) != FAILED || context.calls != 0 ||
      state_count != 0 ||
      memcmp(state_entries, original_entries, sizeof(state_entries)) != 0)
    return FAILED;

  memset(state_entries, 0, sizeof(state_entries));
  memcpy(original_entries, state_entries, sizeof(state_entries));
  state_count = 0;
  initialize_loop_transaction_context(&context);
  context.fail = YES;
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopTransactionAssignment", 5, 14, 2, 7, assignments, 3,
      reservations, 4, state_entries, 8, &state_count, &context,
      apply_join_assignment_transaction) != FAILED || context.calls != 1 ||
      state_count != 0 || context.values[2] != -1 ||
      memcmp(state_entries, original_entries, sizeof(state_entries)) != 0)
    return FAILED;

  initialize_loop_transaction_context(&context);
  reservations[1].block_index = reservations[0].block_index;
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopTransactionReservation", 5, 14, 2, 7, assignments, 3,
      reservations, 4, state_entries, 8, &state_count, &context,
      apply_join_assignment_transaction) != FAILED || context.calls != 0 ||
      state_count != 0 ||
      memcmp(state_entries, original_entries, sizeof(state_entries)) != 0)
    return FAILED;
  return SUCCEEDED;
}

struct loop_supplemental_context {
  int calls;
  int assignment_count;
  int reservation_count;
};

static int apply_loop_supplemental_retention(void *context, int temp_index,
    int slot_index, struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_count) {

  struct loop_supplemental_context *application_context;

  application_context = (struct loop_supplemental_context *)context;
  application_context->calls++;
  application_context->assignment_count = assignment_count;
  application_context->reservation_count = reservation_count;
  if (temp_index != 2 || slot_index != 7 || assignments == NULL ||
      assignment_count != 5 || reservations == NULL || reservation_count != 5)
    return FAILED;
  return SUCCEEDED;
}

static int apply_loop_supplemental_assignments(void *context, int temp_index,
    struct register_allocator_join_assignment *assignments,
    int assignment_count) {

  struct loop_supplemental_context *application_context;

  application_context = (struct loop_supplemental_context *)context;
  application_context->calls++;
  application_context->assignment_count = assignment_count;
  if (temp_index != 2 || assignments == NULL || assignment_count != 5)
    return FAILED;
  return SUCCEEDED;
}

static int test_loop_retention_supplemental_consumers(void) {

  struct register_allocator_basic_block blocks[5];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;
  struct register_allocator_join_assignment assignments[5];
  struct register_allocator_join_assignment supplemental[2];
  struct register_allocator_join_block_reservation reservations[5];
  struct register_allocator_loop_retention_plan plan;
  struct register_allocator_loop_retention_application application;
  struct register_allocator_join_path_state_entry state_entries[8];
  struct loop_supplemental_context context;
  struct register_allocator_join_assignment saved_assignment;
  int state_count;

  initialize_loop_retention_fixture(blocks, edges, &loop_join, &definition,
      assignments, reservations, &plan);
  supplemental[0].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  supplemental[0].instruction = 9;
  supplemental[0].operand = 1;
  supplemental[0].physical_register = -1;
  supplemental[1].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  supplemental[1].instruction = 12;
  supplemental[1].operand = 1;
  supplemental[1].physical_register = -1;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalReady", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 5, reservations, 5,
      &plan) == FAILED || plan.status != RA_LOOP_RETENTION_READY ||
      plan.assignment_count != 5 || plan.reservation_count != 5 ||
      assignments[3].instruction != 9 || assignments[3].operand != 1 ||
      assignments[3].physical_register != 5 ||
      assignments[4].instruction != 12 || assignments[4].operand != 1 ||
      assignments[4].physical_register != 5 ||
      reservations[4].block_index != 4 ||
      reservations[4].end_instruction != 12)
    return FAILED;

  memset(&context, 0, sizeof(context));
  if (register_allocator_apply_loop_retention("coreLoopSupplementalApply", 2,
      7, assignments, 5, reservations, 5, &plan, &context,
      apply_loop_supplemental_retention, &application) == FAILED ||
      context.calls != 1 || context.assignment_count != 5 ||
      context.reservation_count != 5 ||
      application.status != RA_LOOP_APPLICATION_APPLIED ||
      application.assignment_count != 5 || application.reservation_count != 5)
    return FAILED;

  memset(state_entries, 0, sizeof(state_entries));
  memset(&context, 0, sizeof(context));
  state_count = 0;
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopSupplementalTransaction", 5, 14, 2, 7, assignments, 5,
      reservations, 5, state_entries, 8, &state_count, &context,
      apply_loop_supplemental_assignments) == FAILED || context.calls != 1 ||
      context.assignment_count != 5 || state_count != 5 ||
      state_entries[4].block_index != 4)
    return FAILED;

  saved_assignment = assignments[4];
  assignments[4] = assignments[3];
  memset(&context, 0, sizeof(context));
  if (register_allocator_apply_loop_retention(
      "coreLoopSupplementalApplyDuplicate", 2, 7, assignments, 5,
      reservations, 5, &plan, &context, apply_loop_supplemental_retention,
      &application) != FAILED || context.calls != 0)
    return FAILED;
  memset(state_entries, 0, sizeof(state_entries));
  state_count = 0;
  if (register_allocator_commit_loop_retention_transaction(
      "coreLoopSupplementalTransactionDuplicate", 5, 14, 2, 7,
      assignments, 5, reservations, 5, state_entries, 8, &state_count,
      &context, apply_loop_supplemental_assignments) != FAILED ||
      context.calls != 0 || state_count != 0)
    return FAILED;
  assignments[4] = saved_assignment;

  supplemental[1] = supplemental[0];
  if (register_allocator_plan_loop_retention("coreLoopSupplementalDuplicate",
      5, 14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 5, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  supplemental[1].instruction = 12;
  supplemental[1].physical_register = 4;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalConflict",
      5, 14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 5, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  supplemental[1].physical_register = -1;
  supplemental[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalRole", 5,
      14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 5, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  supplemental[1].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalCapacity",
      5, 14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 4, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  supplemental[1].instruction = 1;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalUnreachable",
      5, 14, blocks, edges, 5, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 5, reservations, 5,
      &plan) != FAILED)
    return FAILED;
  return SUCCEEDED;
}

static void initialize_loop_exit_path_fixture(
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_loop_definition *definition) {

  int block_index;

  for (block_index = 0; block_index < 8; block_index++) {
    blocks[block_index].start_tac = block_index * 2;
    blocks[block_index].end_tac = block_index * 2 + 1;
  }
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 3;
  edges[1].to_block = 1;
  edges[1].kind = RA_CFG_EDGE_JUMP;
  edges[2].from_block = 1;
  edges[2].to_block = 2;
  edges[2].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[3].from_block = 2;
  edges[3].to_block = 3;
  edges[3].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[4].from_block = 1;
  edges[4].to_block = 4;
  edges[4].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[5].from_block = 4;
  edges[5].to_block = 5;
  edges[5].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[6].from_block = 4;
  edges[6].to_block = 6;
  edges[6].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[7].from_block = 5;
  edges[7].to_block = 7;
  edges[7].kind = RA_CFG_EDGE_JUMP;
  edges[8].from_block = 6;
  edges[8].to_block = 7;
  edges[8].kind = RA_CFG_EDGE_FALLTHROUGH;
  loop_join->status = RA_LOOP_JOIN_READY;
  loop_join->entry_predecessor = 0;
  loop_join->latch_predecessor = 3;
  loop_join->entry_edge = 0;
  loop_join->back_edge = 1;
  definition->status = RA_LOOP_DEFINITION_READY;
  definition->reason = RA_LOOP_DEFINITION_REASON_NONE;
  definition->entry_definition = 1;
  definition->latch_definition = 6;
  definition->consumer_instruction = 2;
  definition->consumer_operand = TAC_USE_ARG1;
}

static int test_loop_retention_supplemental_paths(void) {

  struct register_allocator_basic_block blocks[8];
  struct register_allocator_cfg_edge edges[9];
  struct register_allocator_loop_join loop_join;
  struct register_allocator_loop_definition definition;
  struct register_allocator_join_assignment assignments[5];
  struct register_allocator_join_assignment supplemental[2];
  struct register_allocator_join_block_reservation reservations[8];
  struct register_allocator_loop_retention_plan plan;
  int reservation_index;

  initialize_loop_exit_path_fixture(blocks, edges, &loop_join, &definition);
  supplemental[0].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  supplemental[0].instruction = 10;
  supplemental[0].operand = TAC_USE_ARG1;
  supplemental[0].physical_register = -1;
  supplemental[1].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  supplemental[1].instruction = 12;
  supplemental[1].operand = TAC_USE_ARG2;
  supplemental[1].physical_register = -1;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalExitArms",
      8, 16, blocks, edges, 9, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 2, assignments, 5, reservations, 8,
      &plan) == FAILED || plan.status != RA_LOOP_RETENTION_READY ||
      plan.assignment_count != 5 || plan.reservation_count != 7)
    return FAILED;
  for (reservation_index = 0; reservation_index < 7; reservation_index++) {
    if (reservations[reservation_index].block_index != reservation_index)
      return FAILED;
  }
  if (reservations[4].end_instruction != 9 ||
      reservations[5].end_instruction != 10 ||
      reservations[6].end_instruction != 12)
    return FAILED;

  supplemental[0].instruction = 14;
  if (register_allocator_plan_loop_retention("coreLoopSupplementalMerge",
      8, 16, blocks, edges, 9, 1, 2, 7, -1, 5, 5, 5, 5, &loop_join,
      &definition, supplemental, 1, assignments, 4, reservations, 8,
      &plan) == FAILED || plan.status != RA_LOOP_RETENTION_READY ||
      plan.assignment_count != 4 || plan.reservation_count != 8)
    return FAILED;
  for (reservation_index = 0; reservation_index < 8; reservation_index++) {
    if (reservations[reservation_index].block_index != reservation_index)
      return FAILED;
  }
  if (reservations[5].end_instruction != 11 ||
      reservations[6].end_instruction != 13 ||
      reservations[7].end_instruction != 14)
    return FAILED;

  assignments[0].instruction = 99;
  reservations[0].block_index = 99;
  if (register_allocator_plan_loop_retention(
      "coreLoopSupplementalMergeCapacity", 8, 16, blocks, edges, 9, 1, 2,
      7, -1, 5, 5, 5, 5, &loop_join, &definition, supplemental, 1,
      assignments, 4, reservations, 7, &plan) != FAILED ||
      plan.status != RA_LOOP_RETENTION_INELIGIBLE ||
      plan.assignment_count != 0 || plan.reservation_count != 0 ||
      assignments[0].instruction != 99 || reservations[0].block_index != 99)
    return FAILED;
  return SUCCEEDED;
}

int main(void) {

  struct register_allocator_liveness_storage storage;
  int maximum_block_count;
  int iterations;

  if (expect_storage("coreLivenessStorageOne", 1, 1) == FAILED ||
      expect_storage("coreLivenessStorageWide", 4, 40) == FAILED ||
      expect_storage("coreLivenessStorageBlocks", 9, 3) == FAILED)
    return 1;
  if (expect_invalid_storage(NULL, 4, 40) == FAILED ||
      expect_invalid_storage("coreLivenessStorageZeroBlocks", 0, 40) == FAILED ||
      expect_invalid_storage("coreLivenessStorageZeroTemps", 4, 0) == FAILED ||
      expect_invalid_storage("coreLivenessStorageNegativeBlocks", -1, 40) == FAILED ||
      expect_invalid_storage("coreLivenessStorageNegativeTemps", 4, -1) == FAILED ||
      expect_invalid_storage("coreLivenessStorageIndexOverflow", INT_MAX, 2) == FAILED)
    return 2;
  maximum_block_count = INT_MAX;
  if ((size_t)maximum_block_count > (size_t)ULONG_MAX / 4) {
    if (expect_invalid_storage("coreLivenessStorageMaximum", maximum_block_count,
        1) == FAILED)
      return 3;
  }
  else if (expect_storage("coreLivenessStorageMaximum", maximum_block_count,
      1) == FAILED)
    return 4;
  storage.set_count = 1;
  storage.buffer_bytes = 2;
  storage.total_bytes = 3;
  if (register_allocator_plan_liveness_storage("coreLivenessStorageNoOutput",
      4, 40, NULL) != FAILED || storage.set_count != 1)
    return 5;
  if (test_storage_lifecycle() == FAILED)
    return 6;
  if (test_liveness_index_resolution() == FAILED)
    return 7;
  if (test_invalid_solve_inputs() == FAILED)
    return 8;
  if (test_use_def_collection() == FAILED)
    return 9;
  if (test_invalid_use_def_inputs() == FAILED)
    return 10;
  if (test_invalid_use_def_callbacks() == FAILED)
    return 11;
  if (test_join_temp_state_classification() == FAILED)
    return 12;
  if (test_join_action_planning() == FAILED)
    return 13;
  if (test_join_predecessor_collection() == FAILED)
    return 14;
  if (test_join_spill_site_planning() == FAILED)
    return 15;
  if (test_join_spill_site_ordering() == FAILED)
    return 24;
  if (test_join_spill_emission_ordering() == FAILED)
    return 25;
  if (test_post_mutation_rebuild_planning() == FAILED)
    return 26;
  if (test_fixed_point_planning() == FAILED)
    return 50;
  if (test_join_retention_planning() == FAILED)
    return 27;
  if (test_reaching_definition_resolution() == FAILED)
    return 28;
  if (test_join_schedule_planning() == FAILED)
    return 29;
  if (test_join_assignment_application() == FAILED)
    return 37;
  if (test_join_reservation_planning() == FAILED)
    return 30;
  if (test_join_path_reservation_planning() == FAILED)
    return 31;
  if (test_join_path_reservation_application() == FAILED)
    return 32;
  if (test_join_path_reservation_commit() == FAILED)
    return 33;
  if (test_join_path_state_storage() == FAILED)
    return 34;
  if (test_join_path_state_query() == FAILED)
    return 35;
  if (test_block_exit_spill_planning() == FAILED)
    return 36;
  if (test_loop_join_classification() == FAILED)
    return 38;
  if (test_loop_flow_profiling() == FAILED)
    return 49;
  if (test_loop_latch_selection() == FAILED)
    return 51;
  if (test_loop_latch_collection() == FAILED)
    return 52;
  if (test_loop_join_candidate_planning() == FAILED)
    return 41;
  if (test_loop_representation_classification() == FAILED)
    return 1;
  if (test_loop_stack_value_collection() == FAILED)
    return 1;
  if (test_loop_stack_value_selection() == FAILED)
    return 1;
  if (test_loop_stack_promotion_plan() == FAILED)
    return 1;
  if (test_loop_stack_rewrite_schedule() == FAILED)
    return 1;
  if (test_loop_stack_promotion_application() == FAILED)
    return 1;
  if (test_loop_stack_promotion_storage_transaction() == FAILED)
    return 1;
  if (test_loop_join_fact_discovery() == FAILED)
    return 43;
  if (test_loop_join_definition_planning() == FAILED)
    return 42;
  if (test_loop_join_schedule_planning() == FAILED)
    return 39;
  if (test_loop_join_path_reservation_planning() == FAILED)
    return 40;
  if (test_loop_retention_planning() == FAILED)
    return 44;
  if (test_multi_latch_retention_planning() == FAILED)
    return 53;
  if (test_loop_register_path_evaluation() == FAILED ||
      test_loop_register_exclusions() == FAILED ||
      test_loop_retention_register_selection() == FAILED ||
      test_loop_retention_register_exclusion_selection() == FAILED)
    return 45;
  if (test_loop_retention_application() == FAILED)
    return 46;
  if (test_loop_retention_transaction() == FAILED)
    return 47;
  if (test_loop_retention_supplemental_consumers() == FAILED)
    return 47;
  if (test_loop_retention_supplemental_paths() == FAILED)
    return 48;
  if (test_join_spill_site_approval() == FAILED)
    return 16;
  if (test_join_spill_mutation_preparation() == FAILED)
    return 17;
  if (test_join_spill_emission_preparation() == FAILED)
    return 18;
  if (test_join_spill_emission_application() == FAILED)
    return 1;
  if (test_join_spill_mutation_application() == FAILED)
    return 19;
  if (test_join_reconciliation() == FAILED)
    return 20;
  if (test_join_no_join_graphs() == FAILED)
    return 21;
  if (test_invalid_join_inputs() == FAILED)
    return 22;
  iterations = 99;
  if (register_allocator_solve_liveness("coreLivenessSolveEmpty", 0, 2, NULL,
      NULL, 0, NULL, NULL, NULL, NULL, &iterations) == FAILED || iterations != 0)
    return 23;
  return 0;
}
