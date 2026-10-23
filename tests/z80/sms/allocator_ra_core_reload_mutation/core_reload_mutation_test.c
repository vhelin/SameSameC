#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_BC 3

static void poison_mutation(struct register_allocator_reload_mutation *mutation) {

  mutation->apply = YES;
  mutation->instruction = 99;
  mutation->temp_index = 98;
  mutation->operand = 96;
  mutation->physical_register = 97;
}

static int mutation_is_empty(struct register_allocator_reload_mutation *mutation, int no_physical_register) {

  return mutation->apply == NO && mutation->instruction == -1 && mutation->temp_index == -1 &&
      mutation->operand == -1 && mutation->physical_register == no_physical_register;
}

int main(void) {

  struct register_allocator_reload_mutation mutation;

  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationInsert", 2, 7, 12,
      RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) == FAILED)
    return 1;
  if (mutation.apply != YES || mutation.instruction != 12 || mutation.temp_index != 7 ||
      mutation.operand != TAC_USE_RESULT || mutation.physical_register != TEST_PHYSICAL_BC)
    return 2;

  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationNone", 2, 7, 12,
      RA_RELOAD_ACTION_NONE, TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, &mutation) == FAILED)
    return 3;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 4;

  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationOpaqueInsert", 2, 7, 12,
      RA_RELOAD_ACTION_INSERT, -1, 0, &mutation) == FAILED)
    return 5;
  if (mutation.apply != YES || mutation.physical_register != 0)
    return 6;

  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation(NULL, 2, 7, 12, RA_RELOAD_ACTION_INSERT,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) != FAILED)
    return 7;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 8;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationBadBlock", -1, 7, 12,
      RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) != FAILED)
    return 9;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 10;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationBadTemp", 2, -1, 12,
      RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) != FAILED)
    return 11;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 12;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationBadInstruction", 2, 7, -1,
      RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) != FAILED)
    return 13;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 14;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationBadAction", 2, 7, 12, 99,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) != FAILED)
    return 15;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 16;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationNoneWithRegister", 2, 7, 12,
      RA_RELOAD_ACTION_NONE, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, &mutation) != FAILED)
    return 17;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 18;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_mutation("coreReloadMutationInsertWithoutRegister", 2, 7, 12,
      RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, &mutation) != FAILED)
    return 19;
  if (mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 20;
  if (register_allocator_prepare_reload_mutation("coreReloadMutationNullOutput", 2, 7, 12,
      RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_BC, NULL) != FAILED)
    return 21;

  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_operand_mutation("coreReloadMutationArg1", 2, 7, 12,
      TAC_USE_ARG1, RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC, &mutation) == FAILED || mutation.operand != TAC_USE_ARG1)
    return 22;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_operand_mutation("coreReloadMutationArg2", 2, 7, 12,
      TAC_USE_ARG2, RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC, &mutation) == FAILED || mutation.operand != TAC_USE_ARG2)
    return 23;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_operand_mutation("coreReloadMutationResult", 2, 7, 12,
      TAC_USE_RESULT, RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC, &mutation) == FAILED || mutation.operand != TAC_USE_RESULT)
    return 24;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_operand_mutation("coreReloadMutationBadOperand", 2, 7, 12,
      -1, RA_RELOAD_ACTION_INSERT, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_BC, &mutation) != FAILED ||
      mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 25;
  poison_mutation(&mutation);
  if (register_allocator_prepare_reload_operand_mutation("coreReloadMutationArg1None", 2, 7, 12,
      TAC_USE_ARG1, RA_RELOAD_ACTION_NONE, TEST_NO_PHYSICAL_REGISTER,
      TEST_NO_PHYSICAL_REGISTER, &mutation) == FAILED ||
      mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 26;

  return 0;
}
