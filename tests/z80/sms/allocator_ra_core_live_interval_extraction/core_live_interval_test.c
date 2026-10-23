#include <stddef.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

int main(void) {

  struct register_allocator_live_interval intervals[5];

  if (register_allocator_reset_live_intervals("coreIntervals", intervals, 5) == FAILED)
    return 1;
  if (register_allocator_note_live_interval("coreIntervals", intervals, 5, 3, 8, 7, NO) == FAILED)
    return 2;
  if (register_allocator_note_live_interval("coreIntervals", intervals, 5, 3, 16, 2, YES) == FAILED)
    return 3;
  if (register_allocator_note_live_interval("coreIntervals", intervals, 5, 3, 8, 9, NO) == FAILED)
    return 4;
  if (register_allocator_note_live_interval("coreIntervals", intervals, 5, 3, 8, 9, NO) == FAILED)
    return 5;
  if (register_allocator_note_live_interval("coreIntervals", intervals, 5, 1, 8, 4, YES) == FAILED)
    return 6;
  if (register_allocator_count_live_intervals("coreIntervals", intervals, 5) != 2)
    return 7;
  if (intervals[3].size != 16 || intervals[3].live_start != 2 || intervals[3].live_end != 9 ||
      intervals[3].read_count != 3 || intervals[3].write_count != 1)
    return 8;
  if (intervals[0].used != NO || intervals[2].used != NO || intervals[4].used != NO)
    return 9;
  if (register_allocator_note_live_interval("coreBadTemp", intervals, 5, 5, 8, 1, NO) != FAILED)
    return 10;
  if (register_allocator_note_live_interval("coreBadSize", intervals, 5, 0, 0, 1, NO) != FAILED)
    return 11;
  if (register_allocator_note_live_interval("coreBadInstruction", intervals, 5, 0, 8, -1, NO) != FAILED)
    return 12;
  if (register_allocator_note_live_interval("coreBadAccess", intervals, 5, 0, 8, 1, 2) != FAILED)
    return 13;
  if (register_allocator_count_live_intervals("coreNull", NULL, 5) != -1)
    return 14;
  if (register_allocator_reset_live_intervals("coreNullReset", NULL, 5) != FAILED)
    return 15;
  if (register_allocator_count_live_intervals("coreBadCapacity", intervals, -1) != -1)
    return 16;
  if (register_allocator_reset_live_intervals("coreReset", intervals, 5) == FAILED)
    return 17;
  if (register_allocator_count_live_intervals("coreReset", intervals, 5) != 0)
    return 18;

  return 0;
}
