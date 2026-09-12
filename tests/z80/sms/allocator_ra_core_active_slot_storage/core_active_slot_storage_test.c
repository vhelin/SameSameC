#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int expect_size(char *function_name, int slot_count) {

  size_t byte_count;

  byte_count = 1;
  if (register_allocator_plan_active_slot_storage(function_name, slot_count, &byte_count) == FAILED)
    return FAILED;
  if (byte_count != (size_t)slot_count * sizeof(struct register_allocator_active_slot))
    return FAILED;

  return SUCCEEDED;
}

static int expect_invalid(char *function_name, int slot_count) {

  size_t byte_count;

  byte_count = 123;
  if (register_allocator_plan_active_slot_storage(function_name, slot_count, &byte_count) != FAILED)
    return FAILED;

  return byte_count == 0 ? SUCCEEDED : FAILED;
}

int main(void) {

  struct register_allocator_active_slot *slots;
  size_t byte_count;
  int slot_index;

  if (expect_size("coreActiveSlotStorageOne", 1) == FAILED)
    return 1;
  if (expect_size("coreActiveSlotStorageThree", 3) == FAILED)
    return 2;
  if (expect_size("coreActiveSlotStorageFive", 5) == FAILED)
    return 3;
  if (expect_size("coreActiveSlotStorageSeven", 7) == FAILED)
    return 4;

  if ((size_t)INT_MAX > ((size_t)-1) / sizeof(struct register_allocator_active_slot)) {
    if (expect_invalid("coreActiveSlotStorageMaximum", INT_MAX) == FAILED)
      return 5;
  }
  else if (expect_size("coreActiveSlotStorageMaximum", INT_MAX) == FAILED)
    return 6;

  if (expect_invalid(NULL, 5) == FAILED)
    return 7;
  if (expect_invalid("coreActiveSlotStorageZero", 0) == FAILED)
    return 8;
  if (expect_invalid("coreActiveSlotStorageNegative", -1) == FAILED)
    return 9;
  if (register_allocator_plan_active_slot_storage("coreActiveSlotStorageNullOutput", 5, NULL) != FAILED)
    return 10;

  if (register_allocator_plan_active_slot_storage("coreActiveSlotStorageLifecycle", 7, &byte_count) == FAILED)
    return 11;
  slots = (struct register_allocator_active_slot *)calloc(1, byte_count);
  if (slots == NULL)
    return 12;
  if (register_allocator_reset_active_slots("coreActiveSlotStorageLifecycle", 4, slots, 7) == FAILED) {
    free(slots);
    return 13;
  }
  for (slot_index = 0; slot_index < 7; slot_index++) {
    if (slots[slot_index].temp_index != -1 || slots[slot_index].next_use != -1 ||
        slots[slot_index].producer_instruction != -1 || slots[slot_index].consumer_operand != 0 ||
        slots[slot_index].interval_retained != NO) {
      free(slots);
      return 14;
    }
    if (register_allocator_assign_active_slot("coreActiveSlotStorageLifecycle", 4, slot_index,
        slots, 7, 20 + slot_index, 40 + slot_index, 30 + slot_index, slot_index,
        slot_index == 6 ? YES : NO) == FAILED) {
      free(slots);
      return 15;
    }
  }
  if (register_allocator_expire_active_slots("coreActiveSlotStorageLifecycle", 4, 46, slots, 7) == FAILED) {
    free(slots);
    return 16;
  }
  for (slot_index = 0; slot_index < 7; slot_index++) {
    if (slots[slot_index].temp_index != -1) {
      free(slots);
      return 17;
    }
  }
  free(slots);

  return 0;
}
