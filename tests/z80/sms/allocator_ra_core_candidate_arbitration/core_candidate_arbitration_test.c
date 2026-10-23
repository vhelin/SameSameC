#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)

static void set_candidate(struct register_allocator_candidate_evaluation *candidate,
    int physical_register, int allowed, int path_safe, int conflict_free) {

  candidate->physical_register = physical_register;
  candidate->allowed = allowed;
  candidate->path_safe = path_safe;
  candidate->conflict_free = conflict_free;
}

static int expect_selection(char *function_name, struct register_allocator_candidate_evaluation *candidates,
    int candidate_count, int expected_register, int expected_index) {

  int selected_register;
  int selected_index;

  selected_register = 700;
  selected_index = 700;
  if (register_allocator_choose_candidate_register(function_name, 2, "test", candidates,
      candidate_count, NO_REGISTER, &selected_register, &selected_index) == FAILED)
    return FAILED;
  if (selected_register != expected_register || selected_index != expected_index)
    return FAILED;

  return SUCCEEDED;
}

static int expect_failure(char *function_name, int block_index, char *reason,
    struct register_allocator_candidate_evaluation *candidates, int candidate_count) {

  int selected_register;
  int selected_index;

  selected_register = 700;
  selected_index = 700;
  if (register_allocator_choose_candidate_register(function_name, block_index, reason, candidates,
      candidate_count, NO_REGISTER, &selected_register, &selected_index) != FAILED)
    return FAILED;
  if (selected_register != NO_REGISTER || selected_index != -1)
    return FAILED;

  return SUCCEEDED;
}

int main(void) {

  struct register_allocator_candidate_evaluation candidates[4];
  int selected_register;
  int selected_index;

  set_candidate(&candidates[0], 41, YES, YES, YES);
  set_candidate(&candidates[1], -7, YES, YES, YES);
  if (expect_selection("coreCandidateArbitrationFirst", candidates, 2, 41, 0) == FAILED)
    return 1;

  set_candidate(&candidates[0], 41, NO, YES, YES);
  set_candidate(&candidates[1], -7, YES, NO, YES);
  set_candidate(&candidates[2], 88, YES, YES, NO);
  set_candidate(&candidates[3], 500, YES, YES, YES);
  if (expect_selection("coreCandidateArbitrationOrdered", candidates, 4, 500, 3) == FAILED)
    return 2;

  set_candidate(&candidates[0], 41, NO, YES, YES);
  set_candidate(&candidates[1], -7, YES, NO, YES);
  set_candidate(&candidates[2], 88, YES, YES, NO);
  if (expect_selection("coreCandidateArbitrationNone", candidates, 3, NO_REGISTER, -1) == FAILED)
    return 3;

  set_candidate(&candidates[0], -7, YES, YES, YES);
  if (expect_selection("coreCandidateArbitrationNegative", candidates, 1, -7, 0) == FAILED)
    return 4;

  set_candidate(&candidates[0], 41, YES, YES, YES);
  set_candidate(&candidates[1], NO_REGISTER, YES, YES, YES);
  if (expect_failure("coreCandidateArbitrationLateSentinel", 2, "test", candidates, 2) == FAILED)
    return 5;

  set_candidate(&candidates[0], 41, YES, YES, YES);
  set_candidate(&candidates[1], 41, YES, YES, YES);
  if (expect_failure("coreCandidateArbitrationLateDuplicate", 2, "test", candidates, 2) == FAILED)
    return 6;

  set_candidate(&candidates[0], 41, 2, YES, YES);
  if (expect_failure("coreCandidateArbitrationBadAllowed", 2, "test", candidates, 1) == FAILED)
    return 7;
  set_candidate(&candidates[0], 41, YES, 2, YES);
  if (expect_failure("coreCandidateArbitrationBadPath", 2, "test", candidates, 1) == FAILED)
    return 8;
  set_candidate(&candidates[0], 41, YES, YES, 2);
  if (expect_failure("coreCandidateArbitrationBadConflict", 2, "test", candidates, 1) == FAILED)
    return 9;
  set_candidate(&candidates[0], 41, YES, YES, YES);

  if (expect_failure(NULL, 2, "test", candidates, 1) == FAILED)
    return 10;
  if (expect_failure("coreCandidateArbitrationBadBlock", -1, "test", candidates, 1) == FAILED)
    return 11;
  if (expect_failure("coreCandidateArbitrationNullReason", 2, NULL, candidates, 1) == FAILED)
    return 12;
  if (expect_failure("coreCandidateArbitrationEmptyReason", 2, "", candidates, 1) == FAILED)
    return 13;
  if (expect_failure("coreCandidateArbitrationNullCandidates", 2, "test", NULL, 1) == FAILED)
    return 14;
  if (expect_failure("coreCandidateArbitrationZeroCount", 2, "test", candidates, 0) == FAILED)
    return 15;
  if (expect_failure("coreCandidateArbitrationNegativeCount", 2, "test", candidates, -1) == FAILED)
    return 16;

  selected_index = 700;
  if (register_allocator_choose_candidate_register("coreCandidateArbitrationNullRegister", 2,
      "test", candidates, 1, NO_REGISTER, NULL, &selected_index) != FAILED || selected_index != -1)
    return 17;
  selected_register = 700;
  if (register_allocator_choose_candidate_register("coreCandidateArbitrationNullIndex", 2,
      "test", candidates, 1, NO_REGISTER, &selected_register, NULL) != FAILED ||
      selected_register != NO_REGISTER)
    return 18;

  return 0;
}
