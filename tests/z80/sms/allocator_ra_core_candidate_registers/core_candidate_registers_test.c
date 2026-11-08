#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)

static int g_mode = 0;

static int get_candidate_count(int size) {

  if (g_mode == 3)
    return 0;
  if (g_mode == 4)
    return -1;
  if (g_mode == 5)
    return 4;
  if (size == 8)
    return 3;
  if (size == 16)
    return 2;

  return 0;
}


static int get_candidate_register(int size, int candidate_index) {

  if (size == 8) {
    if (candidate_index == 0)
      return 41;
    if (candidate_index == 1)
      return g_mode == 1 ? 41 : -7;
    if (candidate_index == 2)
      return g_mode == 2 ? NO_REGISTER : 88;
  }
  if (size == 16) {
    if (candidate_index == 0)
      return 500;
    if (candidate_index == 1)
      return 600;
  }

  return NO_REGISTER;
}

static void initialize_policy(struct register_allocator_target_policy *policy) {

  memset(policy, 0, sizeof(*policy));
  policy->name = "opaque-candidates";
  policy->get_candidate_register_count = get_candidate_count;
  policy->get_candidate_physical_register = get_candidate_register;
}

static void seed_outputs(int *physical_registers, int capacity, int *register_count) {

  int candidate_index;

  for (candidate_index = 0; candidate_index < capacity; candidate_index++)
    physical_registers[candidate_index] = 700 + candidate_index;
  *register_count = 99;
}

static int outputs_are_cleared(int *physical_registers, int capacity, int register_count) {

  int candidate_index;

  if (register_count != 0)
    return NO;
  for (candidate_index = 0; candidate_index < capacity; candidate_index++) {
    if (physical_registers[candidate_index] != NO_REGISTER)
      return NO;
  }

  return YES;
}

static int expect_failure(char *function_name, struct register_allocator_target_policy *policy,
    int size, int *physical_registers, int capacity, int *register_count) {

  seed_outputs(physical_registers, capacity, register_count);
  if (register_allocator_resolve_candidate_registers(function_name, policy, size, NO_REGISTER,
      physical_registers, capacity, register_count) != FAILED)
    return FAILED;
  return outputs_are_cleared(physical_registers, capacity, *register_count) == YES ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_target_policy policy;
  int physical_registers[3];
  int register_count;

  initialize_policy(&policy);
  if (register_allocator_resolve_candidate_registers("coreCandidateRegistersByte", &policy, 8,
      NO_REGISTER, physical_registers, 3, &register_count) == FAILED)
    return 1;
  if (register_count != 3 || physical_registers[0] != 41 || physical_registers[1] != -7 ||
      physical_registers[2] != 88)
    return 2;

  if (register_allocator_resolve_candidate_registers("coreCandidateRegistersWord", &policy, 16,
      NO_REGISTER, physical_registers, 3, &register_count) == FAILED)
    return 3;
  if (register_count != 2 || physical_registers[0] != 500 || physical_registers[1] != 600 ||
      physical_registers[2] != NO_REGISTER)
    return 4;

  g_mode = 1;
  if (expect_failure("coreCandidateRegistersDuplicate", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 5;
  g_mode = 2;
  if (expect_failure("coreCandidateRegistersSentinel", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 6;
  g_mode = 3;
  if (expect_failure("coreCandidateRegistersZeroCount", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 7;
  g_mode = 4;
  if (expect_failure("coreCandidateRegistersNegativeCount", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 8;
  g_mode = 5;
  if (expect_failure("coreCandidateRegistersOverCapacity", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 9;
  g_mode = 0;

  policy.get_candidate_register_count = NULL;
  if (expect_failure("coreCandidateRegistersMissingCount", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 10;
  initialize_policy(&policy);
  policy.get_candidate_physical_register = NULL;
  if (expect_failure("coreCandidateRegistersMissingRegister", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 11;
  initialize_policy(&policy);
  policy.name = NULL;
  if (expect_failure("coreCandidateRegistersNullName", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 12;
  initialize_policy(&policy);
  policy.name = "";
  if (expect_failure("coreCandidateRegistersEmptyName", &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 13;
  initialize_policy(&policy);

  if (expect_failure(NULL, &policy, 8, physical_registers, 3, &register_count) == FAILED)
    return 14;
  if (expect_failure("coreCandidateRegistersNullPolicy", NULL, 8, physical_registers, 3, &register_count) == FAILED)
    return 15;
  if (expect_failure("coreCandidateRegistersZeroSize", &policy, 0, physical_registers, 3, &register_count) == FAILED)
    return 16;
  register_count = 99;
  if (register_allocator_resolve_candidate_registers("coreCandidateRegistersNullOutput", &policy, 8,
      NO_REGISTER, NULL, 3, &register_count) != FAILED || register_count != 0)
    return 17;
  seed_outputs(physical_registers, 3, &register_count);
  if (register_allocator_resolve_candidate_registers("coreCandidateRegistersZeroCapacity", &policy, 8,
      NO_REGISTER, physical_registers, 0, &register_count) != FAILED || register_count != 0)
    return 18;
  seed_outputs(physical_registers, 3, &register_count);
  if (register_allocator_resolve_candidate_registers("coreCandidateRegistersNullCount", &policy, 8,
      NO_REGISTER, physical_registers, 3, NULL) != FAILED ||
      physical_registers[0] != NO_REGISTER || physical_registers[1] != NO_REGISTER ||
      physical_registers[2] != NO_REGISTER)
    return 19;

  return 0;
}
