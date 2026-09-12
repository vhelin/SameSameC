#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int g_mode = 0;
static int g_slot_count = 7;

static int get_slot_count(void) {

  return g_slot_count;
}

static int get_slot_index(int physical_register) {

  if (physical_register == 101)
    return 4;
  if (physical_register == 102)
    return g_mode == 1 ? 4 : 1;
  if (physical_register == 103)
    return g_mode == 2 ? 7 : 6;
  if (physical_register == 104)
    return 0;
  if (physical_register == 105)
    return 3;

  return -1;
}

static char *get_slot_name(int physical_register) {

  if (physical_register == 101)
    return "opaque-a";
  if (physical_register == 102)
    return "opaque-word";
  if (physical_register == 103)
    return "opaque-pair";
  if (physical_register == 104)
    return g_mode == 3 ? NULL : "opaque-high";
  if (physical_register == 105)
    return g_mode == 4 ? "" : "opaque-low";

  return NULL;
}

static void initialize_policy(struct register_allocator_target_policy *policy) {

  memset(policy, 0, sizeof(*policy));
  policy->name = "reordered-seven";
  policy->get_active_slot_count = get_slot_count;
  policy->get_active_slot_index = get_slot_index;
  policy->get_active_slot_name = get_slot_name;
}

static int outputs_are_cleared(int *slot_indices, int role_count) {

  int role_index;

  for (role_index = 0; role_index < role_count; role_index++) {
    if (slot_indices[role_index] != -1)
      return NO;
  }

  return YES;
}

static void seed_outputs(int *slot_indices, int role_count) {

  int role_index;

  for (role_index = 0; role_index < role_count; role_index++)
    slot_indices[role_index] = 90 + role_index;
}

static int expect_policy_failure(char *function_name, struct register_allocator_target_policy *policy, int *physical_registers, int *slot_indices) {

  seed_outputs(slot_indices, 5);
  if (register_allocator_resolve_active_slot_roles(function_name, policy, physical_registers, 5, slot_indices) != FAILED)
    return FAILED;
  return outputs_are_cleared(slot_indices, 5) == YES ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_target_policy policy;
  int physical_registers[5];
  int slot_indices[5];

  physical_registers[0] = 101;
  physical_registers[1] = 102;
  physical_registers[2] = 103;
  physical_registers[3] = 104;
  physical_registers[4] = 105;
  initialize_policy(&policy);

  if (register_allocator_resolve_active_slot_roles("coreActiveSlotRolesReordered", &policy,
      physical_registers, 5, slot_indices) == FAILED)
    return 1;
  if (slot_indices[0] != 4 || slot_indices[1] != 1 || slot_indices[2] != 6 ||
      slot_indices[3] != 0 || slot_indices[4] != 3)
    return 2;

  g_mode = 1;
  if (expect_policy_failure("coreActiveSlotRolesDuplicate", &policy, physical_registers, slot_indices) == FAILED)
    return 3;
  g_mode = 2;
  if (expect_policy_failure("coreActiveSlotRolesOutOfRange", &policy, physical_registers, slot_indices) == FAILED)
    return 4;
  g_mode = 3;
  if (expect_policy_failure("coreActiveSlotRolesNullName", &policy, physical_registers, slot_indices) == FAILED)
    return 5;
  g_mode = 4;
  if (expect_policy_failure("coreActiveSlotRolesEmptyName", &policy, physical_registers, slot_indices) == FAILED)
    return 6;
  g_mode = 0;
  g_slot_count = 0;
  if (expect_policy_failure("coreActiveSlotRolesZeroSlots", &policy, physical_registers, slot_indices) == FAILED)
    return 7;
  g_slot_count = 7;

  policy.get_active_slot_count = NULL;
  if (expect_policy_failure("coreActiveSlotRolesMissingCount", &policy, physical_registers, slot_indices) == FAILED)
    return 8;
  initialize_policy(&policy);
  policy.get_active_slot_index = NULL;
  if (expect_policy_failure("coreActiveSlotRolesMissingIndex", &policy, physical_registers, slot_indices) == FAILED)
    return 9;
  initialize_policy(&policy);
  policy.get_active_slot_name = NULL;
  if (expect_policy_failure("coreActiveSlotRolesMissingName", &policy, physical_registers, slot_indices) == FAILED)
    return 10;
  initialize_policy(&policy);
  policy.name = NULL;
  if (expect_policy_failure("coreActiveSlotRolesNullPolicyName", &policy, physical_registers, slot_indices) == FAILED)
    return 11;
  initialize_policy(&policy);
  policy.name = "";
  if (expect_policy_failure("coreActiveSlotRolesEmptyPolicyName", &policy, physical_registers, slot_indices) == FAILED)
    return 12;
  initialize_policy(&policy);

  if (expect_policy_failure(NULL, &policy, physical_registers, slot_indices) == FAILED)
    return 13;
  if (expect_policy_failure("coreActiveSlotRolesNullPolicy", NULL, physical_registers, slot_indices) == FAILED)
    return 14;
  if (expect_policy_failure("coreActiveSlotRolesNullRegisters", &policy, NULL, slot_indices) == FAILED)
    return 15;
  if (register_allocator_resolve_active_slot_roles("coreActiveSlotRolesNullOutput", &policy,
      physical_registers, 5, NULL) != FAILED)
    return 16;
  if (register_allocator_resolve_active_slot_roles("coreActiveSlotRolesZeroRoles", &policy,
      physical_registers, 0, slot_indices) != FAILED)
    return 17;

  physical_registers[4] = 999;
  if (expect_policy_failure("coreActiveSlotRolesUnknownRegister", &policy, physical_registers, slot_indices) == FAILED)
    return 18;

  return 0;
}