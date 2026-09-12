#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_NO_PHYSICAL_REGISTER 0
#define TEST_PHYSICAL_HL 2
#define TEST_OPERAND_ARG1 1

static void poison_mutation(struct register_allocator_spill_mutation *mutation) {

  mutation->apply = YES;
  mutation->instruction = 99;
  mutation->temp_index = 98;
  mutation->operand = 97;
  mutation->physical_register = 96;
}

static int mutation_is_empty(struct register_allocator_spill_mutation *mutation, int no_physical_register) {

  return mutation->apply == NO && mutation->instruction == -1 && mutation->temp_index == -1 &&
      mutation->operand == -1 && mutation->physical_register == no_physical_register;
}

static int expect_invalid(char *function_name, int block_index, int temp_index, int consumer_instruction,
    int consumer_operand, int split_action, int retained_interval, int no_physical_register,
    int physical_register, struct register_allocator_spill_mutation *mutation) {

  poison_mutation(mutation);
  if (register_allocator_prepare_spill_mutation(function_name, block_index, temp_index,
      consumer_instruction, consumer_operand, split_action, retained_interval, no_physical_register,
      physical_register, mutation) != FAILED)
    return FAILED;
  return mutation_is_empty(mutation, no_physical_register);
}

int main(void) {

  struct register_allocator_spill_mutation mutation;

  poison_mutation(&mutation);
  if (register_allocator_prepare_spill_mutation("coreSpillMutationApply", 2, 7, 12,
      TEST_OPERAND_ARG1, RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, &mutation) == FAILED)
    return 1;
  if (mutation.apply != YES || mutation.instruction != 12 || mutation.temp_index != 7 ||
      mutation.operand != TEST_OPERAND_ARG1 || mutation.physical_register != TEST_PHYSICAL_HL)
    return 2;

  poison_mutation(&mutation);
  if (register_allocator_prepare_spill_mutation("coreSpillMutationNone", 2, 7, 12,
      TEST_OPERAND_ARG1, RA_SPLIT_ACTION_NONE, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, &mutation) == FAILED || mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 3;
  poison_mutation(&mutation);
  if (register_allocator_prepare_spill_mutation("coreSpillMutationNotRetained", 2, 7, 12,
      TEST_OPERAND_ARG1, RA_SPLIT_ACTION_PRESERVE, NO, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, &mutation) == FAILED || mutation_is_empty(&mutation, TEST_NO_PHYSICAL_REGISTER) == NO)
    return 4;
  poison_mutation(&mutation);
  if (register_allocator_prepare_spill_mutation("coreSpillMutationOpaque", 2, 7, 12,
      0, RA_SPLIT_ACTION_PRESERVE, YES, -1, 0, &mutation) == FAILED)
    return 5;
  if (mutation.apply != YES || mutation.operand != 0 || mutation.physical_register != 0)
    return 6;

  if (expect_invalid(NULL, 2, 7, 12, TEST_OPERAND_ARG1, RA_SPLIT_ACTION_PRESERVE, YES,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 7;
  if (expect_invalid("coreSpillMutationBadBlock", -1, 7, 12, TEST_OPERAND_ARG1,
      RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 8;
  if (expect_invalid("coreSpillMutationBadTemp", 2, -1, 12, TEST_OPERAND_ARG1,
      RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 9;
  if (expect_invalid("coreSpillMutationBadInstruction", 2, 7, -1, TEST_OPERAND_ARG1,
      RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 10;
  if (expect_invalid("coreSpillMutationBadOperand", 2, 7, 12, -1,
      RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 11;
  if (expect_invalid("coreSpillMutationRejectedAction", 2, 7, 12, TEST_OPERAND_ARG1,
      RA_SPLIT_ACTION_REJECT_UNSUPPORTED, YES, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 12;
  if (expect_invalid("coreSpillMutationBadAction", 2, 7, 12, TEST_OPERAND_ARG1, 99, YES,
      TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 13;
  if (expect_invalid("coreSpillMutationBadRetained", 2, 7, 12, TEST_OPERAND_ARG1,
      RA_SPLIT_ACTION_PRESERVE, 2, TEST_NO_PHYSICAL_REGISTER, TEST_PHYSICAL_HL, &mutation) == NO)
    return 14;
  if (expect_invalid("coreSpillMutationNoRegister", 2, 7, 12, TEST_OPERAND_ARG1,
      RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER, TEST_NO_PHYSICAL_REGISTER, &mutation) == NO)
    return 15;
  if (register_allocator_prepare_spill_mutation("coreSpillMutationNullOutput", 2, 7, 12,
      TEST_OPERAND_ARG1, RA_SPLIT_ACTION_PRESERVE, YES, TEST_NO_PHYSICAL_REGISTER,
      TEST_PHYSICAL_HL, NULL) != FAILED)
    return 16;

  return 0;
}