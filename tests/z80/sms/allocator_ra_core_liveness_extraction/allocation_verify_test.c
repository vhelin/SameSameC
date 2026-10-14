#define main compiler_program_main
#include "../../../../compiler/main.c"
#undef main
#include "../../../../compiler/pass_5.c"

static int check_verification(struct tree_node *function, struct tac *instructions,
    int count, int expected, char *name) {

  struct register_allocator_basic_block blocks[4];
  struct register_allocator_cfg_edge edges[4];
  struct tac before[4];
  struct temp_register temps_before[2];
  int status;

  int block_count, edge_count, index;

  memset(blocks, 0, sizeof(blocks));
  memset(edges, 0, sizeof(edges));
  block_count = 1;
  edge_count = 0;
  blocks[0].end_tac = count - 1;
  if (strncmp(name, "join_", 5) == 0 || strncmp(name, "loop_", 5) == 0) {
    block_count = count;
    edge_count = count;
    for (index = 0; index < count; index++) {
      blocks[index].start_tac = blocks[index].end_tac = index;
      edges[index].from_block = index == 0 ? 0 : 1;
      edges[index].to_block = index == 0 ? 1 : (index == 1 ? 2 : 1);
    }
    if (strncmp(name, "join_", 5) == 0) {
      edges[2].from_block = 0;
      edges[2].to_block = 2;
    }
  }
  memcpy(before, instructions, (size_t)count * sizeof(*instructions));
  memcpy(temps_before, function->local_variables->temp_registers, sizeof(temps_before));
  g_tacs = instructions;
  g_tacs_count = count;
  status = _debug_register_allocator_liveness(function, blocks, block_count, edges, edge_count, YES);
  if (status != expected || memcmp(before, instructions, (size_t)count * sizeof(*instructions)) != 0 ||
      memcmp(temps_before, function->local_variables->temp_registers, sizeof(temps_before)) != 0) {
    fprintf(stderr, "allocation_verify_test: case=%s status=failed\n", name);
    return FAILED;
  }
  fprintf(stderr, "allocation_verify_test: case=%s result=%s unchanged=yes status=complete\n",
      name, status == SUCCEEDED ? "accepted" : "rejected");
  return SUCCEEDED;
}

int main(int argc, char **argv) {

  struct tree_node function, name, *children[2];
  struct local_variables locals;
  struct temp_register temps[2];
  struct tac instructions[4];
  int index;

  g_register_allocator_trace = !(argc > 1 && strcmp(argv[1], "-q") == 0);
  memset(&function, 0, sizeof(function));
  memset(&name, 0, sizeof(name));
  memset(&locals, 0, sizeof(locals));
  memset(temps, 0, sizeof(temps));
  memset(instructions, 0, sizeof(instructions));
  children[0] = NULL;
  children[1] = &name;
  name.label = "allocationVerifyTest";
  function.children = children;
  function.local_variables = &locals;
  locals.temp_registers = temps;
  locals.temp_registers_count = 2;
  g_backend = BACKEND_Z80;
  for (index = 0; index < 2; index++) {
    temps[index].register_index = index;
    temps[index].size = 8;
    temps[index].spill_required = YES;
    temps[index].offset_to_fp = -6 - index;
  }
  for (index = 0; index < 4; index++) {
    instructions[index].function_node = &function;
    instructions[index].reload_spill_to_physical_operand = -1;
    instructions[index].store_retained_to_spill_operand = -1;
  }
  instructions[0].op = TAC_OP_ASSIGNMENT;
  instructions[0].arg1_type = TAC_ARG_TYPE_CONSTANT;
  instructions[0].arg1_d = 7;
  instructions[0].result_type = TAC_ARG_TYPE_TEMP;
  instructions[0].result_d = 0;
  instructions[0].result_physical_register = Z80_PHY_C;
  instructions[1].op = TAC_OP_XOR;
  instructions[1].arg1_type = TAC_ARG_TYPE_TEMP;
  instructions[1].arg1_d = 0;
  instructions[1].arg1_physical_register = Z80_PHY_C;
  instructions[1].arg2_type = TAC_ARG_TYPE_CONSTANT;
  instructions[1].arg2_d = 3;
  instructions[1].result_type = TAC_ARG_TYPE_TEMP;
  instructions[1].result_d = 1;
  instructions[1].result_physical_register = Z80_PHY_A;
  if (check_verification(&function, instructions, 2, SUCCEEDED, "valid") == FAILED)
    return 1;
  instructions[0].result_physical_register = Z80_PHY_NONE;
  if (check_verification(&function, instructions, 2, FAILED, "missing_definition") == FAILED)
    return 1;
  instructions[0].result_physical_register = Z80_PHY_BC;
  temps[0].size = 16;
  instructions[2] = instructions[1];
  instructions[2].arg1_physical_register = Z80_PHY_BC;
  instructions[1] = instructions[0];
  instructions[1].result_d = 1;
  instructions[1].result_physical_register = Z80_PHY_C;
  if (check_verification(&function, instructions, 3, FAILED, "overlap_bc_c") == FAILED)
    return 1;
  temps[0].size = 8;
  instructions[0].result_physical_register = Z80_PHY_C;
  instructions[2].arg1_physical_register = Z80_PHY_C;
  instructions[1].op = TAC_OP_ASM;
  instructions[1].result_type = TAC_ARG_TYPE_NONE;
  instructions[1].result_physical_register = Z80_PHY_NONE;
  if (check_verification(&function, instructions, 3, FAILED, "asm_clobber") == FAILED)
    return 1;
  instructions[1] = instructions[2];
  instructions[0].result_d = -1;
  if (check_verification(&function, instructions, 2, FAILED, "negative_temp") == FAILED)
    return 1;
  instructions[0].result_d = 0.5;
  if (check_verification(&function, instructions, 2, FAILED, "fractional_temp") == FAILED)
    return 1;
  instructions[0].result_d = 1e100;
  if (check_verification(&function, instructions, 2, FAILED, "overflow_temp") == FAILED)
    return 1;
  instructions[0].result_d = 0;
  temps[0].size = 32;
  if (check_verification(&function, instructions, 2, FAILED, "invalid_width") == FAILED)
    return 1;
  temps[0].size = 8;
  instructions[0].result_physical_register = Z80_PHY_HL;
  if (check_verification(&function, instructions, 2, FAILED, "invalid_register_class") == FAILED)
    return 1;
  instructions[0].result_physical_register = Z80_PHY_NONE;
  instructions[1].reload_spill_to_physical_operand = TAC_USE_ARG1;
  if (check_verification(&function, instructions, 2, SUCCEEDED, "valid_reload") == FAILED)
    return 1;
  temps[0].spill_required = NO;
  if (check_verification(&function, instructions, 2, FAILED, "reload_without_backing") == FAILED)
    return 1;
  temps[0].spill_required = YES;
  temps[0].offset_to_fp = 32768;
  if (check_verification(&function, instructions, 2, FAILED, "invalid_reload_offset") == FAILED)
    return 1;
  temps[0].offset_to_fp = -6;
  instructions[1].reload_spill_to_physical_operand = -1;
  instructions[0].result_physical_register = Z80_PHY_C;
  instructions[1].op = TAC_OP_REGISTER_SPILL;
  instructions[1].result_type = TAC_ARG_TYPE_CONSTANT;
  instructions[1].result_physical_register = Z80_PHY_NONE;
  instructions[1].result_d = -6;
  instructions[1].arg2_d = 1;
  if (check_verification(&function, instructions, 2, SUCCEEDED, "valid_spill") == FAILED)
    return 1;
  instructions[1].arg2_d = 2;
  if (check_verification(&function, instructions, 2, FAILED, "invalid_spill_width") == FAILED)
    return 1;
  instructions[1].arg2_d = 1;
  instructions[1].result_d = -7;
  if (check_verification(&function, instructions, 2, FAILED, "invalid_spill_offset") == FAILED)
    return 1;
  instructions[1] = instructions[2];
  instructions[1].op = TAC_OP_LABEL;
  instructions[1].result_type = TAC_ARG_TYPE_NONE;
  instructions[1].arg1_type = TAC_ARG_TYPE_NONE;
  instructions[1].result_physical_register = Z80_PHY_NONE;
  instructions[1].arg1_physical_register = Z80_PHY_NONE;
  if (check_verification(&function, instructions, 3, SUCCEEDED, "join_same_owner") == FAILED)
    return 1;
  instructions[1].op = TAC_OP_ASSIGNMENT;
  instructions[1].arg1_type = TAC_ARG_TYPE_CONSTANT;
  instructions[1].result_type = TAC_ARG_TYPE_TEMP;
  instructions[1].result_d = 1;
  instructions[1].result_physical_register = Z80_PHY_C;
  if (check_verification(&function, instructions, 3, FAILED, "join_clobbered_owner") == FAILED)
    return 1;
  instructions[1] = instructions[2];
  instructions[1].result_d = 0;
  instructions[1].result_physical_register = Z80_PHY_C;
  if (check_verification(&function, instructions, 3, SUCCEEDED, "loop_reaching_owner") == FAILED)
    return 1;
  instructions[0].result_physical_register = Z80_PHY_NONE;
  if (check_verification(&function, instructions, 3, FAILED, "loop_missing_entry_owner") == FAILED)
    return 1;
  return 0;
}