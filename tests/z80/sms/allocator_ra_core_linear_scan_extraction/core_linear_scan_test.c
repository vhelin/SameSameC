#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"


static int expect_decision(char *name, int actual, int expected) {

  if (actual == expected)
    return SUCCEEDED;

  fprintf(stderr, "core_linear_scan_test: %s expected=%d actual=%d\n", name, expected, actual);
  return FAILED;
}


int main(void) {

  if (expect_decision("empty", register_allocator_linear_scan_choose("coreEmpty", 1, "A", 4, 2, 8, -1, -1, YES, NO), RA_LINEAR_SCAN_ASSIGN) == FAILED)
    return 1;
  if (expect_decision("expired", register_allocator_linear_scan_choose("coreExpired", 1, "HL", 8, 3, 12, 1, 8, YES, NO), RA_LINEAR_SCAN_ASSIGN) == FAILED)
    return 1;
  if (expect_decision("nearer", register_allocator_linear_scan_choose("coreNearer", 2, "HL", 5, 4, 7, 2, 11, YES, NO), RA_LINEAR_SCAN_REPLACE_ACTIVE) == FAILED)
    return 1;
  if (expect_decision("farther", register_allocator_linear_scan_choose("coreFarther", 2, "A", 5, 4, 12, 2, 9, YES, NO), RA_LINEAR_SCAN_KEEP_ACTIVE) == FAILED)
    return 1;
  if (expect_decision("equal keep", register_allocator_linear_scan_choose("coreEqualKeep", 3, "C", 6, 5, 10, 1, 10, YES, NO), RA_LINEAR_SCAN_KEEP_ACTIVE) == FAILED)
    return 1;
  if (expect_decision("equal replace", register_allocator_linear_scan_choose("coreEqualReplace", 3, "A", 6, 5, 10, 1, 10, YES, YES), RA_LINEAR_SCAN_REPLACE_ACTIVE) == FAILED)
    return 1;
  if (expect_decision("active unavailable", register_allocator_linear_scan_choose("coreUnavailable", 3, "A", 6, 5, 8, 1, 10, NO, NO), RA_LINEAR_SCAN_KEEP_ACTIVE) == FAILED)
    return 1;
  if (expect_decision("bad candidate", register_allocator_linear_scan_choose("coreBadCandidate", 0, "A", 6, -1, 10, -1, -1, YES, NO), FAILED) == FAILED)
    return 1;
  if (expect_decision("bad next use", register_allocator_linear_scan_choose("coreBadNext", 0, "A", 6, 1, 6, -1, -1, YES, NO), FAILED) == FAILED)
    return 1;
  if (expect_decision("bad active", register_allocator_linear_scan_choose("coreBadActive", 0, "A", 6, 1, 10, -1, 9, YES, NO), FAILED) == FAILED)
    return 1;
  if (expect_decision("bad replaceable", register_allocator_linear_scan_choose("coreBadReplaceable", 0, "A", 6, 1, 10, -1, -1, 2, NO), FAILED) == FAILED)
    return 1;
  if (expect_decision("bad preference", register_allocator_linear_scan_choose("coreBadPreference", 0, "A", 6, 1, 10, -1, -1, YES, 2), FAILED) == FAILED)
    return 1;
  if (expect_decision("null function", register_allocator_linear_scan_choose(NULL, 0, "A", 6, 1, 10, -1, -1, YES, NO), FAILED) == FAILED)
    return 1;
  if (expect_decision("null slot", register_allocator_linear_scan_choose("coreNullSlot", 0, NULL, 6, 1, 10, -1, -1, YES, NO), FAILED) == FAILED)
    return 1;

  return 0;
}
