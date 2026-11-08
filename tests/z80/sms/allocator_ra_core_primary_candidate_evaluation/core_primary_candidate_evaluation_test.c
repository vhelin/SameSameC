#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)

struct callback_state {
  int mode;
  int allowed_calls;
  int path_calls;
  int active_calls;
};

static int candidate_allowed(void *context, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->allowed_calls++;
  if (state->mode == 3)
    return 2;
  if (state->mode == 1)
    return NO;
  return physical_register == 41 ? YES : NO;
}

static int candidate_path_safe(void *context, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->path_calls++;
  if (state->mode == 4)
    return 2;
  if (state->mode == 2)
    return NO;
  return physical_register == 41 ? YES : NO;
}

static int candidate_active(void *context, int physical_register) {

  struct callback_state *state;

  state = (struct callback_state *)context;
  state->active_calls++;
  if (state->mode == 5)
    return 2;
  if (state->mode == 1 || state->mode == 2)
    return YES;
  return physical_register == 41 ? NO : YES;
}

static void reset_state(struct callback_state *state, int mode) {

  state->mode = mode;
  state->allowed_calls = 0;
  state->path_calls = 0;
  state->active_calls = 0;
}

static void seed_evaluation(struct register_allocator_primary_candidate_evaluation *evaluation) {

  evaluation->physical_register = 700;
  evaluation->allowed = 7;
  evaluation->path_safe = 7;
  evaluation->active = 7;
}

static int evaluation_is_clear(struct register_allocator_primary_candidate_evaluation *evaluation) {

  return evaluation->physical_register == NO_REGISTER && evaluation->allowed == NO &&
      evaluation->path_safe == NO && evaluation->active == NO ? YES : NO;
}

static int expect_failure(char *function_name, int block_index, int physical_register,
    struct callback_state *state, register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_active,
    struct register_allocator_primary_candidate_evaluation *evaluation) {

  if (evaluation != NULL)
    seed_evaluation(evaluation);
  if (register_allocator_evaluate_primary_candidate(function_name, block_index, physical_register,
      NO_REGISTER, state, is_allowed, is_path_safe, is_active, evaluation) != FAILED)
    return FAILED;
  if (evaluation != NULL && evaluation_is_clear(evaluation) == NO)
    return FAILED;
  return SUCCEEDED;
}

int main(void) {

  struct callback_state state;
  struct register_allocator_primary_candidate_evaluation evaluation;

  reset_state(&state, 0);
  if (register_allocator_evaluate_primary_candidate("corePrimaryCandidateAvailable", 3, 41,
      NO_REGISTER, &state, candidate_allowed, candidate_path_safe, candidate_active,
      &evaluation) == FAILED)
    return 1;
  if (evaluation.physical_register != 41 || evaluation.allowed != YES ||
      evaluation.path_safe != YES || evaluation.active != NO || state.allowed_calls != 1 ||
      state.path_calls != 1 || state.active_calls != 1)
    return 2;

  reset_state(&state, 1);
  if (register_allocator_evaluate_primary_candidate("corePrimaryCandidateUnsupported", 3, 41,
      NO_REGISTER, &state, candidate_allowed, candidate_path_safe, candidate_active,
      &evaluation) == FAILED)
    return 3;
  if (evaluation.allowed != NO || evaluation.path_safe != NO || evaluation.active != YES ||
      state.allowed_calls != 1 || state.path_calls != 0 || state.active_calls != 1)
    return 4;

  reset_state(&state, 2);
  if (register_allocator_evaluate_primary_candidate("corePrimaryCandidateUnsafe", 3, 41,
      NO_REGISTER, &state, candidate_allowed, candidate_path_safe, candidate_active,
      &evaluation) == FAILED)
    return 5;
  if (evaluation.allowed != YES || evaluation.path_safe != NO || evaluation.active != YES ||
      state.allowed_calls != 1 || state.path_calls != 1 || state.active_calls != 1)
    return 6;

  reset_state(&state, 3);
  if (expect_failure("corePrimaryCandidateBadAllowed", 3, 41, &state, candidate_allowed,
      candidate_path_safe, candidate_active, &evaluation) == FAILED || state.allowed_calls != 1 ||
      state.path_calls != 0 || state.active_calls != 0)
    return 7;
  reset_state(&state, 4);
  if (expect_failure("corePrimaryCandidateBadPath", 3, 41, &state, candidate_allowed,
      candidate_path_safe, candidate_active, &evaluation) == FAILED || state.allowed_calls != 1 ||
      state.path_calls != 1 || state.active_calls != 0)
    return 8;
  reset_state(&state, 5);
  if (expect_failure("corePrimaryCandidateBadActive", 3, 41, &state, candidate_allowed,
      candidate_path_safe, candidate_active, &evaluation) == FAILED || state.allowed_calls != 1 ||
      state.path_calls != 1 || state.active_calls != 1)
    return 9;

  reset_state(&state, 0);
  if (expect_failure(NULL, 3, 41, &state, candidate_allowed, candidate_path_safe,
      candidate_active, &evaluation) == FAILED)
    return 10;
  if (expect_failure("corePrimaryCandidateBadBlock", -1, 41, &state, candidate_allowed,
      candidate_path_safe, candidate_active, &evaluation) == FAILED)
    return 11;
  if (expect_failure("corePrimaryCandidateSentinel", 3, NO_REGISTER, &state, candidate_allowed,
      candidate_path_safe, candidate_active, &evaluation) == FAILED)
    return 12;
  if (register_allocator_evaluate_primary_candidate("corePrimaryCandidateNullContext", 3, 41,
      NO_REGISTER, NULL, candidate_allowed, candidate_path_safe, candidate_active,
      &evaluation) != FAILED || evaluation_is_clear(&evaluation) == NO)
    return 13;
  if (expect_failure("corePrimaryCandidateMissingAllowed", 3, 41, &state, NULL,
      candidate_path_safe, candidate_active, &evaluation) == FAILED)
    return 14;
  if (expect_failure("corePrimaryCandidateMissingPath", 3, 41, &state, candidate_allowed,
      NULL, candidate_active, &evaluation) == FAILED)
    return 15;
  if (expect_failure("corePrimaryCandidateMissingActive", 3, 41, &state, candidate_allowed,
      candidate_path_safe, NULL, &evaluation) == FAILED)
    return 16;
  if (register_allocator_evaluate_primary_candidate("corePrimaryCandidateNullOutput", 3, 41,
      NO_REGISTER, &state, candidate_allowed, candidate_path_safe, candidate_active, NULL) != FAILED)
    return 17;

  return 0;
}
