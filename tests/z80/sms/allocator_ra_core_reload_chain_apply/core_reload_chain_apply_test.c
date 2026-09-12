#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_INSTRUCTION_COUNT 8
#define TEST_RELOAD_CAPACITY 4
#define TEST_NO_REGISTER -1

struct reload_apply_context {
  char active[TEST_INSTRUCTION_COUNT];
  char reads[TEST_INSTRUCTION_COUNT];
  char writes[TEST_INSTRUCTION_COUNT];
  char eligible[TEST_INSTRUCTION_COUNT];
  int operands[TEST_INSTRUCTION_COUNT];
  int physical_registers[TEST_INSTRUCTION_COUNT];
  int applied_instructions[TEST_RELOAD_CAPACITY];
  int applied_operands[TEST_RELOAD_CAPACITY];
  int applied_registers[TEST_RELOAD_CAPACITY];
  int applied_count;
  int fail_instruction;
};

static int is_active(void *context, int instruction_index, int temp_index) {

  struct reload_apply_context *apply_context;

  (void)temp_index;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->active[instruction_index];
}

static int reads_temp(void *context, int instruction_index, int temp_index) {

  struct reload_apply_context *apply_context;

  (void)temp_index;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->reads[instruction_index];
}

static int writes_temp(void *context, int instruction_index, int temp_index) {

  struct reload_apply_context *apply_context;

  (void)temp_index;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->writes[instruction_index];
}

static int is_reload_eligible(void *context, int instruction_index, int temp_index) {

  struct reload_apply_context *apply_context;

  (void)temp_index;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->eligible[instruction_index];
}

static int apply_reload(void *context, int instruction_index, int temp_index, int operand, int physical_register) {

  struct reload_apply_context *apply_context;

  apply_context = (struct reload_apply_context *)context;
    if (temp_index != 7 ||
      (operand != TAC_USE_RESULT && operand != TAC_USE_ARG1 && operand != TAC_USE_ARG2) ||
      apply_context->applied_count >= TEST_RELOAD_CAPACITY)
    return FAILED;
  if (instruction_index == apply_context->fail_instruction)
    return FAILED;

  apply_context->applied_instructions[apply_context->applied_count] = instruction_index;
  apply_context->applied_operands[apply_context->applied_count] = operand;
  apply_context->applied_registers[apply_context->applied_count] = physical_register;
  apply_context->applied_count++;
  return SUCCEEDED;
}

static int apply_reload_transaction(void *context,
    struct register_allocator_reload_mutation *mutations, int mutation_count) {

  struct reload_apply_context *apply_context;
  int mutation_index;

  apply_context = (struct reload_apply_context *)context;
  if (mutations == NULL || mutation_count < 0 ||
      mutation_count > TEST_RELOAD_CAPACITY)
    return FAILED;
  for (mutation_index = 0; mutation_index < mutation_count; mutation_index++) {
    if (mutations[mutation_index].apply != YES ||
        mutations[mutation_index].temp_index != 7 ||
        mutations[mutation_index].instruction == apply_context->fail_instruction)
      return FAILED;
  }
  for (mutation_index = 0; mutation_index < mutation_count; mutation_index++) {
    apply_context->applied_instructions[mutation_index] =
        mutations[mutation_index].instruction;
    apply_context->applied_operands[mutation_index] = mutations[mutation_index].operand;
    apply_context->applied_registers[mutation_index] =
        mutations[mutation_index].physical_register;
  }
  apply_context->applied_count = mutation_count;
  return SUCCEEDED;
}

static int select_reload_register(void *context, int instruction_index, int temp_index) {

  struct reload_apply_context *apply_context;

  if (temp_index != 7)
    return TEST_NO_REGISTER;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->physical_registers[instruction_index];
}

static int get_reload_operand(void *context, int instruction_index, int temp_index) {

  struct reload_apply_context *apply_context;

  if (temp_index != 7)
    return -1;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->operands[instruction_index];
}

static int select_reload_operand_register(void *context, int instruction_index,
    int temp_index, int operand) {

  struct reload_apply_context *apply_context;

  if (temp_index != 7 || (operand != TAC_USE_RESULT && operand != TAC_USE_ARG1 &&
      operand != TAC_USE_ARG2))
    return TEST_NO_REGISTER;
  apply_context = (struct reload_apply_context *)context;
  return apply_context->physical_registers[instruction_index];
}

static void reset_context(struct reload_apply_context *context) {

  int instruction_index;
  int reload_index;

  for (instruction_index = 0; instruction_index < TEST_INSTRUCTION_COUNT; instruction_index++) {
    context->active[instruction_index] = YES;
    context->reads[instruction_index] = NO;
    context->writes[instruction_index] = NO;
    context->eligible[instruction_index] = NO;
    context->operands[instruction_index] = TAC_USE_RESULT;
    context->physical_registers[instruction_index] = TEST_NO_REGISTER;
  }
  for (reload_index = 0; reload_index < TEST_RELOAD_CAPACITY; reload_index++) {
    context->applied_instructions[reload_index] = -1;
    context->applied_operands[reload_index] = -1;
    context->applied_registers[reload_index] = TEST_NO_REGISTER;
  }
  context->applied_count = 0;
  context->fail_instruction = -1;
}

static int apply_chain(char *function_name, struct reload_apply_context *context, int *discovered_count) {

  struct register_allocator_reload_chain_execution chain_execution;
  struct register_allocator_reload_execution executions[TEST_RELOAD_CAPACITY];
  int reload_instructions[TEST_RELOAD_CAPACITY];

  if (register_allocator_execute_reload_chain(function_name, 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, context, is_active, reads_temp,
      writes_temp, is_reload_eligible, context, select_reload_register, context,
      apply_reload, reload_instructions, executions, TEST_RELOAD_CAPACITY,
      &chain_execution) == FAILED)
    return FAILED;

  *discovered_count = chain_execution.chain.count;
  if (chain_execution.processed_count != chain_execution.chain.count ||
      chain_execution.applied_count != context->applied_count)
    return FAILED;

  fprintf(stderr, "core_reload_chain_apply: function=%s discovered=%d applied=%d status=complete\n",
      function_name, chain_execution.chain.count, context->applied_count);
  return SUCCEEDED;
}

static int reload_execution_is_clear(struct register_allocator_reload_execution *execution) {

  return execution->action == FAILED && execution->mutation.apply == NO &&
      execution->mutation.instruction == -1 && execution->mutation.temp_index == -1 &&
      execution->mutation.physical_register == TEST_NO_REGISTER;
}

static int reload_chain_execution_is_clear(struct register_allocator_reload_chain_execution *execution) {

  return execution->chain.count == 0 && execution->chain.truncated == NO &&
      execution->chain.stop_instruction == -1 && execution->processed_count == 0 &&
      execution->applied_count == 0;
}

static int expect_storage(char *function_name, int capacity) {

  struct register_allocator_reload_chain_storage storage;

  storage.instruction_bytes = 1;
  storage.execution_bytes = 1;
  if (register_allocator_plan_reload_chain_storage(function_name, capacity, &storage) == FAILED)
    return FAILED;
  if (storage.instruction_bytes != (size_t)capacity * sizeof(int) ||
      storage.execution_bytes != (size_t)capacity * sizeof(struct register_allocator_reload_execution))
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_storage(char *function_name, int capacity) {

  struct register_allocator_reload_chain_storage storage;

  storage.instruction_bytes = 123;
  storage.execution_bytes = 456;
  if (register_allocator_plan_reload_chain_storage(function_name, capacity, &storage) != FAILED)
    return FAILED;
  return storage.instruction_bytes == 0 && storage.execution_bytes == 0 ? SUCCEEDED : FAILED;
}

static void add_reload(struct reload_apply_context *context, int instruction, int physical_register);

static int test_operand_chain_execution(struct reload_apply_context *context) {

  struct register_allocator_reload_chain_execution chain_execution;
  struct register_allocator_reload_execution executions[TEST_RELOAD_CAPACITY];
  int reload_instructions[TEST_RELOAD_CAPACITY];

  reset_context(context);
  add_reload(context, 1, 90);
  add_reload(context, 3, 91);
  add_reload(context, 5, 92);
  context->operands[1] = TAC_USE_RESULT;
  context->operands[3] = TAC_USE_ARG1;
  context->operands[5] = TAC_USE_ARG2;
  if (register_allocator_execute_reload_operand_chain("coreOperandChainMixed",
      2, 7, 0, 1, 6, TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER,
      context, is_active, reads_temp, writes_temp, is_reload_eligible,
      context, get_reload_operand, select_reload_operand_register,
      context, apply_reload, reload_instructions, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) == FAILED)
    return FAILED;
  if (chain_execution.chain.count != 3 || chain_execution.processed_count != 3 ||
      chain_execution.applied_count != 3 || context->applied_count != 3 ||
      context->applied_operands[0] != TAC_USE_RESULT ||
      context->applied_operands[1] != TAC_USE_ARG1 ||
      context->applied_operands[2] != TAC_USE_ARG2 ||
      executions[0].mutation.operand != TAC_USE_RESULT ||
      executions[1].mutation.operand != TAC_USE_ARG1 ||
      executions[2].mutation.operand != TAC_USE_ARG2 ||
      executions[0].mutation.physical_register != 90 ||
      executions[1].mutation.physical_register != 91 ||
      executions[2].mutation.physical_register != 92)
    return FAILED;

  reset_context(context);
  add_reload(context, 1, 93);
  context->operands[1] = -1;
  memset(executions, 7, sizeof(executions));
  memset(reload_instructions, 7, sizeof(reload_instructions));
  if (register_allocator_execute_reload_operand_chain("coreOperandChainInvalid",
      2, 7, 0, 1, 6, TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER,
      context, is_active, reads_temp, writes_temp, is_reload_eligible,
      context, get_reload_operand, select_reload_operand_register,
      context, apply_reload, reload_instructions, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) != FAILED ||
      reload_chain_execution_is_clear(&chain_execution) == NO ||
      reload_execution_is_clear(&executions[0]) == NO ||
      reload_instructions[0] != -1 || context->applied_count != 0)
    return FAILED;

  fprintf(stderr, "core_reload_operand_chain: mixed=3 invalid=1 operands=result,arg1,arg2 status=complete\n");
  return SUCCEEDED;
}

static int test_atomic_operand_chain(struct reload_apply_context *context) {

  struct register_allocator_reload_chain_execution chain_execution;
  struct register_allocator_reload_execution executions[TEST_RELOAD_CAPACITY];
  int reload_instructions[TEST_RELOAD_CAPACITY];

  reset_context(context);
  add_reload(context, 1, 94);
  add_reload(context, 3, 95);
  add_reload(context, 5, 96);
  context->operands[1] = TAC_USE_RESULT;
  context->operands[3] = TAC_USE_ARG1;
  context->operands[5] = TAC_USE_ARG2;
  if (register_allocator_execute_reload_operand_chain_atomic(
      "coreOperandChainAtomicCommit", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, context, is_active,
      reads_temp, writes_temp, is_reload_eligible, context,
      get_reload_operand, select_reload_operand_register, context,
      apply_reload_transaction, reload_instructions, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) == FAILED ||
      chain_execution.chain.count != 3 ||
      chain_execution.processed_count != 3 ||
      chain_execution.applied_count != 3 || context->applied_count != 3)
    return FAILED;

  reset_context(context);
  add_reload(context, 1, 97);
  add_reload(context, 3, 98);
  add_reload(context, 5, 99);
  context->fail_instruction = 5;
  if (register_allocator_execute_reload_operand_chain_atomic(
      "coreOperandChainAtomicReject", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, context, is_active,
      reads_temp, writes_temp, is_reload_eligible, context,
      get_reload_operand, select_reload_operand_register, context,
      apply_reload_transaction, reload_instructions, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) != FAILED ||
      context->applied_count != 0 ||
      reload_chain_execution_is_clear(&chain_execution) == NO ||
      reload_instructions[0] != -1 ||
      reload_execution_is_clear(&executions[0]) == NO)
    return FAILED;

  fprintf(stderr, "core_reload_operand_chain_atomic: committed=3 rejected=3 partial=0 status=complete\n");
  return SUCCEEDED;
}

static int test_fallthrough_range_execution(struct reload_apply_context *context) {

  struct register_allocator_basic_block blocks[3];
  struct register_allocator_cfg_edge edges[2];
  struct register_allocator_reload_chain_execution chain_execution;
  struct register_allocator_reload_execution executions[TEST_RELOAD_CAPACITY];
  struct register_allocator_reload_scan_range range;
  int reload_instructions[TEST_RELOAD_CAPACITY];

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 1;
  blocks[1].start_tac = 2;
  blocks[1].end_tac = 4;
  blocks[2].start_tac = 5;
  blocks[2].end_tac = 7;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_FALLTHROUGH;
  if (register_allocator_plan_reload_scan_range("coreRangeExecuteFallthrough",
      3, TEST_INSTRUCTION_COUNT, blocks, edges, 2, 0, &range) == FAILED)
    return FAILED;

  reset_context(context);
  add_reload(context, 6, 82);
  if (register_allocator_execute_reload_chain("coreRangeExecuteFallthrough",
      0, 7, 1, 2, range.end_instruction, TEST_INSTRUCTION_COUNT,
      TEST_NO_REGISTER, context, is_active, reads_temp, writes_temp,
      is_reload_eligible, context, select_reload_register, context,
      apply_reload, reload_instructions, executions, TEST_RELOAD_CAPACITY,
      &chain_execution) == FAILED)
    return FAILED;
  if (range.end_block != 2 || range.extended_block_count != 2 ||
      chain_execution.chain.count != 1 ||
      chain_execution.processed_count != 1 ||
      chain_execution.applied_count != 1 || context->applied_count != 1 ||
      context->applied_instructions[0] != 6 ||
      context->applied_registers[0] != 82)
    return FAILED;
  fprintf(stderr, "core_reload_range_apply: function=coreRangeExecuteFallthrough end_block=%d extended=%d reload=6 applied=1 status=complete\n",
      range.end_block, range.extended_block_count);
  return SUCCEEDED;
}

static int test_reload_graph_discovery(struct reload_apply_context *context) {

  struct register_allocator_basic_block blocks[4];
  struct register_allocator_cfg_edge edges[5];
  struct register_allocator_reload_chain chain;
  struct register_allocator_reload_chain_execution chain_execution;
  struct register_allocator_reload_execution executions[TEST_RELOAD_CAPACITY];
  int reload_instructions[TEST_RELOAD_CAPACITY];

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 1;
  blocks[1].start_tac = 2;
  blocks[1].end_tac = 3;
  blocks[2].start_tac = 4;
  blocks[2].end_tac = 5;
  blocks[3].start_tac = 6;
  blocks[3].end_tac = 7;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_BRANCH_TRUE;
  edges[1].from_block = 0;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_BRANCH_FALSE;
  edges[2].from_block = 1;
  edges[2].to_block = 3;
  edges[2].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[3].from_block = 2;
  edges[3].to_block = 3;
  edges[3].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[4].from_block = 3;
  edges[4].to_block = 0;
  edges[4].kind = RA_CFG_EDGE_FALLTHROUGH;

  reset_context(context);
  add_reload(context, 3, 100);
  add_reload(context, 5, 101);
  add_reload(context, 7, 102);
  if (register_allocator_discover_reload_graph("coreReloadGraphDiamond", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 5, 0, 1, 7, context,
      is_active, reads_temp, writes_temp, is_reload_eligible,
      reload_instructions, TEST_RELOAD_CAPACITY, &chain) == FAILED ||
      chain.count != 3 || chain.truncated != NO ||
      reload_instructions[0] != 3 || reload_instructions[1] != 5 ||
      reload_instructions[2] != 7)
    return FAILED;
  if (register_allocator_execute_reload_graph_transaction(
      "coreReloadGraphDiamond", 0, 7, 1, TEST_NO_REGISTER, context,
      get_reload_operand, select_reload_operand_register, context,
      apply_reload_transaction, reload_instructions, chain.count, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) == FAILED ||
      chain_execution.chain.count != 3 ||
      chain_execution.processed_count != 3 ||
      chain_execution.applied_count != 3 || context->applied_count != 3)
    return FAILED;

  context->applied_count = 0;
  context->fail_instruction = 7;
  if (register_allocator_execute_reload_graph_transaction(
      "coreReloadGraphAtomicReject", 0, 7, 1, TEST_NO_REGISTER, context,
      get_reload_operand, select_reload_operand_register, context,
      apply_reload_transaction, reload_instructions, chain.count, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) != FAILED ||
      context->applied_count != 0 ||
      reload_chain_execution_is_clear(&chain_execution) == NO)
    return FAILED;

  context->fail_instruction = -1;
  reload_instructions[0] = 0;
  if (register_allocator_execute_reload_graph_transaction(
      "coreReloadGraphBackwardReject", 0, 7, 1, TEST_NO_REGISTER, context,
      get_reload_operand, select_reload_operand_register, context,
      apply_reload_transaction, reload_instructions, 1, executions,
      TEST_RELOAD_CAPACITY, &chain_execution) != FAILED ||
      context->applied_count != 0 ||
      reload_chain_execution_is_clear(&chain_execution) == NO)
    return FAILED;

  if (register_allocator_discover_reload_graph("coreReloadGraphTruncated", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 5, 0, 1, 7, context,
      is_active, reads_temp, writes_temp, is_reload_eligible,
      reload_instructions, 2, &chain) == FAILED || chain.count != 0 ||
      chain.truncated != YES || reload_instructions[0] != -1)
    return FAILED;

  blocks[2].start_tac = 3;
  if (register_allocator_discover_reload_graph("coreReloadGraphInvalidBlock", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 5, 0, 1, 7, context,
      is_active, reads_temp, writes_temp, is_reload_eligible,
      reload_instructions, TEST_RELOAD_CAPACITY, &chain) != FAILED)
    return FAILED;
  blocks[2].start_tac = 4;
  edges[0].to_block = 4;
  if (register_allocator_discover_reload_graph("coreReloadGraphInvalidEdge", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 5, 0, 1, 7, context,
      is_active, reads_temp, writes_temp, is_reload_eligible,
      reload_instructions, TEST_RELOAD_CAPACITY, &chain) != FAILED)
    return FAILED;
  edges[0].to_block = 1;

  reset_context(context);
  add_reload(context, 3, 100);
  add_reload(context, 5, 101);
  add_reload(context, 7, 102);
  context->eligible[5] = NO;
  if (register_allocator_discover_reload_graph("coreReloadGraphPathStop", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 4, 0, 1, 7, context,
      is_active, reads_temp, writes_temp, is_reload_eligible,
      reload_instructions, TEST_RELOAD_CAPACITY, &chain) == FAILED ||
      chain.count != 0 || reload_instructions[0] != -1)
    return FAILED;

  fprintf(stderr, "core_reload_graph: diamond=3 cycle_deduplicated=yes path_stop=rejected truncated=rejected invalid=2 transaction_rejected=1 backward_rejected=1 partial=0 join=1 status=complete\n");
  return SUCCEEDED;
}

static int test_storage_lifecycle(struct reload_apply_context *context) {

  struct register_allocator_reload_chain_execution chain_execution;
  struct register_allocator_reload_chain_storage storage;
  struct register_allocator_reload_execution *executions;
  int *instructions;
  int status;

  if (register_allocator_plan_reload_chain_storage("coreReloadChainStorageLifecycle",
      TEST_RELOAD_CAPACITY, &storage) == FAILED)
    return FAILED;
  instructions = (int *)calloc(1, storage.instruction_bytes);
  executions = (struct register_allocator_reload_execution *)calloc(1, storage.execution_bytes);
  if (instructions == NULL || executions == NULL) {
    free(executions);
    free(instructions);
    return FAILED;
  }

  reset_context(context);
  add_reload(context, 1, 80);
  add_reload(context, 3, 81);
  status = register_allocator_execute_reload_chain("coreReloadChainStorageLifecycle", 2, 7,
      0, 1, 6, TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, context, is_active,
      reads_temp, writes_temp, is_reload_eligible, context, select_reload_register,
      context, apply_reload, instructions, executions, TEST_RELOAD_CAPACITY,
      &chain_execution);
  if (status == SUCCEEDED && (chain_execution.chain.count != 2 ||
      chain_execution.processed_count != 2 || chain_execution.applied_count != 2 ||
      instructions[0] != 1 || instructions[1] != 3 ||
      executions[0].mutation.physical_register != 80 ||
      executions[1].mutation.physical_register != 81))
    status = FAILED;

  free(executions);
  free(instructions);
  return status;
}

static void add_reload(struct reload_apply_context *context, int instruction, int physical_register) {

  context->reads[instruction] = YES;
  context->eligible[instruction] = YES;
  context->physical_registers[instruction] = physical_register;
}

int main(void) {

  struct reload_apply_context context;
  struct register_allocator_reload_execution execution;
  struct register_allocator_reload_execution chain_executions[TEST_RELOAD_CAPACITY];
  struct register_allocator_reload_chain_execution chain_execution;
  int reload_instructions[TEST_RELOAD_CAPACITY];
  int discovered_count;

  if (expect_storage("coreReloadChainStorageOne", 1) == FAILED ||
      expect_storage("coreReloadChainStorageFour", TEST_RELOAD_CAPACITY) == FAILED)
    return 23;
  if ((size_t)INT_MAX > ((size_t)-1) / sizeof(int) ||
      (size_t)INT_MAX > ((size_t)-1) / sizeof(struct register_allocator_reload_execution) ||
      (size_t)INT_MAX > (size_t)ULONG_MAX / sizeof(int) ||
      (size_t)INT_MAX > (size_t)ULONG_MAX / sizeof(struct register_allocator_reload_execution)) {
    if (expect_invalid_storage("coreReloadChainStorageMaximum", INT_MAX) == FAILED)
      return 24;
  }
  else if (expect_storage("coreReloadChainStorageMaximum", INT_MAX) == FAILED)
    return 25;
  if (expect_invalid_storage(NULL, TEST_RELOAD_CAPACITY) == FAILED ||
      expect_invalid_storage("coreReloadChainStorageZero", 0) == FAILED ||
      expect_invalid_storage("coreReloadChainStorageNegative", -1) == FAILED ||
      register_allocator_plan_reload_chain_storage("coreReloadChainStorageNoOutput",
      TEST_RELOAD_CAPACITY, NULL) != FAILED)
    return 26;
  if (test_storage_lifecycle(&context) == FAILED)
    return 27;
  if (test_fallthrough_range_execution(&context) == FAILED)
    return 28;
  if (test_operand_chain_execution(&context) == FAILED)
    return 29;
  if (test_atomic_operand_chain(&context) == FAILED)
    return 30;
  if (test_reload_graph_discovery(&context) == FAILED)
    return 31;

  reset_context(&context);
  add_reload(&context, 1, 10);
  add_reload(&context, 3, 11);
  add_reload(&context, 5, 12);
  if (apply_chain("coreChainApplyThree", &context, &discovered_count) == FAILED)
    return 1;
  if (discovered_count != 3 || context.applied_count != 3 ||
      context.applied_instructions[0] != 1 || context.applied_registers[0] != 10 ||
      context.applied_instructions[1] != 3 || context.applied_registers[1] != 11 ||
      context.applied_instructions[2] != 5 || context.applied_registers[2] != 12)
    return 2;

  reset_context(&context);
  add_reload(&context, 1, 20);
  add_reload(&context, 3, TEST_NO_REGISTER);
  add_reload(&context, 5, 22);
  if (apply_chain("coreChainApplyRejectedMiddle", &context, &discovered_count) == FAILED)
    return 3;
  if (discovered_count != 3 || context.applied_count != 2 ||
      context.applied_instructions[0] != 1 || context.applied_registers[0] != 20 ||
      context.applied_instructions[1] != 5 || context.applied_registers[1] != 22)
    return 4;

  reset_context(&context);
  add_reload(&context, 1, 30);
  add_reload(&context, 3, 31);
  add_reload(&context, 5, 32);
  context.fail_instruction = 3;
  if (apply_chain("coreChainApplyCallbackFail", &context, &discovered_count) != FAILED)
    return 5;
  if (context.applied_count != 1 || context.applied_instructions[0] != 1)
    return 6;

  reset_context(&context);
  add_reload(&context, 1, 40);
  context.reads[3] = YES;
  add_reload(&context, 5, 42);
  if (apply_chain("coreChainApplyIneligible", &context, &discovered_count) == FAILED)
    return 7;
  if (discovered_count != 1 || context.applied_count != 1 || context.applied_instructions[0] != 1)
    return 8;

  reset_context(&context);
  add_reload(&context, 2, 50);
  context.writes[2] = YES;
  add_reload(&context, 4, 51);
  if (apply_chain("coreChainApplyReadWrite", &context, &discovered_count) == FAILED)
    return 9;
  if (discovered_count != 1 || context.applied_count != 1 || context.applied_instructions[0] != 2)
    return 10;

  reset_context(&context);
  if (apply_chain("coreChainApplyEmpty", &context, &discovered_count) == FAILED)
    return 11;
  if (discovered_count != 0 || context.applied_count != 0)
    return 12;

  reset_context(&context);
  if (register_allocator_execute_reload("coreReloadExecuteNoTarget", 2, 7, 0, 3,
      TEST_NO_REGISTER, TEST_NO_REGISTER, &context, NULL, &execution) == FAILED ||
      execution.action != RA_RELOAD_ACTION_NONE || execution.mutation.apply != NO ||
      context.applied_count != 0)
    return 13;

  reset_context(&context);
  context.fail_instruction = 3;
  if (register_allocator_execute_reload("coreReloadExecuteFailure", 2, 7, 0, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO || context.applied_count != 0)
    return 14;

  if (register_allocator_execute_reload("coreReloadExecuteNoCallback", 2, 7, 0, 3,
      TEST_NO_REGISTER, 10, &context, NULL, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO || context.applied_count != 0)
    return 15;

  memset(&execution, 7, sizeof(execution));
  if (register_allocator_execute_reload(NULL, 2, 7, 0, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO ||
      register_allocator_execute_reload("coreReloadExecuteBadBlock", -1, 7, 0, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO ||
      register_allocator_execute_reload("coreReloadExecuteBadTemp", 2, -1, 0, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO ||
      register_allocator_execute_reload("coreReloadExecuteBadSpill", 2, 7, -1, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO ||
      register_allocator_execute_reload("coreReloadExecuteInvalid", 2, 7, 3, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, &execution) != FAILED ||
      reload_execution_is_clear(&execution) == NO || context.applied_count != 0)
    return 16;
  if (register_allocator_execute_reload("coreReloadExecuteNoOutput", 2, 7, 0, 3,
      TEST_NO_REGISTER, 10, &context, apply_reload, NULL) != FAILED)
    return 17;

  reset_context(&context);
  add_reload(&context, 1, 60);
  memset(&chain_execution, 7, sizeof(chain_execution));
  memset(chain_executions, 7, sizeof(chain_executions));
  if (register_allocator_execute_reload_chain("coreChainNoSelector", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, NULL, &context, apply_reload,
      reload_instructions, chain_executions, TEST_RELOAD_CAPACITY,
      &chain_execution) != FAILED || chain_execution.chain.count != 0 ||
      chain_execution.processed_count != 0 || chain_execution.applied_count != 0 ||
      reload_execution_is_clear(&chain_executions[0]) == NO)
    return 18;

  reset_context(&context);
  add_reload(&context, 1, TEST_NO_REGISTER);
  if (register_allocator_execute_reload_chain("coreChainNoTargetsNoCallback", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context, NULL,
      reload_instructions, chain_executions, TEST_RELOAD_CAPACITY,
      &chain_execution) == FAILED || chain_execution.chain.count != 1 ||
      chain_execution.processed_count != 1 || chain_execution.applied_count != 0 ||
      chain_executions[0].action != RA_RELOAD_ACTION_NONE ||
      chain_executions[0].mutation.apply != NO || context.applied_count != 0)
    return 19;

  reset_context(&context);
  add_reload(&context, 1, 70);
  if (register_allocator_execute_reload_chain("coreChainSupportedNoCallback", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context, NULL,
      reload_instructions, chain_executions, TEST_RELOAD_CAPACITY,
      &chain_execution) != FAILED || chain_execution.chain.count != 0 ||
      chain_execution.processed_count != 0 || chain_execution.applied_count != 0 ||
      reload_instructions[0] != -1 || reload_execution_is_clear(&chain_executions[0]) == NO ||
      context.applied_count != 0)
    return 20;

  memset(&chain_execution, 7, sizeof(chain_execution));
  memset(chain_executions, 7, sizeof(chain_executions));
  memset(reload_instructions, 7, sizeof(reload_instructions));
  if (register_allocator_execute_reload_chain(NULL, 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context,
      apply_reload, reload_instructions, chain_executions, TEST_RELOAD_CAPACITY,
      &chain_execution) != FAILED || reload_chain_execution_is_clear(&chain_execution) == NO ||
      reload_execution_is_clear(&chain_executions[0]) == NO || reload_instructions[0] != -1 ||
      register_allocator_execute_reload_chain("coreChainBadRange", 2, 7, 3, 3, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context,
      apply_reload, reload_instructions, chain_executions, TEST_RELOAD_CAPACITY,
      &chain_execution) != FAILED || reload_chain_execution_is_clear(&chain_execution) == NO ||
      reload_execution_is_clear(&chain_executions[0]) == NO || reload_instructions[0] != -1)
    return 21;

  if (register_allocator_execute_reload_chain("coreChainNoInstructions", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context,
      apply_reload, NULL, chain_executions, TEST_RELOAD_CAPACITY,
      &chain_execution) != FAILED || reload_chain_execution_is_clear(&chain_execution) == NO ||
      reload_execution_is_clear(&chain_executions[0]) == NO ||
      register_allocator_execute_reload_chain("coreChainNoExecutions", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context,
      apply_reload, reload_instructions, NULL, TEST_RELOAD_CAPACITY,
      &chain_execution) != FAILED || reload_chain_execution_is_clear(&chain_execution) == NO ||
      reload_instructions[0] != -1 ||
      register_allocator_execute_reload_chain("coreChainNoOutput", 2, 7, 0, 1, 6,
      TEST_INSTRUCTION_COUNT, TEST_NO_REGISTER, &context, is_active, reads_temp,
      writes_temp, is_reload_eligible, &context, select_reload_register, &context,
      apply_reload, reload_instructions, chain_executions, TEST_RELOAD_CAPACITY,
      NULL) != FAILED || reload_execution_is_clear(&chain_executions[0]) == NO ||
      reload_instructions[0] != -1)
    return 22;

  return 0;
}
