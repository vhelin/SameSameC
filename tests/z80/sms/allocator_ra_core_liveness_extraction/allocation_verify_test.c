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
  {
    int physical_registers[5] = { Z80_PHY_A, Z80_PHY_HL, Z80_PHY_BC, Z80_PHY_B, Z80_PHY_C };
    int units[5] = { 1, 2, 12, 4, 8 };
    int first_index, second_index;
    for (first_index = 0; first_index < 5; first_index++) {
      for (second_index = 0; second_index < 5; second_index++) {
        if (_register_allocator_physical_registers_overlap(name.label,
            physical_registers[first_index], physical_registers[second_index]) !=
            ((units[first_index] & units[second_index]) != 0 ? YES : NO))
          return 1;
      }
    }
    fprintf(stderr, "allocation_policy_test: register_pairs=25 independent_units=yes status=complete\n");
    {
      struct tac lowering;
      int source_index, register_index, op_index;
      int operations[] = { TAC_OP_COMPLEMENT, TAC_OP_ADD, TAC_OP_SUB, TAC_OP_AND, TAC_OP_OR, TAC_OP_XOR, TAC_OP_FUNCTION_CALL,
        TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE, TAC_OP_ASM };
      int checks = 0;

      memset(&lowering, 0, sizeof(lowering));
      lowering.result_physical_register = Z80_PHY_BC;
      lowering.arg2_var_type = VARIABLE_TYPE_UINT16;
      lowering.arg1_var_type_promoted = lowering.arg2_var_type_promoted = VARIABLE_TYPE_UINT16;
      for (op_index = 0; op_index < (int)(sizeof(operations) / sizeof(operations[0])); op_index++) {
        lowering.op = operations[op_index];
        for (source_index = 0; source_index < 5; source_index++) {
          lowering.arg1_physical_register = physical_registers[source_index];
          for (register_index = 0; register_index < 5; register_index++) {
            int expected = register_index == 1 && (op_index == 0 ||
              (op_index >= 3 && op_index <= 5 && source_index >= 1 && source_index <= 2)) ? YES : NO;
            if (_is_z80_basic_block_transparent_tac_for_physical_register(
                &lowering, physical_registers[register_index]) != expected)
              return 1;
            checks++;
          }
        }
      }
      for (op_index = 3; op_index <= 5; op_index++) {
        lowering.op = operations[op_index];
        lowering.arg1_physical_register = Z80_PHY_NONE;
        lowering.arg2_type = TAC_ARG_TYPE_TEMP;
        for (source_index = 0; source_index < 4; source_index++) {
          lowering.arg2_var_type = source_index % 2 == 0 ? VARIABLE_TYPE_UINT8 : VARIABLE_TYPE_INT8;
          lowering.arg2_physical_register = source_index < 2 ? Z80_PHY_NONE : Z80_PHY_BC;
          for (register_index = 0; register_index < 5; register_index++) {
            int expected = source_index == 0 && register_index == 1 ? YES : NO;
            if (_is_z80_basic_block_transparent_tac_for_physical_register(&lowering, physical_registers[register_index]) != expected)
              return 1;
            checks++;
          }
        }
      }
      lowering.arg2_var_type = VARIABLE_TYPE_UINT16;
      for (op_index = 0; op_index < 2; op_index++) {
        lowering.op = op_index == 0 ? TAC_OP_ADD : TAC_OP_SUB;
        lowering.arg2_physical_register = Z80_PHY_NONE;
        for (source_index = 0; source_index < 4; source_index++) {
          lowering.arg2_type = source_index < 2 ? TAC_ARG_TYPE_CONSTANT : TAC_ARG_TYPE_TEMP;
          lowering.arg2_d = 1 + source_index % 2;
          for (first_index = 1; first_index <= 2; first_index++) {
            lowering.result_physical_register = physical_registers[first_index];
            for (register_index = 0; register_index < 5; register_index++) {
              int expected = source_index == 0 && first_index == 1 && register_index >= 2 ? YES : NO;
              if (_is_z80_basic_block_transparent_tac_for_physical_register(
                  &lowering, physical_registers[register_index]) != expected)
                return 1;
              checks++;
            }
          }
        }
      }
      lowering.op = TAC_OP_ASSIGNMENT;
      for (source_index = 0; source_index < 2; source_index++) {
        lowering.arg1_var_type = source_index == 0 ? VARIABLE_TYPE_INT8 : VARIABLE_TYPE_UINT16;
        lowering.result_var_type = VARIABLE_TYPE_UINT16;
        for (first_index = 1; first_index <= 2; first_index++) {
          lowering.result_physical_register = physical_registers[first_index];
          for (register_index = 0; register_index < 5; register_index++) {
            int expected = (first_index == 2 && register_index == 1) ||
              (source_index == 1 && first_index == 1 && register_index >= 2) ? YES : NO;
            if (_is_z80_basic_block_transparent_tac_for_physical_register(
                &lowering, physical_registers[register_index]) != expected)
              return 1;
            checks++;
          }
        }
      }
      lowering.op = TAC_OP_ADD;
      lowering.arg1_physical_register = Z80_PHY_HL;
      lowering.arg2_physical_register = Z80_PHY_BC;
      for (source_index = 0; source_index < 5; source_index++) {
        lowering.result_physical_register = physical_registers[source_index];
        for (register_index = 0; register_index < 5; register_index++) {
          int expected = source_index < 2 && register_index >= 2 ? YES : NO;
          if (_is_z80_basic_block_transparent_tac_for_physical_register(
              &lowering, physical_registers[register_index]) != expected)
            return 1;
          checks++;
        }
      }
      fprintf(stderr, "allocation_policy_test: lowering_effects=%d direct_bc_complement=yes scratch_hl_rejected=yes full_barriers=yes status=complete\n", checks);
      fprintf(stderr, "allocation_policy_test: direct_bc_bitwise=yes unsigned_widening=yes signed_right_fallback=yes bc_overlap_rejected=yes status=complete\n");
    }
    {
      struct endpoint_case {
        int op, operand, byte_mask, word_mask, copy_byte_mask, copy_word_mask, complement_bc;
      } endpoints[] = {
        { TAC_OP_ASSIGNMENT, TAC_USE_ARG1, 1, 2, 0, 0, YES },
        { TAC_OP_ADD, TAC_USE_ARG1, 25, 2, 24, 0, YES },
        { TAC_OP_ADD, TAC_USE_ARG2, 25, 2, 24, 0, YES },
        { TAC_OP_SUB, TAC_USE_ARG1, 9, 2, 8, 0, NO },
        { TAC_OP_SUB, TAC_USE_ARG2, 0, 0, 0, 0, YES },
        { TAC_OP_AND, TAC_USE_ARG1, 25, 2, 24, 0, YES },
        { TAC_OP_AND, TAC_USE_ARG2, 25, 2, 24, 0, YES },
        { TAC_OP_OR, TAC_USE_ARG1, 25, 2, 24, 0, YES },
        { TAC_OP_OR, TAC_USE_ARG2, 25, 2, 24, 0, YES },
        { TAC_OP_XOR, TAC_USE_ARG1, 25, 2, 24, 0, YES },
        { TAC_OP_XOR, TAC_USE_ARG2, 25, 2, 24, 0, YES },
        { TAC_OP_COMPLEMENT, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_SHIFT_LEFT, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_SHIFT_LEFT, TAC_USE_ARG2, 1, 4, 0, 0, NO },
        { TAC_OP_SHIFT_RIGHT, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_SHIFT_RIGHT, TAC_USE_ARG2, 1, 4, 0, 0, NO },
        { TAC_OP_ARRAY_READ, TAC_USE_ARG1, 0, 2, 0, 2, NO },
        { TAC_OP_ARRAY_READ, TAC_USE_ARG2, 25, 2, 24, 4, YES },
        { TAC_OP_GET_ADDRESS_ARRAY, TAC_USE_ARG1, 0, 2, 0, 2, NO },
        { TAC_OP_GET_ADDRESS_ARRAY, TAC_USE_ARG2, 25, 2, 24, 4, NO },
        { TAC_OP_ARRAY_WRITE, TAC_USE_ARG1, 1, 2, 0, 0, NO },
        { TAC_OP_ARRAY_WRITE, TAC_USE_ARG2, 25, 2, 24, 4, NO },
        { TAC_OP_JUMP_EQ, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_JUMP_EQ, TAC_USE_ARG2, 9, 2, 8, 0, NO },
        { TAC_OP_JUMP_NEQ, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_JUMP_NEQ, TAC_USE_ARG2, 9, 2, 8, 0, NO },
        { TAC_OP_JUMP_LT, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_JUMP_LT, TAC_USE_ARG2, 9, 2, 8, 0, NO },
        { TAC_OP_JUMP_GT, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_JUMP_GT, TAC_USE_ARG2, 9, 2, 8, 0, NO },
        { TAC_OP_JUMP_LTE, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_JUMP_LTE, TAC_USE_ARG2, 9, 2, 8, 0, NO },
        { TAC_OP_JUMP_GTE, TAC_USE_ARG1, 17, 2, 16, 0, NO },
        { TAC_OP_JUMP_GTE, TAC_USE_ARG2, 9, 2, 8, 0, NO }
      };
      int producers[] = { TAC_OP_ASSIGNMENT, TAC_OP_ADD, TAC_OP_SUB, TAC_OP_AND,
        TAC_OP_OR, TAC_OP_XOR, TAC_OP_COMPLEMENT, TAC_OP_ARRAY_READ,
        TAC_OP_SHIFT_LEFT, TAC_OP_SHIFT_RIGHT, TAC_OP_MUL, TAC_OP_DIV,
        TAC_OP_MOD, TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE };
      int types[] = { VARIABLE_TYPE_UINT8, VARIABLE_TYPE_INT8, VARIABLE_TYPE_UINT16, VARIABLE_TYPE_INT16 };
      struct tac producer, consumer;
      struct temp_register temp;
      int endpoint_index, producer_index, type_index, constant_source, expected_mask;
      int combinations = 0, supported = 0;
      memset(&producer, 0, sizeof(producer));
      memset(&consumer, 0, sizeof(consumer));
      memset(&temp, 0, sizeof(temp));
      for (endpoint_index = 0; endpoint_index < (int)(sizeof(endpoints) / sizeof(endpoints[0])); endpoint_index++) {
        struct endpoint_case *endpoint = &endpoints[endpoint_index];
        consumer.op = endpoint->op;
        for (producer_index = 0; producer_index < 14; producer_index++) {
          producer.op = producers[producer_index];
          for (type_index = 0; type_index < 4; type_index++) {
            temp.size = type_index < 2 ? 8 : 16;
            producer.arg1_var_type = types[type_index];
            consumer.arg1_var_type_promoted = consumer.arg2_var_type_promoted = types[type_index];
            for (constant_source = 0; constant_source < (producer_index == 0 ? 2 : 1); constant_source++) {
              producer.arg1_type = constant_source ? TAC_ARG_TYPE_CONSTANT : TAC_ARG_TYPE_TEMP;
              expected_mask = temp.size == 8 ? endpoint->byte_mask : endpoint->word_mask;
              if (producer_index == 0) {
                expected_mask = temp.size == 8 ? endpoint->copy_byte_mask : endpoint->copy_word_mask;
                if (constant_source && temp.size == 8 && endpoint->operand == TAC_USE_ARG1 && endpoint_index >= 22)
                  expected_mask |= 16;
                else if (constant_source)
                  expected_mask &= ~16;
                if (constant_source && endpoint->operand == TAC_USE_ARG2 &&
                    (endpoint->op == TAC_OP_ARRAY_READ || endpoint->op == TAC_OP_ARRAY_WRITE || endpoint->op == TAC_OP_GET_ADDRESS_ARRAY))
                  expected_mask &= ~4;
              }
              if (producer_index >= 10)
                expected_mask &= ~8;
              if (producer.op == TAC_OP_COMPLEMENT && temp.size == 16 && endpoint->complement_bc == YES)
                expected_mask |= 4;
              if (producer_index >= 1 && producer_index <= 5 && temp.size == 16 && endpoint->operand == TAC_USE_ARG2 &&
                  (endpoint->op == TAC_OP_ADD || endpoint->op == TAC_OP_SUB || endpoint->op == TAC_OP_AND || endpoint->op == TAC_OP_OR || endpoint->op == TAC_OP_XOR))
                expected_mask |= 4;
              for (first_index = 0; first_index < 5; first_index++) {
                int expected = (expected_mask & (1 << first_index)) != 0 ? YES : NO;
                if (_is_register_allocator_candidate_allowed_for_physical_register(&producer, &consumer,
                    &temp, physical_registers[first_index], endpoint->operand) != expected) {
                  fprintf(stderr, "allocation_policy_test: endpoint=%d producer=%d type=%d constant=%d register=%d expected=%d status=failed\n",
                      endpoint_index, producer_index, type_index, constant_source, first_index, expected);
                  return 1;
                }
                combinations++;
                supported += expected == YES;
              }
            }
          }
        }
      }
      fprintf(stderr, "allocation_policy_test: endpoints=%d supported=%d rejected=%d independent_table=yes status=complete\n",
          combinations, supported, combinations - supported);
    }
  }
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
  for (index = 0; index < 4; index++) {
    memset(&instructions[index], 0, sizeof(instructions[index]));
    instructions[index].function_node = &function;
    instructions[index].reload_spill_to_physical_operand = -1;
    instructions[index].store_retained_to_spill_operand = -1;
    instructions[index].result_type = TAC_ARG_TYPE_TEMP;
    instructions[index].arg1_type = TAC_ARG_TYPE_CONSTANT;
    instructions[index].arg2_type = TAC_ARG_TYPE_TEMP;
    instructions[index].arg2_physical_register = Z80_PHY_BC;
    instructions[index].result_var_type = instructions[index].arg1_var_type_promoted =
      instructions[index].arg2_var_type_promoted = VARIABLE_TYPE_UINT16;
    instructions[index].result_d = 1;
    instructions[index].result_physical_register = Z80_PHY_HL;
  }
  temps[0].size = temps[1].size = 16;
  instructions[0].op = TAC_OP_COMPLEMENT;
  instructions[0].arg2_type = TAC_ARG_TYPE_NONE;
  instructions[0].arg2_physical_register = Z80_PHY_NONE;
  instructions[0].result_d = 0;
  instructions[0].result_physical_register = Z80_PHY_BC;
  instructions[1].op = TAC_OP_ADD;
  for (index = 2; index < 4; index++) {
    instructions[index].op = index == 2 ? TAC_OP_XOR : TAC_OP_SUB;
    instructions[index].arg1_type = TAC_ARG_TYPE_TEMP;
    instructions[index].arg1_d = 1;
    instructions[index].arg1_physical_register = Z80_PHY_HL;
  }
  if (check_verification(&function, instructions, 4, SUCCEEDED, "word_repeated_bc_owner") == FAILED)
    return 1;
  instructions[1].arg2_type = TAC_ARG_TYPE_CONSTANT;
  instructions[1].arg2_physical_register = Z80_PHY_NONE;
  if (check_verification(&function, instructions, 4, FAILED, "word_repeated_bc_clobber") == FAILED)
    return 1;
  instructions[1].arg2_type = TAC_ARG_TYPE_TEMP;
  instructions[1].arg2_physical_register = Z80_PHY_BC;
  {
    struct register_allocator_candidate candidate;
    instructions[2].result_d = 0;
    if (register_allocator_discover_candidate(name.label, 0, 3, 4, &function,
        _register_allocator_instruction_get_produced_temp, _register_allocator_instruction_is_active,
        _register_allocator_instruction_reads_temp, _register_allocator_instruction_writes_temp,
        _register_allocator_instruction_get_consumer_operand, &candidate) == FAILED || candidate.has_single_read != NO)
      return 1;
    instructions[1].result_d = 0;
    if (register_allocator_discover_candidate(name.label, 0, 3, 4, &function,
        _register_allocator_instruction_get_produced_temp, _register_allocator_instruction_is_active,
        _register_allocator_instruction_reads_temp, _register_allocator_instruction_writes_temp,
        _register_allocator_instruction_get_consumer_operand, &candidate) == FAILED || candidate.has_single_read != YES)
      return 1;
    instructions[1].result_d = instructions[2].result_d = 1;
    fprintf(stderr, "candidate_definition_test: read_before_write=yes previous_definition_excluded=yes status=complete\n");
  }
  {
    struct register_allocator_reload_mutation mutations[2];
    struct tac before[4];
    memset(mutations, 0, sizeof(mutations));
    instructions[1].arg2_physical_register = Z80_PHY_NONE;
    mutations[0].apply = mutations[1].apply = YES;
    mutations[0].instruction = 1;
    mutations[0].operand = TAC_USE_ARG2;
    mutations[0].physical_register = Z80_PHY_BC;
    mutations[1].instruction = 2;
    mutations[1].temp_index = 1;
    mutations[1].operand = TAC_USE_ARG1;
    mutations[1].physical_register = Z80_PHY_HL;
    memcpy(before, instructions, sizeof(before));
    if (_register_allocator_apply_reload_transaction(&function, mutations, 2) != FAILED ||
        memcmp(before, instructions, sizeof(before)) != 0)
      return 1;
    fprintf(stderr, "allocation_mutation_test: invalid_tail unchanged=yes status=complete\n");
  }
  for (index = 0; index < 4; index++) {
    instructions[index].result_var_type = instructions[index].arg1_var_type_promoted =
      instructions[index].arg2_var_type_promoted = VARIABLE_TYPE_UINT8;
    instructions[index].result_physical_register = Z80_PHY_NONE;
    instructions[index].arg1_physical_register = Z80_PHY_NONE;
    instructions[index].arg2_physical_register = Z80_PHY_C;
  }
  temps[0].size = temps[1].size = 8;
  instructions[0].op = TAC_OP_XOR;
  instructions[0].arg2_type = TAC_ARG_TYPE_CONSTANT;
  instructions[0].arg2_physical_register = Z80_PHY_NONE;
  instructions[0].result_physical_register = Z80_PHY_C;
  instructions[1].arg2_physical_register = Z80_PHY_C;
  instructions[2].op = TAC_OP_ADD;
  instructions[3].op = TAC_OP_XOR;
  if (check_verification(&function, instructions, 4, SUCCEEDED, "byte_repeated_c_owner") == FAILED)
    return 1;
  instructions[1].result_d = 0;
  instructions[1].result_physical_register = Z80_PHY_A;
  if (check_verification(&function, instructions, 4, FAILED, "byte_repeated_redefinition") == FAILED)
    return 1;
  instructions[1].result_d = 1;
  instructions[1].result_physical_register = Z80_PHY_NONE;
  {
    struct register_allocator_active_slot slots[5];
    struct register_allocator_candidate_transition_preparation preparation;
    struct register_allocator_slot_transition_context context;
    struct tac before[4];
    struct register_allocator_active_slot slots_before[5];
    long cycles, bytes;
    int slot = _get_register_allocator_target_policy()->get_active_slot_index(Z80_PHY_C);
    memset(&context, 0, sizeof(context));
    context.function_node = &function;
    context.physical_register = Z80_PHY_C;
    context.active_slots = slots;
    context.producer_instruction = 1;
    context.consumer_instruction = 2;
    context.candidate_temp = 1;
    context.candidate_operand = TAC_USE_ARG1;
    context.split_action = RA_SPLIT_ACTION_NONE;
    context.block_end = 3;
    if (register_allocator_reset_active_slots(name.label, 0, slots, 5) == FAILED ||
        register_allocator_assign_active_slot(name.label, 0, slot, slots, 5, 0, 3, 0, TAC_USE_ARG2, YES) == FAILED ||
        register_allocator_prepare_candidate_transition(name.label, 0, 1, 1, 2, TAC_OP_ADD, TAC_USE_ARG1,
        Z80_PHY_C, Z80_PHY_NONE, slots, 5, _get_register_allocator_target_policy(), &function,
        _register_allocator_active_temp_has_metadata, &preparation) == FAILED ||
        preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_REPLACE_ACTIVE)
      return 1;
    _observe_register_allocator_transition_preparation(&context, &preparation);
    if (preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE)
      return 1;
    fprintf(stderr, "allocation_cost_test: nearer_rejected=yes farther_reused=yes status=complete\n");
    memcpy(before, instructions, sizeof(before));
    memcpy(slots_before, slots, sizeof(slots_before));
    cycles = context.saved_cycles;
    bytes = context.saved_bytes;
    context.frequency = 4;
    _observe_register_allocator_transition_preparation(&context, &preparation);
    if (context.saved_cycles != cycles * 4 || context.saved_bytes != bytes ||
        memcmp(before, instructions, sizeof(before)) != 0 ||
        memcmp(slots_before, slots, sizeof(slots_before)) != 0)
      return 1;
    context.frequency = 64;
    _observe_register_allocator_transition_preparation(&context, &preparation);
    if (context.saved_cycles != cycles * 64 || context.saved_bytes != bytes)
      return 1;
    preparation.plan.linear_scan_decision = RA_LINEAR_SCAN_REPLACE_ACTIVE;
    context.split_action = RA_SPLIT_ACTION_PRESERVE;
    context.block_end = context.producer_instruction + 257;
    _observe_register_allocator_transition_preparation(&context, &preparation);
    if (context.cost_known != NO || preparation.plan.linear_scan_decision != RA_LINEAR_SCAN_REPLACE_ACTIVE ||
        memcmp(before, instructions, sizeof(before)) != 0 ||
        memcmp(slots_before, slots, sizeof(slots_before)) != 0)
      return 1;
    fprintf(stderr, "allocation_cost_test: weighted_cycles=yes static_bytes=yes instruction_bound=256 fallback=next_use immutable=yes status=complete\n");
  }
  {
    struct tac ranking[4];
    struct temp_register before[2];
    struct register_allocator_basic_block blocks[2];
    memcpy(before, temps, sizeof(before));
    memset(ranking, 0, sizeof(ranking));
    memset(blocks, 0, sizeof(blocks));
    for (index = 0; index < 4; index++) {
      ranking[index].function_node = &function;
      ranking[index].op = index == 0 ? TAC_OP_COMPLEMENT : TAC_OP_ADD;
      ranking[index].arg1_type = index == 0 ? TAC_ARG_TYPE_CONSTANT : TAC_ARG_TYPE_TEMP;
      ranking[index].arg1_d = index == 0 ? 7 : 0;
      ranking[index].arg2_type = TAC_ARG_TYPE_CONSTANT;
      ranking[index].arg2_d = 1;
      ranking[index].result_type = TAC_ARG_TYPE_TEMP;
      ranking[index].result_d = index == 0 ? 0 : 1;
      ranking[index].result_var_type = ranking[index].arg1_var_type_promoted =
        ranking[index].arg2_var_type_promoted = VARIABLE_TYPE_UINT16;
      ranking[index].arg1_physical_register = ranking[index].arg2_physical_register =
        ranking[index].result_physical_register = Z80_PHY_NONE;
      ranking[index].reload_spill_to_physical_operand = -1;
      ranking[index].store_retained_to_spill_operand = -1;
    }
    temps[0].size = temps[1].size = 16;
    temps[0].spill_required = temps[1].spill_required = YES;
    temps[0].physical_register = temps[1].physical_register = Z80_PHY_NONE;
    temps[0].offset_to_fp = -7;
    temps[1].offset_to_fp = -9;
    g_tacs = ranking;
    g_tacs_count = 4;
    ranking[2].arg2_type = TAC_ARG_TYPE_TEMP;
    for (index = 0; index < 4; index++) {
      int registers[4] = { Z80_PHY_HL, Z80_PHY_BC, Z80_PHY_B, Z80_PHY_C };
      temps[1].size = index < 2 ? 16 : 8;
      ranking[2].arg1_d = index < 2 ? 0 : 1;
      ranking[2].arg2_d = index < 2 ? 1 : 0;
      ranking[2].arg1_physical_register = index < 2 ? Z80_PHY_NONE : registers[index];
      ranking[2].arg2_physical_register = index < 2 ? registers[index] : Z80_PHY_NONE;
      if (_register_allocator_instruction_is_split_reload_eligible(&function, 2, 0) !=
          (index == 1 ? YES : NO))
        return 1;
    }
    temps[1].size = 16;
    ranking[2].arg1_d = 0;
    ranking[2].arg2_type = TAC_ARG_TYPE_CONSTANT;
    ranking[2].arg2_d = 1;
    ranking[2].arg1_physical_register = ranking[2].arg2_physical_register = Z80_PHY_NONE;
    fprintf(stderr, "allocation_cost_test: split_reload_overlap_controls=4 status=complete\n");
    for (index = 0; index < 3; index++) {
      int instruction_index;
      blocks[0].end_tac = index == 2 ? 3 : 1 + index;
      blocks[1].start_tac = 2 + index;
      blocks[1].end_tac = 3;
      temps[0].spill_required = temps[1].spill_required = YES;
      for (instruction_index = 0; instruction_index < 4; instruction_index++) {
        ranking[instruction_index].result_physical_register =
          ranking[instruction_index].arg1_physical_register =
          ranking[instruction_index].arg2_physical_register = Z80_PHY_NONE;
        ranking[instruction_index].store_retained_to_spill_operand = -1;
      }
      if (index == 2) {
        ranking[0].op = TAC_OP_ADD;
        ranking[1].op = TAC_OP_COMPLEMENT;
        ranking[1].arg1_type = TAC_ARG_TYPE_CONSTANT;
        ranking[2].arg2_type = ranking[3].arg2_type = TAC_ARG_TYPE_TEMP;
        ranking[2].arg2_d = ranking[3].arg1_d = 1;
        ranking[3].arg2_d = 0;
        ranking[3].op = TAC_OP_XOR;
      }
      if (_run_basic_block_linear_scan(&function, blocks, index == 2 ? 1 : 2, NULL, 0, NULL, 0) == FAILED ||
          ranking[0].result_physical_register != Z80_PHY_HL)
        return 1;
    }
    memcpy(temps, before, sizeof(before));
    g_tacs = instructions;
    g_tacs_count = 4;
    fprintf(stderr, "allocation_cost_test: rejected_first_alternative=yes known_alternative_over_unknown_fallback=yes status=complete\n");
  }
  instructions[1].store_retained_to_spill_operand = TAC_USE_ARG2;
  _clear_tac_retained_interval(0, 1, TAC_USE_ARG2);
  if (instructions[0].result_physical_register != Z80_PHY_NONE ||
      instructions[1].arg2_physical_register != Z80_PHY_NONE ||
      instructions[1].store_retained_to_spill_operand != -1)
    return 1;
  fprintf(stderr, "allocation_mutation_test: displaced_arg2 cleared=yes status=complete\n");
  for (index = 0; index < 10; index++) {
    struct tac before[10], layout[10], swap;
    int instruction_index, offset = -6;
    memset(instructions, 0, sizeof(instructions));
    memset(temps, 0, sizeof(temps));
    g_allocator_enabled = YES;
    g_allocator_force_all_spill = index == 6 ? YES : NO;
    for (instruction_index = 0; instruction_index < 4; instruction_index++) {
      struct tac *instruction = &instructions[instruction_index];
      instruction->function_node = &function;
      instruction->op = TAC_OP_ASSIGNMENT;
      if (instruction_index % 2 == 0) {
        instruction->arg1_type = TAC_ARG_TYPE_CONSTANT;
        instruction->result_type = TAC_ARG_TYPE_TEMP;
        instruction->result_d = instruction_index / 2;
      }
      else {
        instruction->arg1_type = TAC_ARG_TYPE_TEMP;
        instruction->arg1_d = instruction_index / 2;
      }
    }
    for (instruction_index = 0; instruction_index < 2; instruction_index++) {
      temps[instruction_index].register_index = instruction_index;
      temps[instruction_index].spill_required = YES;
      temps[instruction_index].size = instruction_index == 0 ? 16 : 8;
    }
    if (index == 1) {
      swap = instructions[1];
      instructions[1] = instructions[2];
      instructions[2] = swap;
    }
    else if (index == 3 || index == 9) {
      instructions[1].result_type = TAC_ARG_TYPE_TEMP;
      instructions[1].result_d = 0;
    }
    else if (index == 4) {
      swap = instructions[1];
      instructions[1] = instructions[2];
      instructions[2] = swap;
      instructions[1].op = TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE;
    }
    else if (index == 5) {
      temps[0].size = 8;
      temps[1].size = 16;
    }
    else if (index == 7) {
      instructions[1].op = TAC_OP_REGISTER_SPILL;
      instructions[1].arg1_original_register_index = 0;
      instructions[1].arg1_physical_register = Z80_PHY_HL;
      instructions[1].arg2_type = instructions[1].result_type = TAC_ARG_TYPE_CONSTANT;
      instructions[1].arg2_d = 2;
      instructions[1].result_d = -7;
    }
    memset(layout, 0, sizeof(layout));
    memcpy(layout, instructions, sizeof(instructions));
    g_tacs = layout;
    g_tacs_count = 4;
    if (index == 2) {
      layout[4] = layout[3];
      layout[3] = layout[2];
      layout[2] = layout[1];
      memset(&layout[1], 0, sizeof(layout[1]));
      layout[1].function_node = &function;
      layout[1].op = TAC_OP_LABEL;
      layout[1].result_type = TAC_ARG_TYPE_LABEL;
      layout[1].result_s = "spillLoop";
      layout[4].op = TAC_OP_JUMP_EQ;
      layout[4].result_type = TAC_ARG_TYPE_LABEL;
      layout[4].result_s = "spillLoop";
      layout[4].arg2_type = TAC_ARG_TYPE_CONSTANT;
      g_tacs_count = 5;
    }
    else if (index == 8) {
      memset(layout, 0, sizeof(layout));
      for (instruction_index = 0; instruction_index < 10; instruction_index++)
        layout[instruction_index].function_node = &function;
      layout[0].op = TAC_OP_JUMP_EQ;
      layout[0].result_type = TAC_ARG_TYPE_LABEL;
      layout[0].result_s = "spillRight";
      layout[0].arg1_type = layout[0].arg2_type = TAC_ARG_TYPE_CONSTANT;
      layout[1] = instructions[0];
      layout[2].op = TAC_OP_JUMP;
      layout[2].result_type = TAC_ARG_TYPE_LABEL;
      layout[2].result_s = "spillLeft";
      layout[3].op = layout[7].op = layout[9].op = TAC_OP_LABEL;
      layout[3].result_type = layout[7].result_type = layout[9].result_type = TAC_ARG_TYPE_LABEL;
      layout[3].result_s = "spillRight";
      layout[7].result_s = "spillLeft";
      layout[9].result_s = "spillEnd";
      layout[4] = instructions[2];
      layout[5] = instructions[3];
      layout[6].op = TAC_OP_JUMP;
      layout[6].result_type = TAC_ARG_TYPE_LABEL;
      layout[6].result_s = "spillEnd";
      layout[8] = instructions[1];
      g_tacs_count = 10;
    }
    else if (index == 9) {
      layout[4] = instructions[1];
      layout[4].result_type = TAC_ARG_TYPE_NONE;
      g_tacs_count = 5;
    }
    memcpy(before, layout, sizeof(before));
    if (assign_spill_slot_offsets(&function, &offset) == FAILED ||
        memcmp(before, layout, sizeof(before)) != 0 ||
        offset != (index == 0 || index == 3 || index == 8 ? -8 : -9) ||
        (temps[0].offset_to_fp == temps[1].offset_to_fp) != (index == 0 || index == 3 || index == 8))
      return 1;
  }
  fprintf(stderr, "spill_slot_test: cases=10 overlap_width_boundary_redefinition_controls=yes tacs_unchanged=yes status=complete\n");
  {
    struct temp_register slots[3], slots_before[3];
    struct tac layout[6], before[6];
    double invalid_destinations[] = {0, -7.5, -32769, 1e100, -7};
    int instruction_index, offset = -6;
    memset(slots, 0, sizeof(slots));
    memset(layout, 0, sizeof(layout));
    for (instruction_index = 0; instruction_index < 6; instruction_index++) {
      struct tac *instruction = &layout[instruction_index];
      instruction->function_node = &function;
      instruction->op = TAC_OP_ASSIGNMENT;
      if (instruction_index % 2 == 0) {
        instruction->arg1_type = TAC_ARG_TYPE_CONSTANT;
        instruction->result_type = TAC_ARG_TYPE_TEMP;
        instruction->result_d = instruction_index / 2;
      }
      else {
        instruction->arg1_type = TAC_ARG_TYPE_TEMP;
        instruction->arg1_d = instruction_index / 2;
      }
    }
    for (instruction_index = 0; instruction_index < 3; instruction_index++) {
      slots[instruction_index].register_index = instruction_index;
      slots[instruction_index].spill_required = YES;
      slots[instruction_index].size = 16;
    }
    layout[1].op = TAC_OP_REGISTER_SPILL;
    layout[1].arg1_physical_register = Z80_PHY_HL;
    layout[1].arg2_type = layout[1].result_type = TAC_ARG_TYPE_CONSTANT;
    layout[1].arg2_d = 2;
    layout[1].result_d = -7;
    memcpy(before, layout, sizeof(before));
    locals.temp_registers = slots;
    locals.temp_registers_count = 3;
    g_tacs = layout;
    g_tacs_count = 6;
    g_allocator_force_all_spill = NO;
    if (assign_spill_slot_offsets(&function, &offset) == FAILED || offset != -10 ||
        slots[0].offset_to_fp != -7 || slots[1].offset_to_fp != -9 ||
        slots[2].offset_to_fp != -9 || memcmp(before, layout, sizeof(before)) != 0)
      return 1;
    memcpy(slots_before, slots, sizeof(slots));
    for (instruction_index = 0; instruction_index < 5; instruction_index++) {
      offset = -6;
      layout[1].result_d = invalid_destinations[instruction_index];
      layout[1].result_type = instruction_index == 4 ? TAC_ARG_TYPE_TEMP : TAC_ARG_TYPE_CONSTANT;
      memcpy(before, layout, sizeof(before));
      if (assign_spill_slot_offsets(&function, &offset) != FAILED || offset != -6 ||
          memcmp(before, layout, sizeof(before)) != 0 || memcmp(slots_before, slots, sizeof(slots)) != 0)
        return 1;
    }
    locals.temp_registers = temps;
    locals.temp_registers_count = 2;
    fprintf(stderr, "spill_slot_test: fixed_prefix=yes shared_suffix=yes invalid_fixed=5 immutable=yes status=complete\n");
  }
  {
    int physical_registers[] = { Z80_PHY_A, Z80_PHY_HL, Z80_PHY_BC, Z80_PHY_B, Z80_PHY_C };
    int barriers[] = { TAC_OP_FUNCTION_CALL, TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE, TAC_OP_ASM };
    int register_index, barrier_index, instruction_index;
    memset(instructions, 0, sizeof(instructions));
    temps[0].size = temps[1].size = 16;
    for (instruction_index = 0; instruction_index < 3; instruction_index++) {
      instructions[instruction_index].function_node = &function;
      instructions[instruction_index].reload_spill_to_physical_operand = -1;
      instructions[instruction_index].store_retained_to_spill_operand = -1;
      instructions[instruction_index].arg1_type = TAC_ARG_TYPE_CONSTANT;
      instructions[instruction_index].result_type = TAC_ARG_TYPE_TEMP;
      instructions[instruction_index].arg1_var_type_promoted =
        instructions[instruction_index].arg2_var_type_promoted =
        instructions[instruction_index].result_var_type = VARIABLE_TYPE_UINT16;
    }
    instructions[0].op = TAC_OP_COMPLEMENT;
    instructions[0].result_physical_register = Z80_PHY_HL;
    instructions[1].op = TAC_OP_ADD;
    instructions[1].arg2_type = TAC_ARG_TYPE_CONSTANT;
    instructions[1].result_d = 1;
    instructions[1].result_physical_register = Z80_PHY_BC;
    instructions[2].op = TAC_OP_SHIFT_LEFT;
    instructions[2].arg1_type = instructions[2].arg2_type = TAC_ARG_TYPE_TEMP;
    instructions[2].arg1_physical_register = instructions[2].result_physical_register = Z80_PHY_HL;
    instructions[2].arg2_d = 1;
    instructions[2].arg2_physical_register = Z80_PHY_BC;
    if (check_verification(&function, instructions, 3, FAILED, "word_bc_scratch_clobber") == FAILED)
      return 1;
    for (barrier_index = 0; barrier_index < 3; barrier_index++) {
      if (_get_register_allocator_tac_clobbers(barriers[barrier_index]) != 255)
        return 1;
      for (register_index = 0; register_index < 5; register_index++) {
        char case_name[64];
        memset(instructions, 0, sizeof(instructions));
        for (instruction_index = 0; instruction_index < 3; instruction_index++) {
          instructions[instruction_index].function_node = &function;
          instructions[instruction_index].reload_spill_to_physical_operand = -1;
          instructions[instruction_index].store_retained_to_spill_operand = -1;
        }
        temps[0].size = register_index == 1 || register_index == 2 ? 16 : 8;
        instructions[0].op = TAC_OP_ASSIGNMENT;
        instructions[0].arg1_type = TAC_ARG_TYPE_CONSTANT;
        instructions[0].result_type = TAC_ARG_TYPE_TEMP;
        instructions[0].result_physical_register = physical_registers[register_index];
        instructions[1].op = barriers[barrier_index];
        instructions[2].op = TAC_OP_XOR;
        instructions[2].arg1_type = TAC_ARG_TYPE_TEMP;
        instructions[2].arg1_physical_register = physical_registers[register_index];
        instructions[2].arg2_type = TAC_ARG_TYPE_CONSTANT;
        instructions[2].result_type = TAC_ARG_TYPE_TEMP;
        instructions[2].result_d = 1;
        sprintf(case_name, "matrix_clobber_%d_%d", barrier_index, register_index);
        if (check_verification(&function, instructions, 3, FAILED, case_name) == FAILED)
          return 1;
      }
    }
  }
  return 0;
}