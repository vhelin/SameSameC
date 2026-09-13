#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define NO_REGISTER (-999)
#define PRIMARY_REGISTER 11
#define ALTERNATE_REGISTER 22

struct selector_state {
  int mode;
  int calls;
  int check_path;
  int check_overlap;
  char reason[32];
};

static void reset_state(struct selector_state *state, int mode) {

  state->mode = mode;
  state->calls = 0;
  state->check_path = -1;
  state->check_overlap = -1;
  state->reason[0] = '\0';
}

static int select_alternate(void *context, char *reason, int check_path, int check_overlap,
    int *selected_physical_register, int *selected_candidate_index) {

  struct selector_state *state;

  state = (struct selector_state *)context;
  state->calls++;
  state->check_path = check_path;
  state->check_overlap = check_overlap;
  strcpy(state->reason, reason);

  if (state->mode == 1)
    return SUCCEEDED;
  if (state->mode == 2)
    return FAILED;
  if (state->mode == 3) {
    *selected_candidate_index = 0;
    return SUCCEEDED;
  }
  if (state->mode == 4) {
    *selected_physical_register = ALTERNATE_REGISTER;
    return SUCCEEDED;
  }
  if (state->mode == 5)
    return 2;

  *selected_physical_register = ALTERNATE_REGISTER;
  *selected_candidate_index = 1;
  return SUCCEEDED;
}

static int dispatch_is_cleared(struct register_allocator_candidate_dispatch *dispatch) {

  return dispatch->action == FAILED && dispatch->physical_register == NO_REGISTER &&
      dispatch->candidate_index == -1 && dispatch->check_path == NO &&
      dispatch->check_overlap == NO;
}

static int expect_dispatch(int plan, int selector_mode, int expected_action,
    int expected_register, int expected_index, int expected_calls, char *expected_reason,
    int expected_path, int expected_overlap) {

  struct register_allocator_candidate_dispatch dispatch;
  struct selector_state state;

  reset_state(&state, selector_mode);
  if (register_allocator_dispatch_candidate_plan("coreCandidateDispatch", 3, plan,
      PRIMARY_REGISTER, NO_REGISTER, &state, select_alternate, &dispatch) == FAILED)
    return FAILED;
  if (dispatch.action != expected_action || dispatch.physical_register != expected_register ||
      dispatch.candidate_index != expected_index || dispatch.check_path != expected_path ||
      dispatch.check_overlap != expected_overlap || state.calls != expected_calls)
    return FAILED;
  if (expected_calls > 0 && strcmp(state.reason, expected_reason) != 0)
    return FAILED;
  return SUCCEEDED;
}

static int expect_failure(char *function_name, int block_index, int plan, int primary_register,
    void *context, register_allocator_alternate_selector selector,
    struct register_allocator_candidate_dispatch *dispatch) {

  if (dispatch != NULL) {
    dispatch->action = 777;
    dispatch->physical_register = 777;
    dispatch->candidate_index = 777;
    dispatch->check_path = 777;
    dispatch->check_overlap = 777;
  }
  if (register_allocator_dispatch_candidate_plan(function_name, block_index, plan,
      primary_register, NO_REGISTER, context, selector, dispatch) != FAILED)
    return FAILED;
  if (dispatch != NULL && dispatch_is_cleared(dispatch) == NO)
    return FAILED;
  return SUCCEEDED;
}

int main(void) {

  struct register_allocator_candidate_dispatch dispatch;
  struct selector_state state;

  if (expect_dispatch(RA_CANDIDATE_PLAN_USE_PRIMARY, 0,
      RA_CANDIDATE_DISPATCH_USE_PRIMARY, PRIMARY_REGISTER, -1, 0, "", NO, NO) == FAILED)
    return 1;
  if (expect_dispatch(RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED, 0,
      RA_CANDIDATE_DISPATCH_REJECT, NO_REGISTER, -1, 0, "", NO, NO) == FAILED)
    return 2;
  if (expect_dispatch(RA_CANDIDATE_PLAN_REJECT_PATH, 0,
      RA_CANDIDATE_DISPATCH_REJECT, NO_REGISTER, -1, 0, "", NO, NO) == FAILED)
    return 3;
  if (expect_dispatch(RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED, 0,
      RA_CANDIDATE_DISPATCH_USE_ALTERNATE, ALTERNATE_REGISTER, 1, 1,
      "unsupported_primary", NO, NO) == FAILED)
    return 4;
  if (expect_dispatch(RA_CANDIDATE_PLAN_FALLBACK_PATH, 0,
      RA_CANDIDATE_DISPATCH_USE_ALTERNATE, ALTERNATE_REGISTER, 1, 1,
      "clobber_between", YES, YES) == FAILED)
    return 5;
  if (expect_dispatch(RA_CANDIDATE_PLAN_FALLBACK_CONFLICT, 0,
      RA_CANDIDATE_DISPATCH_USE_ALTERNATE, ALTERNATE_REGISTER, 1, 1,
      "active_register_conflict", YES, YES) == FAILED)
    return 6;
  if (expect_dispatch(RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED, 1,
      RA_CANDIDATE_DISPATCH_REJECT, NO_REGISTER, -1, 1,
      "unsupported_primary", NO, NO) == FAILED)
    return 7;
  if (expect_dispatch(RA_CANDIDATE_PLAN_FALLBACK_PATH, 1,
      RA_CANDIDATE_DISPATCH_REJECT, NO_REGISTER, -1, 1,
      "clobber_between", YES, YES) == FAILED)
    return 8;
  if (expect_dispatch(RA_CANDIDATE_PLAN_FALLBACK_CONFLICT, 1,
      RA_CANDIDATE_DISPATCH_USE_PRIMARY, PRIMARY_REGISTER, -1, 1,
      "active_register_conflict", YES, YES) == FAILED)
    return 9;

  reset_state(&state, 2);
  if (expect_failure("coreCandidateDispatchDependency", 3,
      RA_CANDIDATE_PLAN_FALLBACK_PATH, PRIMARY_REGISTER, &state, select_alternate,
      &dispatch) == FAILED)
    return 10;
  reset_state(&state, 3);
  if (expect_failure("coreCandidateDispatchMissingRegister", 3,
      RA_CANDIDATE_PLAN_FALLBACK_PATH, PRIMARY_REGISTER, &state, select_alternate,
      &dispatch) == FAILED)
    return 11;
  reset_state(&state, 4);
  if (expect_failure("coreCandidateDispatchMissingIndex", 3,
      RA_CANDIDATE_PLAN_FALLBACK_PATH, PRIMARY_REGISTER, &state, select_alternate,
      &dispatch) == FAILED)
    return 12;
  reset_state(&state, 5);
  if (expect_failure("coreCandidateDispatchBadStatus", 3,
      RA_CANDIDATE_PLAN_FALLBACK_PATH, PRIMARY_REGISTER, &state, select_alternate,
      &dispatch) == FAILED)
    return 13;

  reset_state(&state, 0);
  if (expect_failure(NULL, 3, RA_CANDIDATE_PLAN_USE_PRIMARY, PRIMARY_REGISTER,
      &state, select_alternate, &dispatch) == FAILED)
    return 14;
  if (expect_failure("coreCandidateDispatchBadBlock", -1,
      RA_CANDIDATE_PLAN_USE_PRIMARY, PRIMARY_REGISTER, &state, select_alternate,
      &dispatch) == FAILED)
    return 15;
  if (expect_failure("coreCandidateDispatchLowPlan", 3, 0, PRIMARY_REGISTER,
      &state, select_alternate, &dispatch) == FAILED)
    return 16;
  if (expect_failure("coreCandidateDispatchHighPlan", 3, 7, PRIMARY_REGISTER,
      &state, select_alternate, &dispatch) == FAILED)
    return 17;
  if (expect_failure("coreCandidateDispatchNoPrimary", 3,
      RA_CANDIDATE_PLAN_USE_PRIMARY, NO_REGISTER, &state, select_alternate,
      &dispatch) == FAILED)
    return 18;
  if (expect_failure("coreCandidateDispatchNoContext", 3,
      RA_CANDIDATE_PLAN_USE_PRIMARY, PRIMARY_REGISTER, NULL, select_alternate,
      &dispatch) == FAILED)
    return 19;
  if (expect_failure("coreCandidateDispatchNoSelector", 3,
      RA_CANDIDATE_PLAN_USE_PRIMARY, PRIMARY_REGISTER, &state, NULL, &dispatch) == FAILED)
    return 20;
  if (expect_failure("coreCandidateDispatchNoOutput", 3,
      RA_CANDIDATE_PLAN_USE_PRIMARY, PRIMARY_REGISTER, &state, select_alternate, NULL) == FAILED)
    return 21;

  return 0;
}
