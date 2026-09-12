#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_HL 2

static void poison_emission(struct register_allocator_spill_emission *emission) {

  emission->emit = YES;
  emission->temp_index = 99;
  emission->physical_register = 98;
  emission->destination_offset = 97;
  emission->byte_count = 96;
}

static int emission_is_empty(struct register_allocator_spill_emission *emission, int no_physical_register) {

  return emission->emit == NO && emission->temp_index == -1 &&
      emission->physical_register == no_physical_register && emission->destination_offset == 0 &&
      emission->byte_count == 0;
}

int main(void) {

  struct register_allocator_spill_emission emission;

  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionWord", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, 2, &emission) == FAILED)
    return 1;
  if (emission.emit != YES || emission.temp_index != 7 ||
      emission.physical_register != TEST_PHYSICAL_HL || emission.destination_offset != -12 ||
      emission.byte_count != 2)
    return 2;

  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionByte", YES, 8,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, 5, 1, &emission) == FAILED)
    return 3;
  if (emission.emit != YES || emission.destination_offset != 5 || emission.byte_count != 1)
    return 4;

  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionSkip", NO, -1,
      TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, NO, 0, 0, &emission) == FAILED)
    return 5;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 6;

  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionOpaque", YES, 9,
      -1, 0, YES, -32768, 4, &emission) == FAILED)
    return 7;
  if (emission.emit != YES || emission.physical_register != 0 ||
      emission.destination_offset != -32768 || emission.byte_count != 4)
    return 8;

  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission(NULL, YES, 7, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, YES, -12, 2, &emission) != FAILED)
    return 9;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 10;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionBadRequest", 2, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, 2, &emission) != FAILED)
    return 11;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 12;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionBadTemp", YES, -1,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, 2, &emission) != FAILED)
    return 13;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 14;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionNoRegister", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, YES, -12, 2, &emission) != FAILED)
    return 15;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 16;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionBadSpill", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, 2, -12, 2, &emission) != FAILED)
    return 17;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 18;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionNoSpill", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, NO, -12, 2, &emission) != FAILED)
    return 19;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 20;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionZeroBytes", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, 0, &emission) != FAILED)
    return 21;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 22;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionNegativeBytes", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, -1, &emission) != FAILED)
    return 23;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 24;
  poison_emission(&emission);
  if (register_allocator_prepare_spill_emission("coreSpillEmissionStaleSkip", NO, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, 2, &emission) != FAILED)
    return 25;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 26;
  if (register_allocator_prepare_spill_emission("coreSpillEmissionNullOutput", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, YES, -12, 2, NULL) != FAILED)
    return 27;

  return 0;
}