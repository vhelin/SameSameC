#include <stddef.h>
#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int get_units(int physical_register) {

  if (physical_register == 10)
    return 1;
  if (physical_register == 11)
    return 2;
  if (physical_register == 12)
    return 3;
  if (physical_register == 13)
    return 1024;
  if (physical_register == 14)
    return 0;
  if (physical_register == 15)
    return -1;

  return 0;
}


static void initialize_policy(struct register_allocator_target_policy *policy) {

  memset(policy, 0, sizeof(struct register_allocator_target_policy));
  policy->name = "opaque";
  policy->get_physical_register_units = get_units;
}

static int expect_complete(struct register_allocator_target_policy *policy, int left_register, int right_register, int expected_overlap) {

  int overlap;

  overlap = expected_overlap == YES ? NO : YES;
  if (register_allocator_physical_registers_overlap("corePhysicalOverlap", policy,
      left_register, right_register, &overlap) == FAILED)
    return FAILED;

  return overlap == expected_overlap ? SUCCEEDED : FAILED;
}

static int expect_failure(char *function_name, struct register_allocator_target_policy *policy, int left_register, int right_register) {

  int overlap;

  overlap = NO;
  if (register_allocator_physical_registers_overlap(function_name, policy,
      left_register, right_register, &overlap) != FAILED)
    return FAILED;

  return overlap == YES ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_target_policy policy;

  initialize_policy(&policy);
  if (expect_complete(&policy, 10, 10, YES) == FAILED)
    return 1;
  if (expect_complete(&policy, 10, 12, YES) == FAILED)
    return 2;
  if (expect_complete(&policy, 12, 10, YES) == FAILED)
    return 3;
  if (expect_complete(&policy, 10, 11, NO) == FAILED)
    return 4;
  if (expect_complete(&policy, 13, 12, NO) == FAILED)
    return 5;
  if (expect_failure("corePhysicalOverlapZeroLeft", &policy, 14, 10) == FAILED)
    return 6;
  if (expect_failure("corePhysicalOverlapZeroRight", &policy, 10, 14) == FAILED)
    return 7;
  if (expect_failure("corePhysicalOverlapNegative", &policy, 15, 10) == FAILED)
    return 8;
  if (expect_failure(NULL, &policy, 10, 11) == FAILED)
    return 9;
  if (expect_failure("corePhysicalOverlapNullPolicy", NULL, 10, 11) == FAILED)
    return 10;
  policy.name = NULL;
  if (expect_failure("corePhysicalOverlapNullName", &policy, 10, 11) == FAILED)
    return 11;
  policy.name = "";
  if (expect_failure("corePhysicalOverlapEmptyName", &policy, 10, 11) == FAILED)
    return 12;
  initialize_policy(&policy);
  policy.get_physical_register_units = NULL;
  if (expect_failure("corePhysicalOverlapNoUnits", &policy, 10, 11) == FAILED)
    return 13;
  initialize_policy(&policy);
  if (register_allocator_physical_registers_overlap("corePhysicalOverlapNullOutput", &policy,
      10, 11, NULL) != FAILED)
    return 14;

  return 0;
}
