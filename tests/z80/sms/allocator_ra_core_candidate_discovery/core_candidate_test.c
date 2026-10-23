#include <stddef.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_INSTRUCTION_COUNT 8

struct candidate_test_context {
  int produced[TEST_INSTRUCTION_COUNT];
  char active[TEST_INSTRUCTION_COUNT];
  char reads[TEST_INSTRUCTION_COUNT];
  char writes[TEST_INSTRUCTION_COUNT];
  int operands[TEST_INSTRUCTION_COUNT];
};

static int get_produced_temp(void *context, int instruction_index, int temp_index) {

  struct candidate_test_context *test_context;

  (void)temp_index;
  test_context = (struct candidate_test_context *)context;
  return test_context->produced[instruction_index];
}

static int is_active(void *context, int instruction_index, int temp_index) {

  struct candidate_test_context *test_context;

  (void)temp_index;
  test_context = (struct candidate_test_context *)context;
  return test_context->active[instruction_index];
}

static int reads_temp(void *context, int instruction_index, int temp_index) {

  struct candidate_test_context *test_context;

  (void)temp_index;
  test_context = (struct candidate_test_context *)context;
  return test_context->reads[instruction_index];
}

static int writes_temp(void *context, int instruction_index, int temp_index) {

  struct candidate_test_context *test_context;

  (void)temp_index;
  test_context = (struct candidate_test_context *)context;
  return test_context->writes[instruction_index];
}

static int get_consumer_operand(void *context, int instruction_index, int temp_index) {

  struct candidate_test_context *test_context;

  (void)temp_index;
  test_context = (struct candidate_test_context *)context;
  return test_context->operands[instruction_index];
}

static void reset_context(struct candidate_test_context *context) {

  int instruction_index;

  for (instruction_index = 0; instruction_index < TEST_INSTRUCTION_COUNT; instruction_index++) {
    context->produced[instruction_index] = -1;
    context->active[instruction_index] = YES;
    context->reads[instruction_index] = NO;
    context->writes[instruction_index] = NO;
    context->operands[instruction_index] = -1;
  }
}

static int candidate_is_empty(struct register_allocator_candidate *candidate) {

  return candidate->found == NO && candidate->temp_index == -1 && candidate->producer_instruction == -1 &&
      candidate->consumer_instruction == -1 && candidate->consumer_operand == -1 && candidate->has_single_read == NO;
}

int main(void) {

  struct candidate_test_context context;
  struct register_allocator_candidate candidate;

  reset_context(&context);
  context.produced[0] = 7;
  context.reads[2] = YES;
  context.operands[2] = 77;
  context.writes[4] = YES;
  context.reads[5] = YES;
  if (register_allocator_discover_candidate("coreCandidateSingle", 0, 3, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 1;
  if (candidate.found != YES || candidate.temp_index != 7 || candidate.producer_instruction != 0 ||
      candidate.consumer_instruction != 2 || candidate.consumer_operand != 77 || candidate.has_single_read != YES)
    return 2;

  context.writes[4] = NO;
  if (register_allocator_discover_candidate("coreCandidateMulti", 0, 3, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 3;
  if (candidate.found != YES || candidate.has_single_read != NO)
    return 4;

  context.writes[2] = YES;
  if (register_allocator_discover_candidate("coreCandidateReadWrite", 0, 3, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 5;
  if (candidate.found != YES || candidate.has_single_read != YES)
    return 6;

  reset_context(&context);
  candidate.found = YES;
  if (register_allocator_discover_candidate("coreCandidateNoDefinition", 0, 3, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 7;
  if (candidate_is_empty(&candidate) == NO)
    return 8;

  reset_context(&context);
  context.produced[0] = 3;
  context.writes[1] = YES;
  context.reads[2] = YES;
  context.operands[2] = 1;
  if (register_allocator_discover_candidate("coreCandidateRedefined", 0, 3, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 9;
  if (candidate_is_empty(&candidate) == NO)
    return 10;

  reset_context(&context);
  context.produced[0] = 4;
  context.reads[4] = YES;
  context.operands[4] = 2;
  if (register_allocator_discover_candidate("coreCandidateBlockEnd", 0, 3, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 11;
  if (candidate_is_empty(&candidate) == NO)
    return 12;

  context.active[4] = NO;
  context.reads[5] = YES;
  context.operands[5] = 9;
  if (register_allocator_discover_candidate("coreCandidateInactive", 0, 6, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 13;
  if (candidate.found != YES || candidate.consumer_instruction != 5 || candidate.consumer_operand != 9)
    return 14;

  context.operands[5] = -1;
  if (register_allocator_discover_candidate("coreCandidateUnsupported", 0, 6, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) == FAILED)
    return 15;
  if (candidate_is_empty(&candidate) == NO)
    return 16;

  candidate.found = YES;
  candidate.temp_index = 99;
  if (register_allocator_discover_candidate(NULL, 0, 6, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) != FAILED)
    return 17;
  if (candidate_is_empty(&candidate) == NO)
    return 18;
  if (register_allocator_discover_candidate("coreCandidateBadRange", 0, TEST_INSTRUCTION_COUNT, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) != FAILED)
    return 19;
  if (register_allocator_discover_candidate("coreCandidateNullCallback", 0, 6, TEST_INSTRUCTION_COUNT, &context,
      NULL, is_active, reads_temp, writes_temp, get_consumer_operand, &candidate) != FAILED)
    return 20;
  if (register_allocator_discover_candidate("coreCandidateNullOutput", 0, 6, TEST_INSTRUCTION_COUNT, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand, NULL) != FAILED)
    return 21;

  return 0;
}
