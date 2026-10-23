#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_HL 2
#define TEST_PHYSICAL_BC 3

static int expect_action(char *name, int actual, int expected) {

  if (actual == expected)
    return SUCCEEDED;

  fprintf(stderr, "core_reload_action_test: %s expected=%d actual=%d\n", name, expected, actual);
  return FAILED;
}

int main(void) {

  if (expect_action("insert", register_allocator_plan_reload_action(
      "coreReloadActionInsert", 2, 7, 10, 12, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC), RA_RELOAD_ACTION_INSERT) == FAILED)
    return 1;
  if (expect_action("unsupported", register_allocator_plan_reload_action(
      "coreReloadActionUnsupported", 2, 7, 10, 12, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER), RA_RELOAD_ACTION_NONE) == FAILED)
    return 2;

  if (expect_action("null function", register_allocator_plan_reload_action(
      NULL, 2, 7, 10, 12, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC), FAILED) == FAILED)
    return 3;
  if (expect_action("bad block", register_allocator_plan_reload_action(
      "coreReloadActionBadBlock", -1, 7, 10, 12, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC), FAILED) == FAILED)
    return 4;
  if (expect_action("bad temp", register_allocator_plan_reload_action(
      "coreReloadActionBadTemp", 2, -1, 10, 12, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC), FAILED) == FAILED)
    return 5;
  if (expect_action("bad spill", register_allocator_plan_reload_action(
      "coreReloadActionBadSpill", 2, 7, -1, 12, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC), FAILED) == FAILED)
    return 6;
  if (expect_action("equal reload", register_allocator_plan_reload_action(
      "coreReloadActionEqual", 2, 7, 10, 10, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC), FAILED) == FAILED)
    return 7;
  if (expect_action("earlier reload", register_allocator_plan_reload_action(
      "coreReloadActionEarlier", 2, 7, 10, 9, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC), FAILED) == FAILED)
    return 8;
  if (expect_action("opaque sentinel insert", register_allocator_plan_reload_action(
      "coreReloadActionOpaqueInsert", 2, 7, 10, 12, -1, 0), RA_RELOAD_ACTION_INSERT) == FAILED)
    return 9;
  if (expect_action("opaque sentinel none", register_allocator_plan_reload_action(
      "coreReloadActionOpaqueNone", 2, 7, 10, 12, -1, -1), RA_RELOAD_ACTION_NONE) == FAILED)
    return 10;

  return 0;
}
