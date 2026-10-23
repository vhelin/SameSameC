#include <stddef.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_INSTRUCTION_COUNT 9
#define TEST_RELOAD_CAPACITY 4

struct reload_test_context {
  char active[TEST_INSTRUCTION_COUNT];
  char reads[TEST_INSTRUCTION_COUNT];
  char writes[TEST_INSTRUCTION_COUNT];
  char eligible[TEST_INSTRUCTION_COUNT];
};

static int is_active(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->active[instruction_index];
}

static int reads_temp(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->reads[instruction_index];
}

static int writes_temp(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->writes[instruction_index];
}

static int is_reload_eligible(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->eligible[instruction_index];
}

static void reset_context(struct reload_test_context *context) {

  int instruction_index;

  for (instruction_index = 0; instruction_index < TEST_INSTRUCTION_COUNT; instruction_index++) {
    context->active[instruction_index] = YES;
    context->reads[instruction_index] = NO;
    context->writes[instruction_index] = NO;
    context->eligible[instruction_index] = NO;
  }
}

static void dirty_outputs(int *reload_instructions, struct register_allocator_reload_chain *reload_chain) {

  int reload_index;

  for (reload_index = 0; reload_index < TEST_RELOAD_CAPACITY; reload_index++)
    reload_instructions[reload_index] = 99;
  reload_chain->count = 99;
  reload_chain->truncated = YES;
  reload_chain->stop_instruction = 99;
}

static int outputs_are_empty(int *reload_instructions, int capacity, struct register_allocator_reload_chain *reload_chain) {

  int reload_index;

  if (reload_chain->count != 0 || reload_chain->truncated != NO || reload_chain->stop_instruction != -1)
    return NO;
  for (reload_index = 0; reload_index < capacity; reload_index++) {
    if (reload_instructions[reload_index] != -1)
      return NO;
  }
  return YES;
}

int main(void) {

  struct reload_test_context context;
  struct register_allocator_basic_block blocks[4];
  struct register_allocator_cfg_edge edges[4];
  struct register_allocator_reload_chain reload_chain;
  struct register_allocator_reload_scan_range range;
  int reload_instructions[TEST_RELOAD_CAPACITY];

  blocks[0].start_tac = 0;
  blocks[0].end_tac = 1;
  blocks[1].start_tac = 2;
  blocks[1].end_tac = 4;
  blocks[2].start_tac = 5;
  blocks[2].end_tac = 6;
  blocks[3].start_tac = 7;
  blocks[3].end_tac = 8;
  edges[0].from_block = 0;
  edges[0].to_block = 1;
  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[1].from_block = 1;
  edges[1].to_block = 2;
  edges[1].kind = RA_CFG_EDGE_FALLTHROUGH;
  if (register_allocator_plan_reload_scan_range("coreRangeFallthrough", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 2, 0, &range) == FAILED)
    return 28;
  if (range.end_block != 2 || range.end_instruction != 6 ||
      range.extended_block_count != 2 ||
      range.stop_reason != RA_RELOAD_RANGE_FUNCTION_END)
    return 29;

  edges[1].kind = RA_CFG_EDGE_BRANCH_TRUE;
  if (register_allocator_plan_reload_scan_range("coreRangeBranch", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 2, 0, &range) == FAILED)
    return 30;
  if (range.end_block != 1 || range.end_instruction != 4 ||
      range.extended_block_count != 1 ||
      range.stop_reason != RA_RELOAD_RANGE_BRANCH)
    return 31;

  edges[2].from_block = 1;
  edges[2].to_block = 3;
  edges[2].kind = RA_CFG_EDGE_BRANCH_FALSE;
  if (register_allocator_plan_reload_scan_range("coreRangeConditional", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 3, 0, &range) == FAILED)
    return 40;
  if (range.end_block != 1 || range.end_instruction != 4 ||
      range.extended_block_count != 1 ||
      range.stop_reason != RA_RELOAD_RANGE_BRANCH)
    return 41;

  edges[1].kind = RA_CFG_EDGE_JUMP;
  if (register_allocator_plan_reload_scan_range("coreRangeJump", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 2, 0, &range) == FAILED)
    return 42;
  if (range.end_block != 1 || range.end_instruction != 4 ||
      range.extended_block_count != 1 ||
      range.stop_reason != RA_RELOAD_RANGE_BRANCH)
    return 43;

  edges[1].kind = RA_CFG_EDGE_FALLTHROUGH;
  edges[2].from_block = 3;
  edges[2].to_block = 1;
  edges[2].kind = RA_CFG_EDGE_JUMP;
  if (register_allocator_plan_reload_scan_range("coreRangeJoin", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 3, 0, &range) == FAILED)
    return 32;
  if (range.end_block != 0 || range.end_instruction != 1 ||
      range.extended_block_count != 0 ||
      range.stop_reason != RA_RELOAD_RANGE_JOIN)
    return 33;

  edges[0].from_block = 0;
  edges[0].to_block = 0;
  if (register_allocator_plan_reload_scan_range("coreRangeCycle", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 1, 0, &range) == FAILED)
    return 34;
  if (range.end_block != 0 || range.extended_block_count != 0 ||
      range.stop_reason != RA_RELOAD_RANGE_CYCLE)
    return 35;

  edges[0].to_block = 1;
  blocks[1].start_tac = 3;
  if (register_allocator_plan_reload_scan_range("coreRangeNoncontiguous", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 1, 0, &range) == FAILED)
    return 36;
  if (range.end_block != 0 || range.extended_block_count != 0 ||
      range.stop_reason != RA_RELOAD_RANGE_NONCONTIGUOUS)
    return 37;
  blocks[1].start_tac = 2;

  edges[0].kind = 99;
  if (register_allocator_plan_reload_scan_range("coreRangeBadEdge", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 1, 0, &range) != FAILED)
    return 38;
  if (range.end_block != -1 || range.end_instruction != -1 ||
      range.extended_block_count != 0)
    return 39;

  edges[0].kind = RA_CFG_EDGE_FALLTHROUGH;
  blocks[1].start_tac = 1;
  if (register_allocator_plan_reload_scan_range("coreRangeBadBlock", 4,
      TEST_INSTRUCTION_COUNT, blocks, edges, 1, 0, &range) != FAILED)
    return 44;
  if (range.end_block != -1 || range.end_instruction != -1 ||
      range.extended_block_count != 0)
    return 45;
  blocks[1].start_tac = 2;

  reset_context(&context);
  context.reads[1] = YES;
  context.reads[3] = YES;
  context.reads[6] = YES;
  context.eligible[1] = YES;
  context.eligible[3] = YES;
  context.eligible[6] = YES;
  if (register_allocator_discover_reload_chain("coreChainThree", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == FAILED)
    return 1;
  if (reload_chain.count != 3 || reload_chain.truncated != NO || reload_chain.stop_instruction != -1 ||
      reload_instructions[0] != 1 || reload_instructions[1] != 3 || reload_instructions[2] != 6 ||
      reload_instructions[3] != -1)
    return 2;

  dirty_outputs(reload_instructions, &reload_chain);
  if (register_allocator_discover_reload_chain("coreChainCapacity", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, 2, &reload_chain) == FAILED)
    return 3;
  if (reload_chain.count != 2 || reload_chain.truncated != YES || reload_chain.stop_instruction != 6 ||
      reload_instructions[0] != 1 || reload_instructions[1] != 3)
    return 4;

  reset_context(&context);
  context.active[1] = NO;
  context.reads[1] = YES;
  context.reads[4] = YES;
  context.eligible[1] = YES;
  context.eligible[4] = YES;
  if (register_allocator_discover_reload_chain("coreChainInactive", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == FAILED)
    return 5;
  if (reload_chain.count != 1 || reload_instructions[0] != 4)
    return 6;

  reset_context(&context);
  context.reads[1] = YES;
  context.eligible[1] = YES;
  context.writes[3] = YES;
  context.reads[5] = YES;
  context.eligible[5] = YES;
  if (register_allocator_discover_reload_chain("coreChainRedefined", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == FAILED)
    return 7;
  if (reload_chain.count != 1 || reload_chain.stop_instruction != 3 || reload_instructions[0] != 1)
    return 8;

  reset_context(&context);
  context.reads[1] = YES;
  context.reads[3] = YES;
  context.reads[5] = YES;
  context.eligible[1] = YES;
  context.eligible[5] = YES;
  if (register_allocator_discover_reload_chain("coreChainIneligible", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == FAILED)
    return 9;
  if (reload_chain.count != 1 || reload_chain.truncated != NO || reload_chain.stop_instruction != 3 ||
      reload_instructions[0] != 1 || reload_instructions[1] != -1)
    return 10;

  reset_context(&context);
  context.reads[2] = YES;
  context.writes[2] = YES;
  context.eligible[2] = YES;
  context.reads[4] = YES;
  context.eligible[4] = YES;
  if (register_allocator_discover_reload_chain("coreChainReadWrite", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == FAILED)
    return 11;
  if (reload_chain.count != 1 || reload_chain.stop_instruction != 2 || reload_instructions[0] != 2)
    return 12;

  reset_context(&context);
  dirty_outputs(reload_instructions, &reload_chain);
  if (register_allocator_discover_reload_chain("coreChainEmpty", 2, 7, 9, 8,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == FAILED)
    return 13;
  if (outputs_are_empty(reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == NO)
    return 14;

  dirty_outputs(reload_instructions, &reload_chain);
  if (register_allocator_discover_reload_chain(NULL, 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 15;
  if (outputs_are_empty(reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) == NO)
    return 16;
  if (register_allocator_discover_reload_chain("coreChainBadBlock", -1, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 17;
  if (register_allocator_discover_reload_chain("coreChainBadTemp", 2, -1, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 18;
  if (register_allocator_discover_reload_chain("coreChainBadStart", 2, 7, 10, 8,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 19;
  if (register_allocator_discover_reload_chain("coreChainBadEnd", 2, 7, 0, 9,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 20;
  if (register_allocator_discover_reload_chain("coreChainZeroCapacity", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, 0, &reload_chain) != FAILED)
    return 21;
  if (register_allocator_discover_reload_chain("coreChainNullOutput", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, NULL, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 22;
  if (register_allocator_discover_reload_chain("coreChainNullResult", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, NULL) != FAILED)
    return 23;
  if (register_allocator_discover_reload_chain("coreChainNullActive", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, NULL, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 24;
  if (register_allocator_discover_reload_chain("coreChainNullReads", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, NULL, writes_temp,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 25;
  if (register_allocator_discover_reload_chain("coreChainNullWrites", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, NULL,
      is_reload_eligible, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 26;
  if (register_allocator_discover_reload_chain("coreChainNullEligible", 2, 7, 0, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      NULL, reload_instructions, TEST_RELOAD_CAPACITY, &reload_chain) != FAILED)
    return 27;

  return 0;
}
