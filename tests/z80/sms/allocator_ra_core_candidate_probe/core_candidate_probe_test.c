#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define INSTRUCTION_COUNT 8
#define NO_REGISTER (-1)

struct probe_context {
  int produced[INSTRUCTION_COUNT];
  int operands[INSTRUCTION_COUNT];
  char active[INSTRUCTION_COUNT];
  char reads[INSTRUCTION_COUNT];
  char writes[INSTRUCTION_COUNT];
  int reject_stage;
  int malformed_stage;
  int calls[4];
};

static void reset_context(struct probe_context *context) {

  int instruction;

  memset(context, 0, sizeof(*context));
  for (instruction = 0; instruction < INSTRUCTION_COUNT; instruction++) {
    context->produced[instruction] = -1;
    context->operands[instruction] = -1;
    context->active[instruction] = YES;
  }
  context->produced[0] = 7;
  context->reads[2] = YES;
  context->operands[2] = 1;
}

static int get_produced_temp(void *context, int instruction, int temp_index) {

  (void)temp_index;
  return ((struct probe_context *)context)->produced[instruction];
}

static int is_active(void *context, int instruction, int temp_index) {

  (void)temp_index;
  return ((struct probe_context *)context)->active[instruction];
}

static int reads_temp(void *context, int instruction, int temp_index) {

  (void)temp_index;
  return ((struct probe_context *)context)->reads[instruction];
}

static int writes_temp(void *context, int instruction, int temp_index) {

  (void)temp_index;
  return ((struct probe_context *)context)->writes[instruction];
}

static int get_consumer_operand(void *context, int instruction, int temp_index) {

  (void)temp_index;
  return ((struct probe_context *)context)->operands[instruction];
}

static int qualification_predicate(void *context,
    struct register_allocator_candidate *candidate, int physical_register, int stage) {

  struct probe_context *probe_context;

  probe_context = (struct probe_context *)context;
  probe_context->calls[stage - 1]++;
  if (candidate->temp_index != 7 || physical_register != 41)
    return NO;
  if (probe_context->malformed_stage == stage)
    return 2;
  return probe_context->reject_stage == stage ? NO : YES;
}

static int has_metadata(void *context, struct register_allocator_candidate *candidate,
    int physical_register) {

  return qualification_predicate(context, candidate, physical_register, 1);
}

static int interval_is_clear(void *context, struct register_allocator_candidate *candidate,
    int physical_register) {

  return qualification_predicate(context, candidate, physical_register, 2);
}

static int is_allowed(void *context, struct register_allocator_candidate *candidate,
    int physical_register) {

  return qualification_predicate(context, candidate, physical_register, 3);
}

static int is_path_transparent(void *context, struct register_allocator_candidate *candidate,
    int physical_register) {

  return qualification_predicate(context, candidate, physical_register, 4);
}

static void seed_candidate(struct register_allocator_candidate *candidate, int consumer) {

  candidate->found = YES;
  candidate->temp_index = 7;
  candidate->producer_instruction = 0;
  candidate->consumer_instruction = consumer;
  candidate->consumer_operand = 1;
  candidate->has_single_read = YES;
}

static int qualification_is_clear(struct register_allocator_candidate_qualification *qualification) {

  return qualification->eligible == NO &&
      qualification->rejection_reason == RA_CANDIDATE_REJECTION_NONE &&
      qualification->deciding_instruction == -1;
}

static int probe_is_clear(struct register_allocator_candidate_probe *probe) {

  return probe->candidate.found == NO && probe->candidate.temp_index == -1 &&
      probe->candidate.producer_instruction == -1 &&
      probe->candidate.consumer_instruction == -1 &&
      probe->candidate.consumer_operand == -1 &&
      probe->candidate.has_single_read == NO &&
      qualification_is_clear(&probe->qualification) == YES;
}

static int expect_qualification(struct probe_context *context, int reject_stage,
    int expected_reason, int expected_calls) {

  struct register_allocator_candidate candidate;
  struct register_allocator_candidate_qualification qualification;
  int stage;
  int call_count;

  seed_candidate(&candidate, 2);
  context->reject_stage = reject_stage;
  if (register_allocator_qualify_candidate("coreQualification", 41, NO_REGISTER,
      context, &candidate, has_metadata, interval_is_clear, is_allowed,
      is_path_transparent, &qualification) == FAILED)
    return FAILED;
  if (qualification.eligible != (reject_stage == 0 ? YES : NO) ||
      qualification.rejection_reason != expected_reason)
    return FAILED;
  call_count = 0;
  for (stage = 0; stage < 4; stage++)
    call_count += context->calls[stage];
  if (call_count != expected_calls)
    return FAILED;
  return SUCCEEDED;
}

static int test_ranges(void) {

  struct register_allocator_candidate candidate;
  int eligible;

  seed_candidate(&candidate, 2);
  if (register_allocator_candidate_meets_range("coreRangeBefore", &candidate, 3,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &eligible) == FAILED || eligible != YES)
    return 1;
  if (register_allocator_candidate_meets_range("coreRangeAt", &candidate, 2,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &eligible) == FAILED || eligible != NO)
    return 2;
  seed_candidate(&candidate, 4);
  if (register_allocator_candidate_meets_range("coreRangeAfter", &candidate, 3,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &eligible) == FAILED || eligible != NO)
    return 3;
  if (register_allocator_candidate_meets_range("coreRangeWithin", &candidate, 3,
      RA_CANDIDATE_RANGE_WITHIN_BLOCK, &eligible) == FAILED || eligible != YES)
    return 4;

  eligible = 7;
  candidate.has_single_read = 2;
  if (register_allocator_candidate_meets_range("badCandidate", &candidate, 3,
      RA_CANDIDATE_RANGE_WITHIN_BLOCK, &eligible) != FAILED || eligible != NO)
    return 5;
  seed_candidate(&candidate, 2);
  if (register_allocator_candidate_meets_range(NULL, &candidate, 3,
      RA_CANDIDATE_RANGE_WITHIN_BLOCK, &eligible) != FAILED ||
      register_allocator_candidate_meets_range("badDecision", &candidate, 0,
      RA_CANDIDATE_RANGE_WITHIN_BLOCK, &eligible) != FAILED ||
      register_allocator_candidate_meets_range("badMode", &candidate, 3, 99,
      &eligible) != FAILED || register_allocator_candidate_meets_range("badOutput",
      &candidate, 3, RA_CANDIDATE_RANGE_WITHIN_BLOCK, NULL) != FAILED)
    return 6;
  return 0;
}

static int test_qualifications(void) {

  struct probe_context context;
  struct register_allocator_candidate candidate;
  struct register_allocator_candidate_qualification qualification;
  int stage;

  reset_context(&context);
  if (expect_qualification(&context, 0, RA_CANDIDATE_REJECTION_NONE, 4) == FAILED)
    return 1;
  for (stage = 1; stage <= 4; stage++) {
    reset_context(&context);
    if (expect_qualification(&context, stage, RA_CANDIDATE_REJECTION_METADATA + stage - 1,
        stage) == FAILED)
      return 1 + stage;
  }

  seed_candidate(&candidate, 2);
  for (stage = 1; stage <= 4; stage++) {
    reset_context(&context);
    context.malformed_stage = stage;
    memset(&qualification, 7, sizeof(qualification));
    if (register_allocator_qualify_candidate("badCallback", 41, NO_REGISTER,
        &context, &candidate, has_metadata, interval_is_clear, is_allowed,
        is_path_transparent, &qualification) != FAILED ||
        qualification_is_clear(&qualification) == NO)
      return 6 + stage;
  }

  memset(&qualification, 7, sizeof(qualification));
  if (register_allocator_qualify_candidate(NULL, 41, NO_REGISTER, &context,
      &candidate, has_metadata, interval_is_clear, is_allowed, is_path_transparent,
      &qualification) != FAILED || qualification_is_clear(&qualification) == NO ||
      register_allocator_qualify_candidate("badPhysical", NO_REGISTER, NO_REGISTER,
      &context, &candidate, has_metadata, interval_is_clear, is_allowed,
      is_path_transparent, &qualification) != FAILED ||
      register_allocator_qualify_candidate("badContext", 41, NO_REGISTER, NULL,
      &candidate, has_metadata, interval_is_clear, is_allowed, is_path_transparent,
      &qualification) != FAILED || register_allocator_qualify_candidate("badCallback",
      41, NO_REGISTER, &context, &candidate, NULL, interval_is_clear, is_allowed,
      is_path_transparent, &qualification) != FAILED ||
      register_allocator_qualify_candidate("badOutput", 41, NO_REGISTER, &context,
      &candidate, has_metadata, interval_is_clear, is_allowed, is_path_transparent,
      NULL) != FAILED)
    return 11;
  return 0;
}

static int run_probe(char *name, struct probe_context *context, int decision,
    int range_mode, struct register_allocator_candidate_probe *probe) {

  return register_allocator_probe_competing_candidate(name, 0, 6, INSTRUCTION_COUNT,
      decision, range_mode, 41, NO_REGISTER, context, get_produced_temp, is_active,
      reads_temp, writes_temp, get_consumer_operand, has_metadata, interval_is_clear,
      is_allowed, is_path_transparent, probe);
}

static int test_probes(void) {

  struct probe_context context;
  struct register_allocator_candidate_probe probe;
  int stage;

  reset_context(&context);
  if (run_probe("coreProbeEligible", &context, 3,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &probe) == FAILED ||
      probe.candidate.found != YES || probe.qualification.eligible != YES)
    return 1;

  reset_context(&context);
  if (run_probe("coreProbeRange", &context, 2,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &probe) == FAILED ||
      probe.qualification.rejection_reason != RA_CANDIDATE_REJECTION_RANGE ||
      context.calls[0] != 0)
    return 2;

  reset_context(&context);
  if (run_probe("coreProbeWithin", &context, 2,
      RA_CANDIDATE_RANGE_WITHIN_BLOCK, &probe) == FAILED ||
      probe.qualification.eligible != YES)
    return 3;

  reset_context(&context);
  context.produced[0] = -1;
  if (run_probe("coreProbeNotFound", &context, 3,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &probe) == FAILED ||
      probe.candidate.found != NO ||
      probe.qualification.rejection_reason != RA_CANDIDATE_REJECTION_NOT_FOUND)
    return 4;

  for (stage = 1; stage <= 4; stage++) {
    reset_context(&context);
    context.reject_stage = stage;
    if (run_probe("coreProbeRejected", &context, 3,
        RA_CANDIDATE_RANGE_BEFORE_DECISION, &probe) == FAILED ||
        probe.qualification.rejection_reason != RA_CANDIDATE_REJECTION_METADATA + stage - 1)
      return 4 + stage;
  }

  reset_context(&context);
  context.malformed_stage = 3;
  memset(&probe, 7, sizeof(probe));
  if (run_probe("coreProbeBadCallback", &context, 3,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, &probe) != FAILED || probe_is_clear(&probe) == NO)
    return 9;

  reset_context(&context);
  memset(&probe, 7, sizeof(probe));
  if (register_allocator_probe_competing_candidate(NULL, 0, 6, INSTRUCTION_COUNT,
      3, RA_CANDIDATE_RANGE_BEFORE_DECISION, 41, NO_REGISTER, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand,
      has_metadata, interval_is_clear, is_allowed, is_path_transparent, &probe) != FAILED ||
      probe_is_clear(&probe) == NO ||
      register_allocator_probe_competing_candidate("badRange", 0, 8, INSTRUCTION_COUNT,
      3, RA_CANDIDATE_RANGE_BEFORE_DECISION, 41, NO_REGISTER, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand,
      has_metadata, interval_is_clear, is_allowed, is_path_transparent, &probe) != FAILED ||
      register_allocator_probe_competing_candidate("badMode", 0, 6, INSTRUCTION_COUNT,
      3, 99, 41, NO_REGISTER, &context, get_produced_temp, is_active, reads_temp,
      writes_temp, get_consumer_operand, has_metadata, interval_is_clear, is_allowed,
      is_path_transparent, &probe) != FAILED ||
      register_allocator_probe_competing_candidate("badDecision", 0, 6, INSTRUCTION_COUNT,
      INSTRUCTION_COUNT, RA_CANDIDATE_RANGE_BEFORE_DECISION, 41, NO_REGISTER, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand,
      has_metadata, interval_is_clear, is_allowed, is_path_transparent, &probe) != FAILED ||
      register_allocator_probe_competing_candidate("badOutput", 0, 6, INSTRUCTION_COUNT,
      3, RA_CANDIDATE_RANGE_BEFORE_DECISION, 41, NO_REGISTER, &context,
      get_produced_temp, is_active, reads_temp, writes_temp, get_consumer_operand,
      has_metadata, interval_is_clear, is_allowed, is_path_transparent, NULL) != FAILED)
    return 10;
  return 0;
}

int main(void) {

  int result;

  result = test_ranges();
  if (result != 0)
    return result;
  result = test_qualifications();
  if (result != 0)
    return 20 + result;
  result = test_probes();
  if (result != 0)
    return 40 + result;
  return 0;
}
