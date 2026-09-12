#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_BC 3

static void poison_emission(struct register_allocator_reload_emission *emission) {

  emission->emit = YES;
  emission->temp_index = 99;
  emission->operand = 95;
  emission->physical_register = 98;
  emission->source_offset = 97;
  emission->byte_count = 96;
}

static int emission_is_empty(struct register_allocator_reload_emission *emission, int no_physical_register) {

  return emission->emit == NO && emission->temp_index == -1 &&
      emission->operand == -1 && emission->physical_register == no_physical_register && emission->source_offset == 0 &&
      emission->byte_count == 0;
}

int main(void) {

  struct register_allocator_reload_emission emission;

  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionWord", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2, &emission) == FAILED)
    return 1;
  if (emission.emit != YES || emission.temp_index != 7 ||
      emission.operand != TAC_USE_RESULT || emission.physical_register != TEST_PHYSICAL_BC || emission.source_offset != -12 ||
      emission.byte_count != 2)
    return 2;

  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionByte", YES, 8,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, 5, 1, &emission) == FAILED)
    return 3;
  if (emission.emit != YES || emission.source_offset != 5 || emission.byte_count != 1)
    return 4;

  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionSkip", NO, -1,
      TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, NO, 0, 0, &emission) == FAILED)
    return 5;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 6;

  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionOpaque", YES, 9,
      -1, 0, YES, -32768, 4, &emission) == FAILED)
    return 7;
  if (emission.emit != YES || emission.physical_register != 0 ||
      emission.source_offset != -32768 || emission.byte_count != 4)
    return 8;

  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission(NULL, YES, 7, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC, YES, -12, 2, &emission) != FAILED)
    return 9;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 10;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionBadRequest", 2, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2, &emission) != FAILED)
    return 11;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 12;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionBadTemp", YES, -1,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2, &emission) != FAILED)
    return 13;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 14;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionNoRegister", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, YES, -12, 2, &emission) != FAILED)
    return 15;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 16;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionBadSpill", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, 2, -12, 2, &emission) != FAILED)
    return 17;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 18;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionNoSpill", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, NO, -12, 2, &emission) != FAILED)
    return 19;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 20;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionZeroBytes", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 0, &emission) != FAILED)
    return 21;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 22;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionNegativeBytes", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, -1, &emission) != FAILED)
    return 23;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 24;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_emission("coreReloadEmissionStaleSkip", NO, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2, &emission) != FAILED)
    return 25;
  if (emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 26;
  if (register_allocator_prepare_reload_emission("coreReloadEmissionNullOutput", YES, 7,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2, NULL) != FAILED)
    return 27;

  poison_emission(&emission);
  if (register_allocator_prepare_reload_operand_emission("coreReloadEmissionArg1", YES, 7,
      TAC_USE_ARG1, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2,
      &emission) == FAILED || emission.emit != YES || emission.temp_index != 7 ||
      emission.operand != TAC_USE_ARG1 || emission.physical_register != TEST_PHYSICAL_BC ||
      emission.source_offset != -12 || emission.byte_count != 2)
    return 28;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_operand_emission("coreReloadEmissionArg2", YES, 7,
      TAC_USE_ARG2, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2,
      &emission) == FAILED || emission.emit != YES || emission.temp_index != 7 ||
      emission.operand != TAC_USE_ARG2 || emission.physical_register != TEST_PHYSICAL_BC ||
      emission.source_offset != -12 || emission.byte_count != 2)
    return 29;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_operand_emission("coreReloadEmissionResult", YES, 7,
      TAC_USE_RESULT, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2,
      &emission) == FAILED || emission.emit != YES || emission.temp_index != 7 ||
      emission.operand != TAC_USE_RESULT || emission.physical_register != TEST_PHYSICAL_BC ||
      emission.source_offset != -12 || emission.byte_count != 2)
    return 30;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_operand_emission("coreReloadEmissionBadOperand", YES, 7,
      -1, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, YES, -12, 2,
      &emission) != FAILED || emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 31;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_operand_emission("coreReloadEmissionSkipOperand", NO, -1,
      TAC_USE_ARG1, TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, NO, 0, 0,
      &emission) != FAILED || emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 32;
  poison_emission(&emission);
  if (register_allocator_prepare_reload_operand_emission("coreReloadEmissionOperandSkip", NO, -1,
      -1, TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, NO, 0, 0,
      &emission) == FAILED || emission_is_empty(&emission, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 33;

  return 0;
}
