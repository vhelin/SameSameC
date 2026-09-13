#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"


static int expect_slot(char *name, struct register_allocator_active_slot *slot, int temp_index, int next_use, int producer_instruction, int consumer_operand, int interval_retained) {

  if (slot->temp_index == temp_index && slot->next_use == next_use &&
      slot->producer_instruction == producer_instruction && slot->consumer_operand == consumer_operand &&
      slot->interval_retained == interval_retained)
    return SUCCEEDED;

  fprintf(stderr, "core_active_slot_test: %s unexpected slot state\n", name);
  return FAILED;
}


int main(void) {

  struct register_allocator_active_slot slots[5];
  int slot_index;

  if (register_allocator_reset_active_slots("coreSlots", 2, slots, 5) == FAILED)
    return 1;
  for (slot_index = 0; slot_index < 5; slot_index++) {
    if (expect_slot("reset", &slots[slot_index], -1, -1, -1, 0, NO) == FAILED)
      return 1;
    if (register_allocator_assign_active_slot("coreSlots", 2, slot_index, slots, 5,
        10 + slot_index, 10 + slot_index, slot_index, slot_index + 1, slot_index == 4 ? YES : NO) == FAILED)
      return 1;
  }

  if (register_allocator_expire_active_slots("coreSlots", 2, 10, slots, 5) == FAILED)
    return 1;
  if (expect_slot("first expired", &slots[0], -1, -1, -1, 0, NO) == FAILED)
    return 1;
  if (expect_slot("live retained", &slots[4], 14, 14, 4, 5, YES) == FAILED)
    return 1;

  if (register_allocator_assign_active_slot("coreSlots", 2, 0, slots, 5, 99, 12, 10, 2, YES) == FAILED)
    return 1;
  if (register_allocator_expire_active_slots("coreSlots", 2, 12, slots, 5) == FAILED)
    return 1;
  if (expect_slot("reassigned expired", &slots[0], -1, -1, -1, 0, NO) == FAILED ||
      expect_slot("second expired", &slots[1], -1, -1, -1, 0, NO) == FAILED ||
      expect_slot("third expired", &slots[2], -1, -1, -1, 0, NO) == FAILED)
    return 1;
  if (expect_slot("still live", &slots[3], 13, 13, 3, 4, NO) == FAILED)
    return 1;

  if (register_allocator_reset_active_slots("coreAllExpire", 4, slots, 5) == FAILED)
    return 1;
  for (slot_index = 0; slot_index < 5; slot_index++) {
    if (register_allocator_assign_active_slot("coreAllExpire", 4, slot_index, slots, 5,
        20 + slot_index, 31, 30, slot_index == 0 ? 0 : 100 + slot_index, slot_index == 2 ? YES : NO) == FAILED)
      return 1;
  }
  if (register_allocator_expire_active_slots("coreAllExpire", 4, 31, slots, 5) == FAILED)
    return 1;
  for (slot_index = 0; slot_index < 5; slot_index++) {
    if (expect_slot("all expired", &slots[slot_index], -1, -1, -1, 0, NO) == FAILED)
      return 1;
  }

  if (register_allocator_reset_active_slots("coreTransactional", 3, slots, 5) == FAILED)
    return 1;
  if (register_allocator_assign_active_slot("coreTransactional", 3, 0, slots, 5, 7, 20, 5, 1, YES) == FAILED)
    return 1;
  slots[4].next_use = 9;
  if (register_allocator_expire_active_slots("coreTransactional", 3, 20, slots, 5) != FAILED)
    return 1;
  if (expect_slot("transaction preserved", &slots[0], 7, 20, 5, 1, YES) == FAILED)
    return 1;

  if (register_allocator_reset_active_slots(NULL, 0, slots, 5) != FAILED ||
      register_allocator_reset_active_slots("coreBadReset", -1, slots, 5) != FAILED ||
      register_allocator_reset_active_slots("coreBadReset", 0, NULL, 5) != FAILED ||
      register_allocator_reset_active_slots("coreBadReset", 0, slots, 0) != FAILED)
    return 1;
  if (register_allocator_expire_active_slots("coreBadExpire", 0, -1, slots, 5) != FAILED ||
      register_allocator_expire_active_slots("coreBadExpire", 0, 1, NULL, 5) != FAILED)
    return 1;
  if (register_allocator_assign_active_slot("coreBadAssign", 0, -1, slots, 5, 1, 3, 1, 1, NO) != FAILED ||
      register_allocator_assign_active_slot("coreBadAssign", 0, 5, slots, 5, 1, 3, 1, 1, NO) != FAILED ||
      register_allocator_assign_active_slot("coreBadAssign", 0, 0, slots, 5, -1, 3, 1, 1, NO) != FAILED ||
      register_allocator_assign_active_slot("coreBadAssign", 0, 0, slots, 5, 1, 1, 1, 1, NO) != FAILED ||
      register_allocator_assign_active_slot("coreBadAssign", 0, 0, slots, 5, 1, 3, 1, -1, NO) != FAILED ||
      register_allocator_assign_active_slot("coreBadAssign", 0, 0, slots, 5, 1, 3, 1, 1, 2) != FAILED)
    return 1;

  return 0;
}
