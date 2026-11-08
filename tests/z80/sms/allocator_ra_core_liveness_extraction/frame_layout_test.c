#define main compiler_program_main
#include "../../../../compiler/main.c"
#undef main
#include "../../../../compiler/pass_5.c"

int main(void) {

  struct tree_node function, name, nodes[64], *children[2];
  struct local_variables locals;
  struct local_variable variables[64], before[64];
  struct temp_register temps[3];
  struct tac instructions[5], saved[5];
  int variant, index, other, cases = 0;
  int boundaries[] = { -129, -128, 126, 127 };
  int modes[] = { RA_ADDRESS_MODE_FRAME_ADDRESS, RA_ADDRESS_MODE_FRAME_DISPLACEMENT,
    RA_ADDRESS_MODE_FRAME_DISPLACEMENT, RA_ADDRESS_MODE_FRAME_ADDRESS };

#ifdef SAMESAMEC_TRACE
  g_register_allocator_trace = YES;
#endif
  for (index = 0; index < 4; index++) {
    for (other = RA_LOCATION_STACK_LOCAL; other <= RA_LOCATION_STACK_SPILL; other++) {
      g_backend = BACKEND_Z80;
      if (get_register_allocator_address_materialization_mode(other, RA_ADDRESS_TARGET_IX,
          boundaries[index]) != modes[index])
        return 1;
    }
  }
  for (variant = 0; variant < 10; variant++) {
    int offset = -10, final_offset, array_bytes, hot_end, selected, shared;
    int widths[] = { 128, 2, 17, 1 };
    memset(&function, 0, sizeof(function));
    memset(&name, 0, sizeof(name));
    memset(&locals, 0, sizeof(locals));
    memset(nodes, 0, sizeof(nodes));
    memset(variables, 0, sizeof(variables));
    memset(temps, 0, sizeof(temps));
    memset(instructions, 0, sizeof(instructions));
    children[0] = NULL;
    children[1] = &name;
    name.label = "frameLayoutTest";
    function.children = children;
    function.local_variables = &locals;
    locals.local_variables = variables;
    locals.local_variables_count = 6;
    locals.arguments_count = 2;
    locals.temp_registers = temps;
    locals.temp_registers_count = 3;
    if (variant == 6)
      widths[0] = 4;
    if (variant == 7 || variant == 9)
      widths[3] = 2;
    if (variant >= 7)
      locals.local_variables_count += variant == 9 ? 56 : 57;
    for (index = 0; index < locals.local_variables_count; index++) {
      struct local_variable *local = &variables[index];
      int bytes = index < 2 || index >= 6 ? 2 : widths[index - 2];
      local->node = &nodes[index];
      local->size = bytes * 8;
      if (index < 2)
        local->offset_to_fp = -7 - index * 2;
      else {
        local->offset_to_fp = offset - (bytes - 1);
        offset -= bytes;
      }
    }
    nodes[3].flags = TREE_NODE_FLAG_ADDRESS_TAKEN;
    for (index = 0; index < 4; index++) {
      struct tac *instruction = &instructions[index];
      instruction->function_node = &function;
      instruction->op = TAC_OP_ASSIGNMENT;
      if (index % 2 == 0) {
        instruction->arg1_type = TAC_ARG_TYPE_CONSTANT;
        instruction->result_type = TAC_ARG_TYPE_TEMP;
        instruction->result_d = index / 2;
      }
      else {
        instruction->arg1_type = TAC_ARG_TYPE_TEMP;
        instruction->arg1_d = index / 2;
      }
    }
    for (index = 0; index < 3; index++) {
      temps[index].register_index = temps[index].original_register_index = index;
      temps[index].size = index == 1 ? 8 : 16;
      temps[index].spill_required = index < 2 ? YES : NO;
      temps[index].offset_to_fp = REGISTER_ALLOCATOR_POISON_OFFSET;
    }
    g_allocator_enabled = variant == 2 ? NO : YES;
    g_allocator_force_all_spill = variant == 1 ? YES : NO;
    g_backend = variant == 5 ? -1 : BACKEND_Z80;
    g_tacs = instructions;
    g_tacs_count = 4;
    if (variant == 3 || variant == 4) {
      instructions[4].function_node = &function;
      instructions[4].op = variant == 3 ? TAC_OP_ASM : TAC_OP_REGISTER_SPILL;
      instructions[4].result_type = instructions[4].arg2_type = TAC_ARG_TYPE_CONSTANT;
      instructions[4].result_d = offset - 1;
      instructions[4].arg2_d = 2;
      instructions[4].arg1_original_register_index = 0;
      instructions[4].arg1_physical_register = Z80_PHY_HL;
      g_tacs_count++;
    }
    memcpy(before, variables, sizeof(before));
    memcpy(saved, instructions, sizeof(saved));
    shared = variant != 1 && variant != 2 && variant != 4;
    final_offset = offset - (shared ? 2 : 3);
    array_bytes = widths[0] + widths[2];
    hot_end = final_offset + array_bytes;
    selected = (variant == 0 || variant >= 6) && final_offset + 1 < -128 && hot_end + 1 >= -128;
    if (assign_spill_slot_offsets(&function, &offset) != SUCCEEDED ||
        offset != final_offset || memcmp(instructions, saved, sizeof(saved)) != 0 ||
        temps[2].offset_to_fp != REGISTER_ALLOCATOR_POISON_OFFSET ||
        temps[0].offset_to_fp != final_offset + (shared ? 1 : 2) + (selected ? array_bytes : 0) ||
        temps[1].offset_to_fp != final_offset + 1 + (selected ? array_bytes : 0) ||
        (temps[0].offset_to_fp == temps[1].offset_to_fp) != shared) {
      fprintf(stderr, "frame_layout_test: variant=%d slots_or_instructions status=failed\n", variant);
      return 1;
    }
    for (index = 0; index < locals.local_variables_count; index++) {
      struct local_variable *local = &variables[index];
      if (local->node != before[index].node || local->size != before[index].size ||
          (!selected && local->offset_to_fp != before[index].offset_to_fp) ||
          (index < 2 && local->offset_to_fp != before[index].offset_to_fp) ||
          (selected && index >= 2 && local->size <= 16 &&
           get_register_allocator_address_materialization_mode(RA_LOCATION_STACK_LOCAL, RA_ADDRESS_TARGET_IX,
             local->offset_to_fp) != RA_ADDRESS_MODE_FRAME_DISPLACEMENT))
        return 1;
      for (other = 0; other < index; other++) {
        if (local->offset_to_fp < variables[other].offset_to_fp + variables[other].size / 8 &&
            variables[other].offset_to_fp < local->offset_to_fp + local->size / 8)
          return 1;
      }
      for (other = 0; other < 2; other++) {
        if (local->offset_to_fp < temps[other].offset_to_fp + temps[other].size / 8 &&
            temps[other].offset_to_fp < local->offset_to_fp + local->size / 8)
          return 1;
      }
    }
    if (selected && (variables[2].offset_to_fp != hot_end - widths[0] + 1 ||
        variables[4].offset_to_fp != final_offset + 1 ||
        variables[3].offset_to_fp != before[3].offset_to_fp + widths[0] ||
        variables[5].offset_to_fp != before[5].offset_to_fp + array_bytes))
      return 1;
    cases++;
  }
  fprintf(stderr, "frame_layout_test: cases=%d boundaries=-129,-128,126,127 aliases=preserved fixed_forced_asm_target_fallbacks=yes identities=stable status=complete\n", cases);
  return 0;
}
