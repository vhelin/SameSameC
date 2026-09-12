#include <stddef.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int unary_int(int value) {
  return value;
}

static int no_argument_int(void) {
  return 1;
}

static char *register_name(int physical_register) {
  return physical_register == 0 ? "R0" : "R";
}

static int candidate_allowed(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int physical_register, int consumer_arg) {
  return producer != NULL || consumer != NULL || temp_register != NULL || physical_register == consumer_arg;
}

static int tac_transparent(struct tac *t, int physical_register) {
  return t != NULL || physical_register >= 0;
}

static int binary_int(int left, int right) {
  return left + right;
}

static int ternary_int(int first, int second, int third) {
  return first + second + third;
}

static int quaternary_int(int first, int second, int third, int fourth) {
  return first + second + third + fourth;
}

static int split_reload_register(struct tac *consumer, int consumer_arg, int size) {
  return consumer != NULL ? consumer_arg : size;
}

static void initialize_policy(struct register_allocator_target_policy *policy) {

  policy->name = "opaque";
  policy->get_physical_register_units = unary_int;
  policy->get_active_slot_count = no_argument_int;
  policy->get_active_slot_index = unary_int;
  policy->get_active_slot_name = register_name;
  policy->get_candidate_register_count = unary_int;
  policy->get_candidate_physical_register = binary_int;
  policy->can_fallback_candidate = binary_int;
  policy->prefer_candidate_on_equal_next_use = quaternary_int;
  policy->is_candidate_allowed_for_physical_register = candidate_allowed;
  policy->get_tac_clobbers = unary_int;
  policy->is_tac_transparent_for_physical_register = tac_transparent;
  policy->get_call_result_physical_register = unary_int;
  policy->get_call_boundary_spill_reason = unary_int;
  policy->get_stack_return_value_end_offset = unary_int;
  policy->get_return_value_byte_offset = binary_int;
  policy->get_call_frame_prefix_value = unary_int;
  policy->get_stack_argument_end_offset = ternary_int;
  policy->get_argument_transport = binary_int;
  policy->get_argument_byte_offset = ternary_int;
  policy->get_location_kind = binary_int;
  policy->get_address_materialization_mode = ternary_int;
  policy->can_preserve_split_spill = quaternary_int;
  policy->get_split_reload_physical_register = split_reload_register;
}

int main(void) {

  struct register_allocator_target_policy policy;
  int result;

  initialize_policy(&policy);
  if (register_allocator_validate_target_policy("coreTargetPolicyComplete", &policy) == FAILED)
    return 1;
  if (register_allocator_validate_target_policy(NULL, &policy) != FAILED)
    return 2;
  if (register_allocator_validate_target_policy("coreTargetPolicyNull", NULL) != FAILED)
    return 3;
  policy.name = NULL;
  if (register_allocator_validate_target_policy("coreTargetPolicyNullName", &policy) != FAILED)
    return 4;
  policy.name = "";
  if (register_allocator_validate_target_policy("coreTargetPolicyEmptyName", &policy) != FAILED)
    return 5;

  initialize_policy(&policy);
  result = 6;
#define CHECK_MISSING(field) \
  policy.field = NULL; \
  if (register_allocator_validate_target_policy("coreTargetPolicyMissing", &policy) != FAILED) \
    return result; \
  initialize_policy(&policy); \
  result++

  CHECK_MISSING(get_physical_register_units);
  CHECK_MISSING(get_active_slot_count);
  CHECK_MISSING(get_active_slot_index);
  CHECK_MISSING(get_active_slot_name);
  CHECK_MISSING(get_candidate_register_count);
  CHECK_MISSING(get_candidate_physical_register);
  CHECK_MISSING(can_fallback_candidate);
  CHECK_MISSING(prefer_candidate_on_equal_next_use);
  CHECK_MISSING(is_candidate_allowed_for_physical_register);
  CHECK_MISSING(get_tac_clobbers);
  CHECK_MISSING(is_tac_transparent_for_physical_register);
  CHECK_MISSING(get_call_result_physical_register);
  CHECK_MISSING(get_call_boundary_spill_reason);
  CHECK_MISSING(get_stack_return_value_end_offset);
  CHECK_MISSING(get_return_value_byte_offset);
  CHECK_MISSING(get_call_frame_prefix_value);
  CHECK_MISSING(get_stack_argument_end_offset);
  CHECK_MISSING(get_argument_transport);
  CHECK_MISSING(get_argument_byte_offset);
  CHECK_MISSING(get_location_kind);
  CHECK_MISSING(get_address_materialization_mode);
  CHECK_MISSING(can_preserve_split_spill);
  CHECK_MISSING(get_split_reload_physical_register);

#undef CHECK_MISSING

  return 0;
}