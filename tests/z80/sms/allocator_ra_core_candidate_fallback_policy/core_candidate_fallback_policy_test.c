#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int g_mode;
static int g_call_count;
static int g_reasons[3];

static int can_fallback_candidate(int size, int reason) {

  if (g_call_count < 3)
    g_reasons[g_call_count] = reason;
  g_call_count++;

  if ((g_mode == 1 && reason == RA_CANDIDATE_FALLBACK_UNSUPPORTED) ||
      (g_mode == 2 && reason == RA_CANDIDATE_FALLBACK_PATH) ||
      (g_mode == 3 && reason == RA_CANDIDATE_FALLBACK_CONFLICT))
    return 2;
  if (reason == RA_CANDIDATE_FALLBACK_UNSUPPORTED)
    return size == 16 ? YES : NO;
  if (reason == RA_CANDIDATE_FALLBACK_PATH)
    return size == 8 ? YES : NO;
  if (reason == RA_CANDIDATE_FALLBACK_CONFLICT)
    return size == 8 || size == 16 ? YES : NO;

  return 2;
}

static void initialize_policy(struct register_allocator_target_policy *policy) {

  memset(policy, 0, sizeof(*policy));
  policy->name = "opaque-fallback";
  policy->can_fallback_candidate = can_fallback_candidate;
}

static void seed_outputs(int *unsupported, int *path, int *conflict) {

  if (unsupported != NULL)
    *unsupported = 7;
  if (path != NULL)
    *path = 7;
  if (conflict != NULL)
    *conflict = 7;
  g_call_count = 0;
  g_reasons[0] = 0;
  g_reasons[1] = 0;
  g_reasons[2] = 0;
}

static int outputs_are_clear(int unsupported, int path, int conflict) {

  return unsupported == NO && path == NO && conflict == NO ? YES : NO;
}

static int expect_failure(char *function_name, struct register_allocator_target_policy *policy,
    int size, int candidate_count) {

  int unsupported;
  int path;
  int conflict;

  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks(function_name, policy, size, candidate_count,
      &unsupported, &path, &conflict) != FAILED)
    return FAILED;
  return outputs_are_clear(unsupported, path, conflict) == YES ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_target_policy policy;
  int unsupported;
  int path;
  int conflict;

  initialize_policy(&policy);
  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks("coreFallbackPolicyByte", &policy, 8, 3,
      &unsupported, &path, &conflict) == FAILED)
    return 1;
  if (unsupported != NO || path != YES || conflict != YES || g_call_count != 3 ||
      g_reasons[0] != RA_CANDIDATE_FALLBACK_UNSUPPORTED ||
      g_reasons[1] != RA_CANDIDATE_FALLBACK_PATH ||
      g_reasons[2] != RA_CANDIDATE_FALLBACK_CONFLICT)
    return 2;

  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks("coreFallbackPolicyWord", &policy, 16, 2,
      &unsupported, &path, &conflict) == FAILED)
    return 3;
  if (unsupported != YES || path != NO || conflict != YES || g_call_count != 3)
    return 4;

  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks("coreFallbackPolicySingle", &policy, 8, 1,
      &unsupported, &path, &conflict) == FAILED)
    return 5;
  if (outputs_are_clear(unsupported, path, conflict) == NO || g_call_count != 3)
    return 6;

  g_mode = 1;
  if (expect_failure("coreFallbackPolicyBadUnsupported", &policy, 8, 3) == FAILED)
    return 7;
  g_mode = 2;
  if (expect_failure("coreFallbackPolicyBadPath", &policy, 8, 3) == FAILED)
    return 8;
  g_mode = 3;
  if (expect_failure("coreFallbackPolicyBadConflict", &policy, 8, 3) == FAILED)
    return 9;
  g_mode = 0;

  policy.can_fallback_candidate = NULL;
  if (expect_failure("coreFallbackPolicyMissingCallback", &policy, 8, 3) == FAILED)
    return 10;
  initialize_policy(&policy);
  policy.name = NULL;
  if (expect_failure("coreFallbackPolicyNullName", &policy, 8, 3) == FAILED)
    return 11;
  initialize_policy(&policy);
  policy.name = "";
  if (expect_failure("coreFallbackPolicyEmptyName", &policy, 8, 3) == FAILED)
    return 12;
  initialize_policy(&policy);
  if (expect_failure(NULL, &policy, 8, 3) == FAILED)
    return 13;
  if (expect_failure("coreFallbackPolicyNullPolicy", NULL, 8, 3) == FAILED)
    return 14;
  if (expect_failure("coreFallbackPolicyBadSize", &policy, 0, 3) == FAILED)
    return 15;
  if (expect_failure("coreFallbackPolicyBadCount", &policy, 8, 0) == FAILED)
    return 16;

  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks("coreFallbackPolicyNullUnsupported", &policy,
      8, 3, NULL, &path, &conflict) != FAILED || path != NO || conflict != NO)
    return 17;
  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks("coreFallbackPolicyNullPath", &policy,
      8, 3, &unsupported, NULL, &conflict) != FAILED || unsupported != NO || conflict != NO)
    return 18;
  seed_outputs(&unsupported, &path, &conflict);
  if (register_allocator_resolve_candidate_fallbacks("coreFallbackPolicyNullConflict", &policy,
      8, 3, &unsupported, &path, NULL) != FAILED || unsupported != NO || path != NO)
    return 19;

  return 0;
}
