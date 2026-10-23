#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)
#define TEST_OP 40
#define TEST_VALUE_OPERAND 1
#define TEST_INDEX_OPERAND 2
#define TEST_REGISTER 7

static int g_mode;
static int g_calls;

static int prefer_equal(int consumer_op, int candidate_operand, int active_operand,
    int physical_register) {

  g_calls++;
  if (g_mode == 1)
    return 2;
  if (consumer_op == TEST_OP && candidate_operand == TEST_VALUE_OPERAND &&
      active_operand == TEST_INDEX_OPERAND && physical_register == TEST_REGISTER)
    return YES;
  return NO;
}

static void initialize_policy(struct register_allocator_target_policy *policy) {

  memset(policy, 0, sizeof(*policy));
  policy->name = "opaque-equal";
  policy->prefer_candidate_on_equal_next_use = prefer_equal;
}

static int expect_failure(char *function_name, int block_index,
    struct register_allocator_target_policy *policy, int consumer_op, int candidate_operand,
    int active_operand, int physical_register, int candidate_next_use, int active_next_use) {

  int prefer_candidate;

  prefer_candidate = 7;
  if (register_allocator_resolve_equal_next_use_preference(function_name, block_index, policy,
      consumer_op, candidate_operand, active_operand, physical_register, NO_REGISTER,
      candidate_next_use, active_next_use, &prefer_candidate) != FAILED)
    return FAILED;
  return prefer_candidate == NO ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_target_policy policy;
  int prefer_candidate;

  initialize_policy(&policy);
  g_calls = 0;
  if (register_allocator_resolve_equal_next_use_preference("coreEqualPolicyPreferred", 2,
      &policy, TEST_OP, TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, NO_REGISTER,
      9, 9, &prefer_candidate) == FAILED || prefer_candidate != YES || g_calls != 1)
    return 1;

  g_calls = 0;
  if (register_allocator_resolve_equal_next_use_preference("coreEqualPolicyRejected", 2,
      &policy, TEST_OP, TEST_INDEX_OPERAND, TEST_VALUE_OPERAND, TEST_REGISTER, NO_REGISTER,
      9, 9, &prefer_candidate) == FAILED || prefer_candidate != NO || g_calls != 1)
    return 2;

  g_calls = 0;
  if (register_allocator_resolve_equal_next_use_preference("coreEqualPolicyUnequal", 2,
      &policy, TEST_OP, TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, NO_REGISTER,
      8, 9, &prefer_candidate) == FAILED || prefer_candidate != NO || g_calls != 0)
    return 3;

  g_mode = 1;
  g_calls = 0;
  if (expect_failure("coreEqualPolicyBadCallback", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED || g_calls != 1)
    return 4;
  g_mode = 0;

  policy.prefer_candidate_on_equal_next_use = NULL;
  if (expect_failure("coreEqualPolicyMissingCallback", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 5;
  initialize_policy(&policy);
  policy.name = NULL;
  if (expect_failure("coreEqualPolicyNullName", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 6;
  initialize_policy(&policy);
  policy.name = "";
  if (expect_failure("coreEqualPolicyEmptyName", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 7;
  initialize_policy(&policy);
  if (expect_failure(NULL, 2, &policy, TEST_OP, TEST_VALUE_OPERAND,
      TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 8;
  if (expect_failure("coreEqualPolicyBadBlock", -1, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 9;
  if (expect_failure("coreEqualPolicyNullPolicy", 2, NULL, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 10;
  if (expect_failure("coreEqualPolicyBadOp", 2, &policy, -1,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 11;
  if (expect_failure("coreEqualPolicyBadCandidateOperand", 2, &policy, TEST_OP,
      -1, TEST_INDEX_OPERAND, TEST_REGISTER, 9, 9) == FAILED)
    return 12;
  if (expect_failure("coreEqualPolicyBadActiveOperand", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, -1, TEST_REGISTER, 9, 9) == FAILED)
    return 13;
  if (expect_failure("coreEqualPolicySentinel", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, NO_REGISTER, 9, 9) == FAILED)
    return 14;
  if (expect_failure("coreEqualPolicyBadCandidateNext", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, -1, 9) == FAILED)
    return 15;
  if (expect_failure("coreEqualPolicyBadActiveNext", 2, &policy, TEST_OP,
      TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, 9, -2) == FAILED)
    return 16;

  prefer_candidate = 7;
  if (register_allocator_resolve_equal_next_use_preference("coreEqualPolicyNullOutput", 2,
      &policy, TEST_OP, TEST_VALUE_OPERAND, TEST_INDEX_OPERAND, TEST_REGISTER, NO_REGISTER,
      9, 9, NULL) != FAILED)
    return 17;

  return 0;
}
