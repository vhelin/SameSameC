#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_A 1
#define TEST_PHYSICAL_HL 2

static int expect_state(char *name, struct register_allocator_temp_state *state, int spill_required, int physical_register) {

  if (state->spill_required == spill_required && state->physical_register == physical_register)
    return SUCCEEDED;

  fprintf(stderr, "core_temp_state_test: %s expected_spill=%d actual_spill=%d expected_phy=%d actual_phy=%d\n",
      name, spill_required, state->spill_required, physical_register, state->physical_register);
  return FAILED;
}

int main(void) {

  struct register_allocator_temp_state state;

  state.spill_required = YES;
  state.physical_register = TEST_NO_PHYSICAL_REGISTER;
  if (register_allocator_retain_temp_state("coreRegisterOnly", 2, 7, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 1) != RA_TEMP_RETAIN_REGISTER_ONLY)
    return 1;
  if (expect_state("register only", &state, NO, TEST_PHYSICAL_A) == FAILED)
    return 2;

  if (register_allocator_retain_temp_state("coreRegisterOnlyRepeat", 2, 7, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, NO, 1, 1) != RA_TEMP_RETAIN_REGISTER_ONLY)
    return 3;
  if (expect_state("register only repeat", &state, NO, TEST_PHYSICAL_HL) == FAILED)
    return 4;

  if (register_allocator_spill_temp_state("coreDisplace", 2, 7, &state,
      TEST_NO_PHYSICAL_REGISTER, "farther_next_use") == FAILED)
    return 5;
  if (expect_state("displace", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 6;

  if (register_allocator_spill_temp_state("coreDisplaceRepeat", 2, 7, &state,
      TEST_NO_PHYSICAL_REGISTER, "already_spilled") == FAILED)
    return 7;
  if (expect_state("displace repeat", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 8;

  if (register_allocator_retain_temp_state("coreSpillConstraint", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, YES, 1, 1) != RA_TEMP_RETAIN_SPILL_BACKED)
    return 9;
  if (expect_state("spill constraint", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 10;

  if (register_allocator_retain_temp_state("coreMultipleReads", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 2, 1) != RA_TEMP_RETAIN_SPILL_BACKED)
    return 11;
  if (expect_state("multiple reads", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 12;

  if (register_allocator_retain_temp_state("coreMultipleWrites", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 2) != RA_TEMP_RETAIN_SPILL_BACKED)
    return 13;
  if (expect_state("multiple writes", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 14;

  if (register_allocator_retain_temp_state("coreZeroCounts", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 0, 0) != RA_TEMP_RETAIN_SPILL_BACKED)
    return 15;
  if (expect_state("zero counts", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 16;

  if (register_allocator_retain_temp_state(NULL, 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 1) != FAILED)
    return 17;
  if (register_allocator_retain_temp_state("coreBadBlock", -1, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 1) != FAILED)
    return 18;
  if (register_allocator_retain_temp_state("coreBadTemp", 3, -1, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 1) != FAILED)
    return 19;
  if (register_allocator_retain_temp_state("coreNullState", 3, 8, NULL,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 1) != FAILED)
    return 20;
  if (register_allocator_retain_temp_state("coreNoRegister", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, NO, 1, 1) != FAILED)
    return 21;
  if (register_allocator_retain_temp_state("coreBadConstraint", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, 2, 1, 1) != FAILED)
    return 22;
  if (register_allocator_retain_temp_state("coreBadReads", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, -1, 1) != FAILED)
    return 23;
  if (register_allocator_retain_temp_state("coreBadWrites", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, -1) != FAILED)
    return 24;
  if (expect_state("transactional retain failures", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 25;

  state.spill_required = 2;
  state.physical_register = TEST_NO_PHYSICAL_REGISTER;
  if (register_allocator_retain_temp_state("coreBadBooleanState", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A, NO, 1, 1) != FAILED)
    return 26;
  if (expect_state("bad boolean unchanged", &state, 2, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 27;

  state.spill_required = YES;
  state.physical_register = TEST_PHYSICAL_A;
  if (register_allocator_spill_temp_state("coreBadSpilledState", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, "invalid") != FAILED)
    return 28;
  if (expect_state("bad spilled state unchanged", &state, YES, TEST_PHYSICAL_A) == FAILED)
    return 29;

  state.spill_required = NO;
  state.physical_register = TEST_NO_PHYSICAL_REGISTER;
  if (register_allocator_spill_temp_state("coreBadRetainedState", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, "invalid") != FAILED)
    return 30;
  if (expect_state("bad retained state unchanged", &state, NO, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 31;

  state.spill_required = YES;
  state.physical_register = TEST_NO_PHYSICAL_REGISTER;
  if (register_allocator_spill_temp_state(NULL, 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, "invalid") != FAILED)
    return 32;
  if (register_allocator_spill_temp_state("coreNullReason", 3, 8, &state,
      TEST_NO_PHYSICAL_REGISTER, NULL) != FAILED)
    return 33;
  if (register_allocator_spill_temp_state("coreNullSpillState", 3, 8, NULL,
      TEST_NO_PHYSICAL_REGISTER, "invalid") != FAILED)
    return 34;
  if (expect_state("transactional spill failures", &state, YES, TEST_NO_PHYSICAL_REGISTER) == FAILED)
    return 35;

  return 0;
}
