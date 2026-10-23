#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)

struct callback_state {
  int mode;
  int allowed_calls;
  int path_calls;
  int conflict_calls;
};

static int candidate_allowed(void *context, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->allowed_calls++;
  if (state->mode == 1 && physical_register == -7)
    return 2;
  if (physical_register == 41)
    return NO;

  return YES;
}

static int candidate_path_safe(void *context, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->path_calls++;
  if (state->mode == 2 && physical_register == -7)
    return 2;
  if (physical_register == -7)
    return NO;

  return YES;
}

static int candidate_conflict_free(void *context, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->conflict_calls++;
  if (state->mode == 3 && physical_register == 88)
    return 2;

  return YES;
}

static void reset_state(struct callback_state *state, int mode) {

  state->mode = mode;
  state->allowed_calls = 0;
  state->path_calls = 0;
  state->conflict_calls = 0;
}

static void seed_evaluations(struct register_allocator_candidate_evaluation *evaluations, int capacity) {

  int candidate_index;

  for (candidate_index = 0; candidate_index < capacity; candidate_index++) {
    evaluations[candidate_index].physical_register = 700 + candidate_index;
    evaluations[candidate_index].allowed = YES;
    evaluations[candidate_index].path_safe = YES;
    evaluations[candidate_index].conflict_free = YES;
  }
}

static int evaluations_are_cleared(struct register_allocator_candidate_evaluation *evaluations, int capacity) {

  int candidate_index;

  for (candidate_index = 0; candidate_index < capacity; candidate_index++) {
    if (evaluations[candidate_index].physical_register != NO_REGISTER ||
        evaluations[candidate_index].allowed != NO || evaluations[candidate_index].path_safe != NO ||
        evaluations[candidate_index].conflict_free != NO)
      return NO;
  }

  return YES;
}

static int expect_failure(char *function_name, int block_index, char *reason, int *physical_registers,
    int physical_register_count, struct callback_state *state,
    register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_conflict_free,
    struct register_allocator_candidate_evaluation *evaluations, int evaluation_capacity) {

  seed_evaluations(evaluations, evaluation_capacity);
  if (register_allocator_evaluate_candidate_registers(function_name, block_index, reason,
      physical_registers, physical_register_count, NO_REGISTER, state, is_allowed, is_path_safe,
      is_conflict_free, evaluations, evaluation_capacity) != FAILED)
    return FAILED;
  return evaluations_are_cleared(evaluations, evaluation_capacity) == YES ? SUCCEEDED : FAILED;
}

static int selection_is_cleared(struct register_allocator_candidate_selection *selection) {

  return selection->physical_register == NO_REGISTER && selection->candidate_index == -1;
}

static int expect_selection_failure(char *function_name, int block_index, char *reason,
    int *physical_registers, int physical_register_count, struct callback_state *state,
    register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_conflict_free,
    struct register_allocator_candidate_evaluation *evaluations, int evaluation_capacity,
    struct register_allocator_candidate_selection *selection) {

  seed_evaluations(evaluations, evaluation_capacity);
  selection->physical_register = 777;
  selection->candidate_index = 777;
  if (register_allocator_select_candidate_register(function_name, block_index, reason,
      physical_registers, physical_register_count, NO_REGISTER, state, is_allowed, is_path_safe,
      is_conflict_free, evaluations, evaluation_capacity, selection) != FAILED)
    return FAILED;
  if (selection_is_cleared(selection) == NO)
    return FAILED;
  return evaluations_are_cleared(evaluations, evaluation_capacity) == YES ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_candidate_evaluation evaluations[4];
  struct register_allocator_candidate_selection selection;
  struct callback_state state;
  int physical_registers[3];

  physical_registers[0] = 41;
  physical_registers[1] = -7;
  physical_registers[2] = 88;
  reset_state(&state, 0);
  if (register_allocator_evaluate_candidate_registers("coreCandidateEvaluationLazy", 3, "test",
      physical_registers, 3, NO_REGISTER, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4) == FAILED)
    return 1;
  if (state.allowed_calls != 3 || state.path_calls != 2 || state.conflict_calls != 1)
    return 2;
  if (evaluations[0].physical_register != 41 || evaluations[0].allowed != NO ||
      evaluations[0].path_safe != NO || evaluations[0].conflict_free != NO)
    return 3;
  if (evaluations[1].physical_register != -7 || evaluations[1].allowed != YES ||
      evaluations[1].path_safe != NO || evaluations[1].conflict_free != NO)
    return 4;
  if (evaluations[2].physical_register != 88 || evaluations[2].allowed != YES ||
      evaluations[2].path_safe != YES || evaluations[2].conflict_free != YES)
    return 5;
  if (evaluations[3].physical_register != NO_REGISTER)
    return 6;

  reset_state(&state, 1);
  if (expect_failure("coreCandidateEvaluationBadAllowed", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 7;
  if (state.allowed_calls != 2 || state.path_calls != 0 || state.conflict_calls != 0)
    return 8;
  reset_state(&state, 2);
  if (expect_failure("coreCandidateEvaluationBadPath", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 9;
  if (state.allowed_calls != 2 || state.path_calls != 1 || state.conflict_calls != 0)
    return 10;
  reset_state(&state, 3);
  if (expect_failure("coreCandidateEvaluationBadConflict", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 11;
  if (state.allowed_calls != 3 || state.path_calls != 2 || state.conflict_calls != 1)
    return 12;

  reset_state(&state, 0);
  physical_registers[1] = NO_REGISTER;
  if (expect_failure("coreCandidateEvaluationSentinel", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 13;
  if (state.allowed_calls != 0 || state.path_calls != 0 || state.conflict_calls != 0)
    return 14;
  physical_registers[1] = 41;
  if (expect_failure("coreCandidateEvaluationDuplicate", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 15;
  if (state.allowed_calls != 0 || state.path_calls != 0 || state.conflict_calls != 0)
    return 16;
  physical_registers[1] = -7;

  if (expect_failure(NULL, 3, "test", physical_registers, 3, &state, candidate_allowed,
      candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 17;
  if (expect_failure("coreCandidateEvaluationBadBlock", -1, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 18;
  if (expect_failure("coreCandidateEvaluationNullReason", 3, NULL, physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 19;
  if (expect_failure("coreCandidateEvaluationEmptyReason", 3, "", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 20;
  if (expect_failure("coreCandidateEvaluationNullRegisters", 3, "test", NULL, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 21;
  if (expect_failure("coreCandidateEvaluationZeroCount", 3, "test", physical_registers, 0,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 22;
  if (expect_failure("coreCandidateEvaluationNegativeCount", 3, "test", physical_registers, -1,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 23;
  if (expect_failure("coreCandidateEvaluationNullContext", 3, "test", physical_registers, 3,
      NULL, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 24;
  if (expect_failure("coreCandidateEvaluationOverCapacity", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 2) == FAILED)
    return 25;
  if (expect_failure("coreCandidateEvaluationMissingAllowed", 3, "test", physical_registers, 3,
      &state, NULL, candidate_path_safe, candidate_conflict_free, evaluations, 4) == FAILED)
    return 26;
  if (expect_failure("coreCandidateEvaluationMissingPath", 3, "test", physical_registers, 3,
      &state, candidate_allowed, NULL, candidate_conflict_free, evaluations, 4) == FAILED)
    return 27;
  if (expect_failure("coreCandidateEvaluationMissingConflict", 3, "test", physical_registers, 3,
      &state, candidate_allowed, candidate_path_safe, NULL, evaluations, 4) == FAILED)
    return 28;

  reset_state(&state, 0);
  if (register_allocator_select_candidate_register("coreCandidateSelectionThird", 3, "test",
      physical_registers, 3, NO_REGISTER, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4, &selection) == FAILED)
    return 29;
  if (selection.physical_register != 88 || selection.candidate_index != 2)
    return 30;
  if (state.allowed_calls != 3 || state.path_calls != 2 || state.conflict_calls != 1)
    return 31;

  physical_registers[0] = 41;
  physical_registers[1] = -7;
  reset_state(&state, 0);
  if (register_allocator_select_candidate_register("coreCandidateSelectionNone", 4, "test",
      physical_registers, 2, NO_REGISTER, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4, &selection) == FAILED)
    return 32;
  if (selection_is_cleared(&selection) == NO)
    return 33;
  if (state.allowed_calls != 2 || state.path_calls != 1 || state.conflict_calls != 0)
    return 34;

  physical_registers[0] = 88;
  reset_state(&state, 0);
  if (register_allocator_select_candidate_register("coreCandidateSelectionFirst", 5, "test",
      physical_registers, 1, NO_REGISTER, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4, &selection) == FAILED)
    return 35;
  if (selection.physical_register != 88 || selection.candidate_index != 0)
    return 36;

  physical_registers[0] = 41;
  physical_registers[1] = -7;
  physical_registers[2] = 88;
  reset_state(&state, 1);
  if (expect_selection_failure("coreCandidateSelectionBadAllowed", 3, "test",
      physical_registers, 3, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4, &selection) == FAILED)
    return 37;
  reset_state(&state, 2);
  if (expect_selection_failure("coreCandidateSelectionBadPath", 3, "test",
      physical_registers, 3, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4, &selection) == FAILED)
    return 38;
  reset_state(&state, 3);
  if (expect_selection_failure("coreCandidateSelectionBadConflict", 3, "test",
      physical_registers, 3, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 4, &selection) == FAILED)
    return 39;

  reset_state(&state, 0);
  if (expect_selection_failure(NULL, 3, "test", physical_registers, 3, &state,
      candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4,
      &selection) == FAILED)
    return 40;
  if (expect_selection_failure("coreCandidateSelectionBadBlock", -1, "test", physical_registers,
      3, &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4,
      &selection) == FAILED)
    return 41;
  if (expect_selection_failure("coreCandidateSelectionNullReason", 3, NULL, physical_registers,
      3, &state, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4,
      &selection) == FAILED)
    return 42;
  if (expect_selection_failure("coreCandidateSelectionNoContext", 3, "test", physical_registers,
      3, NULL, candidate_allowed, candidate_path_safe, candidate_conflict_free, evaluations, 4,
      &selection) == FAILED)
    return 43;
  if (expect_selection_failure("coreCandidateSelectionOverCapacity", 3, "test",
      physical_registers, 3, &state, candidate_allowed, candidate_path_safe,
      candidate_conflict_free, evaluations, 2, &selection) == FAILED)
    return 44;

  return 0;
}
