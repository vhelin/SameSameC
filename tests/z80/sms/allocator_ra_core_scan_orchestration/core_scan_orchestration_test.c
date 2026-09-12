#include <stdio.h>
#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

struct fact_context {
  int allowed;
  int path_safe;
  int active;
  int invalid_fact;
  int allowed_calls;
  int path_calls;
  int active_calls;
};

static int g_order[5];
static int g_order_count;
static int g_observer_calls;
static int g_observer_valid;
static int g_failure_stage;
static int g_retain_value;
static int g_preference_mode;
static int g_preference_calls;
static int g_metadata_mode;
static int g_metadata_calls;
static int g_selector_mode;
static int g_selector_calls;
static int g_selector_check_path;
static int g_selector_check_overlap;
static char g_selector_reason[32];
static int g_preservation_mode;
static int g_preservation_calls;
static int g_slot_policy_mode;

static void record_order(int value) {

  if (g_order_count < (int)(sizeof(g_order) / sizeof(g_order[0])))
    g_order[g_order_count] = value;
  g_order_count++;
}
static int g_slot_count_calls;
static int g_slot_index_calls;
static int g_slot_name_calls;

static int get_active_slot_count(void) {

  g_slot_count_calls++;
  if (g_slot_policy_mode == 1)
    return 0;
  if (g_slot_policy_mode == 2)
    return 3;
  return 2;
}

static int get_active_slot_index(int physical_register) {

  g_slot_index_calls++;
  if (g_slot_policy_mode == 3)
    return -1;
  if (physical_register == 10)
    return 0;
  if (physical_register == 20)
    return 1;
  return 2;
}

static char *get_active_slot_name(int physical_register) {

  g_slot_name_calls++;
  if (g_slot_policy_mode == 4)
    return NULL;
  if (g_slot_policy_mode == 5)
    return "";
  return physical_register == 10 ? "primary" : "alternate";
}

static void reset_slot_policy(int mode) {

  g_slot_policy_mode = mode;
  g_slot_count_calls = 0;
  g_slot_index_calls = 0;
  g_slot_name_calls = 0;
}

static int can_fallback_candidate(int size, int reason) {

  if (reason == RA_CANDIDATE_FALLBACK_UNSUPPORTED)
    return size == 16 ? YES : NO;
  if (reason == RA_CANDIDATE_FALLBACK_PATH)
    return size == 8 ? YES : NO;
  if (reason == RA_CANDIDATE_FALLBACK_CONFLICT)
    return YES;
  return 2;
}

static int can_preserve_split_spill(int consumer_op, int consumer_arg,
    int size, int physical_register) {

  g_preservation_calls++;
  if (g_preservation_mode == 2)
    return 2;
  if (g_preservation_mode == 1)
    return NO;
  return consumer_op == 40 && consumer_arg == 1 && size == 16 &&
      physical_register == 10 ? YES : NO;
}

static int prefer_equal_next_use(int consumer_op, int candidate_operand,
    int active_operand, int physical_register) {

  g_preference_calls++;
  if (g_preference_mode == 1)
    return 2;
  if (consumer_op == 40 && candidate_operand == 1 && active_operand == 2 &&
      physical_register == 10)
    return YES;
  return NO;
}

static int has_temp_metadata(void *context, int temp_index) {

  (void)context;
  g_metadata_calls++;
  if (g_metadata_mode == 2)
    return 2;
  if (g_metadata_mode == 1)
    return NO;
  return temp_index == 4 ? YES : NO;
}

static int select_alternate(void *context, char *reason, int check_path, int check_overlap,
    int *selected_physical_register, int *selected_candidate_index) {

  (void)context;
  g_selector_calls++;
  g_selector_check_path = check_path;
  g_selector_check_overlap = check_overlap;
  strcpy(g_selector_reason, reason);
  if (g_selector_mode == 1)
    return SUCCEEDED;
  if (g_selector_mode == 2)
    return FAILED;
  if (g_selector_mode == 3)
    return 2;
  if (g_selector_mode == 4) {
    *selected_candidate_index = 1;
    return SUCCEEDED;
  }
  *selected_physical_register = 20;
  *selected_candidate_index = 1;
  return SUCCEEDED;
}

static void reset_selector(int mode) {

  g_selector_mode = mode;
  g_selector_calls = 0;
  g_selector_check_path = -1;
  g_selector_check_overlap = -1;
  g_selector_reason[0] = '\0';
}

static void initialize_policy(struct register_allocator_target_policy *policy) {

  memset(policy, 0, sizeof(*policy));
  policy->name = "orchestration";
  policy->can_fallback_candidate = can_fallback_candidate;
  policy->prefer_candidate_on_equal_next_use = prefer_equal_next_use;
  policy->can_preserve_split_spill = can_preserve_split_spill;
  policy->get_active_slot_count = get_active_slot_count;
  policy->get_active_slot_index = get_active_slot_index;
  policy->get_active_slot_name = get_active_slot_name;
}

static int is_allowed(void *context, int physical_register) {

  struct fact_context *facts;

  (void)physical_register;
  facts = (struct fact_context *)context;
  facts->allowed_calls++;
  return facts->invalid_fact == 1 ? 2 : facts->allowed;
}

static int is_path_safe(void *context, int physical_register) {

  struct fact_context *facts;

  (void)physical_register;
  facts = (struct fact_context *)context;
  facts->path_calls++;
  return facts->invalid_fact == 2 ? 2 : facts->path_safe;
}

static int is_active(void *context, int physical_register) {

  struct fact_context *facts;

  (void)physical_register;
  facts = (struct fact_context *)context;
  facts->active_calls++;
  return facts->invalid_fact == 3 ? 2 : facts->active;
}

static void seed_facts(struct fact_context *facts, int allowed, int path_safe, int active) {

  memset(facts, 0, sizeof(*facts));
  facts->allowed = allowed;
  facts->path_safe = path_safe;
  facts->active = active;
}

static int expect_decision(struct register_allocator_target_policy *policy,
    struct fact_context *facts, int size, int candidate_count, int expected_plan,
    int expected_path_calls) {

  struct register_allocator_primary_candidate_decision decision;

  memset(&decision, 7, sizeof(decision));
  if (register_allocator_decide_primary_candidate("coreDecision", 2, policy, size,
      candidate_count, 10, -1, facts, is_allowed, is_path_safe, is_active,
      &decision) == FAILED)
    return FAILED;
  if (decision.plan != expected_plan || decision.evaluation.physical_register != 10 ||
      decision.evaluation.allowed != facts->allowed ||
      decision.evaluation.path_safe != (facts->allowed == YES ? facts->path_safe : NO) ||
      decision.evaluation.active != facts->active || facts->allowed_calls != 1 ||
      facts->path_calls != expected_path_calls || facts->active_calls != 1)
    return FAILED;
  return SUCCEEDED;
}

static int decision_is_clear(struct register_allocator_primary_candidate_decision *decision) {

  return decision->evaluation.physical_register == -1 &&
      decision->evaluation.allowed == NO && decision->evaluation.path_safe == NO &&
      decision->evaluation.active == NO && decision->fallback_unsupported == NO &&
      decision->fallback_path == NO && decision->fallback_conflict == NO &&
      decision->plan == FAILED;
}

static void reset_callbacks(void) {

  g_order_count = 0;
  g_observer_calls = 0;
  g_observer_valid = YES;
  g_order[0] = 0;
  g_order[1] = 0;
  g_order[2] = 0;
  g_order[3] = 0;
  g_order[4] = 0;
  g_failure_stage = 0;
  g_retain_value = YES;
}

static void observe_transition_preparation(void *context,
    struct register_allocator_candidate_transition_preparation *preparation) {

  (void)context;
  g_observer_calls++;
  if (preparation->slot_index < 0 || preparation->slot_name == NULL ||
      preparation->plan.linear_scan_decision == FAILED)
    g_observer_valid = NO;
  record_order(5);
}

static int clear_interval(void *context, int producer_instruction,
    int consumer_instruction, int consumer_operand) {

  (void)context;
  if (producer_instruction != 3 || consumer_instruction != 20 || consumer_operand != 2)
    return FAILED;
  record_order(1);
  return g_failure_stage == 1 ? FAILED : SUCCEEDED;
}

static int spill_temp(void *context, int temp_index) {

  (void)context;
  if (temp_index != 4)
    return FAILED;
  record_order(2);
  return g_failure_stage == 2 ? FAILED : SUCCEEDED;
}

static int retain_candidate(void *context, int temp_index, int physical_register,
    int producer_instruction, int consumer_instruction, int consumer_operand,
    int *retained_interval) {

  (void)context;
  if (temp_index < 5 || physical_register != 10 || producer_instruction != 8 ||
      consumer_instruction != 12 || consumer_operand != 1)
    return FAILED;
  record_order(3);
  *retained_interval = g_retain_value;
  return g_failure_stage == 3 ? FAILED : SUCCEEDED;
}

static int apply_spill_mutation(void *context, int instruction_index, int temp_index,
    int operand, int physical_register) {

  (void)context;
  if (instruction_index != 12 || temp_index < 5 || operand != 1 || physical_register != 10)
    return FAILED;
  record_order(4);
  return g_failure_stage == 4 ? FAILED : SUCCEEDED;
}

static int expect_slot(struct register_allocator_active_slot *slot, int temp_index,
    int next_use, int producer_instruction, int consumer_operand, int retained) {

  return slot->temp_index == temp_index && slot->next_use == next_use &&
      slot->producer_instruction == producer_instruction &&
      slot->consumer_operand == consumer_operand &&
      slot->interval_retained == retained ? SUCCEEDED : FAILED;
}

static int test_primary_decisions(void) {

  struct register_allocator_target_policy policy;
  struct register_allocator_primary_candidate_decision decision;
  struct fact_context facts;

  initialize_policy(&policy);

  seed_facts(&facts, YES, YES, NO);
  if (expect_decision(&policy, &facts, 8, 3, RA_CANDIDATE_PLAN_USE_PRIMARY, 1) == FAILED)
    return 1;
  seed_facts(&facts, NO, YES, NO);
  if (expect_decision(&policy, &facts, 16, 2, RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED, 0) == FAILED)
    return 2;
  seed_facts(&facts, NO, YES, NO);
  if (expect_decision(&policy, &facts, 8, 3, RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED, 0) == FAILED)
    return 3;
  seed_facts(&facts, YES, NO, NO);
  if (expect_decision(&policy, &facts, 8, 3, RA_CANDIDATE_PLAN_FALLBACK_PATH, 1) == FAILED)
    return 4;
  seed_facts(&facts, YES, NO, NO);
  if (expect_decision(&policy, &facts, 16, 2, RA_CANDIDATE_PLAN_REJECT_PATH, 1) == FAILED)
    return 5;
  seed_facts(&facts, YES, YES, YES);
  if (expect_decision(&policy, &facts, 16, 2, RA_CANDIDATE_PLAN_FALLBACK_CONFLICT, 1) == FAILED)
    return 6;

  seed_facts(&facts, YES, YES, NO);
  facts.invalid_fact = 1;
  memset(&decision, 7, sizeof(decision));
  if (register_allocator_decide_primary_candidate("badAllowed", 0, &policy, 8, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      decision_is_clear(&decision) == NO)
    return 7;
  seed_facts(&facts, YES, YES, NO);
  facts.invalid_fact = 2;
  if (register_allocator_decide_primary_candidate("badPath", 0, &policy, 8, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      decision_is_clear(&decision) == NO)
    return 8;
  seed_facts(&facts, YES, YES, NO);
  facts.invalid_fact = 3;
  if (register_allocator_decide_primary_candidate("badActive", 0, &policy, 8, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      decision_is_clear(&decision) == NO)
    return 9;

  seed_facts(&facts, YES, YES, NO);
  if (register_allocator_decide_primary_candidate(NULL, 0, &policy, 8, 3, 10, -1,
      &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badBlock", -1, &policy, 8, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badPolicy", 0, NULL, 8, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badSize", 0, &policy, 0, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badCount", 0, &policy, 8, 0,
      10, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badPhysical", 0, &policy, 8, 3,
      -1, -1, &facts, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badContext", 0, &policy, 8, 3,
      10, -1, NULL, is_allowed, is_path_safe, is_active, &decision) != FAILED ||
      register_allocator_decide_primary_candidate("badOutput", 0, &policy, 8, 3,
      10, -1, &facts, is_allowed, is_path_safe, is_active, NULL) != FAILED)
    return 10;

  return 0;
}

static int test_slot_transitions(void) {

  struct register_allocator_active_slot slots[1];
  struct register_allocator_slot_transition transition;
  int retained;

  if (register_allocator_reset_active_slots("coreTransition", 1, slots, 1) == FAILED)
    return 1;
  if (register_allocator_plan_slot_transition("coreAssign", 1, RA_LINEAR_SCAN_ASSIGN,
      &slots[0], 5, 12, 8, 1, &transition) == FAILED ||
      transition.retain_candidate != YES || transition.assign_candidate != YES ||
      transition.spill_displaced_temp != NO)
    return 2;
  reset_callbacks();
  if (register_allocator_apply_slot_transition("coreAssign", 1, 0, slots, 1, 10, -1,
      5, 12, 8, 1, NULL, &transition, NULL, NULL, retain_candidate, &retained) == FAILED ||
      retained != YES || g_order_count != 1 || g_order[0] != 3 ||
      expect_slot(&slots[0], 5, 12, 8, 1, YES) == FAILED)
    return 3;

  if (register_allocator_plan_slot_transition("coreKeep", 1, RA_LINEAR_SCAN_KEEP_ACTIVE,
      &slots[0], 6, 12, 8, 1, &transition) == FAILED)
    return 4;
  reset_callbacks();
  retained = 7;
  if (register_allocator_apply_slot_transition("coreKeep", 1, 0, slots, 1, 10, -1,
      6, 12, 8, 1, NULL, &transition, NULL, NULL, NULL, &retained) == FAILED ||
      retained != NO || g_order_count != 0 ||
      expect_slot(&slots[0], 5, 12, 8, 1, YES) == FAILED)
    return 5;

  if (register_allocator_assign_active_slot("coreTransition", 1, 0, slots, 1,
      4, 20, 3, 2, YES) == FAILED ||
      register_allocator_plan_slot_transition("coreReplace", 1,
      RA_LINEAR_SCAN_REPLACE_ACTIVE, &slots[0], 6, 12, 8, 1, &transition) == FAILED ||
      transition.clear_displaced_interval != YES || transition.displaced_temp != 4 ||
      transition.displaced_producer_instruction != 3 ||
      transition.displaced_consumer_instruction != 20 ||
      transition.displaced_consumer_operand != 2)
    return 6;
  reset_callbacks();
  if (register_allocator_apply_slot_transition("coreReplace", 1, 0, slots, 1, 10, -1,
      6, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) == FAILED || retained != YES || g_order_count != 3 ||
      g_order[0] != 1 || g_order[1] != 2 || g_order[2] != 3 ||
      expect_slot(&slots[0], 6, 12, 8, 1, YES) == FAILED)
    return 7;
  reset_callbacks();
  if (register_allocator_apply_slot_transition("doubleApply", 1, 0, slots, 1, 10, -1,
      6, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) != FAILED || retained != NO || g_order_count != 0)
    return 40;

  if (register_allocator_assign_active_slot("coreTransition", 1, 0, slots, 1,
      4, 20, 3, 2, NO) == FAILED ||
      register_allocator_plan_slot_transition("coreReplaceNoClear", 1,
      RA_LINEAR_SCAN_REPLACE_ACTIVE, &slots[0], 7, 12, 8, 1, &transition) == FAILED)
    return 8;
  reset_callbacks();
  if (register_allocator_apply_slot_transition("coreReplaceNoClear", 1, 0, slots, 1,
      10, -1, 7, 12, 8, 1, NULL, &transition, NULL, spill_temp, retain_candidate,
      &retained) == FAILED || g_order_count != 2 || g_order[0] != 2 || g_order[1] != 3)
    return 9;

  if (register_allocator_assign_active_slot("coreTransition", 1, 0, slots, 1,
      4, 20, 3, 2, YES) == FAILED ||
      register_allocator_plan_slot_transition("coreCallbackFailure", 1,
      RA_LINEAR_SCAN_REPLACE_ACTIVE, &slots[0], 8, 12, 8, 1, &transition) == FAILED)
    return 10;
  reset_callbacks();
  g_failure_stage = 1;
  retained = 7;
  if (register_allocator_apply_slot_transition("badClear", 1, 0, slots, 1, 10, -1,
      8, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) != FAILED || retained != NO || g_order_count != 1)
    return 11;
  reset_callbacks();
  g_failure_stage = 2;
  if (register_allocator_apply_slot_transition("badSpill", 1, 0, slots, 1, 10, -1,
      8, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) != FAILED || g_order_count != 2)
    return 12;
  reset_callbacks();
  g_failure_stage = 3;
  if (register_allocator_apply_slot_transition("badRetain", 1, 0, slots, 1, 10, -1,
      8, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) != FAILED || g_order_count != 3)
    return 13;
  reset_callbacks();
  g_retain_value = 2;
  if (register_allocator_apply_slot_transition("badRetainValue", 1, 0, slots, 1, 10, -1,
      8, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) != FAILED || retained != NO)
    return 14;

  if (register_allocator_plan_slot_transition("staleTransition", 1,
      RA_LINEAR_SCAN_REPLACE_ACTIVE, &slots[0], 8, 12, 8, 1, &transition) == FAILED)
    return 15;
  slots[0].next_use = 21;
  reset_callbacks();
  if (register_allocator_apply_slot_transition("staleTransition", 1, 0, slots, 1,
      10, -1, 8, 12, 8, 1, NULL, &transition, clear_interval, spill_temp,
      retain_candidate, &retained) != FAILED || g_order_count != 0 || retained != NO)
    return 16;
  slots[0].next_use = 20;

  slots[0].temp_index = -1;
  if (register_allocator_plan_slot_transition("badSlotState", 1,
      RA_LINEAR_SCAN_KEEP_ACTIVE, &slots[0], 8, 12, 8, 1, &transition) != FAILED ||
      transition.decision != FAILED)
    return 17;
  slots[0].temp_index = 4;

  memset(&transition, 7, sizeof(transition));
  if (register_allocator_apply_slot_transition("badTransition", 1, 0, slots, 1, 10, -1,
      8, 12, 8, 1, NULL, &transition, clear_interval, spill_temp, retain_candidate,
      &retained) != FAILED)
    return 18;
  if (register_allocator_plan_slot_transition(NULL, 1, RA_LINEAR_SCAN_ASSIGN, &slots[0],
      8, 12, 8, 1, &transition) != FAILED || transition.decision != FAILED ||
      register_allocator_plan_slot_transition("badDecision", 1, 99, &slots[0],
      8, 12, 8, 1, &transition) != FAILED ||
      register_allocator_plan_slot_transition("badSlot", 1, RA_LINEAR_SCAN_ASSIGN, NULL,
      8, 12, 8, 1, &transition) != FAILED ||
      register_allocator_plan_slot_transition("badCandidate", 1, RA_LINEAR_SCAN_ASSIGN,
      &slots[0], -1, 12, 8, 1, &transition) != FAILED ||
      register_allocator_plan_slot_transition("badRange", 1, RA_LINEAR_SCAN_ASSIGN,
      &slots[0], 8, 8, 8, 1, &transition) != FAILED ||
      register_allocator_plan_slot_transition("badOutput", 1, RA_LINEAR_SCAN_ASSIGN,
      &slots[0], 8, 12, 8, 1, NULL) != FAILED)
    return 19;

  return 0;
}

static int candidate_transition_plan_is_clear(
    struct register_allocator_candidate_transition_plan *plan) {

  return plan->active_temp == -1 && plan->active_next_use == -1 &&
      plan->active_replaceable == NO && plan->prefer_candidate_on_equal == NO &&
      plan->linear_scan_decision == FAILED &&
      plan->transition.decision == FAILED && plan->transition.clear_displaced_interval == NO &&
      plan->transition.spill_displaced_temp == NO && plan->transition.retain_candidate == NO &&
      plan->transition.assign_candidate == NO && plan->transition.displaced_temp == -1 &&
      plan->transition.displaced_producer_instruction == -1 &&
      plan->transition.displaced_consumer_instruction == -1 &&
      plan->transition.displaced_consumer_operand == -1;
}

static int candidate_transition_preparation_is_clear(
    struct register_allocator_candidate_transition_preparation *preparation) {

  return preparation->slot_index == -1 && preparation->slot_name == NULL &&
      candidate_transition_plan_is_clear(&preparation->plan) == YES;
}

static int test_candidate_transition_preparations(void) {

  struct register_allocator_active_slot slots[2];
  struct register_allocator_candidate_transition_preparation preparation;
  struct register_allocator_target_policy policy;

  initialize_policy(&policy);
  if (register_allocator_reset_active_slots("coreTransitionPrepare", 2, slots, 2) == FAILED)
    return 1;

  reset_slot_policy(0);
  g_metadata_calls = 0;
  g_preference_calls = 0;
  if (register_allocator_prepare_candidate_transition("coreTransitionPreparePrimary", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) == FAILED || preparation.slot_index != 0 ||
      strcmp(preparation.slot_name, "primary") != 0 ||
      preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_ASSIGN ||
      preparation.plan.transition.assign_candidate != YES ||
      g_slot_count_calls != 1 || g_slot_index_calls != 1 || g_slot_name_calls != 1 ||
      g_metadata_calls != 0 || g_preference_calls != 0)
    return 2;

  reset_slot_policy(0);
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareAlternate", 2,
      8, 5, 12, 40, 1, 20, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) == FAILED || preparation.slot_index != 1 ||
      strcmp(preparation.slot_name, "alternate") != 0 ||
      preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_ASSIGN)
    return 3;

  reset_slot_policy(1);
  memset(&preparation, 7, sizeof(preparation));
  g_metadata_calls = 0;
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareBadCount", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO ||
      g_slot_count_calls != 1 || g_slot_index_calls != 1 || g_slot_name_calls != 1 ||
      g_metadata_calls != 0)
    return 4;

  reset_slot_policy(2);
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareCountMismatch", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO)
    return 5;

  reset_slot_policy(3);
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareBadIndex", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO)
    return 6;

  reset_slot_policy(4);
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareNullName", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO)
    return 7;

  reset_slot_policy(5);
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareEmptyName", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO)
    return 8;

  reset_slot_policy(0);
  if (register_allocator_assign_active_slot("coreTransitionPrepare", 2, 0, slots, 2,
      4, 20, 3, 2, YES) == FAILED)
    return 9;
  g_metadata_mode = 2;
  g_metadata_calls = 0;
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareBadMetadata", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO ||
      g_metadata_calls != 1)
    return 10;
  g_metadata_mode = 0;

  policy.get_active_slot_index = NULL;
  reset_slot_policy(0);
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareNoHook", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, &preparation) != FAILED ||
      candidate_transition_preparation_is_clear(&preparation) == NO ||
      g_slot_count_calls != 0 || g_slot_index_calls != 0 || g_slot_name_calls != 0)
    return 11;
  policy.get_active_slot_index = get_active_slot_index;

  if (register_allocator_prepare_candidate_transition("coreTransitionPrepareNoOutput", 2,
      8, 5, 12, 40, 1, 10, -1, slots, 2, &policy, &policy,
      has_temp_metadata, NULL) != FAILED)
    return 12;

  return 0;
}

static int expect_candidate_transition_failure(char *function_name, int block_index,
    char *slot_name, int instruction_index, int candidate_temp, int candidate_next_use,
  int consumer_op, int candidate_operand, int physical_register,
    struct register_allocator_active_slot *active_slot,
  struct register_allocator_target_policy *policy, void *context,
  register_allocator_temp_predicate metadata_callback,
    struct register_allocator_candidate_transition_plan *plan) {

  memset(plan, 7, sizeof(*plan));
  if (register_allocator_plan_candidate_transition(function_name, block_index, slot_name,
      instruction_index, candidate_temp, candidate_next_use, consumer_op, candidate_operand,
      physical_register, -1, active_slot, policy, context, metadata_callback, plan) != FAILED)
    return FAILED;
  return candidate_transition_plan_is_clear(plan) == YES ? SUCCEEDED : FAILED;
}

static int expect_active_replaceability_failure(char *function_name, int block_index,
    int instruction_index, struct register_allocator_active_slot *active_slot, void *context,
    register_allocator_temp_predicate metadata_callback) {

  int active_replaceable;

  active_replaceable = 7;
  if (register_allocator_resolve_active_replaceability(function_name, block_index,
      instruction_index, active_slot, context, metadata_callback,
      &active_replaceable) != FAILED)
    return FAILED;
  return active_replaceable == NO ? SUCCEEDED : FAILED;
}

static int test_active_replaceability(void) {

  struct register_allocator_active_slot slot;
  int active_replaceable;
  int context;

  context = 1;
  if (register_allocator_reset_active_slots("coreActiveReplaceability", 2, &slot, 1) == FAILED)
    return 1;

  g_metadata_calls = 0;
  if (register_allocator_resolve_active_replaceability("coreActiveReplaceabilityEmpty", 2,
      8, &slot, &context, has_temp_metadata, &active_replaceable) == FAILED ||
      active_replaceable != YES || g_metadata_calls != 0)
    return 2;

  if (register_allocator_assign_active_slot("coreActiveReplaceability", 2, 0, &slot, 1,
      4, 8, 3, 2, YES) == FAILED)
    return 3;
  g_metadata_calls = 0;
  if (register_allocator_resolve_active_replaceability("coreActiveReplaceabilityExpired", 2,
      8, &slot, &context, has_temp_metadata, &active_replaceable) == FAILED ||
      active_replaceable != YES || g_metadata_calls != 0)
    return 4;

  slot.next_use = 20;
  g_metadata_mode = 0;
  g_metadata_calls = 0;
  if (register_allocator_resolve_active_replaceability("coreActiveReplaceabilityAvailable", 2,
      8, &slot, &context, has_temp_metadata, &active_replaceable) == FAILED ||
      active_replaceable != YES || g_metadata_calls != 1)
    return 5;

  g_metadata_mode = 1;
  g_metadata_calls = 0;
  if (register_allocator_resolve_active_replaceability("coreActiveReplaceabilityMissing", 2,
      8, &slot, &context, has_temp_metadata, &active_replaceable) == FAILED ||
      active_replaceable != NO || g_metadata_calls != 1)
    return 6;

  g_metadata_mode = 2;
  g_metadata_calls = 0;
  if (expect_active_replaceability_failure("coreActiveReplaceabilityBadCallback", 2, 8,
      &slot, &context, has_temp_metadata) == FAILED || g_metadata_calls != 1)
    return 7;
  g_metadata_mode = 0;

  slot.interval_retained = 2;
  if (expect_active_replaceability_failure("coreActiveReplaceabilityBadSlot", 2, 8,
      &slot, &context, has_temp_metadata) == FAILED)
    return 8;
  slot.interval_retained = YES;

  if (expect_active_replaceability_failure(NULL, 2, 8, &slot, &context,
      has_temp_metadata) == FAILED ||
      expect_active_replaceability_failure("coreActiveReplaceabilityBadBlock", -1, 8,
      &slot, &context, has_temp_metadata) == FAILED ||
      expect_active_replaceability_failure("coreActiveReplaceabilityBadInstruction", 2, -1,
      &slot, &context, has_temp_metadata) == FAILED ||
      expect_active_replaceability_failure("coreActiveReplaceabilityNoSlot", 2, 8,
      NULL, &context, has_temp_metadata) == FAILED ||
      expect_active_replaceability_failure("coreActiveReplaceabilityNoContext", 2, 8,
      &slot, NULL, has_temp_metadata) == FAILED ||
      expect_active_replaceability_failure("coreActiveReplaceabilityNoCallback", 2, 8,
      &slot, &context, NULL) == FAILED)
    return 9;

  active_replaceable = 7;
  if (register_allocator_resolve_active_replaceability("coreActiveReplaceabilityNoOutput", 2,
      8, &slot, &context, has_temp_metadata, NULL) != FAILED)
    return 10;

  return 0;
}

static int test_candidate_transition_plans(void) {

  struct register_allocator_active_slot slot;
  struct register_allocator_candidate_transition_plan plan;
  struct register_allocator_target_policy policy;

  initialize_policy(&policy);
  if (register_allocator_reset_active_slots("coreCandidateTransition", 2, &slot, 1) == FAILED)
    return 1;

  g_preference_calls = 0;
  g_metadata_calls = 0;
  if (register_allocator_plan_candidate_transition("coreCandidateTransitionAssign", 2, "A", 8,
      5, 12, 40, 1, 10, -1, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      plan.active_replaceable != YES ||
      plan.prefer_candidate_on_equal != NO ||
      plan.linear_scan_decision != RA_LINEAR_SCAN_ASSIGN ||
      plan.transition.retain_candidate != YES || plan.transition.assign_candidate != YES ||
      g_preference_calls != 0 || g_metadata_calls != 0)
    return 2;

  if (register_allocator_assign_active_slot("coreCandidateTransition", 2, 0, &slot, 1,
      4, 20, 3, 2, YES) == FAILED)
    return 3;
  g_preference_calls = 0;
    g_metadata_calls = 0;
  if (register_allocator_plan_candidate_transition("coreCandidateTransitionNearer", 2, "HL", 8,
      5, 12, 40, 1, 10, -1, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      plan.active_replaceable != YES ||
      plan.prefer_candidate_on_equal != NO ||
      plan.linear_scan_decision != RA_LINEAR_SCAN_REPLACE_ACTIVE ||
      plan.transition.displaced_temp != 4 || plan.transition.clear_displaced_interval != YES ||
      plan.transition.spill_displaced_temp != YES || g_preference_calls != 0 ||
      g_metadata_calls != 1)
    return 4;

  g_preference_calls = 0;
  g_metadata_calls = 0;
  if (register_allocator_plan_candidate_transition("coreCandidateTransitionEqualPreferred", 2,
      "A", 8, 5, 20, 40, 1, 10, -1, &slot, &policy, &policy,
      has_temp_metadata, &plan) == FAILED || plan.active_replaceable != YES ||
      plan.prefer_candidate_on_equal != YES ||
      plan.linear_scan_decision != RA_LINEAR_SCAN_REPLACE_ACTIVE || g_preference_calls != 1 ||
      g_metadata_calls != 1)
    return 5;

  g_preference_calls = 0;
  g_metadata_calls = 0;
  if (register_allocator_plan_candidate_transition("coreCandidateTransitionEqualKept", 2, "A",
      8, 5, 20, 40, 2, 10, -1, &slot, &policy, &policy,
      has_temp_metadata, &plan) == FAILED || plan.active_replaceable != YES ||
      plan.prefer_candidate_on_equal != NO ||
      plan.linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE ||
      plan.transition.retain_candidate != NO || g_preference_calls != 1 ||
      g_metadata_calls != 1)
    return 6;

  g_preference_calls = 0;
  g_metadata_calls = 0;
  if (register_allocator_plan_candidate_transition("coreCandidateTransitionActiveNearer", 2,
      "A", 8, 5, 22, 40, 1, 10, -1, &slot, &policy, &policy,
      has_temp_metadata, &plan) == FAILED || plan.active_replaceable != YES ||
      plan.linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE || g_preference_calls != 0 ||
      g_metadata_calls != 1)
    return 7;

  g_metadata_mode = 1;
  g_preference_calls = 0;
  g_metadata_calls = 0;
  if (register_allocator_plan_candidate_transition("coreCandidateTransitionUnavailable", 2,
      "A", 8, 5, 12, 40, 1, 10, -1, &slot, &policy, &policy,
      has_temp_metadata, &plan) == FAILED || plan.active_replaceable != NO ||
      plan.linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE || g_preference_calls != 0 ||
      g_metadata_calls != 1)
    return 8;
  g_metadata_mode = 0;

  g_preference_mode = 1;
  g_preference_calls = 0;
  g_metadata_calls = 0;
  if (expect_candidate_transition_failure("coreCandidateTransitionBadPreference", 2, "A", 8,
      5, 20, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      g_preference_calls != 1 || g_metadata_calls != 1)
    return 9;
  g_preference_mode = 0;

  g_metadata_mode = 2;
  g_metadata_calls = 0;
  g_preference_calls = 0;
  if (expect_candidate_transition_failure("coreCandidateTransitionBadMetadata", 2, "A", 8,
      5, 12, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      g_metadata_calls != 1 || g_preference_calls != 0)
    return 10;
  g_metadata_mode = 0;

  slot.consumer_operand = -1;
  if (expect_candidate_transition_failure("coreCandidateTransitionBadSlot", 2, "A", 8,
      5, 12, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED)
    return 11;
  slot.consumer_operand = 2;

  if (expect_candidate_transition_failure(NULL, 2, "A", 8, 5, 12, 40, 1, 10,
      &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadBlock", -1, "A", 8,
      5, 12, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadSlotName", 2, "", 8,
      5, 12, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadInstruction", 2, "A", -1,
      5, 12, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadCandidate", 2, "A", 8,
      -1, 12, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadRange", 2, "A", 8,
      5, 8, 40, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadOp", 2, "A", 8,
      5, 12, -1, 1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadOperand", 2, "A", 8,
      5, 12, 40, -1, 10, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionBadPhysical", 2, "A", 8,
      5, 12, 40, 1, -1, &slot, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionNoSlot", 2, "A", 8,
      5, 12, 40, 1, 10, NULL, &policy, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionNoPolicy", 2, "A", 8,
      5, 12, 40, 1, 10, &slot, NULL, &policy, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionNoContext", 2, "A", 8,
      5, 12, 40, 1, 10, &slot, &policy, NULL, has_temp_metadata, &plan) == FAILED ||
      expect_candidate_transition_failure("coreCandidateTransitionNoMetadata", 2, "A", 8,
      5, 12, 40, 1, 10, &slot, &policy, &policy, NULL, &plan) == FAILED)
    return 12;

  if (register_allocator_plan_candidate_transition("coreCandidateTransitionNoOutput", 2, "A", 8,
      5, 12, 40, 1, 10, -1, &slot, &policy, &policy, has_temp_metadata, NULL) != FAILED)
    return 13;

  return 0;
}

static int candidate_transition_application_is_clear(
    struct register_allocator_candidate_transition_application *application) {

  return application->retained_interval == NO && application->spill_mutation.apply == NO &&
      application->spill_mutation.instruction == -1 &&
      application->spill_mutation.temp_index == -1 &&
      application->spill_mutation.operand == -1 &&
      application->spill_mutation.physical_register == -1;
}

static int candidate_selection_finalization_is_clear(
    struct register_allocator_candidate_selection_finalization *finalization) {

  return finalization->proceed == NO && finalization->decision.plan == FAILED &&
      finalization->decision.evaluation.physical_register == -1 &&
      finalization->decision.evaluation.allowed == NO &&
      finalization->decision.evaluation.path_safe == NO &&
      finalization->decision.evaluation.active == NO &&
      finalization->decision.fallback_unsupported == NO &&
      finalization->decision.fallback_path == NO &&
      finalization->decision.fallback_conflict == NO &&
      finalization->dispatch.action == FAILED &&
      finalization->dispatch.physical_register == -1 &&
      finalization->dispatch.candidate_index == -1 &&
      finalization->dispatch.check_path == NO &&
      finalization->dispatch.check_overlap == NO && finalization->split_action == FAILED;
}

static int candidate_selection_preparation_is_clear(
    struct register_allocator_candidate_selection_preparation *preparation) {

  return preparation->proceed == NO && preparation->interval_blocked == NO &&
      preparation->preservation_queried == NO &&
      preparation->preservation_supported == NO &&
      preparation->preservation_physical_register == -1;
}

static int candidate_selection_orchestration_is_clear(
    struct register_allocator_candidate_selection_orchestration *orchestration) {

  return orchestration->proceed == NO &&
      candidate_selection_preparation_is_clear(&orchestration->preparation) == YES &&
      candidate_selection_finalization_is_clear(&orchestration->finalization) == YES;
}

static int test_candidate_selection_orchestrations(void) {

  struct register_allocator_candidate candidate;
  struct register_allocator_candidate_selection_orchestration orchestration;
  struct register_allocator_target_policy policy;
  struct fact_context facts;

  initialize_policy(&policy);
  candidate.found = YES;
  candidate.temp_index = 5;
  candidate.producer_instruction = 8;
  candidate.consumer_instruction = 12;
  candidate.consumer_operand = 1;
  candidate.has_single_read = YES;

  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  g_preservation_mode = 0;
  g_preservation_calls = 0;
  if (register_allocator_orchestrate_candidate_selection("coreOrchestratePrimary", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) == FAILED || orchestration.proceed != YES ||
      orchestration.preparation.proceed != YES ||
      orchestration.preparation.preservation_queried != NO ||
      orchestration.finalization.proceed != YES ||
      orchestration.finalization.dispatch.physical_register != 10 ||
      orchestration.finalization.split_action != RA_SPLIT_ACTION_NONE ||
      g_preservation_calls != 0 || facts.allowed_calls != 1 || facts.path_calls != 1 ||
      facts.active_calls != 1 || g_selector_calls != 0)
    return 1;

  candidate.has_single_read = NO;
  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  g_preservation_calls = 0;
  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateBlocked", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 4, 0, 9, 10, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) == FAILED || orchestration.proceed != NO ||
      orchestration.preparation.interval_blocked != YES ||
      candidate_selection_finalization_is_clear(&orchestration.finalization) == NO ||
      g_preservation_calls != 0 || facts.allowed_calls != 0 || facts.path_calls != 0 ||
      facts.active_calls != 0 || g_selector_calls != 0)
    return 2;

  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  g_preservation_calls = 0;
  if (register_allocator_orchestrate_candidate_selection("coreOrchestratePreserve", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) == FAILED || orchestration.proceed != YES ||
      orchestration.preparation.preservation_queried != YES ||
      orchestration.preparation.preservation_supported != YES ||
      orchestration.finalization.split_action != RA_SPLIT_ACTION_PRESERVE ||
      g_preservation_calls != 1 || facts.allowed_calls != 1 || g_selector_calls != 0)
    return 3;

  g_preservation_mode = 1;
  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  g_preservation_calls = 0;
  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateUnsupported", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) == FAILED || orchestration.proceed != NO ||
      orchestration.preparation.preservation_supported != NO ||
      orchestration.finalization.split_action != RA_SPLIT_ACTION_REJECT_UNSUPPORTED ||
      g_preservation_calls != 1 || facts.allowed_calls != 1 || g_selector_calls != 0)
    return 4;
  g_preservation_mode = 0;

  seed_facts(&facts, NO, YES, NO);
  reset_selector(0);
  g_preservation_calls = 0;
  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateAlternate", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) == FAILED || orchestration.proceed != NO ||
      orchestration.finalization.dispatch.action != RA_CANDIDATE_DISPATCH_USE_ALTERNATE ||
      orchestration.finalization.dispatch.physical_register != 20 ||
      orchestration.finalization.split_action != RA_SPLIT_ACTION_REJECT_REGISTER ||
      g_preservation_calls != 1 || facts.allowed_calls != 1 || g_selector_calls != 1)
    return 5;

  g_preservation_mode = 2;
  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  g_preservation_calls = 0;
  memset(&orchestration, 7, sizeof(orchestration));
  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateBadPreserve", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) != FAILED ||
      candidate_selection_orchestration_is_clear(&orchestration) == NO ||
      g_preservation_calls != 1 || facts.allowed_calls != 0 || g_selector_calls != 0)
    return 6;
  g_preservation_mode = 0;

  candidate.has_single_read = YES;
  seed_facts(&facts, YES, YES, NO);
  facts.invalid_fact = 1;
  reset_selector(0);
  memset(&orchestration, 7, sizeof(orchestration));
  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateBadFact", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) != FAILED ||
      candidate_selection_orchestration_is_clear(&orchestration) == NO ||
      facts.allowed_calls != 1 || facts.path_calls != 0 || facts.active_calls != 0 ||
      g_selector_calls != 0)
    return 7;

  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  memset(&orchestration, 7, sizeof(orchestration));
  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateBadReasons", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 9, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate,
      &orchestration) != FAILED ||
      candidate_selection_orchestration_is_clear(&orchestration) == NO ||
      facts.allowed_calls != 0 || g_selector_calls != 0)
    return 8;

  if (register_allocator_orchestrate_candidate_selection("coreOrchestrateNoOutput", 3,
      &policy, &candidate, 40, 16, 2, 10, -1, 0, 0, 9, -1, &facts,
      is_allowed, is_path_safe, is_active, &facts, select_alternate, NULL) != FAILED)
    return 9;

  return 0;
}

static int test_candidate_selection_preparations(void) {

  struct register_allocator_candidate candidate;
  struct register_allocator_candidate_selection_preparation preparation;
  struct register_allocator_target_policy policy;

  initialize_policy(&policy);
  candidate.found = YES;
  candidate.temp_index = 5;
  candidate.producer_instruction = 8;
  candidate.consumer_instruction = 12;
  candidate.consumer_operand = 1;
  candidate.has_single_read = YES;

  g_preservation_mode = 0;
  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareSingle", 3, &policy,
      &candidate, 40, 16, 10, -1, 0, 0, 9, -1, &preparation) == FAILED ||
      preparation.proceed != YES || preparation.interval_blocked != NO ||
      preparation.preservation_queried != NO || preparation.preservation_supported != NO ||
      preparation.preservation_physical_register != -1 || g_preservation_calls != 0)
    return 1;

  candidate.has_single_read = NO;
  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareSupported", 3, &policy,
      &candidate, 40, 16, 10, -1, 0, 0, 9, -1, &preparation) == FAILED ||
      preparation.proceed != YES || preparation.interval_blocked != NO ||
      preparation.preservation_queried != YES || preparation.preservation_supported != YES ||
      preparation.preservation_physical_register != 10 || g_preservation_calls != 1)
    return 2;

  g_preservation_mode = 1;
  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareUnsupported", 3, &policy,
      &candidate, 40, 16, 10, -1, 0, 0, 9, -1, &preparation) == FAILED ||
      preparation.proceed != YES || preparation.preservation_queried != YES ||
      preparation.preservation_supported != NO ||
      preparation.preservation_physical_register != -1 || g_preservation_calls != 1)
    return 3;

  g_preservation_mode = 0;
  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareBoundaryBetween", 3,
      &policy, &candidate, 40, 16, 10, -1, 4, 0, 9, 10, &preparation) == FAILED ||
      preparation.proceed != NO || preparation.interval_blocked != YES ||
      preparation.preservation_queried != NO || g_preservation_calls != 0)
    return 4;

  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareBoundaryOutside", 3,
      &policy, &candidate, 40, 16, 10, -1, 4, 0, 9, 12, &preparation) == FAILED ||
      preparation.proceed != YES || preparation.interval_blocked != NO ||
      preparation.preservation_supported != YES || g_preservation_calls != 1)
    return 5;

  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareUnconditional", 3,
      &policy, &candidate, 40, 16, 10, -1, 9, 0, 9, -1, &preparation) == FAILED ||
      preparation.proceed != NO || preparation.interval_blocked != YES ||
      g_preservation_calls != 0)
    return 6;

  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareMissingBoundary", 3,
      &policy, &candidate, 40, 16, 10, -1, 4, 0, 9, -1, &preparation) == FAILED ||
      preparation.proceed != NO || preparation.interval_blocked != YES ||
      g_preservation_calls != 0)
    return 7;

  g_preservation_mode = 2;
  g_preservation_calls = 0;
  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_selection("corePrepareBadCallback", 3,
      &policy, &candidate, 40, 16, 10, -1, 0, 0, 9, -1, &preparation) != FAILED ||
      candidate_selection_preparation_is_clear(&preparation) == NO ||
      g_preservation_calls != 1)
    return 8;
  g_preservation_mode = 0;

  policy.can_preserve_split_spill = NULL;
  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareNoPolicy", 3,
      &policy, &candidate, 40, 16, 10, -1, 0, 0, 9, -1, &preparation) == FAILED ||
      preparation.proceed != YES || preparation.interval_blocked != NO ||
      preparation.preservation_queried != NO || preparation.preservation_supported != NO ||
      preparation.preservation_physical_register != -1 || g_preservation_calls != 0)
    return 9;
  policy.can_preserve_split_spill = can_preserve_split_spill;

  candidate.found = NO;
  memset(&preparation, 7, sizeof(preparation));
  g_preservation_calls = 0;
  if (register_allocator_prepare_candidate_selection("corePrepareBadCandidate", 3,
      &policy, &candidate, 40, 16, 10, -1, 0, 0, 9, -1, &preparation) != FAILED ||
      candidate_selection_preparation_is_clear(&preparation) == NO ||
      g_preservation_calls != 0)
    return 10;
  candidate.found = YES;

  memset(&preparation, 7, sizeof(preparation));
  if (register_allocator_prepare_candidate_selection("corePrepareBadReasons", 3,
      &policy, &candidate, 40, 16, 10, -1, 0, 9, 9, -1, &preparation) != FAILED ||
      candidate_selection_preparation_is_clear(&preparation) == NO)
    return 11;
  if (register_allocator_prepare_candidate_selection("corePrepareNoOutput", 3,
      &policy, &candidate, 40, 16, 10, -1, 0, 0, 9, -1, NULL) != FAILED)
    return 12;

  return 0;
}

static int test_candidate_selection_finalizations(void) {

  struct register_allocator_target_policy policy;
  struct register_allocator_candidate_selection_finalization finalization;
  struct fact_context facts;

  initialize_policy(&policy);
  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  if (register_allocator_finalize_candidate_selection("coreFinalizePrimary", 3, &policy,
      8, 3, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != YES || finalization.decision.plan != RA_CANDIDATE_PLAN_USE_PRIMARY ||
      finalization.dispatch.action != RA_CANDIDATE_DISPATCH_USE_PRIMARY ||
      finalization.dispatch.physical_register != 10 ||
      finalization.split_action != RA_SPLIT_ACTION_NONE || g_selector_calls != 0)
    return 1;

  seed_facts(&facts, NO, YES, NO);
  reset_selector(0);
  if (register_allocator_finalize_candidate_selection("coreFinalizeAlternate", 3, &policy,
      16, 2, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != YES ||
      finalization.decision.plan != RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED ||
      finalization.dispatch.action != RA_CANDIDATE_DISPATCH_USE_ALTERNATE ||
      finalization.dispatch.physical_register != 20 || finalization.dispatch.candidate_index != 1 ||
      finalization.split_action != RA_SPLIT_ACTION_NONE || g_selector_calls != 1 ||
      g_selector_check_path != NO || g_selector_check_overlap != NO ||
      strcmp(g_selector_reason, "unsupported_primary") != 0)
    return 2;

  seed_facts(&facts, NO, YES, NO);
  reset_selector(0);
  if (register_allocator_finalize_candidate_selection("coreFinalizeRejectUnsupported", 3,
      &policy, 8, 3, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed,
      is_path_safe, is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != NO ||
      finalization.decision.plan != RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED ||
      finalization.dispatch.action != RA_CANDIDATE_DISPATCH_REJECT ||
      finalization.split_action != FAILED || g_selector_calls != 0)
    return 3;

  seed_facts(&facts, YES, NO, NO);
  reset_selector(1);
  if (register_allocator_finalize_candidate_selection("coreFinalizeRejectPath", 3, &policy,
      8, 3, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != NO ||
      finalization.decision.plan != RA_CANDIDATE_PLAN_FALLBACK_PATH ||
      finalization.dispatch.action != RA_CANDIDATE_DISPATCH_REJECT ||
      finalization.split_action != FAILED || g_selector_calls != 1 ||
      g_selector_check_path != YES || g_selector_check_overlap != YES ||
      strcmp(g_selector_reason, "clobber_between") != 0)
    return 4;

  seed_facts(&facts, YES, YES, YES);
  reset_selector(1);
  if (register_allocator_finalize_candidate_selection("coreFinalizeConflictPrimary", 3,
      &policy, 16, 2, 10, -1, 5, 8, 12, NO, YES, 10, &facts, is_allowed,
      is_path_safe, is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != YES ||
      finalization.decision.plan != RA_CANDIDATE_PLAN_FALLBACK_CONFLICT ||
      finalization.dispatch.action != RA_CANDIDATE_DISPATCH_USE_PRIMARY ||
      finalization.dispatch.physical_register != 10 ||
      finalization.split_action != RA_SPLIT_ACTION_PRESERVE || g_selector_calls != 1)
    return 5;

  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  if (register_allocator_finalize_candidate_selection("coreFinalizeSplitUnsupported", 3,
      &policy, 8, 3, 10, -1, 5, 8, 12, NO, NO, -1, &facts, is_allowed,
      is_path_safe, is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != NO || finalization.dispatch.physical_register != 10 ||
      finalization.split_action != RA_SPLIT_ACTION_REJECT_UNSUPPORTED || g_selector_calls != 0)
    return 6;

  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  if (register_allocator_finalize_candidate_selection("coreFinalizeSplitRegister", 3,
      &policy, 8, 3, 10, -1, 5, 8, 12, NO, YES, 20, &facts, is_allowed,
      is_path_safe, is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != NO ||
      finalization.split_action != RA_SPLIT_ACTION_REJECT_REGISTER || g_selector_calls != 0)
    return 7;

  seed_facts(&facts, NO, YES, NO);
  reset_selector(0);
  if (register_allocator_finalize_candidate_selection("coreFinalizeAlternatePreserve", 3,
      &policy, 16, 2, 10, -1, 5, 8, 12, NO, YES, 20, &facts, is_allowed,
      is_path_safe, is_active, &facts, select_alternate, &finalization) == FAILED ||
      finalization.proceed != YES || finalization.dispatch.physical_register != 20 ||
      finalization.split_action != RA_SPLIT_ACTION_PRESERVE || g_selector_calls != 1)
    return 8;

  seed_facts(&facts, NO, YES, NO);
  reset_selector(2);
  memset(&finalization, 7, sizeof(finalization));
  if (register_allocator_finalize_candidate_selection("coreFinalizeSelectorFailure", 3,
      &policy, 16, 2, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed,
      is_path_safe, is_active, &facts, select_alternate, &finalization) != FAILED ||
      candidate_selection_finalization_is_clear(&finalization) == NO || g_selector_calls != 1)
    return 9;

  seed_facts(&facts, NO, YES, NO);
  reset_selector(3);
  memset(&finalization, 7, sizeof(finalization));
  if (register_allocator_finalize_candidate_selection("coreFinalizeBadSelector", 3, &policy,
      16, 2, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, &finalization) != FAILED ||
      candidate_selection_finalization_is_clear(&finalization) == NO || g_selector_calls != 1)
    return 10;

  seed_facts(&facts, YES, YES, NO);
  facts.invalid_fact = 1;
  reset_selector(0);
  memset(&finalization, 7, sizeof(finalization));
  if (register_allocator_finalize_candidate_selection("coreFinalizeBadFact", 3, &policy,
      8, 3, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, &finalization) != FAILED ||
      candidate_selection_finalization_is_clear(&finalization) == NO || g_selector_calls != 0)
    return 11;

  seed_facts(&facts, YES, YES, NO);
  reset_selector(0);
  memset(&finalization, 7, sizeof(finalization));
  if (register_allocator_finalize_candidate_selection("coreFinalizeBadSingle", 3, &policy,
      8, 3, 10, -1, 5, 8, 12, 2, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, &finalization) != FAILED ||
      candidate_selection_finalization_is_clear(&finalization) == NO || g_selector_calls != 0)
    return 12;
  if (register_allocator_finalize_candidate_selection("coreFinalizeNoOutput", 3, &policy,
      8, 3, 10, -1, 5, 8, 12, YES, NO, -1, &facts, is_allowed, is_path_safe,
      is_active, &facts, select_alternate, NULL) != FAILED)
    return 13;

  return 0;
}

static int test_candidate_transition_applications(void) {

  struct register_allocator_active_slot slot;
  struct register_allocator_slot_transition transition;
  struct register_allocator_candidate_transition_application application;

  if (register_allocator_reset_active_slots("coreCandidateApply", 3, &slot, 1) == FAILED ||
      register_allocator_plan_slot_transition("coreCandidateApplyAssign", 3,
      RA_LINEAR_SCAN_ASSIGN, &slot, 5, 12, 8, 1, &transition) == FAILED)
    return 1;
  reset_callbacks();
  if (register_allocator_apply_candidate_transition("coreCandidateApplyAssign", 3, 0,
      &slot, 1, 10, -1, 5, 12, 8, 1, RA_SPLIT_ACTION_NONE, NULL, &transition,
      NULL, NULL, retain_candidate, NULL, NULL, &application) == FAILED ||
      application.retained_interval != YES || application.spill_mutation.apply != NO ||
      g_order_count != 1 || g_order[0] != 3 ||
      expect_slot(&slot, 5, 12, 8, 1, YES) == FAILED)
    return 2;

  if (register_allocator_plan_slot_transition("coreCandidateApplyKeep", 3,
      RA_LINEAR_SCAN_KEEP_ACTIVE, &slot, 6, 12, 8, 1, &transition) == FAILED)
    return 3;
  reset_callbacks();
  if (register_allocator_apply_candidate_transition("coreCandidateApplyKeep", 3, 0,
      &slot, 1, 10, -1, 6, 12, 8, 1, RA_SPLIT_ACTION_PRESERVE, NULL, &transition,
      NULL, NULL, NULL, NULL, NULL, &application) == FAILED ||
      candidate_transition_application_is_clear(&application) == NO || g_order_count != 0 ||
      expect_slot(&slot, 5, 12, 8, 1, YES) == FAILED)
    return 4;

  if (register_allocator_reset_active_slots("coreCandidateApply", 3, &slot, 1) == FAILED ||
      register_allocator_plan_slot_transition("coreCandidateApplyPreserve", 3,
      RA_LINEAR_SCAN_ASSIGN, &slot, 7, 12, 8, 1, &transition) == FAILED)
    return 5;
  reset_callbacks();
  if (register_allocator_apply_candidate_transition("coreCandidateApplyPreserve", 3, 0,
      &slot, 1, 10, -1, 7, 12, 8, 1, RA_SPLIT_ACTION_PRESERVE, NULL, &transition,
      NULL, NULL, retain_candidate, NULL, apply_spill_mutation, &application) == FAILED ||
      application.retained_interval != YES || application.spill_mutation.apply != YES ||
      application.spill_mutation.instruction != 12 ||
      application.spill_mutation.temp_index != 7 || application.spill_mutation.operand != 1 ||
      application.spill_mutation.physical_register != 10 || g_order_count != 2 ||
      g_order[0] != 3 || g_order[1] != 4)
    return 6;

  if (register_allocator_reset_active_slots("coreCandidateApply", 3, &slot, 1) == FAILED ||
      register_allocator_plan_slot_transition("coreCandidateApplyBadSpill", 3,
      RA_LINEAR_SCAN_ASSIGN, &slot, 8, 12, 8, 1, &transition) == FAILED)
    return 7;
  reset_callbacks();
  g_failure_stage = 4;
  memset(&application, 7, sizeof(application));
  if (register_allocator_apply_candidate_transition("coreCandidateApplyBadSpill", 3, 0,
      &slot, 1, 10, -1, 8, 12, 8, 1, RA_SPLIT_ACTION_PRESERVE, NULL, &transition,
      NULL, NULL, retain_candidate, NULL, apply_spill_mutation, &application) != FAILED ||
      candidate_transition_application_is_clear(&application) == NO ||
      g_order_count != 2 || g_order[0] != 3 || g_order[1] != 4)
    return 8;

  if (register_allocator_reset_active_slots("coreCandidateApply", 3, &slot, 1) == FAILED ||
      register_allocator_plan_slot_transition("coreCandidateApplyBadRetain", 3,
      RA_LINEAR_SCAN_ASSIGN, &slot, 9, 12, 8, 1, &transition) == FAILED)
    return 9;
  reset_callbacks();
  g_retain_value = 2;
  if (register_allocator_apply_candidate_transition("coreCandidateApplyBadRetain", 3, 0,
      &slot, 1, 10, -1, 9, 12, 8, 1, RA_SPLIT_ACTION_PRESERVE, NULL, &transition,
      NULL, NULL, retain_candidate, NULL, apply_spill_mutation, &application) != FAILED ||
      candidate_transition_application_is_clear(&application) == NO ||
      g_order_count != 1 || g_order[0] != 3)
    return 10;

  if (register_allocator_reset_active_slots("coreCandidateApply", 3, &slot, 1) == FAILED ||
      register_allocator_plan_slot_transition("coreCandidateApplyInvalidSplit", 3,
      RA_LINEAR_SCAN_ASSIGN, &slot, 10, 12, 8, 1, &transition) == FAILED)
    return 11;
  reset_callbacks();
  memset(&application, 7, sizeof(application));
  if (register_allocator_apply_candidate_transition("coreCandidateApplyInvalidSplit", 3, 0,
      &slot, 1, 10, -1, 10, 12, 8, 1, 99, NULL, &transition, NULL, NULL,
      retain_candidate, NULL, apply_spill_mutation, &application) != FAILED ||
      candidate_transition_application_is_clear(&application) == NO || g_order_count != 0 ||
      expect_slot(&slot, -1, -1, -1, 0, NO) == FAILED)
    return 12;

  slot.temp_index = 4;
  if (register_allocator_apply_candidate_transition("coreCandidateApplyStale", 3, 0,
      &slot, 1, 10, -1, 10, 12, 8, 1, RA_SPLIT_ACTION_NONE, NULL, &transition,
      NULL, NULL, retain_candidate, NULL, NULL, &application) != FAILED ||
      candidate_transition_application_is_clear(&application) == NO || g_order_count != 0)
    return 13;

  if (register_allocator_apply_candidate_transition("coreCandidateApplyNoOutput", 3, 0,
      &slot, 1, 10, -1, 10, 12, 8, 1, RA_SPLIT_ACTION_NONE, NULL, &transition,
      NULL, NULL, retain_candidate, NULL, NULL, NULL) != FAILED)
    return 14;

  return 0;
}

static int candidate_transition_execution_is_clear(
    struct register_allocator_candidate_transition_execution *execution) {

  return candidate_transition_preparation_is_clear(&execution->preparation) == YES &&
      candidate_transition_application_is_clear(&execution->application) == YES;
}

static int test_candidate_transition_executions(void) {

  struct register_allocator_active_slot slots[2];
  struct register_allocator_candidate_transition_execution execution;
  struct register_allocator_target_policy policy;

  initialize_policy(&policy);
  if (register_allocator_reset_active_slots("coreTransitionExecute", 4, slots, 2) == FAILED)
    return 1;

  reset_slot_policy(0);
  reset_callbacks();
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteAssign", 4,
      8, 5, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_NONE, slots, 2, &policy,
      &policy, has_temp_metadata, &policy, observe_transition_preparation,
      &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) == FAILED ||
      execution.preparation.slot_index != 0 ||
      execution.preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_ASSIGN ||
      execution.application.retained_interval != YES ||
      execution.application.spill_mutation.apply != NO || g_observer_calls != 1 ||
      g_observer_valid != YES || g_order_count != 2 || g_order[0] != 5 ||
      g_order[1] != 3 || expect_slot(&slots[0], 5, 12, 8, 1, YES) == FAILED)
    return 2;

  if (register_allocator_reset_active_slots("coreTransitionExecute", 4, slots, 2) == FAILED)
    return 3;
  reset_slot_policy(0);
  reset_callbacks();
  if (register_allocator_execute_candidate_transition("coreTransitionExecutePreserve", 4,
      8, 7, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_PRESERVE, slots, 2, &policy,
      &policy, has_temp_metadata, NULL, NULL, &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) == FAILED ||
      execution.application.retained_interval != YES ||
      execution.application.spill_mutation.apply != YES || g_order_count != 2 ||
      g_order[0] != 3 || g_order[1] != 4)
    return 4;

  if (register_allocator_reset_active_slots("coreTransitionExecute", 4, slots, 2) == FAILED ||
      register_allocator_assign_active_slot("coreTransitionExecute", 4, 0, slots, 2,
      4, 20, 3, 2, YES) == FAILED)
    return 5;
  reset_slot_policy(0);
  reset_callbacks();
  g_metadata_calls = 0;
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteReplace", 4,
      8, 6, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_NONE, slots, 2, &policy,
      &policy, has_temp_metadata, &policy, observe_transition_preparation,
      &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) == FAILED ||
      execution.preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_REPLACE_ACTIVE ||
      execution.preparation.plan.transition.displaced_temp != 4 ||
      execution.application.retained_interval != YES || g_metadata_calls != 1 ||
      g_observer_calls != 1 || g_observer_valid != YES || g_order_count != 4 ||
      g_order[0] != 5 || g_order[1] != 1 || g_order[2] != 2 || g_order[3] != 3 ||
      expect_slot(&slots[0], 6, 12, 8, 1, YES) == FAILED)
    return 6;

  if (register_allocator_reset_active_slots("coreTransitionExecute", 4, slots, 2) == FAILED ||
      register_allocator_assign_active_slot("coreTransitionExecute", 4, 0, slots, 2,
      4, 20, 3, 2, YES) == FAILED)
    return 7;
  reset_slot_policy(0);
  reset_callbacks();
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteKeep", 4,
      8, 6, 22, 40, 1, 10, -1, RA_SPLIT_ACTION_NONE, slots, 2, &policy,
      &policy, has_temp_metadata, NULL, NULL, &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) == FAILED ||
      execution.preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE ||
      execution.preparation.plan.active_temp != 4 ||
      execution.preparation.plan.active_next_use != 20 ||
      execution.application.retained_interval != NO || g_order_count != 0 ||
      expect_slot(&slots[0], 4, 20, 3, 2, YES) == FAILED)
    return 8;

  reset_slot_policy(1);
  reset_callbacks();
  g_metadata_calls = 0;
  memset(&execution, 7, sizeof(execution));
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteBadPolicy", 4,
      8, 6, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_NONE, slots, 2, &policy,
      &policy, has_temp_metadata, &policy, observe_transition_preparation,
      &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) != FAILED ||
      candidate_transition_execution_is_clear(&execution) == NO ||
      g_observer_calls != 0 || g_order_count != 0 || g_metadata_calls != 0)
    return 9;

  if (register_allocator_reset_active_slots("coreTransitionExecute", 4, slots, 2) == FAILED)
    return 10;
  reset_slot_policy(0);
  reset_callbacks();
  g_retain_value = 2;
  memset(&execution, 7, sizeof(execution));
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteBadRetain", 4,
      8, 8, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_PRESERVE, slots, 2, &policy,
      &policy, has_temp_metadata, NULL, NULL, &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) != FAILED ||
      candidate_transition_execution_is_clear(&execution) == NO ||
      g_order_count != 1 || g_order[0] != 3)
    return 11;

  if (register_allocator_reset_active_slots("coreTransitionExecute", 4, slots, 2) == FAILED)
    return 12;
  reset_slot_policy(0);
  reset_callbacks();
  g_failure_stage = 4;
  memset(&execution, 7, sizeof(execution));
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteBadSpill", 4,
      8, 9, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_PRESERVE, slots, 2, &policy,
      &policy, has_temp_metadata, NULL, NULL, &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, &execution) != FAILED ||
      candidate_transition_execution_is_clear(&execution) == NO ||
      g_order_count != 2 || g_order[0] != 3 || g_order[1] != 4)
    return 13;

  reset_slot_policy(0);
  reset_callbacks();
  memset(&execution, 7, sizeof(execution));
  if (register_allocator_execute_candidate_transition("coreTransitionExecuteBadSplit", 4,
      8, 9, 12, 40, 1, 10, -1, 99, slots, 2, &policy, &policy,
      has_temp_metadata, NULL, NULL, &policy, clear_interval, spill_temp, retain_candidate,
      &policy, apply_spill_mutation, &execution) != FAILED ||
      candidate_transition_execution_is_clear(&execution) == NO ||
      g_slot_count_calls != 0 || g_order_count != 0)
    return 14;

  if (register_allocator_execute_candidate_transition("coreTransitionExecuteNoOutput", 4,
      8, 9, 12, 40, 1, 10, -1, RA_SPLIT_ACTION_NONE, slots, 2, &policy,
      &policy, has_temp_metadata, NULL, NULL, &policy, clear_interval, spill_temp,
      retain_candidate, &policy, apply_spill_mutation, NULL) != FAILED)
    return 15;

  return 0;
}

int main(void) {

  int result;

  result = test_primary_decisions();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: primary checkpoint %d failed\n", result);
    return result;
  }
  result = test_slot_transitions();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: transition checkpoint %d failed\n", result);
    return 20 + result;
  }
  result = test_active_replaceability();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: active replaceability checkpoint %d failed\n",
        result);
    return 70 + result;
  }
  result = test_candidate_transition_plans();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: candidate transition checkpoint %d failed\n",
        result);
    return 90 + result;
  }
  result = test_candidate_transition_preparations();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: transition preparation checkpoint %d failed\n",
        result);
    return 110 + result;
  }
  result = test_candidate_selection_finalizations();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: selection finalization checkpoint %d failed\n",
        result);
    return 130 + result;
  }
  result = test_candidate_selection_preparations();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: selection preparation checkpoint %d failed\n",
        result);
    return 150 + result;
  }
  result = test_candidate_selection_orchestrations();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: selection orchestration checkpoint %d failed\n",
        result);
    return 170 + result;
  }
  result = test_candidate_transition_applications();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: candidate application checkpoint %d failed\n",
        result);
    return 190 + result;
  }
  result = test_candidate_transition_executions();
  if (result != 0) {
    fprintf(stderr, "core_scan_orchestration_test: candidate execution checkpoint %d failed\n",
        result);
    return 210 + result;
  }
  return 0;
}
