#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

struct apply_context {
  int calls;
  int instruction;
  int temp_index;
  int operand;
  int physical_register;
  int result;
};

static int apply_spill(void *context, int instruction_index, int temp_index, int operand, int physical_register) {

  struct apply_context *apply_context;

  apply_context = (struct apply_context *)context;
  apply_context->calls++;
  apply_context->instruction = instruction_index;
  apply_context->temp_index = temp_index;
  apply_context->operand = operand;
  apply_context->physical_register = physical_register;
  return apply_context->result;
}

static int apply_reload(void *context, int instruction_index, int temp_index, int operand, int physical_register) {

  struct apply_context *apply_context;

  apply_context = (struct apply_context *)context;
  apply_context->calls++;
  apply_context->instruction = instruction_index;
  apply_context->temp_index = temp_index;
  apply_context->operand = operand;
  apply_context->physical_register = physical_register;
  return apply_context->result;
}

static void reset_context(struct apply_context *context, int result) {

  context->calls = 0;
  context->instruction = -1;
  context->temp_index = -1;
  context->operand = -1;
  context->physical_register = -1;
  context->result = result;
}

int main(void) {

  struct apply_context context;
  struct register_allocator_spill_mutation spill;
  struct register_allocator_reload_mutation reload;

  spill.apply = YES;
  spill.instruction = 12;
  spill.temp_index = 7;
  spill.operand = 1;
  spill.physical_register = 2;
  reset_context(&context, SUCCEEDED);
  if (register_allocator_apply_spill_mutation("coreApplySpill", 2, 0, &context, &spill, apply_spill) == FAILED)
    return 1;
  if (context.calls != 1 || context.instruction != 12 || context.temp_index != 7 ||
      context.operand != 1 || context.physical_register != 2)
    return 2;

  spill.apply = NO;
  spill.instruction = -1;
  spill.temp_index = -1;
  spill.operand = -1;
  spill.physical_register = 0;
  reset_context(&context, FAILED);
  if (register_allocator_apply_spill_mutation("coreApplySpillEmpty", 2, 0, &context, &spill, NULL) == FAILED)
    return 3;
  if (context.calls != 0)
    return 4;

  spill.apply = YES;
  spill.instruction = 12;
  spill.temp_index = 7;
  spill.operand = 0;
  spill.physical_register = 0;
  reset_context(&context, SUCCEEDED);
  if (register_allocator_apply_spill_mutation("coreApplySpillOpaque", 2, -1, &context, &spill, apply_spill) == FAILED)
    return 5;
  reset_context(&context, FAILED);
  if (register_allocator_apply_spill_mutation("coreApplySpillCallbackFail", 2, -1, &context, &spill, apply_spill) != FAILED)
    return 6;
  if (context.calls != 1)
    return 7;

  reload.apply = YES;
  reload.instruction = 14;
  reload.temp_index = 7;
  reload.operand = TAC_USE_ARG2;
  reload.physical_register = 3;
  reset_context(&context, SUCCEEDED);
  if (register_allocator_apply_reload_mutation("coreApplyReload", 2, 0, &context, &reload, apply_reload) == FAILED)
    return 8;
  if (context.calls != 1 || context.instruction != 14 || context.temp_index != 7 ||
      context.operand != TAC_USE_ARG2 || context.physical_register != 3)
    return 9;

  reload.apply = NO;
  reload.instruction = -1;
  reload.temp_index = -1;
  reload.operand = -1;
  reload.physical_register = -1;
  reset_context(&context, FAILED);
  if (register_allocator_apply_reload_mutation("coreApplyReloadEmpty", 2, -1, &context, &reload, NULL) == FAILED)
    return 10;
  if (context.calls != 0)
    return 11;

  reload.apply = YES;
  reload.instruction = 14;
  reload.temp_index = 7;
  reload.operand = TAC_USE_ARG1;
  reload.physical_register = 0;
  reset_context(&context, FAILED);
  if (register_allocator_apply_reload_mutation("coreApplyReloadCallbackFail", 2, -1, &context, &reload, apply_reload) != FAILED)
    return 12;
  if (context.calls != 1)
    return 13;

  spill.apply = 2;
  if (register_allocator_apply_spill_mutation("coreApplySpillBadApply", 2, 0, &context, &spill, apply_spill) != FAILED)
    return 14;
  spill.apply = NO;
  spill.instruction = 1;
  if (register_allocator_apply_spill_mutation("coreApplySpillBadEmpty", 2, 0, &context, &spill, apply_spill) != FAILED)
    return 15;
  spill.apply = YES;
  spill.instruction = -1;
  spill.temp_index = 7;
  spill.operand = 1;
  spill.physical_register = 2;
  if (register_allocator_apply_spill_mutation("coreApplySpillBadInstruction", 2, 0, &context, &spill, apply_spill) != FAILED)
    return 16;
  spill.instruction = 12;
  spill.physical_register = 0;
  if (register_allocator_apply_spill_mutation("coreApplySpillNoRegister", 2, 0, &context, &spill, apply_spill) != FAILED)
    return 17;
  spill.physical_register = 2;
  if (register_allocator_apply_spill_mutation("coreApplySpillNullCallback", 2, 0, &context, &spill, NULL) != FAILED)
    return 18;
  if (register_allocator_apply_spill_mutation(NULL, 2, 0, &context, &spill, apply_spill) != FAILED)
    return 19;
  if (register_allocator_apply_spill_mutation("coreApplySpillBadBlock", -1, 0, &context, &spill, apply_spill) != FAILED)
    return 20;
  if (register_allocator_apply_spill_mutation("coreApplySpillNullPlan", 2, 0, &context, NULL, apply_spill) != FAILED)
    return 21;

  reload.apply = 2;
  if (register_allocator_apply_reload_mutation("coreApplyReloadBadApply", 2, 0, &context, &reload, apply_reload) != FAILED)
    return 22;
  reload.apply = NO;
  reload.instruction = -1;
  reload.temp_index = -1;
  reload.operand = -1;
  reload.physical_register = 3;
  if (register_allocator_apply_reload_mutation("coreApplyReloadBadEmpty", 2, 0, &context, &reload, apply_reload) != FAILED)
    return 23;
  reload.apply = YES;
  reload.instruction = 14;
  reload.temp_index = -1;
  reload.operand = TAC_USE_RESULT;
  reload.physical_register = 3;
  if (register_allocator_apply_reload_mutation("coreApplyReloadBadTemp", 2, 0, &context, &reload, apply_reload) != FAILED)
    return 24;
  reload.temp_index = 7;
  reload.physical_register = 0;
  if (register_allocator_apply_reload_mutation("coreApplyReloadNoRegister", 2, 0, &context, &reload, apply_reload) != FAILED)
    return 25;
  reload.physical_register = 3;
  if (register_allocator_apply_reload_mutation("coreApplyReloadNullCallback", 2, 0, &context, &reload, NULL) != FAILED)
    return 26;
  if (register_allocator_apply_reload_mutation(NULL, 2, 0, &context, &reload, apply_reload) != FAILED)
    return 27;
  if (register_allocator_apply_reload_mutation("coreApplyReloadBadBlock", -1, 0, &context, &reload, apply_reload) != FAILED)
    return 28;
  if (register_allocator_apply_reload_mutation("coreApplyReloadNullPlan", 2, 0, &context, NULL, apply_reload) != FAILED)
    return 29;

  return 0;
}
