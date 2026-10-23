#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_A 1
#define TEST_PHYSICAL_HL 2
#define TEST_PHYSICAL_BC 3

static int expect_action(char *name, int actual, int expected) {

  if (actual == expected)
    return SUCCEEDED;

  fprintf(stderr, "core_split_action_test: %s expected=%d actual=%d\n", name, expected, actual);
  return FAILED;
}

int main(void) {

  if (expect_action("single read", register_allocator_plan_split_action(
      "coreSplitSingle", 2, 7, 10, 12, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), RA_SPLIT_ACTION_NONE) == FAILED)
    return 1;
  if (expect_action("preserve", register_allocator_plan_split_action(
      "coreSplitPreserve", 2, 7, 10, 12, NO, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, TEST_PHYSICAL_HL), RA_SPLIT_ACTION_PRESERVE) == FAILED)
    return 2;
  if (expect_action("unsupported", register_allocator_plan_split_action(
      "coreSplitUnsupported", 2, 7, 10, 12, NO, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), RA_SPLIT_ACTION_REJECT_UNSUPPORTED) == FAILED)
    return 3;
  if (expect_action("register changed", register_allocator_plan_split_action(
      "coreSplitRegisterChanged", 2, 7, 10, 12, NO, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, TEST_PHYSICAL_BC), RA_SPLIT_ACTION_REJECT_REGISTER) == FAILED)
    return 4;

  if (expect_action("null function", register_allocator_plan_split_action(
      NULL, 2, 7, 10, 12, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 5;
  if (expect_action("bad block", register_allocator_plan_split_action(
      "coreSplitBadBlock", -1, 7, 10, 12, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 6;
  if (expect_action("bad temp", register_allocator_plan_split_action(
      "coreSplitBadTemp", 2, -1, 10, 12, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 7;
  if (expect_action("bad producer", register_allocator_plan_split_action(
      "coreSplitBadProducer", 2, 7, -1, 12, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 8;
  if (expect_action("equal consumer", register_allocator_plan_split_action(
      "coreSplitEqualConsumer", 2, 7, 10, 10, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 9;
  if (expect_action("earlier consumer", register_allocator_plan_split_action(
      "coreSplitEarlierConsumer", 2, 7, 10, 9, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 10;
  if (expect_action("bad single read", register_allocator_plan_split_action(
      "coreSplitBadSingleRead", 2, 7, 10, 12, 2, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 11;
  if (expect_action("bad support", register_allocator_plan_split_action(
      "coreSplitBadSupport", 2, 7, 10, 12, NO, 2, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 12;
  if (expect_action("no selected register", register_allocator_plan_split_action(
      "coreSplitNoSelected", 2, 7, 10, 12, YES, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER), FAILED) == FAILED)
    return 13;
  if (expect_action("supported without register", register_allocator_plan_split_action(
      "coreSplitNoPreservationRegister", 2, 7, 10, 12, NO, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL), FAILED) == FAILED)
    return 14;
  if (expect_action("unsupported with register", register_allocator_plan_split_action(
      "coreSplitUnexpectedPreservationRegister", 2, 7, 10, 12, NO, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, TEST_PHYSICAL_A), FAILED) == FAILED)
    return 15;
  if (expect_action("single with preservation", register_allocator_plan_split_action(
      "coreSplitSingleWithPreservation", 2, 7, 10, 12, YES, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, TEST_PHYSICAL_HL), FAILED) == FAILED)
    return 16;

  return 0;
}
