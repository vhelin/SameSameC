#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int expect_plan(char *name, int actual, int expected) {

  if (actual != expected)
    return FAILED;
  return SUCCEEDED;
}

int main(void) {

  if (expect_plan("primary", register_allocator_plan_candidate_fallback(
      "coreFallbackPrimary", 2, YES, YES, NO, YES, YES, YES),
      RA_CANDIDATE_PLAN_USE_PRIMARY) == FAILED)
    return 1;
  if (expect_plan("unsupported fallback", register_allocator_plan_candidate_fallback(
      "coreFallbackUnsupported", 2, NO, NO, YES, YES, YES, YES),
      RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED) == FAILED)
    return 2;
  if (expect_plan("unsupported reject", register_allocator_plan_candidate_fallback(
      "coreRejectUnsupported", 2, NO, YES, NO, NO, YES, YES),
      RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED) == FAILED)
    return 3;
  if (expect_plan("path fallback", register_allocator_plan_candidate_fallback(
      "coreFallbackPath", 2, YES, NO, YES, YES, YES, YES),
      RA_CANDIDATE_PLAN_FALLBACK_PATH) == FAILED)
    return 4;
  if (expect_plan("path reject", register_allocator_plan_candidate_fallback(
      "coreRejectPath", 2, YES, NO, NO, YES, NO, YES),
      RA_CANDIDATE_PLAN_REJECT_PATH) == FAILED)
    return 5;
  if (expect_plan("conflict fallback", register_allocator_plan_candidate_fallback(
      "coreFallbackConflict", 2, YES, YES, YES, NO, NO, YES),
      RA_CANDIDATE_PLAN_FALLBACK_CONFLICT) == FAILED)
    return 6;
  if (expect_plan("conflict deferred", register_allocator_plan_candidate_fallback(
      "coreDeferConflict", 2, YES, YES, YES, NO, NO, NO),
      RA_CANDIDATE_PLAN_USE_PRIMARY) == FAILED)
    return 7;
  if (expect_plan("unsupported precedence", register_allocator_plan_candidate_fallback(
      "coreUnsupportedPrecedence", 2, NO, NO, YES, YES, YES, YES),
      RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED) == FAILED)
    return 8;
  if (expect_plan("path precedence", register_allocator_plan_candidate_fallback(
      "corePathPrecedence", 2, YES, NO, YES, NO, YES, YES),
      RA_CANDIDATE_PLAN_FALLBACK_PATH) == FAILED)
    return 9;

  if (register_allocator_plan_candidate_fallback(NULL, 2, YES, YES, NO, YES, YES, YES) != FAILED)
    return 10;
  if (register_allocator_plan_candidate_fallback("coreBadBlock", -1, YES, YES, NO, YES, YES, YES) != FAILED)
    return 11;
  if (register_allocator_plan_candidate_fallback("coreBadAllowed", 2, 2, YES, NO, YES, YES, YES) != FAILED)
    return 12;
  if (register_allocator_plan_candidate_fallback("coreBadPath", 2, YES, 2, NO, YES, YES, YES) != FAILED)
    return 13;
  if (register_allocator_plan_candidate_fallback("coreBadActive", 2, YES, YES, 2, YES, YES, YES) != FAILED)
    return 14;
  if (register_allocator_plan_candidate_fallback("coreBadUnsupportedFlag", 2, YES, YES, NO, 2, YES, YES) != FAILED)
    return 15;
  if (register_allocator_plan_candidate_fallback("coreBadPathFlag", 2, YES, YES, NO, YES, 2, YES) != FAILED)
    return 16;
  if (register_allocator_plan_candidate_fallback("coreBadConflictFlag", 2, YES, YES, NO, YES, YES, 2) != FAILED)
    return 17;

  return 0;
}
