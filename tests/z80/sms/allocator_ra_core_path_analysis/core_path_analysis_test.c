#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)
#define NO_SPILL_REASON 0
#define UNCONDITIONAL_SPILL_REASON 9

struct callback_state {
  int mode;
  int transparent_calls;
  int nearer_calls;
  int overlap_calls;
  int special_calls;
  int target_calls;
};

static void reset_state(struct callback_state *state, int mode) {

  state->mode = mode;
  state->transparent_calls = 0;
  state->nearer_calls = 0;
  state->overlap_calls = 0;
  state->special_calls = 0;
  state->target_calls = 0;
}

static int transparent_callback(void *context, int instruction_index, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->transparent_calls++;
  if (state->mode == 2 && instruction_index == 2)
    return 2;
  if (state->mode == 1 && instruction_index == 2)
    return NO;
  return physical_register == 41 ? YES : NO;
}

static int nearer_callback(void *context, int instruction_index, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->nearer_calls++;
  (void)physical_register;
  if (state->mode == 5)
    return 2;
  return state->mode == 1 && instruction_index == 2 ? YES : NO;
}

static int overlap_callback(void *context, int instruction_index, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->overlap_calls++;
  (void)physical_register;
  if (state->mode == 6)
    return 2;
  return state->mode == 2 && instruction_index == 1 ? YES : NO;
}

static int special_callback(void *context, int instruction_index, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->special_calls++;
  (void)physical_register;
  if (state->mode == 7)
    return 2;
  return state->mode == 3 && instruction_index == 1 ? YES : NO;
}

static int target_callback(void *context, int instruction_index, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->target_calls++;
  if (state->mode == 8)
    return 2;
  if (state->mode == 4 && instruction_index == 2)
    return NO;
  return physical_register == 41 ? YES : NO;
}

static int expect_interval_failure(char *function_name, int producer_instruction,
    int consumer_instruction, int no_spill_reason, int unconditional_spill_reason,
    int spill_boundary_instruction, int *blocked) {

  if (blocked != NULL)
    *blocked = 7;
  if (register_allocator_interval_is_blocked(function_name, producer_instruction,
      consumer_instruction, 3, no_spill_reason, unconditional_spill_reason,
      spill_boundary_instruction, blocked) != FAILED)
    return FAILED;
  if (blocked != NULL && *blocked != NO)
    return FAILED;
  return SUCCEEDED;
}

static int expect_range_failure(char *function_name, int start_instruction,
    int end_instruction, int instruction_count, int physical_register, void *context,
    register_allocator_path_predicate callback, int *transparent, int *blocking_instruction) {

  if (transparent != NULL)
    *transparent = 7;
  if (blocking_instruction != NULL)
    *blocking_instruction = 7;
  if (register_allocator_is_transparent_range(function_name, start_instruction,
      end_instruction, instruction_count, physical_register, NO_REGISTER, context, callback,
      transparent, blocking_instruction) != FAILED)
    return FAILED;
  if (transparent != NULL && *transparent != NO)
    return FAILED;
  if (blocking_instruction != NULL && *blocking_instruction != -1)
    return FAILED;
  return SUCCEEDED;
}

static int expect_path_failure(char *function_name, int block_index, int start_instruction,
    int end_instruction, int instruction_count, int physical_register, void *context,
    register_allocator_path_predicate nearer, register_allocator_path_predicate overlap,
    register_allocator_path_predicate special, register_allocator_path_predicate target,
    int *path_safe, int *deciding_instruction) {

  if (path_safe != NULL)
    *path_safe = 7;
  if (deciding_instruction != NULL)
    *deciding_instruction = 7;
  if (register_allocator_evaluate_candidate_path(function_name, block_index,
      start_instruction, end_instruction, instruction_count, physical_register, NO_REGISTER,
      context, nearer, overlap, special, target, path_safe, deciding_instruction) != FAILED)
    return FAILED;
  if (path_safe != NULL && *path_safe != NO)
    return FAILED;
  if (deciding_instruction != NULL && *deciding_instruction != -1)
    return FAILED;
  return SUCCEEDED;
}

int main(void) {

  struct callback_state state;
  int blocked;
  int transparent;
  int blocking_instruction;
  int path_safe;
  int deciding_instruction;

  if (register_allocator_interval_is_blocked("coreIntervalNone", 2, 8, NO_SPILL_REASON,
      NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, -1, &blocked) == FAILED || blocked != NO)
    return 1;
  if (register_allocator_interval_is_blocked("coreIntervalUnconditional", 2, 8,
      UNCONDITIONAL_SPILL_REASON, NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, -1,
      &blocked) == FAILED || blocked != YES)
    return 2;
  if (register_allocator_interval_is_blocked("coreIntervalMissingBoundary", 2, 8, 3,
      NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, -1, &blocked) == FAILED || blocked != YES)
    return 3;
  if (register_allocator_interval_is_blocked("coreIntervalBetween", 2, 8, 3,
      NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, 5, &blocked) == FAILED || blocked != YES)
    return 4;
  if (register_allocator_interval_is_blocked("coreIntervalBefore", 2, 8, 3,
      NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, 1, &blocked) == FAILED || blocked != NO)
    return 5;
  if (register_allocator_interval_is_blocked("coreIntervalAtProducer", 2, 8, 3,
      NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, 2, &blocked) == FAILED || blocked != NO)
    return 6;
  if (register_allocator_interval_is_blocked("coreIntervalAtConsumer", 2, 8, 3,
      NO_SPILL_REASON, UNCONDITIONAL_SPILL_REASON, 8, &blocked) == FAILED || blocked != NO)
    return 7;
  if (expect_interval_failure(NULL, 2, 8, NO_SPILL_REASON,
      UNCONDITIONAL_SPILL_REASON, 5, &blocked) == FAILED)
    return 8;
  if (expect_interval_failure("coreIntervalBadProducer", -1, 8, NO_SPILL_REASON,
      UNCONDITIONAL_SPILL_REASON, 5, &blocked) == FAILED)
    return 9;
  if (expect_interval_failure("coreIntervalEqual", 2, 2, NO_SPILL_REASON,
      UNCONDITIONAL_SPILL_REASON, 2, &blocked) == FAILED)
    return 10;
  if (expect_interval_failure("coreIntervalReverse", 8, 2, NO_SPILL_REASON,
      UNCONDITIONAL_SPILL_REASON, 5, &blocked) == FAILED)
    return 11;
  if (expect_interval_failure("coreIntervalSameReasons", 2, 8, 3, 3, 5,
      &blocked) == FAILED)
    return 12;
  if (expect_interval_failure("coreIntervalBadBoundary", 2, 8, NO_SPILL_REASON,
      UNCONDITIONAL_SPILL_REASON, -2, &blocked) == FAILED)
    return 13;
  if (expect_interval_failure("coreIntervalNullOutput", 2, 8, NO_SPILL_REASON,
      UNCONDITIONAL_SPILL_REASON, 5, NULL) == FAILED)
    return 14;

  reset_state(&state, 0);
  if (register_allocator_is_transparent_range("coreRangeTransparent", 0, 4, 5, 41,
      NO_REGISTER, &state, transparent_callback, &transparent,
      &blocking_instruction) == FAILED || transparent != YES || blocking_instruction != -1 ||
      state.transparent_calls != 3)
    return 15;
  reset_state(&state, 1);
  if (register_allocator_is_transparent_range("coreRangeBlocked", 0, 4, 5, 41,
      NO_REGISTER, &state, transparent_callback, &transparent,
      &blocking_instruction) == FAILED || transparent != NO || blocking_instruction != 2 ||
      state.transparent_calls != 2)
    return 16;
  reset_state(&state, 0);
  if (register_allocator_is_transparent_range("coreRangeAdjacent", 2, 3, 5, 41,
      NO_REGISTER, &state, transparent_callback, &transparent,
      &blocking_instruction) == FAILED || transparent != YES || state.transparent_calls != 0)
    return 17;
  reset_state(&state, 2);
  if (expect_range_failure("coreRangeBadCallback", 0, 4, 5, 41, &state,
      transparent_callback, &transparent, &blocking_instruction) == FAILED ||
      state.transparent_calls != 2)
    return 18;
  reset_state(&state, 0);
  if (expect_range_failure(NULL, 0, 4, 5, 41, &state, transparent_callback,
      &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeBadStart", -1, 4, 5, 41, &state,
      transparent_callback, &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeEqual", 2, 2, 5, 41, &state,
      transparent_callback, &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeBadEnd", 0, 5, 5, 41, &state,
      transparent_callback, &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeSentinel", 0, 4, 5, NO_REGISTER, &state,
      transparent_callback, &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeNullContext", 0, 4, 5, 41, NULL,
      transparent_callback, &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeNullCallback", 0, 4, 5, 41, &state, NULL,
      &transparent, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeNullTransparent", 0, 4, 5, 41, &state,
      transparent_callback, NULL, &blocking_instruction) == FAILED ||
      expect_range_failure("coreRangeNullBlocking", 0, 4, 5, 41, &state,
      transparent_callback, &transparent, NULL) == FAILED)
    return 19;

  reset_state(&state, 0);
  if (register_allocator_evaluate_candidate_path("corePathTransparent", 3, 0, 4, 5, 41,
      NO_REGISTER, &state, nearer_callback, overlap_callback, special_callback,
      target_callback, &path_safe, &deciding_instruction) == FAILED || path_safe != YES ||
      deciding_instruction != -1 || state.nearer_calls != 3 || state.overlap_calls != 3 ||
      state.special_calls != 3 || state.target_calls != 3)
    return 20;
  reset_state(&state, 1);
  if (register_allocator_evaluate_candidate_path("corePathNearer", 3, 0, 4, 5, 41,
      NO_REGISTER, &state, nearer_callback, overlap_callback, special_callback,
      target_callback, &path_safe, &deciding_instruction) == FAILED || path_safe != YES ||
      deciding_instruction != 2 || state.nearer_calls != 2 || state.overlap_calls != 1 ||
      state.special_calls != 1 || state.target_calls != 1)
    return 21;
  reset_state(&state, 2);
  if (register_allocator_evaluate_candidate_path("corePathOverlap", 3, 0, 4, 5, 41,
      NO_REGISTER, &state, nearer_callback, overlap_callback, special_callback,
      target_callback, &path_safe, &deciding_instruction) == FAILED || path_safe != YES ||
      deciding_instruction != -1 || state.special_calls != 2 || state.target_calls != 2)
    return 22;
  reset_state(&state, 3);
  if (register_allocator_evaluate_candidate_path("corePathSpecial", 3, 0, 4, 5, 41,
      NO_REGISTER, &state, nearer_callback, overlap_callback, special_callback,
      target_callback, &path_safe, &deciding_instruction) == FAILED || path_safe != YES ||
      deciding_instruction != -1 || state.target_calls != 2)
    return 23;
  reset_state(&state, 4);
  if (register_allocator_evaluate_candidate_path("corePathBlocked", 3, 0, 4, 5, 41,
      NO_REGISTER, &state, nearer_callback, overlap_callback, special_callback,
      target_callback, &path_safe, &deciding_instruction) == FAILED || path_safe != NO ||
      deciding_instruction != 2 || state.target_calls != 2)
    return 24;
  reset_state(&state, 5);
  if (expect_path_failure("corePathBadNearer", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback,
      &path_safe, &deciding_instruction) == FAILED || state.nearer_calls != 1)
    return 25;
  reset_state(&state, 6);
  if (expect_path_failure("corePathBadOverlap", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback,
      &path_safe, &deciding_instruction) == FAILED || state.overlap_calls != 1)
    return 26;
  reset_state(&state, 7);
  if (expect_path_failure("corePathBadSpecial", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback,
      &path_safe, &deciding_instruction) == FAILED || state.special_calls != 1)
    return 27;
  reset_state(&state, 8);
  if (expect_path_failure("corePathBadTarget", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback,
      &path_safe, &deciding_instruction) == FAILED || state.target_calls != 1)
    return 28;
  reset_state(&state, 0);
  if (expect_path_failure(NULL, 3, 0, 4, 5, 41, &state, nearer_callback,
      overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathBadBlock", -1, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathBadStart", 3, -1, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathEqual", 3, 2, 2, 5, 41, &state, nearer_callback,
      overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathBadEnd", 3, 0, 5, 5, 41, &state, nearer_callback,
      overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathSentinel", 3, 0, 4, 5, NO_REGISTER, &state,
      nearer_callback, overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullContext", 3, 0, 4, 5, 41, NULL,
      nearer_callback, overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullNearer", 3, 0, 4, 5, 41, &state, NULL,
      overlap_callback, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullOverlap", 3, 0, 4, 5, 41, &state,
      nearer_callback, NULL, special_callback, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullSpecial", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, NULL, target_callback, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullTarget", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, NULL, &path_safe,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullSafe", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback, NULL,
      &deciding_instruction) == FAILED ||
      expect_path_failure("corePathNullDeciding", 3, 0, 4, 5, 41, &state,
      nearer_callback, overlap_callback, special_callback, target_callback, &path_safe,
      NULL) == FAILED)
    return 29;

  return 0;
}
