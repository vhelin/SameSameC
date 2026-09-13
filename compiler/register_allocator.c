#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "register_allocator.h"


static int _block_end_falls_through(int end_reason) {

  if (end_reason == RA_BLOCK_END_LABEL ||
      end_reason == RA_BLOCK_END_FUNCTION_CALL ||
      end_reason == RA_BLOCK_END_INLINE_ASM ||
      end_reason == RA_BLOCK_END_UNMIGRATED)
    return YES;

  return NO;
}


static int _is_valid_tac_use_operand(int operand) {

  if (operand == TAC_USE_RESULT || operand == TAC_USE_ARG1 ||
      operand == TAC_USE_ARG2)
    return YES;

  return NO;
}


static char *_get_missing_target_policy_hook(struct register_allocator_target_policy *policy) {

  if (policy->get_physical_register_units == NULL)
    return "physical_register_units";
  if (policy->get_active_slot_count == NULL)
    return "active_slot_count";
  if (policy->get_active_slot_index == NULL)
    return "active_slot_index";
  if (policy->get_active_slot_name == NULL)
    return "active_slot_name";
  if (policy->get_candidate_register_count == NULL)
    return "candidate_register_count";
  if (policy->get_candidate_physical_register == NULL)
    return "candidate_register";
  if (policy->can_fallback_candidate == NULL)
    return "candidate_fallback";
  if (policy->prefer_candidate_on_equal_next_use == NULL)
    return "equal_next_use_preference";
  if (policy->is_candidate_allowed_for_physical_register == NULL)
    return "candidate_legality";
  if (policy->get_tac_clobbers == NULL)
    return "tac_clobbers";
  if (policy->is_tac_transparent_for_physical_register == NULL)
    return "tac_transparency";
  if (policy->get_call_result_physical_register == NULL)
    return "call_result_register";
  if (policy->get_call_boundary_spill_reason == NULL)
    return "call_boundary_spill";
  if (policy->get_stack_return_value_end_offset == NULL)
    return "stack_return_transport";
  if (policy->get_return_value_byte_offset == NULL)
    return "return_slot_access";
  if (policy->get_call_frame_prefix_value == NULL)
    return "call_frame_prefix";
  if (policy->get_stack_argument_end_offset == NULL)
    return "stack_argument_layout";
  if (policy->get_argument_transport == NULL)
    return "argument_transport";
  if (policy->get_argument_byte_offset == NULL)
    return "argument_byte_access";
  if (policy->get_location_kind == NULL)
    return "location_materialization";
  if (policy->get_address_materialization_mode == NULL)
    return "address_materialization";
  if (policy->can_preserve_split_spill == NULL)
    return "split_spill";
  if (policy->get_split_reload_physical_register == NULL)
    return "split_reload";

  return NULL;
}


int register_allocator_validate_target_policy(char *function_name, struct register_allocator_target_policy *policy) {

  char *missing_hook;

  if (function_name == NULL || policy == NULL || policy->name == NULL || policy->name[0] == '\0') {
    fprintf(stderr, "register_allocator_core: target_policy_validate function=%s target=%s hooks=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        policy != NULL && policy->name != NULL && policy->name[0] != '\0' ? policy->name : "<null>");
    return FAILED;
  }

  missing_hook = _get_missing_target_policy_hook(policy);
  if (missing_hook != NULL) {
    fprintf(stderr, "register_allocator_core: target_policy_validate function=%s target=%s hooks=23 missing=%s status=invalid_policy\n",
        function_name, policy->name, missing_hook);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_core: target_policy_validate function=%s target=%s hooks=23 status=complete\n",
      function_name, policy->name);
  return SUCCEEDED;
}




int register_allocator_physical_registers_overlap(char *function_name, struct register_allocator_target_policy *policy, int left_register, int right_register, int *overlap) {

  int left_units;
  int right_units;

  if (overlap != NULL)
    *overlap = YES;

  if (function_name == NULL || policy == NULL || policy->name == NULL || policy->name[0] == '\0' ||
      policy->get_physical_register_units == NULL || overlap == NULL) {
    fprintf(stderr, "register_allocator_core: physical_overlap function=%s target=%s left=%d right=%d overlap=yes status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        policy != NULL && policy->name != NULL && policy->name[0] != '\0' ? policy->name : "<null>",
        left_register, right_register);
    return FAILED;
  }

  left_units = policy->get_physical_register_units(left_register);
  right_units = policy->get_physical_register_units(right_register);
  if (left_units <= 0 || right_units <= 0) {
    fprintf(stderr, "register_allocator_core: physical_overlap function=%s target=%s left=%d left_units=%d right=%d right_units=%d overlap=yes status=invalid_policy\n",
        function_name, policy->name, left_register, left_units, right_register, right_units);
    return FAILED;
  }

  if ((left_units & right_units) == 0)
    *overlap = NO;

  fprintf(stderr, "register_allocator_core: physical_overlap function=%s target=%s left=%d left_units=%d right=%d right_units=%d overlap=%s status=complete\n",
      function_name, policy->name, left_register, left_units, right_register, right_units,
      *overlap == YES ? "yes" : "no");
  return SUCCEEDED;
}


int register_allocator_plan_active_slot_storage(char *function_name, int slot_count, size_t *byte_count) {

  if (byte_count != NULL)
    *byte_count = 0;

  if (function_name == NULL || slot_count <= 0 || byte_count == NULL ||
      (size_t)slot_count > ((size_t)-1) / sizeof(struct register_allocator_active_slot)) {
    fprintf(stderr, "register_allocator_core: active_slot_storage function=%s slots=%d bytes=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", slot_count);
    return FAILED;
  }

  *byte_count = (size_t)slot_count * sizeof(struct register_allocator_active_slot);
  fprintf(stderr, "register_allocator_core: active_slot_storage function=%s slots=%d bytes=%lu status=complete\n",
      function_name, slot_count, (unsigned long)*byte_count);
  return SUCCEEDED;
}


static void _clear_liveness_storage(struct register_allocator_liveness_storage *storage) {

  storage->set_count = 0;
  storage->buffer_bytes = 0;
  storage->total_bytes = 0;
}


int register_allocator_plan_liveness_storage(char *function_name, int block_count, int temp_count,
    struct register_allocator_liveness_storage *storage) {

  int set_count;

  if (storage != NULL)
    _clear_liveness_storage(storage);
  if (function_name == NULL || block_count <= 0 || temp_count <= 0 || storage == NULL ||
      block_count > INT_MAX / temp_count) {
    fprintf(stderr, "register_allocator_core: liveness_storage function=%s blocks=%d temps=%d cells=0 buffer_bytes=0 total_bytes=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_count, temp_count);
    return FAILED;
  }

  set_count = block_count * temp_count;
  if ((size_t)set_count > ((size_t)-1) / sizeof(char) ||
      (size_t)set_count > (size_t)ULONG_MAX / sizeof(char) ||
      (size_t)set_count * sizeof(char) > ((size_t)-1) / 4 ||
      (size_t)set_count * sizeof(char) > (size_t)ULONG_MAX / 4) {
    fprintf(stderr, "register_allocator_core: liveness_storage function=%s blocks=%d temps=%d cells=0 buffer_bytes=0 total_bytes=0 status=invalid_input\n",
        function_name, block_count, temp_count);
    return FAILED;
  }

  storage->set_count = set_count;
  storage->buffer_bytes = (size_t)set_count * sizeof(char);
  storage->total_bytes = storage->buffer_bytes * 4;
  fprintf(stderr, "register_allocator_core: liveness_storage function=%s blocks=%d temps=%d cells=%d buffer_bytes=%lu total_bytes=%lu status=complete\n",
      function_name, block_count, temp_count, storage->set_count,
      (unsigned long)storage->buffer_bytes, (unsigned long)storage->total_bytes);
  return SUCCEEDED;
}


int register_allocator_resolve_active_slot_roles(char *function_name, struct register_allocator_target_policy *policy, int *physical_registers, int role_count, int *slot_indices) {

  int role_index;
  int previous_role;
  int slot_count;

  if (slot_indices != NULL && role_count > 0) {
    for (role_index = 0; role_index < role_count; role_index++)
      slot_indices[role_index] = -1;
  }

  if (function_name == NULL || policy == NULL || policy->name == NULL || policy->name[0] == '\0' ||
      policy->get_active_slot_count == NULL || policy->get_active_slot_index == NULL ||
      policy->get_active_slot_name == NULL || physical_registers == NULL || role_count <= 0 ||
      slot_indices == NULL) {
    fprintf(stderr, "register_allocator_core: active_slot_roles function=%s target=%s roles=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        policy != NULL && policy->name != NULL ? policy->name : "<null>", role_count);
    return FAILED;
  }

  slot_count = policy->get_active_slot_count();
  if (slot_count <= 0)
    goto invalid_policy;

  for (role_index = 0; role_index < role_count; role_index++) {
    char *slot_name;

    slot_indices[role_index] = policy->get_active_slot_index(physical_registers[role_index]);
    slot_name = policy->get_active_slot_name(physical_registers[role_index]);
    if (slot_indices[role_index] < 0 || slot_indices[role_index] >= slot_count || slot_name == NULL || slot_name[0] == '\0') {
      fprintf(stderr, "register_allocator_core: active_slot_role function=%s target=%s role=%d phy=%d slot=%d name=%s status=invalid_policy\n",
          function_name, policy->name, role_index, physical_registers[role_index], slot_indices[role_index],
          slot_name != NULL ? slot_name : "<null>");
        goto invalid_policy;
    }
    for (previous_role = 0; previous_role < role_index; previous_role++) {
      if (slot_indices[previous_role] == slot_indices[role_index]) {
        fprintf(stderr, "register_allocator_core: active_slot_role function=%s target=%s role=%d phy=%d slot=%d name=%s duplicate_role=%d status=invalid_policy\n",
            function_name, policy->name, role_index, physical_registers[role_index], slot_indices[role_index],
            slot_name, previous_role);
        goto invalid_policy;
      }
    }
    fprintf(stderr, "register_allocator_core: active_slot_role function=%s target=%s role=%d phy=%d slot=%d name=%s status=complete\n",
        function_name, policy->name, role_index, physical_registers[role_index], slot_indices[role_index], slot_name);
  }

  fprintf(stderr, "register_allocator_core: active_slot_roles function=%s target=%s roles=%d slots=%d status=complete\n",
      function_name, policy->name, role_count, slot_count);
  return SUCCEEDED;

invalid_policy:
  for (role_index = 0; role_index < role_count; role_index++)
    slot_indices[role_index] = -1;
  fprintf(stderr, "register_allocator_core: active_slot_roles function=%s target=%s roles=%d slots=%d status=invalid_policy\n",
      function_name, policy->name, role_count, slot_count);
  return FAILED;
}


static int _add_basic_block(struct register_allocator_basic_block *blocks, int max_blocks, int *block_count, int start_tac, int end_tac, int end_reason) {

  if (start_tac < 0 || end_tac < start_tac)
    return SUCCEEDED;
  if (*block_count >= max_blocks)
    return FAILED;

  blocks[*block_count].start_tac = start_tac;
  blocks[*block_count].end_tac = end_tac;
  blocks[*block_count].end_reason = end_reason;
  (*block_count)++;
  return SUCCEEDED;
}


static void _clear_control_flow_storage(struct register_allocator_control_flow_storage *storage) {

  storage->max_blocks = 0;
  storage->max_edges = 0;
  storage->instruction_bytes = 0;
  storage->block_bytes = 0;
  storage->edge_bytes = 0;
}


int register_allocator_plan_control_flow_storage(char *function_name, int instruction_capacity,
    struct register_allocator_control_flow_storage *storage) {

  int edge_capacity;

  if (storage != NULL)
    _clear_control_flow_storage(storage);
  if (function_name == NULL || instruction_capacity <= 0 || storage == NULL ||
      instruction_capacity > INT_MAX / 2 ||
      (size_t)instruction_capacity > ((size_t)-1) / sizeof(struct register_allocator_instruction) ||
      (size_t)instruction_capacity > (size_t)ULONG_MAX / sizeof(struct register_allocator_instruction) ||
      (size_t)instruction_capacity > ((size_t)-1) / sizeof(struct register_allocator_basic_block) ||
      (size_t)instruction_capacity > (size_t)ULONG_MAX / sizeof(struct register_allocator_basic_block)) {
    fprintf(stderr, "register_allocator_core: control_flow_storage function=%s instructions=%d blocks=0 edges=0 instruction_bytes=0 block_bytes=0 edge_bytes=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", instruction_capacity);
    return FAILED;
  }

  edge_capacity = instruction_capacity * 2;
  if ((size_t)edge_capacity > ((size_t)-1) / sizeof(struct register_allocator_cfg_edge) ||
      (size_t)edge_capacity > (size_t)ULONG_MAX / sizeof(struct register_allocator_cfg_edge)) {
    fprintf(stderr, "register_allocator_core: control_flow_storage function=%s instructions=%d blocks=0 edges=0 instruction_bytes=0 block_bytes=0 edge_bytes=0 status=invalid_input\n",
        function_name, instruction_capacity);
    return FAILED;
  }

  storage->max_blocks = instruction_capacity;
  storage->max_edges = edge_capacity;
    storage->instruction_bytes = (size_t)instruction_capacity * sizeof(struct register_allocator_instruction);
  storage->block_bytes = (size_t)instruction_capacity * sizeof(struct register_allocator_basic_block);
  storage->edge_bytes = (size_t)edge_capacity * sizeof(struct register_allocator_cfg_edge);
    fprintf(stderr, "register_allocator_core: control_flow_storage function=%s instructions=%d blocks=%d edges=%d instruction_bytes=%lu block_bytes=%lu edge_bytes=%lu status=complete\n",
      function_name, instruction_capacity, storage->max_blocks, storage->max_edges,
      (unsigned long)storage->instruction_bytes, (unsigned long)storage->block_bytes,
      (unsigned long)storage->edge_bytes);
  return SUCCEEDED;
}


int register_allocator_build_basic_blocks(char *function_name, struct register_allocator_instruction *instructions, int instruction_count, struct register_allocator_basic_block *blocks, int max_blocks, int *block_count) {

  int instruction_index;
  int block_start;
  int last_tac;

  *block_count = 0;
  block_start = -1;
  last_tac = -1;

  for (instruction_index = 0; instruction_index < instruction_count; instruction_index++) {
    struct register_allocator_instruction *instruction;

    instruction = &instructions[instruction_index];
    if (instruction->active == NO)
      continue;

    if (instruction->is_label == YES) {
      if (_add_basic_block(blocks, max_blocks, block_count, block_start, last_tac, RA_BLOCK_END_LABEL) == FAILED) {
        fprintf(stderr, "register_allocator_core: block_build function=%s instructions=%d blocks=%d status=capacity_exceeded\n", function_name, instruction_count, *block_count);
        return FAILED;
      }
      block_start = instruction->tac_index;
      last_tac = instruction->tac_index;
      continue;
    }

    if (block_start < 0)
      block_start = instruction->tac_index;
    last_tac = instruction->tac_index;

    if (instruction->end_reason != RA_BLOCK_END_NONE) {
      if (_add_basic_block(blocks, max_blocks, block_count, block_start, instruction->tac_index, instruction->end_reason) == FAILED) {
        fprintf(stderr, "register_allocator_core: block_build function=%s instructions=%d blocks=%d status=capacity_exceeded\n", function_name, instruction_count, *block_count);
        return FAILED;
      }
      block_start = -1;
      last_tac = -1;
    }
  }

  if (_add_basic_block(blocks, max_blocks, block_count, block_start, last_tac, RA_BLOCK_END_FUNCTION_END) == FAILED) {
    fprintf(stderr, "register_allocator_core: block_build function=%s instructions=%d blocks=%d status=capacity_exceeded\n", function_name, instruction_count, *block_count);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_core: block_build function=%s instructions=%d blocks=%d algorithm=leader_partition status=complete\n", function_name, instruction_count, *block_count);
  return SUCCEEDED;
}


static struct register_allocator_instruction *_find_instruction(struct register_allocator_instruction *instructions, int instruction_count, int tac_index) {

  int instruction_index;

  for (instruction_index = 0; instruction_index < instruction_count; instruction_index++) {
    if (instructions[instruction_index].active == YES && instructions[instruction_index].tac_index == tac_index)
      return &instructions[instruction_index];
  }
  return NULL;
}


static int _find_label_block(struct register_allocator_instruction *instructions, int instruction_count, struct register_allocator_basic_block *blocks, int block_count, char *label) {

  int block_index;

  if (label == NULL)
    return -1;

  for (block_index = 0; block_index < block_count; block_index++) {
    struct register_allocator_instruction *instruction;

    instruction = _find_instruction(instructions, instruction_count, blocks[block_index].start_tac);
    if (instruction == NULL || instruction->is_label == NO || instruction->label == NULL)
      continue;
    if (strcmp(instruction->label, label) == 0)
      return block_index;
  }
  return -1;
}


static int _add_cfg_edge(struct register_allocator_cfg_edge *edges, int max_edges, int *edge_count, int from_block, int to_block, int kind, char *target) {

  if (*edge_count >= max_edges)
    return FAILED;

  edges[*edge_count].from_block = from_block;
  edges[*edge_count].to_block = to_block;
  edges[*edge_count].kind = kind;
  edges[*edge_count].target = target;
  (*edge_count)++;
  return SUCCEEDED;
}


int register_allocator_build_cfg(char *function_name, struct register_allocator_instruction *instructions, int instruction_count, struct register_allocator_basic_block *blocks, int block_count, struct register_allocator_cfg_edge *edges, int max_edges, int *edge_count) {

  int block_index;

  *edge_count = 0;
  for (block_index = 0; block_index < block_count; block_index++) {
    struct register_allocator_instruction *instruction;

    instruction = _find_instruction(instructions, instruction_count, blocks[block_index].end_tac);
    if (instruction == NULL) {
      fprintf(stderr, "register_allocator_core: cfg_build function=%s blocks=%d edges=%d status=missing_instruction\n", function_name, block_count, *edge_count);
      return FAILED;
    }

    if (_block_end_falls_through(blocks[block_index].end_reason) == YES) {
      if (block_index + 1 < block_count && _add_cfg_edge(edges, max_edges, edge_count, block_index, block_index + 1, RA_CFG_EDGE_FALLTHROUGH, NULL) == FAILED) {
        fprintf(stderr, "register_allocator_core: cfg_build function=%s blocks=%d edges=%d status=capacity_exceeded\n", function_name, block_count, *edge_count);
        return FAILED;
      }
      continue;
    }

    if (instruction->is_jump == YES) {
      int target_block;
      int kind;

      target_block = _find_label_block(instructions, instruction_count, blocks, block_count, instruction->jump_target);
      if (target_block < 0) {
        fprintf(stderr, "register_allocator_core: cfg_build function=%s blocks=%d edges=%d from_block=%d target=%s status=missing_target\n", function_name, block_count, *edge_count, block_index, instruction->jump_target != NULL ? instruction->jump_target : "<null>");
        return FAILED;
      }

      kind = instruction->is_conditional_jump == YES ? RA_CFG_EDGE_BRANCH_TRUE : RA_CFG_EDGE_JUMP;
      if (_add_cfg_edge(edges, max_edges, edge_count, block_index, target_block, kind, instruction->jump_target) == FAILED) {
        fprintf(stderr, "register_allocator_core: cfg_build function=%s blocks=%d edges=%d status=capacity_exceeded\n", function_name, block_count, *edge_count);
        return FAILED;
      }
      if (instruction->is_conditional_jump == YES && block_index + 1 < block_count) {
        if (_add_cfg_edge(edges, max_edges, edge_count, block_index, block_index + 1, RA_CFG_EDGE_BRANCH_FALSE, NULL) == FAILED) {
          fprintf(stderr, "register_allocator_core: cfg_build function=%s blocks=%d edges=%d status=capacity_exceeded\n", function_name, block_count, *edge_count);
          return FAILED;
        }
      }
    }
  }

  fprintf(stderr, "register_allocator_core: cfg_build function=%s blocks=%d edges=%d algorithm=label_resolved status=complete\n", function_name, block_count, *edge_count);
  return SUCCEEDED;
}


int register_allocator_find_next_use(char *function_name, int temp_index, int start_instruction, int end_instruction, int instruction_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp) {

  int instruction_index;

  if (start_instruction < 0 || start_instruction > instruction_count || end_instruction >= instruction_count || is_active == NULL || reads_temp == NULL || writes_temp == NULL) {
    fprintf(stderr, "register_allocator_core: next_use function=%s temp=r%d range=%d..%d status=invalid_range\n", function_name, temp_index, start_instruction, end_instruction);
    return -1;
  }

  if (start_instruction > end_instruction) {
    fprintf(stderr, "register_allocator_core: next_use function=%s temp=r%d range=%d..%d next=none stop=block_end status=complete\n", function_name, temp_index, start_instruction, end_instruction);
    return -1;
  }

  for (instruction_index = start_instruction; instruction_index <= end_instruction; instruction_index++) {
    if (is_active(context, instruction_index, temp_index) == NO)
      continue;
    if (reads_temp(context, instruction_index, temp_index) == YES) {
      fprintf(stderr, "register_allocator_core: next_use function=%s temp=r%d range=%d..%d next=%d stop=read status=found\n", function_name, temp_index, start_instruction, end_instruction, instruction_index);
      return instruction_index;
    }
    if (writes_temp(context, instruction_index, temp_index) == YES) {
      fprintf(stderr, "register_allocator_core: next_use function=%s temp=r%d range=%d..%d next=none stop=redefined at=%d status=complete\n", function_name, temp_index, start_instruction, end_instruction, instruction_index);
      return -1;
    }
  }

  fprintf(stderr, "register_allocator_core: next_use function=%s temp=r%d range=%d..%d next=none stop=block_end status=complete\n", function_name, temp_index, start_instruction, end_instruction);
  return -1;
}


static void _clear_candidate(struct register_allocator_candidate *candidate) {

  candidate->found = NO;
  candidate->temp_index = -1;
  candidate->producer_instruction = -1;
  candidate->consumer_instruction = -1;
  candidate->consumer_operand = -1;
  candidate->has_single_read = NO;
}


int register_allocator_discover_candidate(char *function_name, int producer_instruction, int block_end_instruction, int instruction_count, void *context, register_allocator_instruction_query get_produced_temp, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_query get_consumer_operand, struct register_allocator_candidate *candidate) {

  int temp_index;
  int consumer_instruction;
  int consumer_operand;
  int instruction_index;

  if (candidate != NULL)
    _clear_candidate(candidate);

  if (function_name == NULL || producer_instruction < 0 || producer_instruction >= instruction_count ||
      block_end_instruction < producer_instruction || block_end_instruction >= instruction_count ||
      get_produced_temp == NULL || is_active == NULL || reads_temp == NULL || writes_temp == NULL ||
      get_consumer_operand == NULL || candidate == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_discover function=%s producer=%d block_end=%d instructions=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", producer_instruction, block_end_instruction, instruction_count);
    return FAILED;
  }

  temp_index = get_produced_temp(context, producer_instruction, -1);
  if (temp_index < 0) {
    fprintf(stderr, "register_allocator_core: candidate_discover function=%s producer=%d block_end=%d status=not_found reason=not_temp_definition\n",
        function_name, producer_instruction, block_end_instruction);
    return SUCCEEDED;
  }

  consumer_instruction = register_allocator_find_next_use(function_name, temp_index, producer_instruction + 1,
      block_end_instruction, instruction_count, context, is_active, reads_temp, writes_temp);
  if (consumer_instruction < 0) {
    fprintf(stderr, "register_allocator_core: candidate_discover function=%s producer=%d temp=r%d block_end=%d status=not_found reason=no_next_read\n",
        function_name, producer_instruction, temp_index, block_end_instruction);
    return SUCCEEDED;
  }

  consumer_operand = get_consumer_operand(context, consumer_instruction, temp_index);
  if (consumer_operand < 0) {
    fprintf(stderr, "register_allocator_core: candidate_discover function=%s producer=%d temp=r%d consumer=%d status=not_found reason=unsupported_consumer\n",
        function_name, producer_instruction, temp_index, consumer_instruction);
    return SUCCEEDED;
  }

  candidate->found = YES;
  candidate->temp_index = temp_index;
  candidate->producer_instruction = producer_instruction;
  candidate->consumer_instruction = consumer_instruction;
  candidate->consumer_operand = consumer_operand;
  candidate->has_single_read = YES;

  if (writes_temp(context, consumer_instruction, temp_index) == NO) {
    for (instruction_index = consumer_instruction + 1; instruction_index < instruction_count; instruction_index++) {
      if (is_active(context, instruction_index, temp_index) == NO)
        continue;
      if (writes_temp(context, instruction_index, temp_index) == YES)
        break;
      if (reads_temp(context, instruction_index, temp_index) == YES) {
        candidate->has_single_read = NO;
        break;
      }
    }
  }

    fprintf(stderr, "register_allocator_core: candidate_discover function=%s producer=%d temp=r%d consumer=%d operand=%d single_read=%s block_end=%d read_scope=definition algorithm=definition_next_read status=found\n",
      function_name, producer_instruction, temp_index, consumer_instruction, consumer_operand,
      candidate->has_single_read == YES ? "yes" : "no", block_end_instruction);
  return SUCCEEDED;
}


static int _is_valid_candidate(struct register_allocator_candidate *candidate) {

  return candidate != NULL && candidate->found == YES && candidate->temp_index >= 0 &&
      candidate->producer_instruction >= 0 &&
      candidate->consumer_instruction > candidate->producer_instruction &&
      candidate->consumer_operand >= 0 &&
      (candidate->has_single_read == NO || candidate->has_single_read == YES);
}


int register_allocator_candidate_meets_range(char *function_name,
    struct register_allocator_candidate *candidate, int decision_instruction,
    int range_mode, int *eligible) {

  char *reason;

  if (eligible != NULL)
    *eligible = NO;
  if (function_name == NULL || _is_valid_candidate(candidate) == NO ||
      decision_instruction <= (candidate != NULL ? candidate->producer_instruction : -1) ||
      (range_mode != RA_CANDIDATE_RANGE_WITHIN_BLOCK &&
       range_mode != RA_CANDIDATE_RANGE_BEFORE_DECISION) || eligible == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_range function=%s temp=r%d producer=%d consumer=%d decision=%d mode=%d eligible=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        candidate != NULL ? candidate->temp_index : -1,
        candidate != NULL ? candidate->producer_instruction : -1,
        candidate != NULL ? candidate->consumer_instruction : -1,
        decision_instruction, range_mode);
    return FAILED;
  }

  if (range_mode == RA_CANDIDATE_RANGE_BEFORE_DECISION &&
      candidate->consumer_instruction >= decision_instruction) {
    *eligible = NO;
    reason = candidate->consumer_instruction == decision_instruction ?
        "consumer_at_decision" : "consumer_after_decision";
  }
  else {
    *eligible = YES;
    reason = range_mode == RA_CANDIDATE_RANGE_BEFORE_DECISION ?
        "consumer_before_decision" : "within_block";
  }

  fprintf(stderr, "register_allocator_core: candidate_range function=%s temp=r%d producer=%d consumer=%d decision=%d mode=%s eligible=%s reason=%s status=complete\n",
      function_name, candidate->temp_index, candidate->producer_instruction,
      candidate->consumer_instruction, decision_instruction,
      range_mode == RA_CANDIDATE_RANGE_BEFORE_DECISION ? "before_decision" : "within_block",
      *eligible == YES ? "yes" : "no", reason);
  return SUCCEEDED;
}


static void _clear_candidate_qualification(struct register_allocator_candidate_qualification *qualification) {

  qualification->eligible = NO;
  qualification->rejection_reason = RA_CANDIDATE_REJECTION_NONE;
  qualification->deciding_instruction = -1;
}


static char *_candidate_rejection_name(int rejection_reason) {

  if (rejection_reason == RA_CANDIDATE_REJECTION_NOT_FOUND)
    return "not_found";
  if (rejection_reason == RA_CANDIDATE_REJECTION_RANGE)
    return "range";
  if (rejection_reason == RA_CANDIDATE_REJECTION_METADATA)
    return "metadata";
  if (rejection_reason == RA_CANDIDATE_REJECTION_INTERVAL)
    return "interval";
  if (rejection_reason == RA_CANDIDATE_REJECTION_LEGALITY)
    return "legality";
  if (rejection_reason == RA_CANDIDATE_REJECTION_TRANSPARENCY)
    return "transparency";
  return "none";
}


int register_allocator_qualify_candidate(char *function_name, int physical_register,
    int no_physical_register, void *context, struct register_allocator_candidate *candidate,
    register_allocator_discovered_candidate_predicate has_metadata,
    register_allocator_discovered_candidate_predicate interval_is_clear,
    register_allocator_discovered_candidate_predicate is_allowed,
    register_allocator_discovered_candidate_predicate is_path_transparent,
    struct register_allocator_candidate_qualification *qualification) {

  int predicate_result;
  char *predicate_name;

  predicate_name = NULL;
  if (qualification != NULL)
    _clear_candidate_qualification(qualification);
  if (function_name == NULL || physical_register == no_physical_register || context == NULL ||
      _is_valid_candidate(candidate) == NO || has_metadata == NULL ||
      interval_is_clear == NULL || is_allowed == NULL || is_path_transparent == NULL ||
      qualification == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_qualification function=%s temp=r%d phy=%d eligible=no rejection=none deciding=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        candidate != NULL ? candidate->temp_index : -1, physical_register);
    return FAILED;
  }

  predicate_name = "metadata";
  predicate_result = has_metadata(context, candidate, physical_register);
  if (predicate_result != NO && predicate_result != YES)
    goto invalid_callback;
  if (predicate_result == NO) {
    qualification->rejection_reason = RA_CANDIDATE_REJECTION_METADATA;
    qualification->deciding_instruction = candidate->producer_instruction;
    goto rejected;
  }

  predicate_name = "interval";
  predicate_result = interval_is_clear(context, candidate, physical_register);
  if (predicate_result != NO && predicate_result != YES)
    goto invalid_callback;
  if (predicate_result == NO) {
    qualification->rejection_reason = RA_CANDIDATE_REJECTION_INTERVAL;
    qualification->deciding_instruction = candidate->consumer_instruction;
    goto rejected;
  }

  predicate_name = "legality";
  predicate_result = is_allowed(context, candidate, physical_register);
  if (predicate_result != NO && predicate_result != YES)
    goto invalid_callback;
  if (predicate_result == NO) {
    qualification->rejection_reason = RA_CANDIDATE_REJECTION_LEGALITY;
    qualification->deciding_instruction = candidate->consumer_instruction;
    goto rejected;
  }

  predicate_name = "transparency";
  predicate_result = is_path_transparent(context, candidate, physical_register);
  if (predicate_result != NO && predicate_result != YES)
    goto invalid_callback;
  if (predicate_result == NO) {
    qualification->rejection_reason = RA_CANDIDATE_REJECTION_TRANSPARENCY;
    qualification->deciding_instruction = candidate->consumer_instruction;
    goto rejected;
  }

  qualification->eligible = YES;
  fprintf(stderr, "register_allocator_core: candidate_qualification function=%s temp=r%d producer=%d consumer=%d operand=%d phy=%d eligible=yes rejection=none deciding=-1 status=complete\n",
      function_name, candidate->temp_index, candidate->producer_instruction,
      candidate->consumer_instruction, candidate->consumer_operand, physical_register);
  return SUCCEEDED;

rejected:
  fprintf(stderr, "register_allocator_core: candidate_qualification function=%s temp=r%d producer=%d consumer=%d operand=%d phy=%d eligible=no rejection=%s deciding=%d status=complete\n",
      function_name, candidate->temp_index, candidate->producer_instruction,
      candidate->consumer_instruction, candidate->consumer_operand, physical_register,
      _candidate_rejection_name(qualification->rejection_reason),
      qualification->deciding_instruction);
  return SUCCEEDED;

invalid_callback:
  _clear_candidate_qualification(qualification);
  fprintf(stderr, "register_allocator_core: candidate_qualification function=%s temp=r%d phy=%d eligible=no rejection=none deciding=-1 invalid_predicate=%s status=invalid_callback\n",
      function_name, candidate->temp_index, physical_register, predicate_name);
  return FAILED;
}


static void _clear_candidate_probe(struct register_allocator_candidate_probe *probe) {

  _clear_candidate(&probe->candidate);
  _clear_candidate_qualification(&probe->qualification);
}


int register_allocator_probe_competing_candidate(char *function_name,
    int producer_instruction, int block_end_instruction, int instruction_count,
    int decision_instruction, int range_mode, int physical_register,
    int no_physical_register, void *context,
    register_allocator_instruction_query get_produced_temp,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp,
    register_allocator_instruction_query get_consumer_operand,
    register_allocator_discovered_candidate_predicate has_metadata,
    register_allocator_discovered_candidate_predicate interval_is_clear,
    register_allocator_discovered_candidate_predicate is_allowed,
    register_allocator_discovered_candidate_predicate is_path_transparent,
    struct register_allocator_candidate_probe *probe) {

  int range_eligible;

  if (probe != NULL)
    _clear_candidate_probe(probe);
  if (function_name == NULL || producer_instruction < 0 || block_end_instruction < producer_instruction ||
      block_end_instruction >= instruction_count || instruction_count <= 0 ||
      decision_instruction <= producer_instruction || decision_instruction >= instruction_count ||
      (range_mode != RA_CANDIDATE_RANGE_WITHIN_BLOCK &&
       range_mode != RA_CANDIDATE_RANGE_BEFORE_DECISION) ||
      physical_register == no_physical_register || context == NULL ||
      get_produced_temp == NULL || is_active == NULL || reads_temp == NULL ||
      writes_temp == NULL || get_consumer_operand == NULL || has_metadata == NULL ||
      interval_is_clear == NULL || is_allowed == NULL || is_path_transparent == NULL ||
      probe == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_probe function=%s producer=%d block_end=%d decision=%d mode=%d phy=%d eligible=no rejection=none deciding=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", producer_instruction,
        block_end_instruction, decision_instruction, range_mode, physical_register);
    return FAILED;
  }

  if (register_allocator_discover_candidate(function_name, producer_instruction,
      block_end_instruction, instruction_count, context, get_produced_temp, is_active,
      reads_temp, writes_temp, get_consumer_operand, &probe->candidate) == FAILED)
    goto dependency_failed;
  if (probe->candidate.found == NO) {
    probe->qualification.rejection_reason = RA_CANDIDATE_REJECTION_NOT_FOUND;
    fprintf(stderr, "register_allocator_core: candidate_probe function=%s producer=%d block_end=%d decision=%d mode=%s phy=%d found=no eligible=no rejection=not_found deciding=-1 status=complete\n",
        function_name, producer_instruction, block_end_instruction, decision_instruction,
        range_mode == RA_CANDIDATE_RANGE_BEFORE_DECISION ? "before_decision" : "within_block",
        physical_register);
    return SUCCEEDED;
  }

  if (register_allocator_candidate_meets_range(function_name, &probe->candidate,
      decision_instruction, range_mode, &range_eligible) == FAILED)
    goto dependency_failed;
  if (range_eligible == NO) {
    probe->qualification.rejection_reason = RA_CANDIDATE_REJECTION_RANGE;
    probe->qualification.deciding_instruction = probe->candidate.consumer_instruction;
  }
  else if (register_allocator_qualify_candidate(function_name, physical_register,
      no_physical_register, context, &probe->candidate, has_metadata, interval_is_clear,
      is_allowed, is_path_transparent, &probe->qualification) == FAILED)
    goto dependency_failed;

  fprintf(stderr, "register_allocator_core: candidate_probe function=%s producer=%d block_end=%d decision=%d mode=%s phy=%d found=yes temp=r%d consumer=%d eligible=%s rejection=%s deciding=%d status=complete\n",
      function_name, producer_instruction, block_end_instruction, decision_instruction,
      range_mode == RA_CANDIDATE_RANGE_BEFORE_DECISION ? "before_decision" : "within_block",
      physical_register, probe->candidate.temp_index, probe->candidate.consumer_instruction,
      probe->qualification.eligible == YES ? "yes" : "no",
      _candidate_rejection_name(probe->qualification.rejection_reason),
      probe->qualification.deciding_instruction);
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_probe(probe);
  fprintf(stderr, "register_allocator_core: candidate_probe function=%s producer=%d block_end=%d decision=%d mode=%d phy=%d eligible=no rejection=none deciding=-1 status=dependency_failed\n",
      function_name, producer_instruction, block_end_instruction, decision_instruction,
      range_mode, physical_register);
  return FAILED;
}


int register_allocator_reset_live_intervals(char *function_name, struct register_allocator_live_interval *intervals, int interval_count) {

  int interval_index;

  if (intervals == NULL || interval_count < 0) {
    fprintf(stderr, "register_allocator_core: interval_reset function=%s capacity=%d status=invalid_input\n", function_name, interval_count);
    return FAILED;
  }

  for (interval_index = 0; interval_index < interval_count; interval_index++) {
    intervals[interval_index].used = NO;
    intervals[interval_index].size = 0;
    intervals[interval_index].read_count = 0;
    intervals[interval_index].write_count = 0;
    intervals[interval_index].live_start = -1;
    intervals[interval_index].live_end = -1;
  }

  fprintf(stderr, "register_allocator_core: interval_reset function=%s capacity=%d status=complete\n", function_name, interval_count);
  return SUCCEEDED;
}


int register_allocator_note_live_interval(char *function_name, struct register_allocator_live_interval *intervals, int interval_count, int temp_index, int size, int instruction_index, int is_write) {

  struct register_allocator_live_interval *interval;
  char *access_name;

  if (is_write == YES)
    access_name = "write";
  else if (is_write == NO)
    access_name = "read";
  else
    access_name = "invalid";

  if (intervals == NULL || temp_index < 0 || temp_index >= interval_count || size <= 0 || instruction_index < 0 || (is_write != NO && is_write != YES)) {
    fprintf(stderr, "register_allocator_core: interval_note function=%s temp=r%d size=%d instruction=%d access=%s status=invalid_input\n", function_name, temp_index, size, instruction_index, access_name);
    return FAILED;
  }

  interval = &intervals[temp_index];
  interval->used = YES;
  if (size > interval->size)
    interval->size = size;
  if (is_write == YES)
    interval->write_count++;
  else
    interval->read_count++;
  if (interval->live_start < 0 || instruction_index < interval->live_start)
    interval->live_start = instruction_index;
  if (instruction_index > interval->live_end)
    interval->live_end = instruction_index;

  fprintf(stderr, "register_allocator_core: interval_note function=%s temp=r%d size=%d instruction=%d access=%s range=%d..%d reads=%d writes=%d status=complete\n", function_name, temp_index, interval->size, instruction_index, access_name, interval->live_start, interval->live_end, interval->read_count, interval->write_count);
  return SUCCEEDED;
}


int register_allocator_count_live_intervals(char *function_name, struct register_allocator_live_interval *intervals, int interval_count) {

  int interval_index;
  int used_count;

  if (intervals == NULL || interval_count < 0) {
    fprintf(stderr, "register_allocator_core: interval_build function=%s capacity=%d status=invalid_input\n", function_name, interval_count);
    return -1;
  }

  used_count = 0;
  for (interval_index = 0; interval_index < interval_count; interval_index++) {
    if (intervals[interval_index].used == YES)
      used_count++;
  }

  fprintf(stderr, "register_allocator_core: interval_build function=%s capacity=%d intervals=%d algorithm=aggregate_access_range status=complete\n", function_name, interval_count, used_count);
  return used_count;
}


static void _clear_active_slot(struct register_allocator_active_slot *slot) {

  slot->temp_index = -1;
  slot->next_use = -1;
  slot->producer_instruction = -1;
  slot->consumer_operand = 0;
  slot->interval_retained = NO;
}


static int _is_valid_active_slot(struct register_allocator_active_slot *slot) {

  if (slot->interval_retained != NO && slot->interval_retained != YES)
    return NO;
  if (slot->temp_index < 0)
    return slot->temp_index == -1 && slot->next_use == -1 &&
        slot->producer_instruction == -1 && slot->consumer_operand == 0 &&
        slot->interval_retained == NO;
  return slot->producer_instruction >= 0 && slot->next_use > slot->producer_instruction &&
      slot->consumer_operand >= 0;
}


int register_allocator_reset_active_slots(char *function_name, int block_index, struct register_allocator_active_slot *slots, int slot_count) {

  int slot_index;

  if (function_name == NULL || block_index < 0 || slots == NULL || slot_count <= 0) {
    fprintf(stderr, "register_allocator_core: active_slots_reset function=%s block=%d slots=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, slot_count);
    return FAILED;
  }

  for (slot_index = 0; slot_index < slot_count; slot_index++)
    _clear_active_slot(&slots[slot_index]);

  fprintf(stderr, "register_allocator_core: active_slots_reset function=%s block=%d slots=%d status=complete\n",
      function_name, block_index, slot_count);
  return SUCCEEDED;
}


int register_allocator_expire_active_slots(char *function_name, int block_index, int instruction_index, struct register_allocator_active_slot *slots, int slot_count) {

  int slot_index;
  int expired_count;

  if (function_name == NULL || block_index < 0 || instruction_index < 0 || slots == NULL || slot_count <= 0) {
    fprintf(stderr, "register_allocator_core: active_slots_expire function=%s block=%d instruction=%d slots=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, instruction_index, slot_count);
    return FAILED;
  }

  for (slot_index = 0; slot_index < slot_count; slot_index++) {
    struct register_allocator_active_slot *slot = &slots[slot_index];

    if (_is_valid_active_slot(slot) == NO) {
      fprintf(stderr, "register_allocator_core: active_slots_expire function=%s block=%d instruction=%d slot=%d temp=r%d next=%d producer=%d operand=%d retained=%d status=invalid_state\n",
          function_name, block_index, instruction_index, slot_index, slot->temp_index, slot->next_use,
          slot->producer_instruction, slot->consumer_operand, slot->interval_retained);
      return FAILED;
    }
  }

  expired_count = 0;
  for (slot_index = 0; slot_index < slot_count; slot_index++) {
    struct register_allocator_active_slot *slot = &slots[slot_index];

    if (slot->temp_index < 0)
      continue;
    if (slot->next_use > instruction_index)
      continue;

    fprintf(stderr, "register_allocator_core: active_slot_expire function=%s block=%d instruction=%d slot=%d temp=r%d next=%d status=expired\n",
        function_name, block_index, instruction_index, slot_index, slot->temp_index, slot->next_use);
    _clear_active_slot(slot);
    expired_count++;
  }

  fprintf(stderr, "register_allocator_core: active_slots_expire function=%s block=%d instruction=%d slots=%d expired=%d status=complete\n",
      function_name, block_index, instruction_index, slot_count, expired_count);
  return SUCCEEDED;
}


int register_allocator_assign_active_slot(char *function_name, int block_index, int slot_index, struct register_allocator_active_slot *slots, int slot_count, int temp_index, int next_use, int producer_instruction, int consumer_operand, int interval_retained) {

  struct register_allocator_active_slot *slot;

  if (function_name == NULL || block_index < 0 || slots == NULL || slot_count <= 0 || slot_index < 0 || slot_index >= slot_count ||
      temp_index < 0 || producer_instruction < 0 || next_use <= producer_instruction || consumer_operand < 0 ||
      (interval_retained != NO && interval_retained != YES)) {
    fprintf(stderr, "register_allocator_core: active_slot_assign function=%s block=%d slot=%d slots=%d temp=r%d producer=%d next=%d operand=%d retained=%s status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, slot_index, slot_count, temp_index, producer_instruction, next_use, consumer_operand,
        interval_retained == YES ? "yes" : (interval_retained == NO ? "no" : "invalid"));
    return FAILED;
  }

  slot = &slots[slot_index];
  slot->temp_index = temp_index;
  slot->next_use = next_use;
  slot->producer_instruction = producer_instruction;
  slot->consumer_operand = consumer_operand;
  slot->interval_retained = interval_retained;

  fprintf(stderr, "register_allocator_core: active_slot_assign function=%s block=%d slot=%d temp=r%d producer=%d next=%d operand=%d retained=%s status=complete\n",
      function_name, block_index, slot_index, temp_index, producer_instruction, next_use, consumer_operand, interval_retained == YES ? "yes" : "no");
  return SUCCEEDED;
}


int register_allocator_linear_scan_choose(char *function_name, int block_index, char *slot_name, int instruction_index, int candidate_temp, int candidate_next_use, int active_temp, int active_next_use, int active_replaceable, int prefer_candidate_on_equal) {

  int decision;
  char *reason;

  if (function_name == NULL || slot_name == NULL || block_index < 0 || instruction_index < 0 || candidate_temp < 0 || candidate_next_use <= instruction_index ||
      (active_temp < 0 && active_next_use >= 0) || (active_temp >= 0 && active_next_use < 0) ||
      (active_replaceable != NO && active_replaceable != YES) ||
      (prefer_candidate_on_equal != NO && prefer_candidate_on_equal != YES)) {
    fprintf(stderr, "register_allocator_core: linear_scan_choose function=%s block=%d slot=%s instruction=%d candidate=r%d candidate_next=%d active=r%d active_next=%d active_replaceable=%s prefer_equal=%s status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, slot_name != NULL ? slot_name : "<null>", instruction_index,
        candidate_temp, candidate_next_use, active_temp, active_next_use,
        active_replaceable == YES ? "yes" : (active_replaceable == NO ? "no" : "invalid"),
        prefer_candidate_on_equal == YES ? "yes" : (prefer_candidate_on_equal == NO ? "no" : "invalid"));
    return FAILED;
  }

  if (active_temp < 0 || active_next_use <= instruction_index) {
    decision = RA_LINEAR_SCAN_ASSIGN;
    reason = active_temp < 0 ? "empty_slot" : "active_expired";
  }
  else if (active_replaceable == NO) {
    decision = RA_LINEAR_SCAN_KEEP_ACTIVE;
    reason = "active_unavailable";
  }
  else if (candidate_next_use < active_next_use) {
    decision = RA_LINEAR_SCAN_REPLACE_ACTIVE;
    reason = "nearer_next_use";
  }
  else if (candidate_next_use == active_next_use && prefer_candidate_on_equal == YES) {
    decision = RA_LINEAR_SCAN_REPLACE_ACTIVE;
    reason = "equal_next_use_preference";
  }
  else {
    decision = RA_LINEAR_SCAN_KEEP_ACTIVE;
    reason = candidate_next_use == active_next_use ? "equal_next_use" : "active_nearer";
  }

    fprintf(stderr, "register_allocator_core: linear_scan_choose function=%s block=%d slot=%s instruction=%d candidate=r%d candidate_next=%d active=r%d active_next=%d active_replaceable=%s prefer_equal=%s decision=%s reason=%s status=complete\n",
      function_name, block_index, slot_name, instruction_index, candidate_temp, candidate_next_use, active_temp, active_next_use,
      active_replaceable == YES ? "yes" : "no", prefer_candidate_on_equal == YES ? "yes" : "no",
      decision == RA_LINEAR_SCAN_ASSIGN ? "assign" : (decision == RA_LINEAR_SCAN_REPLACE_ACTIVE ? "replace_active" : "keep_active"), reason);
  return decision;
}


static void _clear_slot_transition(struct register_allocator_slot_transition *transition) {

  transition->decision = FAILED;
  transition->clear_displaced_interval = NO;
  transition->spill_displaced_temp = NO;
  transition->retain_candidate = NO;
  transition->assign_candidate = NO;
  transition->displaced_temp = -1;
  transition->displaced_producer_instruction = -1;
  transition->displaced_consumer_instruction = -1;
  transition->displaced_consumer_operand = -1;
}


int register_allocator_plan_slot_transition(char *function_name, int block_index,
    int linear_scan_decision, struct register_allocator_active_slot *active_slot,
    int candidate_temp, int candidate_next_use, int producer_instruction,
    int consumer_operand, struct register_allocator_slot_transition *transition) {

  if (transition != NULL)
    _clear_slot_transition(transition);
  if (function_name == NULL || block_index < 0 ||
      (linear_scan_decision != RA_LINEAR_SCAN_ASSIGN &&
       linear_scan_decision != RA_LINEAR_SCAN_REPLACE_ACTIVE &&
       linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE) || active_slot == NULL ||
      candidate_temp < 0 || producer_instruction < 0 ||
      candidate_next_use <= producer_instruction || consumer_operand < 0 || transition == NULL ||
      (active_slot != NULL && _is_valid_active_slot(active_slot) == NO)) {
    fprintf(stderr, "register_allocator_core: slot_transition_plan function=%s block=%d decision=%d candidate=r%d producer=%d next=%d operand=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, linear_scan_decision,
        candidate_temp, producer_instruction, candidate_next_use, consumer_operand);
    return FAILED;
  }

  if ((linear_scan_decision == RA_LINEAR_SCAN_ASSIGN && active_slot->temp_index >= 0) ||
      (linear_scan_decision == RA_LINEAR_SCAN_REPLACE_ACTIVE &&
       (active_slot->temp_index < 0 || active_slot->producer_instruction < 0 ||
        active_slot->next_use <= active_slot->producer_instruction ||
        active_slot->consumer_operand < 0))) {
    fprintf(stderr, "register_allocator_core: slot_transition_plan function=%s block=%d decision=%d candidate=r%d active=r%d active_producer=%d active_next=%d status=invalid_state\n",
        function_name, block_index, linear_scan_decision, candidate_temp, active_slot->temp_index,
        active_slot->producer_instruction, active_slot->next_use);
    return FAILED;
  }

  transition->decision = linear_scan_decision;
  if (linear_scan_decision == RA_LINEAR_SCAN_REPLACE_ACTIVE) {
    transition->clear_displaced_interval = active_slot->interval_retained;
    transition->spill_displaced_temp = YES;
    transition->displaced_temp = active_slot->temp_index;
    transition->displaced_producer_instruction = active_slot->producer_instruction;
    transition->displaced_consumer_instruction = active_slot->next_use;
    transition->displaced_consumer_operand = active_slot->consumer_operand;
  }
  if (linear_scan_decision != RA_LINEAR_SCAN_KEEP_ACTIVE) {
    transition->retain_candidate = YES;
    transition->assign_candidate = YES;
  }

  fprintf(stderr, "register_allocator_core: slot_transition_plan function=%s block=%d decision=%d candidate=r%d displaced=r%d clear=%s spill=%s retain=%s assign=%s status=complete\n",
      function_name, block_index, linear_scan_decision, candidate_temp,
      transition->displaced_temp, transition->clear_displaced_interval == YES ? "yes" : "no",
      transition->spill_displaced_temp == YES ? "yes" : "no",
      transition->retain_candidate == YES ? "yes" : "no",
      transition->assign_candidate == YES ? "yes" : "no");
  return SUCCEEDED;
}


static void _clear_candidate_transition_plan(
    struct register_allocator_candidate_transition_plan *plan) {

  plan->active_temp = -1;
  plan->active_next_use = -1;
  plan->active_replaceable = NO;
  plan->prefer_candidate_on_equal = NO;
  plan->linear_scan_decision = FAILED;
  _clear_slot_transition(&plan->transition);
}


int register_allocator_resolve_active_replaceability(char *function_name, int block_index,
    int instruction_index, struct register_allocator_active_slot *active_slot, void *context,
    register_allocator_temp_predicate has_temp_metadata, int *active_replaceable) {

  int metadata_available;

  if (active_replaceable != NULL)
    *active_replaceable = NO;
  if (function_name == NULL || block_index < 0 || instruction_index < 0 || active_slot == NULL ||
      context == NULL || has_temp_metadata == NULL || active_replaceable == NULL ||
      (active_slot != NULL && _is_valid_active_slot(active_slot) == NO)) {
    fprintf(stderr, "register_allocator_core: active_replaceability function=%s block=%d instruction=%d active=r%d active_next=%d queried=no replaceable=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, instruction_index,
        active_slot != NULL ? active_slot->temp_index : -1,
        active_slot != NULL ? active_slot->next_use : -1);
    return FAILED;
  }

  if (active_slot->temp_index < 0 || active_slot->next_use <= instruction_index) {
    *active_replaceable = YES;
    fprintf(stderr, "register_allocator_core: active_replaceability function=%s block=%d instruction=%d active=r%d active_next=%d queried=no replaceable=yes reason=%s status=complete\n",
        function_name, block_index, instruction_index, active_slot->temp_index,
        active_slot->next_use, active_slot->temp_index < 0 ? "empty_slot" : "active_expired");
    return SUCCEEDED;
  }

  metadata_available = has_temp_metadata(context, active_slot->temp_index);
  if (metadata_available != NO && metadata_available != YES) {
    fprintf(stderr, "register_allocator_core: active_replaceability function=%s block=%d instruction=%d active=r%d active_next=%d queried=yes replaceable=no status=invalid_callback\n",
        function_name, block_index, instruction_index, active_slot->temp_index,
        active_slot->next_use);
    return FAILED;
  }

  *active_replaceable = metadata_available;
  fprintf(stderr, "register_allocator_core: active_replaceability function=%s block=%d instruction=%d active=r%d active_next=%d queried=yes replaceable=%s reason=%s status=complete\n",
      function_name, block_index, instruction_index, active_slot->temp_index,
      active_slot->next_use, metadata_available == YES ? "yes" : "no",
      metadata_available == YES ? "metadata_available" : "metadata_missing");
  return SUCCEEDED;
}


int register_allocator_plan_candidate_transition(char *function_name, int block_index,
    char *slot_name, int instruction_index, int candidate_temp, int candidate_next_use,
    int consumer_op, int candidate_operand, int physical_register, int no_physical_register,
    struct register_allocator_active_slot *active_slot, struct register_allocator_target_policy *policy,
    void *context, register_allocator_temp_predicate has_temp_metadata,
    struct register_allocator_candidate_transition_plan *plan) {

  if (plan != NULL)
    _clear_candidate_transition_plan(plan);
  if (function_name == NULL || block_index < 0 || slot_name == NULL || slot_name[0] == '\0' ||
      instruction_index < 0 || candidate_temp < 0 || candidate_next_use <= instruction_index ||
      consumer_op < 0 || candidate_operand < 0 || physical_register == no_physical_register ||
      active_slot == NULL || policy == NULL || context == NULL || has_temp_metadata == NULL ||
      plan == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_transition function=%s block=%d slot=%s instruction=%d candidate=r%d next=%d phy=%d prefer=no decision=failed status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        slot_name != NULL && slot_name[0] != '\0' ? slot_name : "<null>", instruction_index,
        candidate_temp, candidate_next_use, physical_register);
    return FAILED;
  }

  plan->active_temp = active_slot->temp_index;
  plan->active_next_use = active_slot->next_use;

  if (register_allocator_resolve_active_replaceability(function_name, block_index,
      instruction_index, active_slot, context, has_temp_metadata,
      &plan->active_replaceable) == FAILED)
    goto dependency_failed;

  if (register_allocator_resolve_equal_next_use_preference(function_name, block_index, policy,
      consumer_op, candidate_operand, active_slot->consumer_operand, physical_register,
      no_physical_register, candidate_next_use, active_slot->next_use,
      &plan->prefer_candidate_on_equal) == FAILED)
    goto dependency_failed;

  plan->linear_scan_decision = register_allocator_linear_scan_choose(function_name, block_index,
      slot_name, instruction_index, candidate_temp, candidate_next_use, active_slot->temp_index,
      active_slot->next_use, plan->active_replaceable, plan->prefer_candidate_on_equal);
  if (plan->linear_scan_decision == FAILED)
    goto dependency_failed;

  if (register_allocator_plan_slot_transition(function_name, block_index,
      plan->linear_scan_decision, active_slot, candidate_temp, candidate_next_use,
      instruction_index, candidate_operand, &plan->transition) == FAILED)
    goto dependency_failed;

  fprintf(stderr, "register_allocator_core: candidate_transition function=%s block=%d slot=%s instruction=%d candidate=r%d next=%d active=r%d active_next=%d phy=%d replaceable=%s prefer=%s decision=%d clear=%s spill=%s retain=%s assign=%s status=complete\n",
      function_name, block_index, slot_name, instruction_index, candidate_temp,
      candidate_next_use, active_slot->temp_index, active_slot->next_use, physical_register,
      plan->active_replaceable == YES ? "yes" : "no",
      plan->prefer_candidate_on_equal == YES ? "yes" : "no",
      plan->linear_scan_decision,
      plan->transition.clear_displaced_interval == YES ? "yes" : "no",
      plan->transition.spill_displaced_temp == YES ? "yes" : "no",
      plan->transition.retain_candidate == YES ? "yes" : "no",
      plan->transition.assign_candidate == YES ? "yes" : "no");
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_transition_plan(plan);
  fprintf(stderr, "register_allocator_core: candidate_transition function=%s block=%d slot=%s instruction=%d candidate=r%d next=%d phy=%d prefer=no decision=failed status=dependency_failed\n",
      function_name, block_index, slot_name, instruction_index, candidate_temp,
      candidate_next_use, physical_register);
  return FAILED;
}


static void _clear_candidate_transition_preparation(
    struct register_allocator_candidate_transition_preparation *preparation) {

  preparation->slot_index = -1;
  preparation->slot_name = NULL;
  _clear_candidate_transition_plan(&preparation->plan);
}


int register_allocator_prepare_candidate_transition(char *function_name, int block_index,
    int instruction_index, int candidate_temp, int candidate_next_use, int consumer_op,
    int candidate_operand, int physical_register, int no_physical_register,
    struct register_allocator_active_slot *active_slots, int active_slot_count,
    struct register_allocator_target_policy *policy, void *context,
    register_allocator_temp_predicate has_temp_metadata,
    struct register_allocator_candidate_transition_preparation *preparation) {

  int policy_slot_count;

  if (preparation != NULL)
    _clear_candidate_transition_preparation(preparation);
  if (function_name == NULL || block_index < 0 || instruction_index < 0 ||
      candidate_temp < 0 || candidate_next_use <= instruction_index || consumer_op < 0 ||
      candidate_operand < 0 || physical_register == no_physical_register ||
      active_slots == NULL || active_slot_count <= 0 || policy == NULL ||
      policy->name == NULL || policy->name[0] == '\0' ||
      policy->get_active_slot_count == NULL || policy->get_active_slot_index == NULL ||
      policy->get_active_slot_name == NULL || context == NULL ||
      has_temp_metadata == NULL || preparation == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_transition_prepare function=%s block=%d target=%s instruction=%d candidate=r%d next=%d phy=%d slot=-1 name=<null> status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        policy != NULL && policy->name != NULL ? policy->name : "<null>",
        instruction_index, candidate_temp, candidate_next_use, physical_register);
    return FAILED;
  }

  policy_slot_count = policy->get_active_slot_count();
  preparation->slot_index = policy->get_active_slot_index(physical_register);
  preparation->slot_name = policy->get_active_slot_name(physical_register);
  if (policy_slot_count != active_slot_count || preparation->slot_index < 0 ||
      preparation->slot_index >= active_slot_count || preparation->slot_name == NULL ||
      preparation->slot_name[0] == '\0') {
    fprintf(stderr, "register_allocator_core: candidate_transition_prepare function=%s block=%d target=%s instruction=%d candidate=r%d next=%d phy=%d policy_slots=%d storage_slots=%d slot=%d name=%s status=invalid_policy\n",
        function_name, block_index, policy->name, instruction_index, candidate_temp,
        candidate_next_use, physical_register, policy_slot_count, active_slot_count,
        preparation->slot_index,
        preparation->slot_name != NULL ? preparation->slot_name : "<null>");
    _clear_candidate_transition_preparation(preparation);
    return FAILED;
  }

  if (register_allocator_plan_candidate_transition(function_name, block_index,
      preparation->slot_name, instruction_index, candidate_temp, candidate_next_use,
      consumer_op, candidate_operand, physical_register, no_physical_register,
      &active_slots[preparation->slot_index], policy, context, has_temp_metadata,
      &preparation->plan) == FAILED) {
    _clear_candidate_transition_preparation(preparation);
    fprintf(stderr, "register_allocator_core: candidate_transition_prepare function=%s block=%d target=%s instruction=%d candidate=r%d next=%d phy=%d slot=-1 name=<null> stage=transition_plan status=dependency_failed\n",
        function_name, block_index, policy->name, instruction_index, candidate_temp,
        candidate_next_use, physical_register);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_core: candidate_transition_prepare function=%s block=%d target=%s instruction=%d candidate=r%d next=%d phy=%d slot=%d name=%s decision=%d order=slot_resolve_transition_plan status=complete\n",
      function_name, block_index, policy->name, instruction_index, candidate_temp,
      candidate_next_use, physical_register, preparation->slot_index,
      preparation->slot_name, preparation->plan.linear_scan_decision);
  return SUCCEEDED;
}


static int _is_valid_slot_transition(struct register_allocator_slot_transition *transition) {

  if (transition->decision == RA_LINEAR_SCAN_KEEP_ACTIVE)
    return transition->clear_displaced_interval == NO && transition->spill_displaced_temp == NO &&
        transition->retain_candidate == NO && transition->assign_candidate == NO &&
        transition->displaced_temp == -1 && transition->displaced_producer_instruction == -1 &&
        transition->displaced_consumer_instruction == -1 && transition->displaced_consumer_operand == -1;
  if (transition->decision == RA_LINEAR_SCAN_ASSIGN)
    return transition->clear_displaced_interval == NO && transition->spill_displaced_temp == NO &&
        transition->retain_candidate == YES && transition->assign_candidate == YES &&
        transition->displaced_temp == -1 && transition->displaced_producer_instruction == -1 &&
        transition->displaced_consumer_instruction == -1 && transition->displaced_consumer_operand == -1;
  if (transition->decision == RA_LINEAR_SCAN_REPLACE_ACTIVE)
    return (transition->clear_displaced_interval == NO || transition->clear_displaced_interval == YES) &&
        transition->spill_displaced_temp == YES && transition->retain_candidate == YES &&
        transition->assign_candidate == YES && transition->displaced_temp >= 0 &&
        transition->displaced_producer_instruction >= 0 &&
        transition->displaced_consumer_instruction > transition->displaced_producer_instruction &&
        transition->displaced_consumer_operand >= 0;
  return NO;
}


static int _slot_matches_transition(struct register_allocator_active_slot *slot,
    struct register_allocator_slot_transition *transition) {

  if (_is_valid_active_slot(slot) == NO)
    return NO;
  if (transition->decision == RA_LINEAR_SCAN_ASSIGN)
    return slot->temp_index == -1;
  if (transition->decision == RA_LINEAR_SCAN_REPLACE_ACTIVE)
    return slot->temp_index == transition->displaced_temp &&
        slot->producer_instruction == transition->displaced_producer_instruction &&
        slot->next_use == transition->displaced_consumer_instruction &&
        slot->consumer_operand == transition->displaced_consumer_operand &&
        slot->interval_retained == transition->clear_displaced_interval;
  return YES;
}


int register_allocator_apply_slot_transition(char *function_name, int block_index,
    int slot_index, struct register_allocator_active_slot *slots, int slot_count,
    int physical_register, int no_physical_register, int candidate_temp,
    int candidate_next_use, int producer_instruction, int consumer_operand, void *context,
    struct register_allocator_slot_transition *transition,
    register_allocator_clear_interval_callback clear_interval,
    register_allocator_spill_temp_callback spill_temp,
    register_allocator_retain_candidate_callback retain_candidate, int *retained_interval) {

  int retained;

  retained = NO;
  if (retained_interval != NULL)
    *retained_interval = NO;
  if (function_name == NULL || block_index < 0 || slot_index < 0 || slots == NULL ||
      slot_count <= 0 || slot_index >= slot_count || physical_register == no_physical_register ||
      candidate_temp < 0 || producer_instruction < 0 || candidate_next_use <= producer_instruction ||
      consumer_operand < 0 || transition == NULL || retained_interval == NULL ||
      (transition != NULL && _is_valid_slot_transition(transition) == NO) ||
      (transition != NULL && slots != NULL && slot_index >= 0 && slot_index < slot_count &&
       _slot_matches_transition(&slots[slot_index], transition) == NO) ||
      (transition != NULL && transition->clear_displaced_interval == YES && clear_interval == NULL) ||
      (transition != NULL && transition->spill_displaced_temp == YES && spill_temp == NULL) ||
      (transition != NULL && transition->retain_candidate == YES && retain_candidate == NULL)) {
    fprintf(stderr, "register_allocator_core: slot_transition_apply function=%s block=%d slot=%d candidate=r%d phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, slot_index,
        candidate_temp, physical_register);
    return FAILED;
  }

  if (transition->clear_displaced_interval == YES &&
      clear_interval(context, transition->displaced_producer_instruction,
      transition->displaced_consumer_instruction,
      transition->displaced_consumer_operand) == FAILED)
    goto callback_failed;
  if (transition->spill_displaced_temp == YES &&
      spill_temp(context, transition->displaced_temp) == FAILED)
    goto callback_failed;
  if (transition->retain_candidate == YES) {
    if (retain_candidate(context, candidate_temp, physical_register, producer_instruction,
        candidate_next_use, consumer_operand, &retained) == FAILED)
      goto callback_failed;
    if (retained != NO && retained != YES)
      goto invalid_callback;
  }
  if (transition->assign_candidate == YES && register_allocator_assign_active_slot(
      function_name, block_index, slot_index, slots, slot_count, candidate_temp,
      candidate_next_use, producer_instruction, consumer_operand, retained) == FAILED)
    goto callback_failed;

  *retained_interval = retained;
  fprintf(stderr, "register_allocator_core: slot_transition_apply function=%s block=%d slot=%d decision=%d candidate=r%d displaced=r%d retained=%s order=clear_spill_retain_assign status=complete\n",
      function_name, block_index, slot_index, transition->decision, candidate_temp,
      transition->displaced_temp, retained == YES ? "yes" : "no");
  return SUCCEEDED;

invalid_callback:
  fprintf(stderr, "register_allocator_core: slot_transition_apply function=%s block=%d slot=%d decision=%d candidate=r%d retained=invalid callback=retain_candidate status=invalid_callback\n",
      function_name, block_index, slot_index, transition->decision, candidate_temp);
  return FAILED;

callback_failed:
  fprintf(stderr, "register_allocator_core: slot_transition_apply function=%s block=%d slot=%d decision=%d candidate=r%d displaced=r%d retained=no order=clear_spill_retain_assign status=callback_failed\n",
      function_name, block_index, slot_index, transition->decision, candidate_temp,
      transition->displaced_temp);
  return FAILED;
}


static int _is_valid_temp_state(struct register_allocator_temp_state *state, int no_physical_register) {

  if (state->spill_required != NO && state->spill_required != YES)
    return NO;
  if (state->spill_required == YES && state->physical_register != no_physical_register)
    return NO;
  if (state->spill_required == NO && state->physical_register == no_physical_register)
    return NO;
  return YES;
}


int register_allocator_retain_temp_state(char *function_name, int block_index, int temp_index, struct register_allocator_temp_state *state, int no_physical_register, int requested_physical_register, int has_spill_constraint, int read_count, int write_count) {

  int prior_spill_required;
  int prior_physical_register;
  int result;

  if (function_name == NULL || block_index < 0 || temp_index < 0 || state == NULL ||
      requested_physical_register == no_physical_register ||
      (has_spill_constraint != NO && has_spill_constraint != YES) || read_count < 0 || write_count < 0 ||
      (state != NULL && _is_valid_temp_state(state, no_physical_register) == NO)) {
    fprintf(stderr, "register_allocator_core: temp_state_retain function=%s block=%d temp=r%d requested_phy=%d spill_constraint=%s reads=%d writes=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, requested_physical_register,
        has_spill_constraint == YES ? "yes" : (has_spill_constraint == NO ? "no" : "invalid"), read_count, write_count);
    return FAILED;
  }

  prior_spill_required = state->spill_required;
  prior_physical_register = state->physical_register;
  if (has_spill_constraint == NO && read_count == 1 && write_count == 1) {
    state->spill_required = NO;
    state->physical_register = requested_physical_register;
    result = RA_TEMP_RETAIN_REGISTER_ONLY;
  }
  else
    result = RA_TEMP_RETAIN_SPILL_BACKED;

  fprintf(stderr, "register_allocator_core: temp_state_retain function=%s block=%d temp=r%d requested_phy=%d spill_constraint=%s reads=%d writes=%d prior_spill=%s prior_phy=%d spill=%s phy=%d storage=%s status=complete\n",
      function_name, block_index, temp_index, requested_physical_register, has_spill_constraint == YES ? "yes" : "no",
      read_count, write_count, prior_spill_required == YES ? "yes" : "no", prior_physical_register,
      state->spill_required == YES ? "yes" : "no", state->physical_register,
      result == RA_TEMP_RETAIN_REGISTER_ONLY ? "register_only" : "spill_backed");
  return result;
}


int register_allocator_spill_temp_state(char *function_name, int block_index, int temp_index, struct register_allocator_temp_state *state, int no_physical_register, char *reason) {

  int prior_spill_required;
  int prior_physical_register;

  if (function_name == NULL || block_index < 0 || temp_index < 0 || state == NULL || reason == NULL ||
      (state != NULL && _is_valid_temp_state(state, no_physical_register) == NO)) {
    fprintf(stderr, "register_allocator_core: temp_state_spill function=%s block=%d temp=r%d reason=%s status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, reason != NULL ? reason : "<null>");
    return FAILED;
  }

  prior_spill_required = state->spill_required;
  prior_physical_register = state->physical_register;
  state->spill_required = YES;
  state->physical_register = no_physical_register;

  fprintf(stderr, "register_allocator_core: temp_state_spill function=%s block=%d temp=r%d reason=%s prior_spill=%s prior_phy=%d spill=yes phy=%d status=complete\n",
      function_name, block_index, temp_index, reason, prior_spill_required == YES ? "yes" : "no",
      prior_physical_register, state->physical_register);
  return SUCCEEDED;
}


int register_allocator_classify_join_temp_state(char *function_name, int block_index, int predecessor_count, int temp_index, struct register_allocator_temp_state *state, int no_physical_register, int *stack_resident) {

  if (stack_resident != NULL)
    *stack_resident = NO;
  if (function_name == NULL || block_index < 0 || predecessor_count <= 1 ||
      temp_index < 0 || state == NULL || stack_resident == NULL ||
      (state != NULL && _is_valid_temp_state(state, no_physical_register) == NO)) {
    fprintf(stderr, "register_allocator_core: join_temp_state function=%s block=%d predecessors=%d temp=r%d spill=%s phy=%d no_phy=%d stack=no state=invalid status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        predecessor_count, temp_index,
        state != NULL ? (state->spill_required == YES ? "yes" :
        (state->spill_required == NO ? "no" : "invalid")) : "invalid",
        state != NULL ? state->physical_register : no_physical_register,
        no_physical_register);
    return FAILED;
  }

  *stack_resident = state->spill_required;
  fprintf(stderr, "register_allocator_core: join_temp_state function=%s block=%d predecessors=%d temp=r%d spill=%s phy=%d no_phy=%d stack=%s state=%s status=complete\n",
      function_name, block_index, predecessor_count, temp_index,
      state->spill_required == YES ? "yes" : "no", state->physical_register,
      no_physical_register, *stack_resident == YES ? "yes" : "no",
      *stack_resident == YES ? "stack" : "retained");
  return SUCCEEDED;
}


int register_allocator_plan_join_action(char *function_name, int block_index, int predecessor_count, int temp_index, int stack_resident, int *action) {

  if (action != NULL)
    *action = 0;
  if (function_name == NULL || block_index < 0 || predecessor_count <= 1 ||
      temp_index < 0 || (stack_resident != NO && stack_resident != YES) ||
      action == NULL) {
    fprintf(stderr, "register_allocator_core: join_action_plan function=%s block=%d predecessors=%d temp=r%d stack=%s action=invalid status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        predecessor_count, temp_index, stack_resident == YES ? "yes" :
        (stack_resident == NO ? "no" : "invalid"));
    return FAILED;
  }

  *action = stack_resident == YES ? RA_JOIN_ACTION_RELOAD_ON_DEMAND :
      RA_JOIN_ACTION_SPILL_PREDECESSORS;
  fprintf(stderr, "register_allocator_core: join_action_plan function=%s block=%d predecessors=%d temp=r%d stack=%s action=%s status=complete\n",
      function_name, block_index, predecessor_count, temp_index,
      stack_resident == YES ? "yes" : "no",
      *action == RA_JOIN_ACTION_RELOAD_ON_DEMAND ? "reload_on_demand" :
      "spill_predecessors");
  return SUCCEEDED;
}


int register_allocator_collect_join_predecessors(char *function_name,
    int block_count, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int *predecessors, int predecessor_capacity, int *predecessor_count) {

  int edge_index;
  int found_count;
  int output_index;

  if (predecessor_count != NULL)
    *predecessor_count = 0;
  if (function_name == NULL || block_count <= 0 || join_block_index < 0 ||
      join_block_index >= block_count || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || predecessor_count == NULL ||
      (predecessors == NULL && predecessor_capacity != 0) ||
      (predecessors != NULL && predecessor_capacity < 0)) {
    fprintf(stderr, "register_allocator_core: join_predecessors function=%s block=%d blocks=%d edges=%d capacity=%d predecessors=0 mode=%s status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        block_count, edge_count, predecessor_capacity,
        predecessors == NULL ? "count" : "collect");
    return FAILED;
  }

  found_count = 0;
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count) {
      fprintf(stderr, "register_allocator_core: join_predecessors function=%s block=%d blocks=%d edges=%d capacity=%d predecessors=0 mode=%s edge=%d from_block=%d to_block=%d status=invalid_edge\n",
          function_name, join_block_index, block_count, edge_count,
          predecessor_capacity, predecessors == NULL ? "count" : "collect",
          edge_index, edges[edge_index].from_block, edges[edge_index].to_block);
      return FAILED;
    }
    if (edges[edge_index].to_block == join_block_index)
      found_count++;
  }

  if (predecessors != NULL && found_count > predecessor_capacity) {
    fprintf(stderr, "register_allocator_core: join_predecessors function=%s block=%d blocks=%d edges=%d capacity=%d predecessors=%d mode=collect status=insufficient_capacity\n",
        function_name, join_block_index, block_count, edge_count,
        predecessor_capacity, found_count);
    return FAILED;
  }

  output_index = 0;
  if (predecessors != NULL) {
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (edges[edge_index].to_block == join_block_index)
        predecessors[output_index++] = edges[edge_index].from_block;
    }
  }
  *predecessor_count = found_count;
  fprintf(stderr, "register_allocator_core: join_predecessors function=%s block=%d blocks=%d edges=%d capacity=%d predecessors=%d mode=%s status=complete\n",
      function_name, join_block_index, block_count, edge_count,
      predecessor_capacity, found_count,
      predecessors == NULL ? "count" : "collect");
  return SUCCEEDED;
}


static char *_register_allocator_join_edge_name(int edge_kind) {

  if (edge_kind == RA_CFG_EDGE_FALLTHROUGH)
    return "fallthrough";
  if (edge_kind == RA_CFG_EDGE_JUMP)
    return "jump";
  if (edge_kind == RA_CFG_EDGE_BRANCH_TRUE)
    return "branch_true";
  if (edge_kind == RA_CFG_EDGE_BRANCH_FALSE)
    return "branch_false";
  return "invalid";
}


static char *_register_allocator_join_spill_placement_name(int placement) {

  if (placement == RA_JOIN_SPILL_BEFORE_ANCHOR)
    return "before";
  if (placement == RA_JOIN_SPILL_AFTER_ANCHOR)
    return "after";
  return "invalid";
}


int register_allocator_plan_join_spill_sites(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_join_spill_site *sites, int site_capacity,
    int *site_count) {

  int edge_index;
  int found_count;
  int output_index;

  if (site_count != NULL)
    *site_count = 0;
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || join_block_index < 0 ||
      join_block_index >= block_count || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || site_count == NULL ||
      (sites == NULL && site_capacity != 0) ||
      (sites != NULL && site_capacity < 0)) {
    fprintf(stderr, "register_allocator_core: join_spill_sites function=%s block=%d blocks=%d instructions=%d edges=%d capacity=%d sites=0 mode=%s status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        block_count, instruction_count, edge_count, site_capacity,
        sites == NULL ? "count" : "collect");
    return FAILED;
  }

  found_count = 0;
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    struct register_allocator_basic_block *predecessor;

    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count) {
      fprintf(stderr, "register_allocator_core: join_spill_sites function=%s block=%d blocks=%d instructions=%d edges=%d capacity=%d sites=0 mode=%s edge=%d from_block=%d to_block=%d status=invalid_edge\n",
          function_name, join_block_index, block_count, instruction_count,
          edge_count, site_capacity, sites == NULL ? "count" : "collect",
          edge_index, edges[edge_index].from_block,
          edges[edge_index].to_block);
      return FAILED;
    }
    if (edges[edge_index].to_block != join_block_index)
      continue;

    predecessor = &blocks[edges[edge_index].from_block];
    if (predecessor->start_tac < 0 ||
        predecessor->end_tac < predecessor->start_tac ||
        predecessor->end_tac >= instruction_count ||
        (edges[edge_index].kind == RA_CFG_EDGE_FALLTHROUGH &&
        (edges[edge_index].from_block + 1 != join_block_index ||
        _block_end_falls_through(predecessor->end_reason) == NO)) ||
        ((edges[edge_index].kind == RA_CFG_EDGE_JUMP ||
        edges[edge_index].kind == RA_CFG_EDGE_BRANCH_TRUE ||
        edges[edge_index].kind == RA_CFG_EDGE_BRANCH_FALSE) &&
        predecessor->end_reason != RA_BLOCK_END_JUMP) ||
        (edges[edge_index].kind == RA_CFG_EDGE_BRANCH_FALSE &&
        edges[edge_index].from_block + 1 != join_block_index) ||
        (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
        edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      fprintf(stderr, "register_allocator_core: join_spill_sites function=%s block=%d predecessor=%d edge=%s anchor=%d end_reason=%d placement=invalid status=invalid_site\n",
          function_name, join_block_index, edges[edge_index].from_block,
          _register_allocator_join_edge_name(edges[edge_index].kind),
          predecessor->end_tac, predecessor->end_reason);
      return FAILED;
    }
    found_count++;
  }

  if (sites != NULL && found_count > site_capacity) {
    fprintf(stderr, "register_allocator_core: join_spill_sites function=%s block=%d blocks=%d instructions=%d edges=%d capacity=%d sites=%d mode=collect status=insufficient_capacity\n",
        function_name, join_block_index, block_count, instruction_count,
        edge_count, site_capacity, found_count);
    return FAILED;
  }

  output_index = 0;
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    struct register_allocator_basic_block *predecessor;
    int placement;

    if (edges[edge_index].to_block != join_block_index)
      continue;
    predecessor = &blocks[edges[edge_index].from_block];
    placement = edges[edge_index].kind == RA_CFG_EDGE_FALLTHROUGH ?
        RA_JOIN_SPILL_AFTER_ANCHOR : RA_JOIN_SPILL_BEFORE_ANCHOR;
    if (sites != NULL) {
      sites[output_index].predecessor_block = edges[edge_index].from_block;
      sites[output_index].edge_kind = edges[edge_index].kind;
      sites[output_index].anchor_instruction = predecessor->end_tac;
      sites[output_index].placement = placement;
    }
    fprintf(stderr, "register_allocator_core: join_spill_site function=%s block=%d predecessor=%d edge=%s anchor=%d end_reason=%d placement=%s status=complete\n",
        function_name, join_block_index, edges[edge_index].from_block,
        _register_allocator_join_edge_name(edges[edge_index].kind),
        predecessor->end_tac, predecessor->end_reason,
        _register_allocator_join_spill_placement_name(placement));
    output_index++;
  }
  *site_count = found_count;
  fprintf(stderr, "register_allocator_core: join_spill_sites function=%s block=%d blocks=%d instructions=%d edges=%d capacity=%d sites=%d mode=%s status=complete\n",
      function_name, join_block_index, block_count, instruction_count,
      edge_count, site_capacity, found_count,
      sites == NULL ? "count" : "collect");
  return SUCCEEDED;
}


int register_allocator_order_join_spill_sites(char *function_name,
    int join_block_index, int instruction_count,
    struct register_allocator_join_spill_site *sites, int site_count) {

  int left_index;
  int right_index;

  if (function_name == NULL || join_block_index < 0 ||
      instruction_count <= 0 || site_count < 0 ||
      (site_count > 0 && sites == NULL)) {
    fprintf(stderr, "register_allocator_core: join_spill_order function=%s block=%d instructions=%d sites=%d order=descending_anchor status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        instruction_count, site_count);
    return FAILED;
  }

  for (left_index = 0; left_index < site_count; left_index++) {
    if (sites[left_index].predecessor_block < 0 ||
        sites[left_index].anchor_instruction < 0 ||
        sites[left_index].anchor_instruction >= instruction_count ||
        (sites[left_index].edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
        sites[left_index].edge_kind != RA_CFG_EDGE_JUMP &&
        sites[left_index].edge_kind != RA_CFG_EDGE_BRANCH_TRUE &&
        sites[left_index].edge_kind != RA_CFG_EDGE_BRANCH_FALSE) ||
        (sites[left_index].placement != RA_JOIN_SPILL_BEFORE_ANCHOR &&
        sites[left_index].placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
        (sites[left_index].edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
        sites[left_index].placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
        (sites[left_index].edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
        sites[left_index].placement != RA_JOIN_SPILL_BEFORE_ANCHOR)) {
      fprintf(stderr, "register_allocator_core: join_spill_order function=%s block=%d instructions=%d sites=%d site=%d predecessor=%d edge=%s anchor=%d placement=%s order=descending_anchor status=invalid_site\n",
          function_name, join_block_index, instruction_count, site_count,
          left_index, sites[left_index].predecessor_block,
          _register_allocator_join_edge_name(sites[left_index].edge_kind),
          sites[left_index].anchor_instruction,
          _register_allocator_join_spill_placement_name(
          sites[left_index].placement));
      return FAILED;
    }
  }

  for (left_index = 1; left_index < site_count; left_index++) {
    struct register_allocator_join_spill_site site;

    site = sites[left_index];
    right_index = left_index;
    while (right_index > 0 && sites[right_index - 1].anchor_instruction <
        site.anchor_instruction) {
      sites[right_index] = sites[right_index - 1];
      right_index--;
    }
    sites[right_index] = site;
  }
  for (left_index = 0; left_index < site_count; left_index++) {
    fprintf(stderr, "register_allocator_core: join_spill_order function=%s block=%d site=%d predecessor=%d edge=%s anchor=%d placement=%s order=descending_anchor status=complete\n",
        function_name, join_block_index, left_index,
        sites[left_index].predecessor_block,
        _register_allocator_join_edge_name(sites[left_index].edge_kind),
        sites[left_index].anchor_instruction,
        _register_allocator_join_spill_placement_name(
        sites[left_index].placement));
  }
  fprintf(stderr, "register_allocator_core: join_spill_order function=%s block=%d instructions=%d sites=%d order=descending_anchor status=complete\n",
      function_name, join_block_index, instruction_count, site_count);
  return SUCCEEDED;
}


int register_allocator_approve_join_spill_sites(char *function_name,
    int join_block_index, int instruction_count,
    struct register_allocator_join_spill_site *sites, int site_count,
    void *context, register_allocator_join_spill_site_approver approve_site,
    int *approved_count) {

  int approval;
  int site_index;

  if (approved_count != NULL)
    *approved_count = 0;
  if (function_name == NULL || join_block_index < 0 || instruction_count <= 0 ||
      site_count < 0 || (site_count > 0 && sites == NULL) ||
      (site_count > 0 && (context == NULL || approve_site == NULL)) ||
      approved_count == NULL) {
    fprintf(stderr, "register_allocator_core: join_spill_approval function=%s block=%d instructions=%d sites=%d approved=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        instruction_count, site_count);
    return FAILED;
  }

  for (site_index = 0; site_index < site_count; site_index++) {
    if (sites[site_index].predecessor_block < 0 ||
        sites[site_index].anchor_instruction < 0 ||
        sites[site_index].anchor_instruction >= instruction_count ||
        (sites[site_index].edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
        sites[site_index].edge_kind != RA_CFG_EDGE_JUMP &&
        sites[site_index].edge_kind != RA_CFG_EDGE_BRANCH_TRUE &&
        sites[site_index].edge_kind != RA_CFG_EDGE_BRANCH_FALSE) ||
        (sites[site_index].placement != RA_JOIN_SPILL_BEFORE_ANCHOR &&
        sites[site_index].placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
        (sites[site_index].edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
        sites[site_index].placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
        (sites[site_index].edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
        sites[site_index].placement != RA_JOIN_SPILL_BEFORE_ANCHOR)) {
      fprintf(stderr, "register_allocator_core: join_spill_approval function=%s block=%d site=%d predecessor=%d edge=%s anchor=%d placement=%s approved=no status=invalid_site\n",
          function_name, join_block_index, site_index,
          sites[site_index].predecessor_block,
          _register_allocator_join_edge_name(sites[site_index].edge_kind),
          sites[site_index].anchor_instruction,
          _register_allocator_join_spill_placement_name(
          sites[site_index].placement));
      return FAILED;
    }
  }

  for (site_index = 0; site_index < site_count; site_index++) {
    approval = approve_site(context, join_block_index, &sites[site_index]);
    if (approval != NO && approval != YES) {
      fprintf(stderr, "register_allocator_core: join_spill_approval function=%s block=%d site=%d predecessor=%d edge=%s anchor=%d placement=%s approved=invalid status=invalid_callback\n",
          function_name, join_block_index, site_index,
          sites[site_index].predecessor_block,
          _register_allocator_join_edge_name(sites[site_index].edge_kind),
          sites[site_index].anchor_instruction,
          _register_allocator_join_spill_placement_name(
          sites[site_index].placement));
      return FAILED;
    }
    fprintf(stderr, "register_allocator_core: join_spill_approval function=%s block=%d site=%d predecessor=%d edge=%s anchor=%d placement=%s approved=%s status=%s\n",
        function_name, join_block_index, site_index,
        sites[site_index].predecessor_block,
        _register_allocator_join_edge_name(sites[site_index].edge_kind),
        sites[site_index].anchor_instruction,
        _register_allocator_join_spill_placement_name(
        sites[site_index].placement), approval == YES ? "yes" : "no",
        approval == YES ? "complete" : "rejected");
    if (approval == NO)
      return FAILED;
  }

  *approved_count = site_count;
  fprintf(stderr, "register_allocator_core: join_spill_approval function=%s block=%d instructions=%d sites=%d approved=%d order=site_order policy=all_or_nothing status=complete\n",
      function_name, join_block_index, instruction_count, site_count,
      *approved_count);
  return SUCCEEDED;
}


static void _clear_join_spill_mutation(
    struct register_allocator_join_spill_mutation *mutation,
    int no_physical_register) {

  mutation->apply = NO;
  mutation->site.predecessor_block = -1;
  mutation->site.edge_kind = 0;
  mutation->site.anchor_instruction = -1;
  mutation->site.placement = 0;
  mutation->temp_index = -1;
  mutation->physical_register = no_physical_register;
}


int register_allocator_prepare_join_spill_mutation(char *function_name,
    int join_block_index, int temp_index, int join_action,
    int no_physical_register, int physical_register,
    struct register_allocator_join_spill_site *site,
    struct register_allocator_join_spill_mutation *mutation) {

  if (mutation != NULL)
    _clear_join_spill_mutation(mutation, no_physical_register);
  if (function_name == NULL || join_block_index < 0 || temp_index < 0 ||
      (join_action != RA_JOIN_ACTION_RELOAD_ON_DEMAND &&
      join_action != RA_JOIN_ACTION_SPILL_PREDECESSORS) || site == NULL ||
      mutation == NULL ||
      (site != NULL && (site->predecessor_block < 0 ||
      site->anchor_instruction < 0 ||
      (site->edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      site->edge_kind != RA_CFG_EDGE_JUMP &&
      site->edge_kind != RA_CFG_EDGE_BRANCH_TRUE &&
      site->edge_kind != RA_CFG_EDGE_BRANCH_FALSE) ||
      (site->placement != RA_JOIN_SPILL_BEFORE_ANCHOR &&
      site->placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (site->edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
      site->placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (site->edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      site->placement != RA_JOIN_SPILL_BEFORE_ANCHOR))) ||
      (join_action == RA_JOIN_ACTION_RELOAD_ON_DEMAND &&
      physical_register != no_physical_register) ||
      (join_action == RA_JOIN_ACTION_SPILL_PREDECESSORS &&
      physical_register == no_physical_register)) {
    fprintf(stderr, "register_allocator_core: join_spill_mutation function=%s block=%d temp=r%d predecessor=%d edge=%s anchor=%d placement=%s action=%s phy=%d apply=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index, site != NULL ? site->predecessor_block : -1,
        site != NULL ? _register_allocator_join_edge_name(site->edge_kind) :
        "invalid", site != NULL ? site->anchor_instruction : -1,
        site != NULL ? _register_allocator_join_spill_placement_name(
        site->placement) : "invalid",
        join_action == RA_JOIN_ACTION_RELOAD_ON_DEMAND ? "reload_on_demand" :
        (join_action == RA_JOIN_ACTION_SPILL_PREDECESSORS ?
        "spill_predecessors" : "invalid"), physical_register);
    return FAILED;
  }

  if (join_action == RA_JOIN_ACTION_RELOAD_ON_DEMAND) {
    fprintf(stderr, "register_allocator_core: join_spill_mutation function=%s block=%d temp=r%d predecessor=%d edge=%s anchor=%d placement=%s action=reload_on_demand phy=%d apply=no reason=stack_resident status=complete\n",
        function_name, join_block_index, temp_index, site->predecessor_block,
        _register_allocator_join_edge_name(site->edge_kind),
        site->anchor_instruction,
        _register_allocator_join_spill_placement_name(site->placement),
        physical_register);
    return SUCCEEDED;
  }

  mutation->apply = YES;
  mutation->site = *site;
  mutation->temp_index = temp_index;
  mutation->physical_register = physical_register;
  fprintf(stderr, "register_allocator_core: join_spill_mutation function=%s block=%d temp=r%d predecessor=%d edge=%s anchor=%d placement=%s action=spill_predecessors phy=%d apply=yes reason=retained_live_in status=complete\n",
      function_name, join_block_index, temp_index, site->predecessor_block,
      _register_allocator_join_edge_name(site->edge_kind),
      site->anchor_instruction,
      _register_allocator_join_spill_placement_name(site->placement),
      physical_register);
  return SUCCEEDED;
}


int register_allocator_apply_join_spill_mutation(char *function_name,
    int join_block_index, int no_physical_register, void *context,
    struct register_allocator_join_spill_mutation *mutation,
    register_allocator_join_spill_mutation_applier apply_mutation) {

  if (function_name == NULL || join_block_index < 0 || mutation == NULL ||
      (mutation != NULL && mutation->apply != NO && mutation->apply != YES) ||
      (mutation != NULL && mutation->apply == NO &&
      (mutation->site.predecessor_block != -1 ||
      mutation->site.edge_kind != 0 ||
      mutation->site.anchor_instruction != -1 ||
      mutation->site.placement != 0 || mutation->temp_index != -1 ||
      mutation->physical_register != no_physical_register)) ||
      (mutation != NULL && mutation->apply == YES &&
      (mutation->site.predecessor_block < 0 ||
      mutation->site.anchor_instruction < 0 || mutation->temp_index < 0 ||
      mutation->physical_register == no_physical_register ||
      (mutation->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      mutation->site.edge_kind != RA_CFG_EDGE_JUMP &&
      mutation->site.edge_kind != RA_CFG_EDGE_BRANCH_TRUE &&
      mutation->site.edge_kind != RA_CFG_EDGE_BRANCH_FALSE) ||
      (mutation->site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR &&
      mutation->site.placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (mutation->site.edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
      mutation->site.placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (mutation->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      mutation->site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR) ||
      apply_mutation == NULL))) {
    fprintf(stderr, "register_allocator_core: join_spill_mutation_apply function=%s block=%d apply=%s predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        mutation != NULL && mutation->apply == YES ? "yes" :
        (mutation != NULL && mutation->apply == NO ? "no" : "invalid"),
        mutation != NULL ? mutation->site.predecessor_block : -1,
        mutation != NULL ? _register_allocator_join_edge_name(
        mutation->site.edge_kind) : "invalid",
        mutation != NULL ? mutation->site.anchor_instruction : -1,
        mutation != NULL ? _register_allocator_join_spill_placement_name(
        mutation->site.placement) : "invalid",
        mutation != NULL ? mutation->temp_index : -1,
        mutation != NULL ? mutation->physical_register : no_physical_register);
    return FAILED;
  }

  if (mutation->apply == NO) {
    fprintf(stderr, "register_allocator_core: join_spill_mutation_apply function=%s block=%d apply=no status=skipped reason=empty_plan\n",
        function_name, join_block_index);
    return SUCCEEDED;
  }

  if (apply_mutation(context, join_block_index, &mutation->site,
      mutation->temp_index, mutation->physical_register) == FAILED) {
    fprintf(stderr, "register_allocator_core: join_spill_mutation_apply function=%s block=%d apply=yes predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d status=callback_failed\n",
        function_name, join_block_index, mutation->site.predecessor_block,
        _register_allocator_join_edge_name(mutation->site.edge_kind),
        mutation->site.anchor_instruction,
        _register_allocator_join_spill_placement_name(
        mutation->site.placement), mutation->temp_index,
        mutation->physical_register);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_core: join_spill_mutation_apply function=%s block=%d apply=yes predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d status=complete\n",
      function_name, join_block_index, mutation->site.predecessor_block,
      _register_allocator_join_edge_name(mutation->site.edge_kind),
      mutation->site.anchor_instruction,
      _register_allocator_join_spill_placement_name(mutation->site.placement),
      mutation->temp_index, mutation->physical_register);
  return SUCCEEDED;
}


static void _clear_join_spill_emission(
    struct register_allocator_join_spill_emission *emission,
    int no_physical_register) {

  emission->emit = NO;
  emission->site.predecessor_block = -1;
  emission->site.edge_kind = 0;
  emission->site.anchor_instruction = -1;
  emission->site.placement = 0;
  emission->temp_index = -1;
  emission->physical_register = no_physical_register;
  emission->destination_offset = 0;
  emission->byte_count = 0;
}


int register_allocator_prepare_join_spill_emission(char *function_name,
    int join_block_index, int no_physical_register,
    struct register_allocator_join_spill_mutation *mutation,
    int spill_available, int destination_offset, int byte_count,
    struct register_allocator_join_spill_emission *emission) {

  struct register_allocator_spill_emission spill_emission;
  int valid_mutation;

  if (emission != NULL)
    _clear_join_spill_emission(emission, no_physical_register);
  valid_mutation = mutation != NULL &&
      (mutation->apply == NO || mutation->apply == YES) &&
      ((mutation->apply == NO &&
      mutation->site.predecessor_block == -1 &&
      mutation->site.edge_kind == 0 &&
      mutation->site.anchor_instruction == -1 &&
      mutation->site.placement == 0 && mutation->temp_index == -1 &&
      mutation->physical_register == no_physical_register) ||
      (mutation->apply == YES && mutation->site.predecessor_block >= 0 &&
      mutation->site.anchor_instruction >= 0 && mutation->temp_index >= 0 &&
      mutation->physical_register != no_physical_register &&
      (mutation->site.edge_kind == RA_CFG_EDGE_FALLTHROUGH ||
      mutation->site.edge_kind == RA_CFG_EDGE_JUMP ||
      mutation->site.edge_kind == RA_CFG_EDGE_BRANCH_TRUE ||
      mutation->site.edge_kind == RA_CFG_EDGE_BRANCH_FALSE) &&
      (mutation->site.placement == RA_JOIN_SPILL_BEFORE_ANCHOR ||
      mutation->site.placement == RA_JOIN_SPILL_AFTER_ANCHOR) &&
      ((mutation->site.edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
      mutation->site.placement == RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (mutation->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      mutation->site.placement == RA_JOIN_SPILL_BEFORE_ANCHOR))));
  if (function_name == NULL || join_block_index < 0 || valid_mutation == NO ||
      emission == NULL) {
    fprintf(stderr, "register_allocator_core: join_spill_emission function=%s block=%d apply=%s predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d spill=%s destination_offset=%d bytes=%d emit=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        mutation != NULL && mutation->apply == YES ? "yes" :
        (mutation != NULL && mutation->apply == NO ? "no" : "invalid"),
        mutation != NULL ? mutation->site.predecessor_block : -1,
        mutation != NULL ? _register_allocator_join_edge_name(
        mutation->site.edge_kind) : "invalid",
        mutation != NULL ? mutation->site.anchor_instruction : -1,
        mutation != NULL ? _register_allocator_join_spill_placement_name(
        mutation->site.placement) : "invalid",
        mutation != NULL ? mutation->temp_index : -1,
        mutation != NULL ? mutation->physical_register : no_physical_register,
        spill_available == YES ? "yes" :
        (spill_available == NO ? "no" : "invalid"), destination_offset,
        byte_count);
    return FAILED;
  }

  if (register_allocator_prepare_spill_emission(function_name,
      mutation->apply, mutation->temp_index, no_physical_register,
      mutation->physical_register, spill_available, destination_offset,
      byte_count, &spill_emission) == FAILED) {
    fprintf(stderr, "register_allocator_core: join_spill_emission function=%s block=%d apply=%s predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d spill=%s destination_offset=%d bytes=%d emit=no status=invalid_spill_emission\n",
        function_name, join_block_index,
        mutation->apply == YES ? "yes" : "no",
        mutation->site.predecessor_block,
        _register_allocator_join_edge_name(mutation->site.edge_kind),
        mutation->site.anchor_instruction,
        _register_allocator_join_spill_placement_name(
        mutation->site.placement), mutation->temp_index,
        mutation->physical_register, spill_available == YES ? "yes" :
        (spill_available == NO ? "no" : "invalid"), destination_offset,
        byte_count);
    return FAILED;
  }

  if (spill_emission.emit == NO) {
    fprintf(stderr, "register_allocator_core: join_spill_emission function=%s block=%d apply=no emit=no status=complete reason=empty_plan\n",
        function_name, join_block_index);
    return SUCCEEDED;
  }

  emission->emit = YES;
  emission->site = mutation->site;
  emission->temp_index = spill_emission.temp_index;
  emission->physical_register = spill_emission.physical_register;
  emission->destination_offset = spill_emission.destination_offset;
  emission->byte_count = spill_emission.byte_count;
  fprintf(stderr, "register_allocator_core: join_spill_emission function=%s block=%d apply=yes predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d spill=yes destination_offset=%d bytes=%d emit=yes status=complete\n",
      function_name, join_block_index, emission->site.predecessor_block,
      _register_allocator_join_edge_name(emission->site.edge_kind),
      emission->site.anchor_instruction,
      _register_allocator_join_spill_placement_name(emission->site.placement),
      emission->temp_index, emission->physical_register,
      emission->destination_offset, emission->byte_count);
  return SUCCEEDED;
}


int register_allocator_order_join_spill_emissions(char *function_name,
    int block_count, int instruction_count, int no_physical_register,
    struct register_allocator_join_spill_work_item *work_items,
    int work_item_count) {

  int left_index;
  int right_index;

  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      work_item_count < 0 || (work_item_count > 0 && work_items == NULL)) {
    fprintf(stderr, "register_allocator_core: join_spill_emission_order function=%s blocks=%d instructions=%d items=%d order=descending_anchor status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_count,
        instruction_count, work_item_count);
    return FAILED;
  }

  for (left_index = 0; left_index < work_item_count; left_index++) {
    struct register_allocator_join_spill_emission *emission;

    emission = &work_items[left_index].emission;
    if (work_items[left_index].join_block_index < 0 ||
        work_items[left_index].join_block_index >= block_count ||
        emission->emit != YES || emission->site.predecessor_block < 0 ||
        emission->site.anchor_instruction < 0 ||
        emission->site.anchor_instruction >= instruction_count ||
        emission->temp_index < 0 ||
        emission->physical_register == no_physical_register ||
        emission->byte_count <= 0 ||
        (emission->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
        emission->site.edge_kind != RA_CFG_EDGE_JUMP &&
        emission->site.edge_kind != RA_CFG_EDGE_BRANCH_TRUE &&
        emission->site.edge_kind != RA_CFG_EDGE_BRANCH_FALSE) ||
        (emission->site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR &&
        emission->site.placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
        (emission->site.edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
        emission->site.placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
        (emission->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
        emission->site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR)) {
        fprintf(stderr, "register_allocator_core: join_spill_emission_order function=%s blocks=%d instructions=%d items=%d item=%d block=%d emit=%s predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d destination_offset=%d bytes=%d order=descending_anchor status=invalid_item\n",
          function_name, block_count, instruction_count, work_item_count,
          left_index, work_items[left_index].join_block_index,
          emission->emit == YES ? "yes" :
          (emission->emit == NO ? "no" : "invalid"),
          emission->site.predecessor_block,
          _register_allocator_join_edge_name(emission->site.edge_kind),
          emission->site.anchor_instruction,
          _register_allocator_join_spill_placement_name(
          emission->site.placement), emission->temp_index,
          emission->physical_register, emission->destination_offset,
          emission->byte_count);
      return FAILED;
    }
  }

  for (left_index = 1; left_index < work_item_count; left_index++) {
    struct register_allocator_join_spill_work_item work_item;

    work_item = work_items[left_index];
    right_index = left_index;
    while (right_index > 0 &&
      (work_items[right_index - 1].emission.site.anchor_instruction <
      work_item.emission.site.anchor_instruction ||
      (work_items[right_index - 1].emission.site.anchor_instruction ==
      work_item.emission.site.anchor_instruction &&
      work_items[right_index - 1].emission.site.placement ==
      RA_JOIN_SPILL_BEFORE_ANCHOR &&
      work_item.emission.site.placement ==
      RA_JOIN_SPILL_AFTER_ANCHOR))) {
      work_items[right_index] = work_items[right_index - 1];
      right_index--;
    }
    work_items[right_index] = work_item;
  }
  for (left_index = 0; left_index < work_item_count; left_index++) {
    struct register_allocator_join_spill_emission *emission;

    emission = &work_items[left_index].emission;
    fprintf(stderr, "register_allocator_core: join_spill_emission_order function=%s item=%d block=%d predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d destination_offset=%d bytes=%d order=descending_anchor status=complete\n",
        function_name, left_index, work_items[left_index].join_block_index,
        emission->site.predecessor_block,
        _register_allocator_join_edge_name(
        emission->site.edge_kind), emission->site.anchor_instruction,
        _register_allocator_join_spill_placement_name(
        emission->site.placement), emission->temp_index,
        emission->physical_register, emission->destination_offset,
        emission->byte_count);
  }
  fprintf(stderr, "register_allocator_core: join_spill_emission_order function=%s blocks=%d instructions=%d items=%d order=descending_anchor status=complete\n",
      function_name, block_count, instruction_count, work_item_count);
  return SUCCEEDED;
}


int register_allocator_plan_post_mutation_rebuild(char *function_name,
    int original_instruction_count, int current_instruction_count,
    int inserted_instruction_count, int rewritten_operand_count,
    int added_temp_count, int *rebuild_required) {

  int mutation_count;

  if (rebuild_required != NULL)
    *rebuild_required = NO;
  if (function_name == NULL || original_instruction_count <= 0 ||
      current_instruction_count <= 0 || inserted_instruction_count < 0 ||
      rewritten_operand_count < 0 || added_temp_count < 0 ||
      rebuild_required == NULL ||
      original_instruction_count > INT_MAX - inserted_instruction_count ||
      inserted_instruction_count > INT_MAX - rewritten_operand_count ||
      inserted_instruction_count + rewritten_operand_count >
      INT_MAX - added_temp_count ||
      current_instruction_count !=
      original_instruction_count + inserted_instruction_count) {
    fprintf(stderr, "register_allocator_core: post_mutation_rebuild function=%s original_instructions=%d current_instructions=%d inserted=%d rewritten_operands=%d added_temps=%d rebuild=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        original_instruction_count, current_instruction_count,
        inserted_instruction_count, rewritten_operand_count,
        added_temp_count);
    return FAILED;
  }

  mutation_count = inserted_instruction_count + rewritten_operand_count +
      added_temp_count;
  *rebuild_required = mutation_count > 0 ? YES : NO;
  fprintf(stderr, "register_allocator_core: post_mutation_rebuild function=%s original_instructions=%d current_instructions=%d inserted=%d rewritten_operands=%d added_temps=%d rebuild=%s status=complete\n",
      function_name, original_instruction_count, current_instruction_count,
      inserted_instruction_count, rewritten_operand_count, added_temp_count,
      *rebuild_required == YES ? "yes" : "no");
  return SUCCEEDED;
}


int register_allocator_plan_fixed_point_limit(char *function_name,
    int instruction_capacity, int *pass_limit) {

  if (pass_limit != NULL)
    *pass_limit = 0;
  if (function_name == NULL || instruction_capacity <= 0 ||
      instruction_capacity == INT_MAX || pass_limit == NULL) {
    fprintf(stderr, "register_allocator_core: fixed_point_limit function=%s instruction_capacity=%d limit=0 basis=initial_instructions status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        instruction_capacity);
    return FAILED;
  }

  *pass_limit = instruction_capacity + 1;
  fprintf(stderr, "register_allocator_core: fixed_point_limit function=%s instruction_capacity=%d limit=%d basis=initial_instructions status=complete\n",
      function_name, instruction_capacity, *pass_limit);
  return SUCCEEDED;
}


int register_allocator_plan_fixed_point_step(char *function_name, int pass,
    int pass_limit, int rebuild_required,
    struct register_allocator_fixed_point_step *step) {

  char *action;

  if (step != NULL) {
    step->status = 0;
    step->pass = 0;
    step->pass_limit = 0;
  }
  if (function_name == NULL || pass <= 0 || pass_limit <= 0 ||
      pass > pass_limit ||
      (rebuild_required != NO && rebuild_required != YES) || step == NULL) {
    fprintf(stderr, "register_allocator_core: fixed_point_step function=%s pass=%d limit=%d rebuild=%s action=invalid status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", pass, pass_limit,
        rebuild_required == YES ? "yes" : "no");
    return FAILED;
  }

  step->pass = pass;
  step->pass_limit = pass_limit;
  if (rebuild_required == NO)
    step->status = RA_FIXED_POINT_STEP_STABLE;
  else if (pass == pass_limit)
    step->status = RA_FIXED_POINT_STEP_ITERATION_LIMIT;
  else
    step->status = RA_FIXED_POINT_STEP_REBUILD;
  action = step->status == RA_FIXED_POINT_STEP_REBUILD ? "rebuild" :
      (step->status == RA_FIXED_POINT_STEP_STABLE ? "stable" :
      "iteration_limit");
  fprintf(stderr, "register_allocator_core: fixed_point_step function=%s pass=%d limit=%d rebuild=%s action=%s status=complete\n",
      function_name, pass, pass_limit,
      rebuild_required == YES ? "yes" : "no", action);
  return SUCCEEDED;
}


int register_allocator_plan_join_retention(char *function_name,
    int block_count, int instruction_count, int join_block_index,
    int temp_index, int no_physical_register,
    struct register_allocator_join_retention_path *paths, int path_count,
    int consumer_supported, int *selected_physical_register) {

  char *reason;
  int definition_unsupported;
  int missing_definition;
  int path_index;
  int path_clobber;
  int physical_register;
  int register_mismatch;

  if (selected_physical_register != NULL)
    *selected_physical_register = no_physical_register;
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      join_block_index < 0 || join_block_index >= block_count ||
      temp_index < 0 || paths == NULL || path_count < 2 ||
      (consumer_supported != NO && consumer_supported != YES) ||
      selected_physical_register == NULL) {
    fprintf(stderr, "register_allocator_core: join_retention function=%s block=%d temp=r%d blocks=%d instructions=%d paths=%d consumer_supported=%s selected=%d eligible=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index, block_count, instruction_count, path_count,
        consumer_supported == YES ? "yes" :
        (consumer_supported == NO ? "no" : "invalid"),
        no_physical_register);
    return FAILED;
  }

  for (path_index = 0; path_index < path_count; path_index++) {
    int previous_path;

    if (paths[path_index].predecessor_block < 0 ||
        paths[path_index].predecessor_block >= block_count ||
        paths[path_index].predecessor_block == join_block_index ||
        (paths[path_index].definition_found != NO &&
        paths[path_index].definition_found != YES) ||
        (paths[path_index].definition_found == YES &&
        (paths[path_index].definition_instruction < 0 ||
        paths[path_index].definition_instruction >= instruction_count ||
        paths[path_index].anchor_instruction <
        paths[path_index].definition_instruction)) ||
        (paths[path_index].definition_found == NO &&
        (paths[path_index].definition_instruction != -1 ||
        paths[path_index].definition_supported != NO ||
        paths[path_index].path_transparent != NO)) ||
        paths[path_index].anchor_instruction >= instruction_count ||
        paths[path_index].physical_register == no_physical_register ||
        (paths[path_index].definition_supported != NO &&
        paths[path_index].definition_supported != YES) ||
        (paths[path_index].path_transparent != NO &&
        paths[path_index].path_transparent != YES)) {
      fprintf(stderr, "register_allocator_core: join_retention_path function=%s block=%d temp=r%d path=%d predecessor=%d definition=%d anchor=%d phy=%d definition_found=%s definition_supported=%s path_transparent=%s status=invalid_path\n",
          function_name, join_block_index, temp_index, path_index,
          paths[path_index].predecessor_block,
          paths[path_index].definition_instruction,
          paths[path_index].anchor_instruction,
          paths[path_index].physical_register,
          paths[path_index].definition_found == YES ? "yes" :
          (paths[path_index].definition_found == NO ? "no" : "invalid"),
          paths[path_index].definition_supported == YES ? "yes" :
          (paths[path_index].definition_supported == NO ? "no" : "invalid"),
          paths[path_index].path_transparent == YES ? "yes" :
          (paths[path_index].path_transparent == NO ? "no" : "invalid"));
      return FAILED;
    }
    for (previous_path = 0; previous_path < path_index; previous_path++) {
      if (paths[previous_path].predecessor_block ==
          paths[path_index].predecessor_block) {
        fprintf(stderr, "register_allocator_core: join_retention_path function=%s block=%d temp=r%d path=%d predecessor=%d definition=%d anchor=%d phy=%d definition_found=%s definition_supported=%s path_transparent=%s status=duplicate_predecessor\n",
            function_name, join_block_index, temp_index, path_index,
            paths[path_index].predecessor_block,
            paths[path_index].definition_instruction,
            paths[path_index].anchor_instruction,
            paths[path_index].physical_register,
            paths[path_index].definition_found == YES ? "yes" : "no",
            paths[path_index].definition_supported == YES ? "yes" : "no",
            paths[path_index].path_transparent == YES ? "yes" : "no");
        return FAILED;
      }
    }
  }

  physical_register = paths[0].physical_register;
  definition_unsupported = NO;
  missing_definition = NO;
  path_clobber = NO;
  register_mismatch = NO;
  for (path_index = 0; path_index < path_count; path_index++) {
    fprintf(stderr, "register_allocator_core: join_retention_path function=%s block=%d temp=r%d path=%d predecessor=%d definition=%d anchor=%d phy=%d definition_found=%s definition_supported=%s path_transparent=%s status=complete\n",
        function_name, join_block_index, temp_index, path_index,
        paths[path_index].predecessor_block,
        paths[path_index].definition_instruction,
        paths[path_index].anchor_instruction,
        paths[path_index].physical_register,
        paths[path_index].definition_found == YES ? "yes" : "no",
        paths[path_index].definition_supported == YES ? "yes" : "no",
        paths[path_index].path_transparent == YES ? "yes" : "no");
    if (paths[path_index].definition_found == NO)
      missing_definition = YES;
    else if (paths[path_index].definition_supported == NO)
      definition_unsupported = YES;
    if (paths[path_index].definition_found == YES &&
        paths[path_index].path_transparent == NO)
      path_clobber = YES;
    if (paths[path_index].physical_register != physical_register)
      register_mismatch = YES;
  }
  reason = "all_paths_proven";
  if (consumer_supported == NO)
    reason = "consumer_unsupported";
  else if (missing_definition == YES)
    reason = "missing_definition";
  else if (definition_unsupported == YES)
    reason = "definition_unsupported";
  else if (path_clobber == YES)
    reason = "path_clobber";
  else if (register_mismatch == YES)
    reason = "register_mismatch";
  if (strcmp(reason, "all_paths_proven") == 0)
    *selected_physical_register = physical_register;
  fprintf(stderr, "register_allocator_core: join_retention function=%s block=%d temp=r%d blocks=%d instructions=%d paths=%d consumer_supported=%s selected=%d eligible=%s reason=%s status=complete\n",
      function_name, join_block_index, temp_index, block_count,
      instruction_count, path_count, consumer_supported == YES ? "yes" : "no",
      *selected_physical_register,
      *selected_physical_register == no_physical_register ? "no" : "yes",
      reason);
  return SUCCEEDED;
}


int register_allocator_resolve_reaching_definition(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_reaching_definition_block *block_facts,
    int predecessor_block, int temp_index,
    struct register_allocator_reaching_definition *result) {

  int *definitions;
  char *transparent;
  int block_index;
  int changed;
  int edge_index;
  int iteration;
  int valid;

  if (result != NULL) {
    result->status = RA_REACHING_DEFINITION_MISSING;
    result->definition_instruction = -1;
    result->path_transparent = NO;
    result->iterations = 0;
  }
  valid = function_name != NULL && block_count > 0 && instruction_count > 0 &&
      edge_count >= 0 && (edge_count == 0 || edges != NULL) &&
      block_facts != NULL && predecessor_block >= 0 &&
      predecessor_block < block_count && temp_index >= 0 && result != NULL &&
      (size_t)block_count <= ((size_t)-1) / sizeof(int);
  if (valid == YES) {
    for (block_index = 0; block_index < block_count; block_index++) {
      if ((block_facts[block_index].definition_found != NO &&
          block_facts[block_index].definition_found != YES) ||
          (block_facts[block_index].path_transparent != NO &&
          block_facts[block_index].path_transparent != YES) ||
          (block_facts[block_index].definition_found == YES &&
          (block_facts[block_index].definition_instruction < 0 ||
          block_facts[block_index].definition_instruction >=
          instruction_count)) ||
          (block_facts[block_index].definition_found == NO &&
          block_facts[block_index].definition_instruction != -1)) {
        valid = NO;
        break;
      }
    }
  }
  if (valid == YES) {
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (edges[edge_index].from_block < 0 ||
          edges[edge_index].from_block >= block_count ||
          edges[edge_index].to_block < 0 ||
          edges[edge_index].to_block >= block_count ||
          (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
          edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
          edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
          edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
        valid = NO;
        break;
      }
    }
  }
  if (valid == NO) {
    fprintf(stderr, "register_allocator_core: reaching_definition function=%s predecessor=%d temp=r%d blocks=%d instructions=%d edges=%d definition=-1 transparent=no iterations=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", predecessor_block,
        temp_index, block_count, instruction_count, edge_count);
    return FAILED;
  }

  definitions = (int *)malloc((size_t)block_count * sizeof(int));
  transparent = (char *)malloc((size_t)block_count);
  if (definitions == NULL || transparent == NULL) {
    free(definitions);
    free(transparent);
    fprintf(stderr, "register_allocator_core: reaching_definition function=%s predecessor=%d temp=r%d blocks=%d instructions=%d edges=%d definition=-1 transparent=no iterations=0 status=out_of_memory\n",
        function_name, predecessor_block, temp_index, block_count,
        instruction_count, edge_count);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    definitions[block_index] = block_facts[block_index].definition_found == YES ?
        block_facts[block_index].definition_instruction : -1;
    transparent[block_index] =
      (char)block_facts[block_index].path_transparent;
  }

  iteration = 0;
  do {
    changed = NO;
    iteration++;
    for (block_index = 0; block_index < block_count; block_index++) {
      int incoming_definition;
      int incoming_transparent;

      if (block_facts[block_index].definition_found == YES)
        continue;
      incoming_definition = -1;
      incoming_transparent = YES;
      for (edge_index = 0; edge_index < edge_count; edge_index++) {
        int source_block;
        int source_definition;

        if (edges[edge_index].to_block != block_index)
          continue;
        source_block = edges[edge_index].from_block;
        source_definition = definitions[source_block];
        if (source_definition == -1)
          continue;
        if (source_definition == -2 || incoming_definition == -2 ||
            (incoming_definition >= 0 &&
            incoming_definition != source_definition))
          incoming_definition = -2;
        else
          incoming_definition = source_definition;
        if (transparent[source_block] == NO)
          incoming_transparent = NO;
      }
      incoming_transparent = block_facts[block_index].path_transparent == YES &&
          incoming_transparent == YES ? YES : NO;
      if (definitions[block_index] != incoming_definition ||
          transparent[block_index] != incoming_transparent) {
        definitions[block_index] = incoming_definition;
        transparent[block_index] = (char)incoming_transparent;
        changed = YES;
      }
    }
  } while (changed == YES && iteration <= block_count);

  result->iterations = iteration;
  if (definitions[predecessor_block] == -2)
    result->status = RA_REACHING_DEFINITION_AMBIGUOUS;
  else if (definitions[predecessor_block] >= 0) {
    result->status = RA_REACHING_DEFINITION_UNIQUE;
    result->definition_instruction = definitions[predecessor_block];
    result->path_transparent = transparent[predecessor_block];
  }
  fprintf(stderr, "register_allocator_core: reaching_definition function=%s predecessor=%d temp=r%d blocks=%d instructions=%d edges=%d definition=%d transparent=%s iterations=%d status=%s\n",
      function_name, predecessor_block, temp_index, block_count,
      instruction_count, edge_count, result->definition_instruction,
      result->path_transparent == YES ? "yes" : "no", result->iterations,
      result->status == RA_REACHING_DEFINITION_UNIQUE ? "unique" :
      (result->status == RA_REACHING_DEFINITION_AMBIGUOUS ? "ambiguous" :
      "missing"));
  free(definitions);
  free(transparent);
  return SUCCEEDED;
}


int register_allocator_plan_join_schedule(char *function_name,
    int instruction_count, int temp_index, int no_physical_register,
    struct register_allocator_join_retention_path *paths, int path_count,
    int *producer_physical_registers, int consumer_instruction,
    int consumer_operand, int consumer_physical_register,
    int selected_physical_register,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity,
    struct register_allocator_join_schedule *schedule) {

  int assignment_count;
  int conflict_instruction;
  int path_index;

  if (schedule != NULL) {
    schedule->status = RA_JOIN_SCHEDULE_INELIGIBLE;
    schedule->assignment_count = 0;
    schedule->conflict_instruction = -1;
  }
  if (function_name == NULL || instruction_count <= 0 || temp_index < 0 ||
      paths == NULL || path_count < 2 || path_count >= INT_MAX ||
      producer_physical_registers == NULL ||
      assignment_capacity < 0 || schedule == NULL ||
      (assignment_capacity > 0 && assignments == NULL)) {
    fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d instructions=%d paths=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        instruction_count, path_count, consumer_instruction, consumer_operand,
        selected_physical_register, assignment_capacity);
    return FAILED;
  }
    if (selected_physical_register == no_physical_register) {
      fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d instructions=%d paths=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=-1 schedule=ineligible status=complete\n",
          function_name, temp_index, instruction_count, path_count,
          consumer_instruction, consumer_operand, selected_physical_register,
          assignment_capacity);
      return SUCCEEDED;
    }
  if (consumer_instruction < 0 ||
      consumer_instruction >= instruction_count ||
      _is_valid_tac_use_operand(consumer_operand) == NO) {
    fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d instructions=%d paths=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=-1 status=invalid_input\n",
        function_name, temp_index, instruction_count, path_count,
        consumer_instruction, consumer_operand, selected_physical_register,
        assignment_capacity);
    return FAILED;
  }
  for (path_index = 0; path_index < path_count; path_index++) {
    if (paths[path_index].definition_found != YES ||
        paths[path_index].definition_instruction < 0 ||
      paths[path_index].definition_instruction >= consumer_instruction ||
        paths[path_index].definition_supported != YES ||
        paths[path_index].path_transparent != YES ||
        (selected_physical_register != no_physical_register &&
        paths[path_index].physical_register != selected_physical_register)) {
      fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d path=%d definition=%d phy=%d selected=%d assignments=0 conflict=-1 status=invalid_path\n",
          function_name, temp_index, path_index,
          paths[path_index].definition_instruction,
          paths[path_index].physical_register, selected_physical_register);
      return FAILED;
    }
  }

  assignment_count = 1;
  conflict_instruction = -1;
  for (path_index = 0; path_index < path_count; path_index++) {
    int previous_path;
    int unique_definition;

    unique_definition = YES;
    for (previous_path = 0; previous_path < path_index; previous_path++) {
      if (paths[previous_path].definition_instruction ==
          paths[path_index].definition_instruction) {
        unique_definition = NO;
        break;
      }
    }
    if (unique_definition == YES)
      assignment_count++;
    if (producer_physical_registers[path_index] != no_physical_register &&
        producer_physical_registers[path_index] != selected_physical_register &&
        conflict_instruction < 0)
      conflict_instruction = paths[path_index].definition_instruction;
  }
  if (consumer_physical_register != no_physical_register &&
      consumer_physical_register != selected_physical_register &&
      conflict_instruction < 0)
    conflict_instruction = consumer_instruction;
  if (conflict_instruction >= 0) {
    schedule->status = RA_JOIN_SCHEDULE_CONFLICT;
    schedule->conflict_instruction = conflict_instruction;
    fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d instructions=%d paths=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=%d schedule=conflict status=complete\n",
        function_name, temp_index, instruction_count, path_count,
        consumer_instruction, consumer_operand, selected_physical_register,
        assignment_capacity, conflict_instruction);
    return SUCCEEDED;
  }
  if (assignment_count > assignment_capacity) {
    fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d instructions=%d paths=%d consumer=%d operand=%d selected=%d assignments=%d capacity=%d conflict=-1 status=insufficient_capacity\n",
        function_name, temp_index, instruction_count, path_count,
        consumer_instruction, consumer_operand, selected_physical_register,
        assignment_count, assignment_capacity);
    return FAILED;
  }

  assignment_count = 0;
  for (path_index = 0; path_index < path_count; path_index++) {
    int previous_path;
    int unique_definition;

    unique_definition = YES;
    for (previous_path = 0; previous_path < path_index; previous_path++) {
      if (paths[previous_path].definition_instruction ==
          paths[path_index].definition_instruction) {
        unique_definition = NO;
        break;
      }
    }
    if (unique_definition == NO)
      continue;
    assignments[assignment_count].role = RA_JOIN_ASSIGNMENT_PRODUCER;
    assignments[assignment_count].instruction =
        paths[path_index].definition_instruction;
    assignments[assignment_count].operand = 0;
    assignments[assignment_count].physical_register =
        selected_physical_register;
    fprintf(stderr, "register_allocator_core: join_schedule_assignment function=%s temp=r%d assignment=%d role=producer instruction=%d operand=0 phy=%d status=complete\n",
        function_name, temp_index, assignment_count,
        assignments[assignment_count].instruction,
        selected_physical_register);
    assignment_count++;
  }
  assignments[assignment_count].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[assignment_count].instruction = consumer_instruction;
  assignments[assignment_count].operand = consumer_operand;
  assignments[assignment_count].physical_register = selected_physical_register;
  fprintf(stderr, "register_allocator_core: join_schedule_assignment function=%s temp=r%d assignment=%d role=consumer instruction=%d operand=%d phy=%d status=complete\n",
      function_name, temp_index, assignment_count, consumer_instruction,
      consumer_operand, selected_physical_register);
  assignment_count++;
  schedule->status = RA_JOIN_SCHEDULE_READY;
  schedule->assignment_count = assignment_count;
  fprintf(stderr, "register_allocator_core: join_schedule function=%s temp=r%d instructions=%d paths=%d consumer=%d operand=%d selected=%d assignments=%d capacity=%d conflict=-1 schedule=ready status=complete\n",
      function_name, temp_index, instruction_count, path_count,
      consumer_instruction, consumer_operand, selected_physical_register,
      assignment_count, assignment_capacity);
  return SUCCEEDED;
}


int register_allocator_classify_loop_join(char *function_name,
    int block_count, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_loop_join *loop_join) {

  int edge_index;
  int entry_count;
  int latch_count;

  if (loop_join != NULL) {
    loop_join->status = RA_LOOP_JOIN_NOT_LOOP;
    loop_join->entry_predecessor = -1;
    loop_join->latch_predecessor = -1;
    loop_join->entry_edge = -1;
    loop_join->back_edge = -1;
  }
  if (function_name == NULL || block_count <= 0 || join_block_index < 0 ||
      join_block_index >= block_count || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || loop_join == NULL) {
    fprintf(stderr, "register_allocator_core: loop_join function=%s block=%d blocks=%d edges=%d entry=-1 latch=-1 entry_edge=-1 back_edge=-1 topology=not_loop status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        block_count, edge_count);
    return FAILED;
  }
  entry_count = 0;
  latch_count = 0;
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    struct register_allocator_cfg_edge *edge;

    edge = &edges[edge_index];
    if (edge->from_block < 0 || edge->from_block >= block_count ||
        edge->to_block < 0 || edge->to_block >= block_count ||
        (edge->kind != RA_CFG_EDGE_FALLTHROUGH &&
        edge->kind != RA_CFG_EDGE_JUMP &&
        edge->kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edge->kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      fprintf(stderr, "register_allocator_core: loop_join_edge function=%s block=%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, join_block_index, edge_index, edge->from_block,
          edge->to_block, edge->kind);
      return FAILED;
    }
    if (edge->to_block != join_block_index)
      continue;
    if (edge->from_block < join_block_index) {
      entry_count++;
      if (entry_count == 1) {
        loop_join->entry_predecessor = edge->from_block;
        loop_join->entry_edge = edge_index;
      }
    }
    else {
      latch_count++;
      if (latch_count == 1) {
        loop_join->latch_predecessor = edge->from_block;
        loop_join->back_edge = edge_index;
      }
    }
  }
  if (entry_count == 1 && latch_count == 1)
    loop_join->status = RA_LOOP_JOIN_READY;
  else if (entry_count > 1 || latch_count > 1)
    loop_join->status = RA_LOOP_JOIN_AMBIGUOUS;
  fprintf(stderr, "register_allocator_core: loop_join function=%s block=%d blocks=%d edges=%d entries=%d latches=%d entry=%d latch=%d entry_edge=%d back_edge=%d topology=%s status=complete\n",
      function_name, join_block_index, block_count, edge_count, entry_count,
      latch_count, loop_join->entry_predecessor,
      loop_join->latch_predecessor, loop_join->entry_edge,
      loop_join->back_edge,
      loop_join->status == RA_LOOP_JOIN_READY ? "ready" :
      (loop_join->status == RA_LOOP_JOIN_AMBIGUOUS ? "ambiguous" :
      "not_loop"));
  return SUCCEEDED;
}


int register_allocator_profile_loop_flow(char *function_name, int block_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count,
    struct register_allocator_loop_flow_profile *profile) {

  char *loop_blocks;
  char *terminal_exits;
  int changed;
  int edge_index;
  int status;

  if (profile != NULL) {
    profile->status = RA_LOOP_JOIN_NOT_LOOP;
    profile->entry_edge_count = 0;
    profile->back_edge_count = 0;
    profile->exit_edge_count = 0;
    profile->terminal_exit_count = 0;
  }
  if (function_name == NULL || block_count <= 0 || blocks == NULL ||
      join_block_index < 0 || join_block_index >= block_count ||
      edge_count < 0 || (edge_count > 0 && edges == NULL) || profile == NULL) {
    fprintf(stderr, "register_allocator_core: loop_flow_profile function=%s block=%d blocks=%d edges=%d entries=0 back_edges=0 exits=0 terminal_exits=0 topology=not_loop status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        block_count, edge_count);
    return FAILED;
  }
  for (edge_index = 0; edge_index < block_count; edge_index++) {
    if (blocks[edge_index].start_tac < 0 ||
        blocks[edge_index].end_tac < blocks[edge_index].start_tac ||
        blocks[edge_index].end_reason < RA_BLOCK_END_NONE ||
        blocks[edge_index].end_reason > RA_BLOCK_END_FUNCTION_END) {
      fprintf(stderr, "register_allocator_core: loop_flow_profile_block function=%s block=%d start=%d end=%d reason=%d status=invalid_block\n",
          function_name, edge_index, blocks[edge_index].start_tac,
          blocks[edge_index].end_tac, blocks[edge_index].end_reason);
      return FAILED;
    }
  }
  loop_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  terminal_exits = (char *)calloc((size_t)block_count, sizeof(char));
  if (loop_blocks == NULL || terminal_exits == NULL) {
    free(loop_blocks);
    free(terminal_exits);
    fprintf(stderr, "register_allocator_core: loop_flow_profile function=%s block=%d blocks=%d edges=%d status=out_of_memory\n",
        function_name, join_block_index, block_count, edge_count);
    return FAILED;
  }
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count ||
        (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
        edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      free(loop_blocks);
      free(terminal_exits);
      fprintf(stderr, "register_allocator_core: loop_flow_profile_edge function=%s block=%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, join_block_index, edge_index,
          edges[edge_index].from_block, edges[edge_index].to_block,
          edges[edge_index].kind);
      return FAILED;
    }
    if (edges[edge_index].to_block != join_block_index)
      continue;
    if (edges[edge_index].from_block < join_block_index)
      profile->entry_edge_count++;
    else {
      profile->back_edge_count++;
      loop_blocks[edges[edge_index].from_block] = YES;
    }
  }
  loop_blocks[join_block_index] = YES;
  do {
    changed = NO;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      int from_block;
      int to_block;

      from_block = edges[edge_index].from_block;
      to_block = edges[edge_index].to_block;
      if (from_block >= join_block_index &&
          to_block != join_block_index && loop_blocks[to_block] == YES &&
          loop_blocks[from_block] == NO) {
        loop_blocks[from_block] = YES;
        changed = YES;
      }
    }
  } while (changed == YES);
  if (profile->back_edge_count > 0) {
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      int to_block;

      to_block = edges[edge_index].to_block;
      if (loop_blocks[edges[edge_index].from_block] == NO ||
          loop_blocks[to_block] == YES)
        continue;
      profile->exit_edge_count++;
      if (blocks[to_block].end_reason == RA_BLOCK_END_RETURN &&
          terminal_exits[to_block] == NO) {
        terminal_exits[to_block] = YES;
        profile->terminal_exit_count++;
      }
    }
  }
  if (profile->entry_edge_count == 1 && profile->back_edge_count == 1)
    status = RA_LOOP_JOIN_READY;
  else if (profile->entry_edge_count > 1 || profile->back_edge_count > 1)
    status = RA_LOOP_JOIN_AMBIGUOUS;
  else
    status = RA_LOOP_JOIN_NOT_LOOP;
  profile->status = status;
  fprintf(stderr, "register_allocator_core: loop_flow_profile function=%s block=%d blocks=%d edges=%d entries=%d back_edges=%d exits=%d terminal_exits=%d topology=%s status=complete\n",
      function_name, join_block_index, block_count, edge_count,
      profile->entry_edge_count, profile->back_edge_count,
      profile->exit_edge_count, profile->terminal_exit_count,
      status == RA_LOOP_JOIN_READY ? "ready" :
      (status == RA_LOOP_JOIN_AMBIGUOUS ? "ambiguous" : "not_loop"));
  free(loop_blocks);
  free(terminal_exits);
  return SUCCEEDED;
}


int register_allocator_select_loop_latch(char *function_name, int block_count,
    int instruction_count, struct register_allocator_basic_block *blocks,
    int join_block_index, struct register_allocator_cfg_edge *edges,
    int edge_count, int temp_index, void *context,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate writes_temp,
    struct register_allocator_loop_latch_selection *selection) {

  char *visited_latches;
  int block_index;
  int edge_index;

  if (selection != NULL) {
    selection->status = RA_LOOP_LATCH_SELECTION_INELIGIBLE;
    selection->entry_edge_count = 0;
    selection->back_edge_count = 0;
    selection->defining_latch_count = 0;
    selection->latch_definition = -1;
    selection->loop_join.status = RA_LOOP_JOIN_NOT_LOOP;
    selection->loop_join.entry_predecessor = -1;
    selection->loop_join.latch_predecessor = -1;
    selection->loop_join.entry_edge = -1;
    selection->loop_join.back_edge = -1;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || join_block_index < 0 ||
      join_block_index >= block_count || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || temp_index < 0 || context == NULL ||
      is_active == NULL || writes_temp == NULL || selection == NULL) {
    fprintf(stderr, "register_allocator_core: loop_latch_selection function=%s block=%d temp=r%d entries=0 back_edges=0 defining_latches=0 latch=-1 definition=-1 selection=ineligible status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
        blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: loop_latch_selection_block function=%s block=%d start=%d end=%d status=invalid_block\n",
          function_name, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      return FAILED;
    }
  }
  visited_latches = (char *)calloc((size_t)block_count, sizeof(char));
  if (visited_latches == NULL) {
    fprintf(stderr, "register_allocator_core: loop_latch_selection function=%s block=%d temp=r%d status=out_of_memory\n",
        function_name, join_block_index, temp_index);
    return FAILED;
  }
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    int from_block;

    from_block = edges[edge_index].from_block;
    if (from_block < 0 || from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count ||
        (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
        edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      free(visited_latches);
      fprintf(stderr, "register_allocator_core: loop_latch_selection_edge function=%s block=%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, join_block_index, edge_index, from_block,
          edges[edge_index].to_block, edges[edge_index].kind);
      return FAILED;
    }
    if (edges[edge_index].to_block != join_block_index)
      continue;
    if (from_block < join_block_index) {
      selection->entry_edge_count++;
      if (selection->entry_edge_count == 1) {
        selection->loop_join.entry_predecessor = from_block;
        selection->loop_join.entry_edge = edge_index;
      }
      continue;
    }
    selection->back_edge_count++;
    if (visited_latches[from_block] == NO) {
      int instruction_index;

      visited_latches[from_block] = YES;
      for (instruction_index = blocks[from_block].end_tac;
          instruction_index >= blocks[from_block].start_tac;
          instruction_index--) {
        int active;
        int writes;

        active = is_active(context, instruction_index, temp_index);
        writes = writes_temp(context, instruction_index, temp_index);
        if ((active != NO && active != YES) ||
            (writes != NO && writes != YES)) {
          free(visited_latches);
          fprintf(stderr, "register_allocator_core: loop_latch_selection_callback function=%s block=%d temp=r%d instruction=%d predicate=%s status=invalid_callback\n",
              function_name, join_block_index, temp_index, instruction_index,
              (active != NO && active != YES) ? "is_active" : "writes_temp");
          return FAILED;
        }
        if (active == YES && writes == YES) {
          selection->defining_latch_count++;
          if (selection->defining_latch_count == 1) {
            selection->loop_join.latch_predecessor = from_block;
            selection->loop_join.back_edge = edge_index;
            selection->latch_definition = instruction_index;
          }
          break;
        }
      }
    }
  }
  free(visited_latches);
  if (selection->entry_edge_count == 1 && selection->back_edge_count == 1 &&
      selection->defining_latch_count == 1) {
    selection->status = RA_LOOP_LATCH_SELECTION_READY;
    selection->loop_join.status = RA_LOOP_JOIN_READY;
  }
  else if (selection->entry_edge_count > 1 ||
      selection->back_edge_count > 1 ||
      selection->defining_latch_count > 1) {
    selection->status = RA_LOOP_LATCH_SELECTION_AMBIGUOUS;
    selection->loop_join.status = RA_LOOP_JOIN_AMBIGUOUS;
  }
  fprintf(stderr, "register_allocator_core: loop_latch_selection function=%s block=%d temp=r%d entries=%d back_edges=%d defining_latches=%d latch=%d definition=%d selection=%s status=complete\n",
      function_name, join_block_index, temp_index,
      selection->entry_edge_count, selection->back_edge_count,
      selection->defining_latch_count,
      selection->loop_join.latch_predecessor,
      selection->latch_definition,
      selection->status == RA_LOOP_LATCH_SELECTION_READY ? "ready" :
      (selection->status == RA_LOOP_LATCH_SELECTION_AMBIGUOUS ?
      "ambiguous" : "ineligible"));
  return SUCCEEDED;
}


int register_allocator_collect_loop_latch_definitions(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    struct register_allocator_cfg_edge *edges, int edge_count, int temp_index,
    void *context, register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate writes_temp,
    struct register_allocator_loop_latch_definition *definitions,
    int definition_capacity,
    struct register_allocator_loop_latch_collection *collection) {

  struct register_allocator_loop_latch_definition *staged_definitions;
  char *visited_latches;
  int block_index;
  int definition_count;
  int edge_index;

  staged_definitions = NULL;
  visited_latches = NULL;
  if (collection != NULL) {
    collection->status = RA_LOOP_LATCH_SELECTION_INELIGIBLE;
    collection->entry_edge_count = 0;
    collection->back_edge_count = 0;
    collection->definition_count = 0;
    collection->entry_predecessor = -1;
    collection->entry_edge = -1;
  }
  if (definitions != NULL && definition_capacity > 0) {
    for (definition_count = 0; definition_count < definition_capacity;
        definition_count++) {
      definitions[definition_count].block_index = -1;
      definitions[definition_count].edge_index = -1;
      definitions[definition_count].definition_instruction = -1;
    }
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || join_block_index < 0 ||
      join_block_index >= block_count || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || temp_index < 0 || context == NULL ||
      is_active == NULL || writes_temp == NULL || definition_capacity < 0 ||
      (definition_capacity > 0 && definitions == NULL) ||
      collection == NULL) {
    fprintf(stderr, "register_allocator_core: loop_latch_collection function=%s block=%d temp=r%d entries=0 back_edges=0 definitions=0 capacity=%d collection=ineligible status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index, definition_capacity);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
        blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: loop_latch_collection_block function=%s block=%d start=%d end=%d status=invalid_block\n",
          function_name, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      return FAILED;
    }
  }
  if ((size_t)definition_capacity > ((size_t)-1) /
      sizeof(struct register_allocator_loop_latch_definition))
    goto out_of_memory;
  visited_latches = (char *)calloc((size_t)block_count, sizeof(char));
  if (definition_capacity > 0)
    staged_definitions =
        (struct register_allocator_loop_latch_definition *)malloc(
        (size_t)definition_capacity *
        sizeof(struct register_allocator_loop_latch_definition));
  if (visited_latches == NULL ||
      (definition_capacity > 0 && staged_definitions == NULL))
    goto out_of_memory;
  definition_count = 0;
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    int from_block;

    from_block = edges[edge_index].from_block;
    if (from_block < 0 || from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count ||
        (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
        edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      fprintf(stderr, "register_allocator_core: loop_latch_collection_edge function=%s block=%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, join_block_index, edge_index, from_block,
          edges[edge_index].to_block, edges[edge_index].kind);
      goto failed;
    }
    if (edges[edge_index].to_block != join_block_index)
      continue;
    if (from_block < join_block_index) {
      collection->entry_edge_count++;
      if (collection->entry_edge_count == 1) {
        collection->entry_predecessor = from_block;
        collection->entry_edge = edge_index;
      }
      continue;
    }
    collection->back_edge_count++;
    if (visited_latches[from_block] == YES)
      continue;
    visited_latches[from_block] = YES;
    for (block_index = blocks[from_block].end_tac;
        block_index >= blocks[from_block].start_tac; block_index--) {
      int active;
      int writes;

      active = is_active(context, block_index, temp_index);
      writes = writes_temp(context, block_index, temp_index);
      if ((active != NO && active != YES) ||
          (writes != NO && writes != YES)) {
        fprintf(stderr, "register_allocator_core: loop_latch_collection_callback function=%s block=%d temp=r%d instruction=%d predicate=%s status=invalid_callback\n",
            function_name, join_block_index, temp_index, block_index,
            (active != NO && active != YES) ? "is_active" : "writes_temp");
        goto failed;
      }
      if (active == YES && writes == YES) {
        if (definition_count >= definition_capacity) {
          fprintf(stderr, "register_allocator_core: loop_latch_collection function=%s block=%d temp=r%d entries=%d back_edges=%d definitions=%d capacity=%d status=insufficient_capacity\n",
              function_name, join_block_index, temp_index,
              collection->entry_edge_count, collection->back_edge_count,
              definition_count + 1, definition_capacity);
          goto failed;
        }
        staged_definitions[definition_count].block_index = from_block;
        staged_definitions[definition_count].edge_index = edge_index;
        staged_definitions[definition_count].definition_instruction =
            block_index;
        definition_count++;
        break;
      }
    }
  }
  collection->definition_count = definition_count;
  if (collection->entry_edge_count == 1 &&
      collection->back_edge_count > 0 && definition_count > 0)
    collection->status = RA_LOOP_LATCH_SELECTION_READY;
  else if (collection->entry_edge_count > 1)
    collection->status = RA_LOOP_LATCH_SELECTION_AMBIGUOUS;
  if (definition_count > 0)
    memcpy(definitions, staged_definitions,
        (size_t)definition_count *
        sizeof(struct register_allocator_loop_latch_definition));
  for (block_index = 0; block_index < definition_count; block_index++)
    fprintf(stderr, "register_allocator_core: loop_latch_collection_definition function=%s block=%d temp=r%d definition=%d latch=%d edge=%d instruction=%d status=complete\n",
        function_name, join_block_index, temp_index, block_index,
        definitions[block_index].block_index,
        definitions[block_index].edge_index,
        definitions[block_index].definition_instruction);
  fprintf(stderr, "register_allocator_core: loop_latch_collection function=%s block=%d temp=r%d entries=%d back_edges=%d definitions=%d capacity=%d collection=%s status=complete\n",
      function_name, join_block_index, temp_index,
      collection->entry_edge_count, collection->back_edge_count,
      collection->definition_count, definition_capacity,
      collection->status == RA_LOOP_LATCH_SELECTION_READY ? "ready" :
      (collection->status == RA_LOOP_LATCH_SELECTION_AMBIGUOUS ?
      "ambiguous" : "ineligible"));
  free(staged_definitions);
  free(visited_latches);
  return SUCCEEDED;

out_of_memory:
  fprintf(stderr, "register_allocator_core: loop_latch_collection function=%s block=%d temp=r%d capacity=%d status=out_of_memory\n",
      function_name, join_block_index, temp_index, definition_capacity);
failed:
  free(staged_definitions);
  free(visited_latches);
  return FAILED;
}


int register_allocator_plan_loop_join_candidate(char *function_name,
    int block_count, int temp_count,
    struct register_allocator_liveness_storage *storage,
    int join_block_index, struct register_allocator_loop_join *loop_join,
    char *live_in, struct register_allocator_loop_candidate *candidate) {

  int temp_index;

  if (candidate != NULL) {
    candidate->status = RA_LOOP_CANDIDATE_INELIGIBLE;
    candidate->reason = RA_LOOP_CANDIDATE_REASON_NONE;
    candidate->candidate_count = 0;
    candidate->temp_index = -1;
  }
  if (function_name == NULL || block_count <= 0 || temp_count <= 0 ||
      block_count > INT_MAX / temp_count || storage == NULL ||
      storage->set_count != block_count * temp_count ||
      storage->buffer_bytes < (size_t)storage->set_count ||
      storage->total_bytes < storage->buffer_bytes * 4 ||
      join_block_index < 0 || join_block_index >= block_count ||
      loop_join == NULL || live_in == NULL || candidate == NULL) {
    fprintf(stderr, "register_allocator_core: loop_join_candidate function=%s block=%d blocks=%d temps=%d candidates=0 temp=r-1 candidate=ineligible status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        block_count, temp_count);
    return FAILED;
  }
  if (loop_join->status == RA_LOOP_JOIN_NOT_LOOP) {
    candidate->reason = RA_LOOP_CANDIDATE_REASON_TOPOLOGY;
    fprintf(stderr, "register_allocator_core: loop_join_candidate function=%s block=%d blocks=%d temps=%d candidates=0 temp=r-1 candidate=ineligible reason=topology status=complete\n",
        function_name, join_block_index, block_count, temp_count);
    return SUCCEEDED;
  }
  if (loop_join->status != RA_LOOP_JOIN_READY &&
      loop_join->status != RA_LOOP_JOIN_AMBIGUOUS) {
    fprintf(stderr, "register_allocator_core: loop_join_candidate function=%s block=%d blocks=%d temps=%d candidates=0 temp=r-1 candidate=ineligible status=invalid_topology\n",
        function_name, join_block_index, block_count, temp_count);
    return FAILED;
  }
  for (temp_index = 0; temp_index < temp_count; temp_index++) {
    int set_index;

    set_index = join_block_index * temp_count + temp_index;
    if (live_in[set_index] != NO && live_in[set_index] != YES) {
      fprintf(stderr, "register_allocator_core: loop_join_candidate_value function=%s block=%d temp=r%d value=%d status=invalid_liveness\n",
          function_name, join_block_index, temp_index,
          (int)live_in[set_index]);
      return FAILED;
    }
    if (live_in[set_index] == YES) {
      if (candidate->candidate_count == 0)
        candidate->temp_index = temp_index;
      candidate->candidate_count++;
    }
  }
  if (loop_join->status == RA_LOOP_JOIN_AMBIGUOUS) {
    candidate->status = RA_LOOP_CANDIDATE_AMBIGUOUS;
    candidate->reason = RA_LOOP_CANDIDATE_REASON_TOPOLOGY;
    candidate->temp_index = -1;
  }
  else if (candidate->candidate_count > 0) {
    candidate->status = RA_LOOP_CANDIDATE_READY;
    candidate->reason = RA_LOOP_CANDIDATE_REASON_NONE;
  }
  else
    candidate->reason = RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN;
  fprintf(stderr, "register_allocator_core: loop_join_candidate function=%s block=%d blocks=%d temps=%d candidates=%d temp=r%d candidate=%s reason=%s status=complete\n",
      function_name, join_block_index, block_count, temp_count,
      candidate->candidate_count, candidate->temp_index,
      candidate->status == RA_LOOP_CANDIDATE_READY ? "ready" :
      (candidate->status == RA_LOOP_CANDIDATE_AMBIGUOUS ? "ambiguous" :
      "ineligible"),
      candidate->reason == RA_LOOP_CANDIDATE_REASON_TOPOLOGY ? "topology" :
      (candidate->reason == RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN ?
      "no_live_in" :
      (candidate->reason == RA_LOOP_CANDIDATE_REASON_MULTIPLE_LIVE_IN ?
      "multiple_live_in" :
      (candidate->candidate_count > 1 ? "first_live_in" :
      "unique_live_in"))));
  return SUCCEEDED;
}


int register_allocator_classify_loop_representation(char *function_name,
    int join_block_index, struct register_allocator_loop_candidate *candidate,
    int stack_value_count,
    struct register_allocator_loop_representation *representation) {

  char *status;

  if (representation != NULL) {
    representation->status = RA_LOOP_REPRESENTATION_NOT_APPLICABLE;
    representation->stack_value_count = 0;
  }
  if (function_name == NULL || join_block_index < 0 || candidate == NULL ||
      stack_value_count < 0 || representation == NULL) {
    fprintf(stderr, "register_allocator_core: loop_representation function=%s block=%d candidate=ineligible reason=invalid stack_values=%d representation=not_applicable status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        stack_value_count);
    return FAILED;
  }
  if ((candidate->status == RA_LOOP_CANDIDATE_READY &&
      (candidate->reason != RA_LOOP_CANDIDATE_REASON_NONE ||
      candidate->candidate_count <= 0 || candidate->temp_index < 0)) ||
      (candidate->status == RA_LOOP_CANDIDATE_INELIGIBLE &&
      candidate->reason == RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN &&
      (candidate->candidate_count != 0 || candidate->temp_index != -1)) ||
      (candidate->status != RA_LOOP_CANDIDATE_READY &&
      candidate->status != RA_LOOP_CANDIDATE_INELIGIBLE &&
      candidate->status != RA_LOOP_CANDIDATE_AMBIGUOUS) ||
      candidate->reason < RA_LOOP_CANDIDATE_REASON_NONE ||
      candidate->reason > RA_LOOP_CANDIDATE_REASON_MULTIPLE_LIVE_IN) {
    fprintf(stderr, "register_allocator_core: loop_representation function=%s block=%d candidate=%d reason=%d stack_values=%d representation=not_applicable status=invalid_candidate\n",
        function_name, join_block_index, candidate->status,
        candidate->reason, stack_value_count);
    return FAILED;
  }

  representation->stack_value_count = stack_value_count;
  if (candidate->status == RA_LOOP_CANDIDATE_READY)
    representation->status = RA_LOOP_REPRESENTATION_TEMP_READY;
  else if (candidate->status == RA_LOOP_CANDIDATE_INELIGIBLE &&
      candidate->reason == RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN) {
    if (stack_value_count == 0)
      representation->status = RA_LOOP_REPRESENTATION_NO_VALUE;
    else if (stack_value_count == 1)
      representation->status = RA_LOOP_REPRESENTATION_UNIQUE_STACK_VALUE;
    else
      representation->status =
          RA_LOOP_REPRESENTATION_AMBIGUOUS_STACK_VALUES;
  }
  status = representation->status == RA_LOOP_REPRESENTATION_TEMP_READY ?
      "temp_ready" :
      (representation->status == RA_LOOP_REPRESENTATION_NO_VALUE ?
      "no_value" :
      (representation->status == RA_LOOP_REPRESENTATION_UNIQUE_STACK_VALUE ?
      "unique_stack_value" :
      (representation->status ==
      RA_LOOP_REPRESENTATION_AMBIGUOUS_STACK_VALUES ?
      "ambiguous_stack_values" : "not_applicable")));
  fprintf(stderr, "register_allocator_core: loop_representation function=%s block=%d candidate=%s reason=%d stack_values=%d representation=%s status=complete\n",
      function_name, join_block_index,
      candidate->status == RA_LOOP_CANDIDATE_READY ? "ready" :
      (candidate->status == RA_LOOP_CANDIDATE_AMBIGUOUS ? "ambiguous" :
      "ineligible"), candidate->reason, stack_value_count, status);
  return SUCCEEDED;
}


int register_allocator_collect_loop_stack_values(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_loop_stack_value_observation *observations,
    int observation_count, int *identities, int identity_capacity,
    int *identity_count) {

  int block_index;
  int observation_index;
  int staged_count;
  int *staged_identities;

  if (identity_count != NULL)
    *identity_count = 0;
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || join_block_index < 0 ||
      join_block_index >= block_count || loop_join == NULL ||
      observation_count < 0 || identity_capacity < 0 ||
      (observation_count > 0 && observations == NULL) ||
      (identity_capacity > 0 && identities == NULL) || identity_count == NULL) {
    fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=0 capacity=%d collection=rejected status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        observation_count, identity_capacity);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
        blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: loop_stack_values_block function=%s block=%d start=%d end=%d status=invalid_block\n",
          function_name, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      return FAILED;
    }
  }
  if (loop_join->status != RA_LOOP_JOIN_READY) {
    if (loop_join->status != RA_LOOP_JOIN_NOT_LOOP &&
        loop_join->status != RA_LOOP_JOIN_AMBIGUOUS) {
      fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=0 capacity=%d collection=rejected status=invalid_topology\n",
          function_name, join_block_index, observation_count,
          identity_capacity);
      return FAILED;
    }
    fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=0 capacity=%d collection=skipped reason=topology status=complete\n",
        function_name, join_block_index, observation_count, identity_capacity);
    return SUCCEEDED;
  }
  if (loop_join->entry_predecessor < 0 ||
      loop_join->entry_predecessor >= join_block_index ||
      loop_join->latch_predecessor < join_block_index ||
      loop_join->latch_predecessor >= block_count ||
      loop_join->entry_edge < 0 || loop_join->back_edge < 0) {
    fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=0 capacity=%d collection=rejected status=invalid_topology\n",
        function_name, join_block_index, observation_count, identity_capacity);
    return FAILED;
  }
  staged_identities = NULL;
  if (identity_capacity > 0) {
    if ((size_t)identity_capacity > ((size_t)-1) / sizeof(int)) {
      fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=0 capacity=%d collection=rejected status=invalid_input\n",
          function_name, join_block_index, observation_count,
          identity_capacity);
      return FAILED;
    }
    staged_identities = malloc((size_t)identity_capacity * sizeof(int));
    if (staged_identities == NULL) {
      fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=0 capacity=%d collection=rejected status=out_of_memory\n",
          function_name, join_block_index, observation_count,
          identity_capacity);
      return FAILED;
    }
  }
  staged_count = 0;
  for (observation_index = 0; observation_index < observation_count;
      observation_index++) {
    int identity_index;
    int instruction_index;
    int selected;

    instruction_index = observations[observation_index].instruction_index;
    if (instruction_index < 0 || instruction_index >= instruction_count ||
        observations[observation_index].identity < 0) {
      fprintf(stderr, "register_allocator_core: loop_stack_value function=%s block=%d observation=%d instruction=%d identity=%d status=invalid_observation\n",
          function_name, join_block_index, observation_index,
          instruction_index, observations[observation_index].identity);
      free(staged_identities);
      return FAILED;
    }
    selected = NO;
    if ((instruction_index >=
        blocks[loop_join->entry_predecessor].start_tac &&
        instruction_index <=
        blocks[loop_join->entry_predecessor].end_tac) ||
        (instruction_index >= blocks[join_block_index].start_tac &&
        instruction_index <= blocks[join_block_index].end_tac) ||
        (instruction_index >=
        blocks[loop_join->latch_predecessor].start_tac &&
        instruction_index <=
        blocks[loop_join->latch_predecessor].end_tac))
      selected = YES;
    if (selected == NO)
      continue;
    for (identity_index = 0; identity_index < staged_count;
        identity_index++) {
      if (staged_identities[identity_index] ==
          observations[observation_index].identity)
        break;
    }
    if (identity_index < staged_count)
      continue;
    if (staged_count >= identity_capacity) {
      fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=%d capacity=%d collection=rejected status=insufficient_capacity\n",
          function_name, join_block_index, observation_count,
          staged_count + 1, identity_capacity);
      free(staged_identities);
      return FAILED;
    }
    staged_identities[staged_count++] = observations[observation_index].identity;
  }
  for (observation_index = 0; observation_index < staged_count;
      observation_index++) {
    identities[observation_index] = staged_identities[observation_index];
    fprintf(stderr, "register_allocator_core: loop_stack_value function=%s block=%d value=%d identity=%d order=first_observation status=complete\n",
        function_name, join_block_index, observation_index,
        staged_identities[observation_index]);
  }
  *identity_count = staged_count;
  fprintf(stderr, "register_allocator_core: loop_stack_values function=%s block=%d observations=%d values=%d capacity=%d collection=complete order=first_observation status=complete\n",
      function_name, join_block_index, observation_count, staged_count,
      identity_capacity);
  free(staged_identities);
  return SUCCEEDED;
}


int register_allocator_select_loop_stack_value(char *function_name,
    int join_block_index,
    struct register_allocator_loop_stack_value_roles *values,
    int value_count,
    struct register_allocator_loop_stack_value_selection *selection) {

  int candidate_count;
  int selected_identity;
  int value_index;

  if (selection != NULL) {
    selection->status = RA_LOOP_STACK_SELECTION_INELIGIBLE;
    selection->candidate_count = 0;
    selection->identity = -1;
  }
  if (function_name == NULL || join_block_index < 0 || value_count < 0 ||
      (value_count > 0 && values == NULL) || selection == NULL) {
    fprintf(stderr, "register_allocator_core: loop_stack_selection function=%s block=%d values=%d candidates=0 identity=-1 selection=ineligible status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        value_count);
    return FAILED;
  }
  candidate_count = 0;
  selected_identity = -1;
  for (value_index = 0; value_index < value_count; value_index++) {
    int other_index;

    if (values[value_index].identity < 0 || values[value_index].role_mask < 0 ||
        (values[value_index].role_mask & ~RA_LOOP_STACK_ROLE_REQUIRED) != 0) {
      fprintf(stderr, "register_allocator_core: loop_stack_selection_value function=%s block=%d value=%d identity=%d roles=%d status=invalid_value\n",
          function_name, join_block_index, value_index,
          values[value_index].identity, values[value_index].role_mask);
      return FAILED;
    }
    for (other_index = 0; other_index < value_index; other_index++) {
      if (values[other_index].identity == values[value_index].identity) {
        fprintf(stderr, "register_allocator_core: loop_stack_selection_value function=%s block=%d value=%d identity=%d roles=%d status=duplicate_identity\n",
            function_name, join_block_index, value_index,
            values[value_index].identity, values[value_index].role_mask);
        return FAILED;
      }
    }
    fprintf(stderr, "register_allocator_core: loop_stack_selection_value function=%s block=%d value=%d identity=%d roles=%d required=%d candidate=%s status=complete\n",
        function_name, join_block_index, value_index,
        values[value_index].identity, values[value_index].role_mask,
        RA_LOOP_STACK_ROLE_REQUIRED,
        (values[value_index].role_mask & RA_LOOP_STACK_ROLE_REQUIRED) ==
        RA_LOOP_STACK_ROLE_REQUIRED ? "yes" : "no");
    if ((values[value_index].role_mask & RA_LOOP_STACK_ROLE_REQUIRED) ==
        RA_LOOP_STACK_ROLE_REQUIRED) {
      if (candidate_count == 0)
        selected_identity = values[value_index].identity;
      candidate_count++;
    }
  }
  selection->candidate_count = candidate_count;
  if (candidate_count > 0) {
    selection->status = RA_LOOP_STACK_SELECTION_READY;
    selection->identity = selected_identity;
  }
  fprintf(stderr, "register_allocator_core: loop_stack_selection function=%s block=%d values=%d candidates=%d identity=%d selection=%s required=%d status=complete\n",
      function_name, join_block_index, value_count, candidate_count,
      selection->identity,
      selection->status == RA_LOOP_STACK_SELECTION_READY ? "ready" :
      (selection->status == RA_LOOP_STACK_SELECTION_AMBIGUOUS ?
      "ambiguous" : "ineligible"), RA_LOOP_STACK_ROLE_REQUIRED);
  return SUCCEEDED;
}


int register_allocator_plan_loop_stack_promotion(char *function_name,
    int join_block_index,
    struct register_allocator_loop_stack_value_selection *selection,
    int value_size, int proposed_temp_index, int *existing_temp_indices,
    int existing_temp_count,
    struct register_allocator_loop_stack_promotion_plan *plan) {

  int temp_index;

  if (plan != NULL) {
    plan->status = RA_LOOP_STACK_PROMOTION_INELIGIBLE;
    plan->reason = RA_LOOP_STACK_PROMOTION_REASON_SELECTION;
    plan->identity = -1;
    plan->temp_index = -1;
    plan->size = 0;
  }
  if (function_name == NULL || join_block_index < 0 || selection == NULL ||
      proposed_temp_index < 0 || existing_temp_count < 0 ||
      (existing_temp_count > 0 && existing_temp_indices == NULL) ||
      plan == NULL) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion function=%s block=%d identity=-1 size=%d proposed_temp=r%d existing_temps=%d plan=ineligible reason=selection status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        value_size, proposed_temp_index, existing_temp_count);
    return FAILED;
  }
    if ((selection->status == RA_LOOP_STACK_SELECTION_READY &&
      (selection->candidate_count <= 0 || selection->identity < 0)) ||
      (selection->status == RA_LOOP_STACK_SELECTION_INELIGIBLE &&
      (selection->candidate_count != 0 || selection->identity != -1)) ||
      (selection->status == RA_LOOP_STACK_SELECTION_AMBIGUOUS &&
      (selection->candidate_count <= 1 || selection->identity != -1)) ||
      (selection->status != RA_LOOP_STACK_SELECTION_READY &&
      selection->status != RA_LOOP_STACK_SELECTION_INELIGIBLE &&
      selection->status != RA_LOOP_STACK_SELECTION_AMBIGUOUS)) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion function=%s block=%d identity=%d size=%d proposed_temp=r%d existing_temps=%d plan=ineligible reason=selection status=invalid_selection\n",
        function_name, join_block_index, selection->identity, value_size,
        proposed_temp_index, existing_temp_count);
    return FAILED;
  }
  for (temp_index = 0; temp_index < existing_temp_count; temp_index++) {
    int other_index;

    if (existing_temp_indices[temp_index] < 0) {
      fprintf(stderr, "register_allocator_core: loop_stack_promotion_temp function=%s block=%d temp=%d value=%d status=invalid_temp\n",
          function_name, join_block_index, temp_index,
          existing_temp_indices[temp_index]);
      return FAILED;
    }
    for (other_index = 0; other_index < temp_index; other_index++) {
      if (existing_temp_indices[other_index] ==
          existing_temp_indices[temp_index]) {
        fprintf(stderr, "register_allocator_core: loop_stack_promotion_temp function=%s block=%d temp=%d value=%d status=duplicate_temp\n",
            function_name, join_block_index, temp_index,
            existing_temp_indices[temp_index]);
        return FAILED;
      }
    }
  }
  if (selection->status != RA_LOOP_STACK_SELECTION_READY) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion function=%s block=%d identity=-1 size=%d proposed_temp=r%d existing_temps=%d plan=ineligible reason=selection status=complete\n",
        function_name, join_block_index, value_size, proposed_temp_index,
        existing_temp_count);
    return SUCCEEDED;
  }
  plan->identity = selection->identity;
  plan->size = value_size;
  if (value_size != 8 && value_size != 16) {
    plan->reason = RA_LOOP_STACK_PROMOTION_REASON_SIZE;
    fprintf(stderr, "register_allocator_core: loop_stack_promotion function=%s block=%d identity=%d size=%d proposed_temp=r%d existing_temps=%d plan=ineligible reason=unsupported_size status=complete\n",
        function_name, join_block_index, selection->identity, value_size,
        proposed_temp_index, existing_temp_count);
    return SUCCEEDED;
  }
  for (temp_index = 0; temp_index < existing_temp_count; temp_index++) {
    if (existing_temp_indices[temp_index] == proposed_temp_index) {
      plan->reason = RA_LOOP_STACK_PROMOTION_REASON_TEMP_CONFLICT;
      fprintf(stderr, "register_allocator_core: loop_stack_promotion function=%s block=%d identity=%d size=%d proposed_temp=r%d existing_temps=%d plan=ineligible reason=temp_conflict status=complete\n",
          function_name, join_block_index, selection->identity, value_size,
          proposed_temp_index, existing_temp_count);
      return SUCCEEDED;
    }
  }
  plan->status = RA_LOOP_STACK_PROMOTION_READY;
  plan->reason = RA_LOOP_STACK_PROMOTION_REASON_NONE;
  plan->temp_index = proposed_temp_index;
  fprintf(stderr, "register_allocator_core: loop_stack_promotion function=%s block=%d identity=%d size=%d proposed_temp=r%d existing_temps=%d plan=ready reason=none status=complete\n",
      function_name, join_block_index, selection->identity, value_size,
      proposed_temp_index, existing_temp_count);
  return SUCCEEDED;
}


int register_allocator_plan_loop_stack_rewrites(char *function_name,
    int instruction_count,
    struct register_allocator_loop_stack_promotion_plan *promotion,
    struct register_allocator_loop_stack_rewrite_observation *observations,
    int observation_count,
    struct register_allocator_loop_stack_rewrite *rewrites,
    int rewrite_capacity,
    struct register_allocator_loop_stack_rewrite_schedule *schedule) {

  int observation_index;
  int rewrite_count;
  int role_mask;

  if (schedule != NULL) {
    schedule->status = RA_LOOP_STACK_REWRITE_INELIGIBLE;
    schedule->reason = RA_LOOP_STACK_REWRITE_REASON_PROMOTION;
    schedule->rewrite_count = 0;
    schedule->role_mask = 0;
  }
  if (function_name == NULL || instruction_count <= 0 || promotion == NULL ||
      observation_count < 0 || rewrite_capacity < 0 ||
      (observation_count > 0 && observations == NULL) ||
      (rewrite_capacity > 0 && rewrites == NULL) || schedule == NULL) {
    fprintf(stderr, "register_allocator_core: loop_stack_rewrite_schedule function=%s temp=r-1 observations=%d rewrites=0 capacity=%d roles=0 schedule=ineligible reason=promotion status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", observation_count,
        rewrite_capacity);
    return FAILED;
  }
  if ((promotion->status == RA_LOOP_STACK_PROMOTION_READY &&
      (promotion->reason != RA_LOOP_STACK_PROMOTION_REASON_NONE ||
      promotion->identity < 0 || promotion->temp_index < 0 ||
      (promotion->size != 8 && promotion->size != 16))) ||
      (promotion->status == RA_LOOP_STACK_PROMOTION_INELIGIBLE &&
      (promotion->reason == RA_LOOP_STACK_PROMOTION_REASON_NONE ||
      promotion->temp_index != -1)) ||
      (promotion->status != RA_LOOP_STACK_PROMOTION_READY &&
      promotion->status != RA_LOOP_STACK_PROMOTION_INELIGIBLE)) {
    fprintf(stderr, "register_allocator_core: loop_stack_rewrite_schedule function=%s temp=r%d observations=%d rewrites=0 capacity=%d roles=0 schedule=ineligible reason=promotion status=invalid_promotion\n",
        function_name, promotion->temp_index, observation_count,
        rewrite_capacity);
    return FAILED;
  }
  if (promotion->status == RA_LOOP_STACK_PROMOTION_INELIGIBLE) {
    fprintf(stderr, "register_allocator_core: loop_stack_rewrite_schedule function=%s temp=r-1 observations=%d rewrites=0 capacity=%d roles=0 schedule=ineligible reason=promotion status=complete\n",
        function_name, observation_count, rewrite_capacity);
    return SUCCEEDED;
  }
  rewrite_count = 0;
  role_mask = 0;
  for (observation_index = 0; observation_index < observation_count;
      observation_index++) {
    int other_index;
    int role;

    role = observations[observation_index].role;
    if (observations[observation_index].instruction_index < 0 ||
        observations[observation_index].instruction_index >=
        instruction_count || observations[observation_index].operand < 0 ||
        observations[observation_index].operand > 2 ||
        observations[observation_index].identity < 0 || role <= 0 ||
        (role & ~RA_LOOP_STACK_ROLE_REQUIRED) != 0 ||
        (role & (role - 1)) != 0) {
      fprintf(stderr, "register_allocator_core: loop_stack_rewrite_observation function=%s observation=%d instruction=%d operand=%d identity=%d role=%d status=invalid_observation\n",
          function_name, observation_index,
          observations[observation_index].instruction_index,
          observations[observation_index].operand,
          observations[observation_index].identity, role);
      return FAILED;
    }
    if (observations[observation_index].identity != promotion->identity)
      continue;
    for (other_index = 0; other_index < observation_index; other_index++) {
      if (observations[other_index].identity == promotion->identity &&
          observations[other_index].instruction_index ==
          observations[observation_index].instruction_index &&
          observations[other_index].operand ==
          observations[observation_index].operand) {
        fprintf(stderr, "register_allocator_core: loop_stack_rewrite_observation function=%s observation=%d instruction=%d operand=%d identity=%d role=%d status=duplicate_target\n",
            function_name, observation_index,
            observations[observation_index].instruction_index,
            observations[observation_index].operand,
            observations[observation_index].identity, role);
        return FAILED;
      }
    }
    rewrite_count++;
    role_mask |= role;
  }
  if ((role_mask & RA_LOOP_STACK_ROLE_REQUIRED) !=
      RA_LOOP_STACK_ROLE_REQUIRED) {
    schedule->reason = RA_LOOP_STACK_REWRITE_REASON_ROLES;
    schedule->role_mask = role_mask;
    fprintf(stderr, "register_allocator_core: loop_stack_rewrite_schedule function=%s temp=r%d observations=%d rewrites=0 capacity=%d roles=%d schedule=ineligible reason=incomplete_roles status=complete\n",
        function_name, promotion->temp_index, observation_count,
        rewrite_capacity, role_mask);
    return SUCCEEDED;
  }
  if (rewrite_count > rewrite_capacity) {
    fprintf(stderr, "register_allocator_core: loop_stack_rewrite_schedule function=%s temp=r%d observations=%d rewrites=%d capacity=%d roles=%d schedule=ineligible reason=promotion status=insufficient_capacity\n",
        function_name, promotion->temp_index, observation_count,
        rewrite_count, rewrite_capacity, role_mask);
    return FAILED;
  }
  rewrite_count = 0;
  for (observation_index = 0; observation_index < observation_count;
      observation_index++) {
    if (observations[observation_index].identity != promotion->identity)
      continue;
    rewrites[rewrite_count].instruction_index =
        observations[observation_index].instruction_index;
    rewrites[rewrite_count].operand = observations[observation_index].operand;
    rewrites[rewrite_count].temp_index = promotion->temp_index;
    rewrites[rewrite_count].role = observations[observation_index].role;
    fprintf(stderr, "register_allocator_core: loop_stack_rewrite function=%s rewrite=%d instruction=%d operand=%d identity=%d temp=r%d role=%d order=observation status=complete\n",
        function_name, rewrite_count,
        observations[observation_index].instruction_index,
        observations[observation_index].operand, promotion->identity,
        promotion->temp_index, observations[observation_index].role);
    rewrite_count++;
  }
  schedule->status = RA_LOOP_STACK_REWRITE_READY;
  schedule->reason = RA_LOOP_STACK_REWRITE_REASON_NONE;
  schedule->rewrite_count = rewrite_count;
  schedule->role_mask = role_mask;
  fprintf(stderr, "register_allocator_core: loop_stack_rewrite_schedule function=%s temp=r%d observations=%d rewrites=%d capacity=%d roles=%d schedule=ready reason=none status=complete\n",
      function_name, promotion->temp_index, observation_count, rewrite_count,
      rewrite_capacity, role_mask);
  return SUCCEEDED;
}


int register_allocator_apply_loop_stack_promotion(char *function_name,
    int instruction_count,
    struct register_allocator_loop_stack_promotion_plan *promotion,
    struct register_allocator_loop_stack_rewrite *rewrites,
    int rewrite_capacity,
    struct register_allocator_loop_stack_rewrite_schedule *schedule,
    void *context,
    register_allocator_loop_stack_transaction_applier apply_transaction,
    struct register_allocator_loop_stack_application *application) {

  int result;
  int rewrite_index;
  int role_mask;

  if (application != NULL) {
    application->status = RA_LOOP_STACK_APPLICATION_INELIGIBLE;
    application->rewrite_count = 0;
  }
  if (function_name == NULL || instruction_count <= 0 || promotion == NULL ||
      rewrite_capacity < 0 || schedule == NULL || context == NULL ||
      apply_transaction == NULL || application == NULL ||
      (rewrite_capacity > 0 && rewrites == NULL)) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r%d rewrites=0 application=ineligible status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        promotion != NULL ? promotion->temp_index : -1);
    return FAILED;
  }
  if (promotion->status == RA_LOOP_STACK_PROMOTION_INELIGIBLE &&
      promotion->reason != RA_LOOP_STACK_PROMOTION_REASON_NONE &&
      promotion->temp_index == -1 &&
      schedule->status == RA_LOOP_STACK_REWRITE_INELIGIBLE &&
      schedule->rewrite_count == 0) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r-1 rewrites=0 application=ineligible status=complete\n",
        function_name);
    return SUCCEEDED;
  }
  if (promotion->status != RA_LOOP_STACK_PROMOTION_READY ||
      promotion->reason != RA_LOOP_STACK_PROMOTION_REASON_NONE ||
      promotion->identity < 0 || promotion->temp_index < 0 ||
      (promotion->size != 8 && promotion->size != 16) ||
      schedule->status != RA_LOOP_STACK_REWRITE_READY ||
      schedule->reason != RA_LOOP_STACK_REWRITE_REASON_NONE ||
      schedule->rewrite_count <= 0 ||
      schedule->rewrite_count > rewrite_capacity ||
      schedule->role_mask != RA_LOOP_STACK_ROLE_REQUIRED || rewrites == NULL) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r%d rewrites=%d application=ineligible status=invalid_plan\n",
        function_name, promotion->temp_index, schedule->rewrite_count);
    return FAILED;
  }
  role_mask = 0;
  for (rewrite_index = 0; rewrite_index < schedule->rewrite_count;
      rewrite_index++) {
    int other_index;
    int role;

    role = rewrites[rewrite_index].role;
    if (rewrites[rewrite_index].instruction_index < 0 ||
        rewrites[rewrite_index].instruction_index >= instruction_count ||
        rewrites[rewrite_index].operand < 0 ||
        rewrites[rewrite_index].operand > 2 ||
        rewrites[rewrite_index].temp_index != promotion->temp_index ||
        role <= 0 || (role & ~RA_LOOP_STACK_ROLE_REQUIRED) != 0 ||
        (role & (role - 1)) != 0) {
      fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply_rewrite function=%s temp=r%d rewrite=%d status=invalid_rewrite\n",
          function_name, promotion->temp_index, rewrite_index);
      return FAILED;
    }
    for (other_index = 0; other_index < rewrite_index; other_index++) {
      if (rewrites[other_index].instruction_index ==
          rewrites[rewrite_index].instruction_index &&
          rewrites[other_index].operand == rewrites[rewrite_index].operand) {
        fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply_rewrite function=%s temp=r%d rewrite=%d status=duplicate_target\n",
            function_name, promotion->temp_index, rewrite_index);
        return FAILED;
      }
    }
    role_mask |= role;
  }
  if (role_mask != RA_LOOP_STACK_ROLE_REQUIRED) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r%d rewrites=%d application=ineligible roles=%d status=invalid_plan\n",
        function_name, promotion->temp_index, schedule->rewrite_count,
        role_mask);
    return FAILED;
  }
  result = apply_transaction(context, promotion, rewrites,
      schedule->rewrite_count);
  if (result != SUCCEEDED && result != FAILED) {
    fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r%d rewrites=%d application=ineligible status=invalid_callback\n",
        function_name, promotion->temp_index, schedule->rewrite_count);
    return FAILED;
  }
  if (result == FAILED) {
    application->status = RA_LOOP_STACK_APPLICATION_FAILED;
    fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r%d rewrites=0 application=failed atomic=yes status=complete\n",
        function_name, promotion->temp_index);
    return SUCCEEDED;
  }
  application->status = RA_LOOP_STACK_APPLICATION_APPLIED;
  application->rewrite_count = schedule->rewrite_count;
  fprintf(stderr, "register_allocator_core: loop_stack_promotion_apply function=%s temp=r%d rewrites=%d application=applied atomic=yes status=complete\n",
      function_name, promotion->temp_index, application->rewrite_count);
  return SUCCEEDED;
}


int register_allocator_commit_loop_stack_promotion_storage(
    char *function_name, int instruction_count,
    struct register_allocator_loop_stack_promotion_plan *promotion,
    struct register_allocator_loop_stack_rewrite *rewrites,
    int rewrite_count,
    struct register_allocator_loop_stack_operand_storage *operands,
    int operand_count,
    struct register_allocator_loop_stack_temp_storage *temps,
    int temp_capacity, int *temp_count) {

  struct register_allocator_loop_stack_operand_storage *staged_operands;
  struct register_allocator_loop_stack_temp_storage *staged_temps;
  int required_operand_count;
  int rewrite_index;
  int temp_index;

  staged_operands = NULL;
  staged_temps = NULL;
  if (function_name == NULL || instruction_count <= 0 ||
      instruction_count > INT_MAX / 3 || promotion == NULL ||
      promotion->status != RA_LOOP_STACK_PROMOTION_READY ||
      promotion->reason != RA_LOOP_STACK_PROMOTION_REASON_NONE ||
      promotion->identity < 0 || promotion->temp_index < 0 ||
      (promotion->size != 8 && promotion->size != 16) || rewrites == NULL ||
      rewrite_count <= 0 || operands == NULL || operand_count < 0 ||
      temps == NULL || temp_capacity <= 0 || temp_count == NULL ||
      *temp_count < 0 || *temp_count >= temp_capacity) {
    fprintf(stderr, "register_allocator_core: loop_stack_storage_transaction function=%s temp=r%d rewrites=%d temps=%d capacity=%d transaction=rejected atomic=yes status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        promotion != NULL ? promotion->temp_index : -1, rewrite_count,
        temp_count != NULL ? *temp_count : -1, temp_capacity);
    return FAILED;
  }
  required_operand_count = instruction_count * 3;
  if (operand_count < required_operand_count) {
    fprintf(stderr, "register_allocator_core: loop_stack_storage_transaction function=%s temp=r%d rewrites=%d temps=%d capacity=%d transaction=rejected atomic=yes status=invalid_storage\n",
        function_name, promotion->temp_index, rewrite_count, *temp_count,
        temp_capacity);
    return FAILED;
  }
  for (temp_index = 0; temp_index < *temp_count; temp_index++) {
    if (temps[temp_index].temp_index == promotion->temp_index) {
      fprintf(stderr, "register_allocator_core: loop_stack_storage_transaction function=%s temp=r%d rewrites=%d temps=%d capacity=%d transaction=rejected atomic=yes status=temp_conflict\n",
          function_name, promotion->temp_index, rewrite_count, *temp_count,
          temp_capacity);
      return FAILED;
    }
  }
  for (rewrite_index = 0; rewrite_index < rewrite_count; rewrite_index++) {
    int operand_index;
    int other_index;

    if (rewrites[rewrite_index].instruction_index < 0 ||
        rewrites[rewrite_index].instruction_index >= instruction_count ||
        rewrites[rewrite_index].operand < 0 ||
        rewrites[rewrite_index].operand > 2 ||
        rewrites[rewrite_index].temp_index != promotion->temp_index) {
      fprintf(stderr, "register_allocator_core: loop_stack_storage_rewrite function=%s temp=r%d rewrite=%d transaction=rejected status=invalid_rewrite\n",
          function_name, promotion->temp_index, rewrite_index);
      return FAILED;
    }
    operand_index = rewrites[rewrite_index].instruction_index * 3 +
        rewrites[rewrite_index].operand;
    if (operands[operand_index].kind != RA_LOOP_STACK_STORAGE_STACK ||
        operands[operand_index].identity != promotion->identity) {
      fprintf(stderr, "register_allocator_core: loop_stack_storage_rewrite function=%s temp=r%d rewrite=%d instruction=%d operand=%d expected_identity=%d actual_identity=%d transaction=rejected status=stale_operand\n",
          function_name, promotion->temp_index, rewrite_index,
          rewrites[rewrite_index].instruction_index,
          rewrites[rewrite_index].operand, promotion->identity,
          operands[operand_index].identity);
      return FAILED;
    }
    for (other_index = 0; other_index < rewrite_index; other_index++) {
      if (rewrites[other_index].instruction_index ==
          rewrites[rewrite_index].instruction_index &&
          rewrites[other_index].operand == rewrites[rewrite_index].operand) {
        fprintf(stderr, "register_allocator_core: loop_stack_storage_rewrite function=%s temp=r%d rewrite=%d transaction=rejected status=duplicate_target\n",
            function_name, promotion->temp_index, rewrite_index);
        return FAILED;
      }
    }
  }
  staged_operands =
      (struct register_allocator_loop_stack_operand_storage *)malloc(
      sizeof(struct register_allocator_loop_stack_operand_storage) *
      (size_t)required_operand_count);
  staged_temps =
      (struct register_allocator_loop_stack_temp_storage *)malloc(
      sizeof(struct register_allocator_loop_stack_temp_storage) *
      (size_t)temp_capacity);
  if (staged_operands == NULL || staged_temps == NULL) {
    free(staged_operands);
    free(staged_temps);
    fprintf(stderr, "register_allocator_core: loop_stack_storage_transaction function=%s temp=r%d rewrites=%d temps=%d capacity=%d transaction=rejected atomic=yes status=out_of_memory\n",
        function_name, promotion->temp_index, rewrite_count, *temp_count,
        temp_capacity);
    return FAILED;
  }
  memcpy(staged_operands, operands,
      sizeof(struct register_allocator_loop_stack_operand_storage) *
      (size_t)required_operand_count);
  memcpy(staged_temps, temps,
      sizeof(struct register_allocator_loop_stack_temp_storage) *
      (size_t)(*temp_count));
  for (rewrite_index = 0; rewrite_index < rewrite_count; rewrite_index++) {
    int operand_index;

    operand_index = rewrites[rewrite_index].instruction_index * 3 +
        rewrites[rewrite_index].operand;
    staged_operands[operand_index].kind = RA_LOOP_STACK_STORAGE_TEMP;
    staged_operands[operand_index].identity = -1;
    staged_operands[operand_index].temp_index = promotion->temp_index;
    fprintf(stderr, "register_allocator_core: loop_stack_storage_rewrite function=%s temp=r%d rewrite=%d instruction=%d operand=%d transaction=staged status=complete\n",
        function_name, promotion->temp_index, rewrite_index,
        rewrites[rewrite_index].instruction_index,
        rewrites[rewrite_index].operand);
  }
  staged_temps[*temp_count].temp_index = promotion->temp_index;
  staged_temps[*temp_count].size = promotion->size;
  memcpy(operands, staged_operands,
      sizeof(struct register_allocator_loop_stack_operand_storage) *
      (size_t)required_operand_count);
  memcpy(temps, staged_temps,
      sizeof(struct register_allocator_loop_stack_temp_storage) *
      (size_t)(*temp_count + 1));
  (*temp_count)++;
  free(staged_operands);
  free(staged_temps);
  fprintf(stderr, "register_allocator_core: loop_stack_storage_transaction function=%s temp=r%d rewrites=%d temps=%d capacity=%d transaction=committed atomic=yes status=complete\n",
      function_name, promotion->temp_index, rewrite_count, *temp_count,
      temp_capacity);
  return SUCCEEDED;
}


int register_allocator_discover_loop_join_facts(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    int temp_index, struct register_allocator_loop_join *loop_join,
    void *context, register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp,
    register_allocator_instruction_query get_consumer_operand,
    struct register_allocator_loop_facts *facts) {

  int block_index;
  int instruction_index;

  if (facts != NULL) {
    facts->status = RA_LOOP_FACTS_INELIGIBLE;
    facts->reason = RA_LOOP_FACTS_REASON_NONE;
    facts->entry_definition = -1;
    facts->latch_definition = -1;
    facts->consumer_instruction = -1;
    facts->consumer_operand = 0;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || join_block_index < 0 ||
      join_block_index >= block_count || temp_index < 0 || loop_join == NULL ||
      context == NULL || is_active == NULL || reads_temp == NULL ||
      writes_temp == NULL ||
      get_consumer_operand == NULL || facts == NULL) {
    fprintf(stderr, "register_allocator_core: loop_join_fact_discovery function=%s block=%d temp=r%d entry=-1 latch=-1 consumer=-1 operand=0 facts=ineligible reason=none status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
        blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: loop_join_fact_discovery_block function=%s block=%d start=%d end=%d status=invalid_block\n",
          function_name, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      return FAILED;
    }
  }
  if (loop_join->status != RA_LOOP_JOIN_READY) {
    if (loop_join->status != RA_LOOP_JOIN_NOT_LOOP &&
        loop_join->status != RA_LOOP_JOIN_AMBIGUOUS) {
      fprintf(stderr, "register_allocator_core: loop_join_fact_discovery function=%s block=%d temp=r%d entry=-1 latch=-1 consumer=-1 operand=0 facts=ineligible reason=none status=invalid_topology\n",
          function_name, join_block_index, temp_index);
      return FAILED;
    }
    facts->reason = RA_LOOP_FACTS_REASON_TOPOLOGY;
    fprintf(stderr, "register_allocator_core: loop_join_fact_discovery function=%s block=%d temp=r%d entry=-1 latch=-1 consumer=-1 operand=0 facts=ineligible reason=topology status=complete\n",
        function_name, join_block_index, temp_index);
    return SUCCEEDED;
  }
  if (loop_join->entry_predecessor < 0 ||
      loop_join->entry_predecessor >= join_block_index ||
      loop_join->latch_predecessor < join_block_index ||
      loop_join->latch_predecessor >= block_count ||
      loop_join->entry_edge < 0 || loop_join->back_edge < 0) {
    fprintf(stderr, "register_allocator_core: loop_join_fact_discovery function=%s block=%d temp=r%d entry=-1 latch=-1 consumer=-1 operand=0 facts=ineligible reason=none status=invalid_topology\n",
        function_name, join_block_index, temp_index);
    return FAILED;
  }
  for (instruction_index = blocks[loop_join->entry_predecessor].end_tac;
      instruction_index >= blocks[loop_join->entry_predecessor].start_tac;
      instruction_index--) {
    int active;
    int writes;

    active = is_active(context, instruction_index, temp_index);
    writes = writes_temp(context, instruction_index, temp_index);
    if ((active != NO && active != YES) || (writes != NO && writes != YES)) {
      fprintf(stderr, "register_allocator_core: loop_join_fact_discovery_callback function=%s block=%d temp=r%d instruction=%d predicate=%s status=invalid_callback\n",
          function_name, join_block_index, temp_index, instruction_index,
          (active != NO && active != YES) ? "is_active" : "writes_temp");
      return FAILED;
    }
    if (active == YES && writes == YES) {
      facts->entry_definition = instruction_index;
      break;
    }
  }
  for (instruction_index = blocks[loop_join->latch_predecessor].end_tac;
      instruction_index >= blocks[loop_join->latch_predecessor].start_tac;
      instruction_index--) {
    int active;
    int writes;

    active = is_active(context, instruction_index, temp_index);
    writes = writes_temp(context, instruction_index, temp_index);
    if ((active != NO && active != YES) || (writes != NO && writes != YES)) {
      fprintf(stderr, "register_allocator_core: loop_join_fact_discovery_callback function=%s block=%d temp=r%d instruction=%d predicate=%s status=invalid_callback\n",
          function_name, join_block_index, temp_index, instruction_index,
          (active != NO && active != YES) ? "is_active" : "writes_temp");
      return FAILED;
    }
    if (active == YES && writes == YES) {
      facts->latch_definition = instruction_index;
      break;
    }
  }
  for (instruction_index = blocks[join_block_index].start_tac;
      instruction_index <= blocks[join_block_index].end_tac;
      instruction_index++) {
    int active;
    int reads;

    active = is_active(context, instruction_index, temp_index);
    reads = reads_temp(context, instruction_index, temp_index);
    if ((active != NO && active != YES) || (reads != NO && reads != YES)) {
      fprintf(stderr, "register_allocator_core: loop_join_fact_discovery_callback function=%s block=%d temp=r%d instruction=%d predicate=%s status=invalid_callback\n",
          function_name, join_block_index, temp_index, instruction_index,
          (active != NO && active != YES) ? "is_active" : "reads_temp");
      return FAILED;
    }
    if (active == YES && reads == YES) {
      facts->consumer_instruction = instruction_index;
      facts->consumer_operand = get_consumer_operand(context,
          instruction_index, temp_index);
      if (_is_valid_tac_use_operand(facts->consumer_operand) == NO) {
        fprintf(stderr, "register_allocator_core: loop_join_fact_discovery_callback function=%s block=%d temp=r%d instruction=%d predicate=get_consumer_operand value=%d status=invalid_callback\n",
            function_name, join_block_index, temp_index, instruction_index,
            facts->consumer_operand);
        facts->consumer_instruction = -1;
        facts->consumer_operand = 0;
        return FAILED;
      }
      break;
    }
  }
  if (facts->entry_definition < 0)
    facts->reason = RA_LOOP_FACTS_REASON_MISSING_ENTRY;
  else if (facts->latch_definition < 0)
    facts->reason = RA_LOOP_FACTS_REASON_MISSING_LATCH;
  else if (facts->consumer_instruction < 0)
    facts->reason = RA_LOOP_FACTS_REASON_NO_CONSUMER;
  else
    facts->status = RA_LOOP_FACTS_READY;
  fprintf(stderr, "register_allocator_core: loop_join_fact_discovery function=%s block=%d temp=r%d entry=%d latch=%d consumer=%d operand=%d facts=%s reason=%s status=complete\n",
      function_name, join_block_index, temp_index, facts->entry_definition,
      facts->latch_definition, facts->consumer_instruction,
      facts->consumer_operand,
      facts->status == RA_LOOP_FACTS_READY ? "ready" : "ineligible",
      facts->reason == RA_LOOP_FACTS_REASON_NONE ? "none" :
      (facts->reason == RA_LOOP_FACTS_REASON_TOPOLOGY ? "topology" :
      (facts->reason == RA_LOOP_FACTS_REASON_MISSING_ENTRY ? "missing_entry" :
      (facts->reason == RA_LOOP_FACTS_REASON_MISSING_LATCH ? "missing_latch" :
      "no_consumer"))));
  return SUCCEEDED;
}


int register_allocator_plan_loop_join_definition(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks, int join_block_index,
    int temp_index, int live_in, struct register_allocator_loop_join *loop_join,
    int entry_definition, int latch_definition, int consumer_instruction,
    int consumer_operand, struct register_allocator_loop_definition *definition) {

  int block_index;

  if (definition != NULL) {
    definition->status = RA_LOOP_DEFINITION_INELIGIBLE;
    definition->reason = RA_LOOP_DEFINITION_REASON_NONE;
    definition->entry_definition = -1;
    definition->latch_definition = -1;
    definition->consumer_instruction = -1;
    definition->consumer_operand = 0;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || join_block_index < 0 ||
      join_block_index >= block_count || temp_index < 0 ||
      (live_in != NO && live_in != YES) || loop_join == NULL ||
      definition == NULL) {
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=%d entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=none status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index, live_in, entry_definition, latch_definition,
        consumer_instruction, consumer_operand);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
        blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: loop_join_definition_block function=%s block=%d start=%d end=%d status=invalid_block\n",
          function_name, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      return FAILED;
    }
  }
  if (loop_join->status != RA_LOOP_JOIN_READY) {
    definition->reason = RA_LOOP_DEFINITION_REASON_TOPOLOGY;
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=%d entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=topology status=complete\n",
        function_name, join_block_index, temp_index, live_in,
        entry_definition, latch_definition, consumer_instruction,
        consumer_operand);
    return SUCCEEDED;
  }
  if (loop_join->entry_predecessor < 0 ||
      loop_join->entry_predecessor >= join_block_index ||
      loop_join->latch_predecessor < join_block_index ||
      loop_join->latch_predecessor >= block_count ||
      loop_join->entry_edge < 0 || loop_join->back_edge < 0) {
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=%d entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=none status=invalid_topology\n",
        function_name, join_block_index, temp_index, live_in,
        entry_definition, latch_definition, consumer_instruction,
        consumer_operand);
    return FAILED;
  }
  if (live_in == NO) {
    definition->reason = RA_LOOP_DEFINITION_REASON_NOT_LIVE_IN;
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=no entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=not_live_in status=complete\n",
        function_name, join_block_index, temp_index, entry_definition,
        latch_definition, consumer_instruction, consumer_operand);
    return SUCCEEDED;
  }
  if (entry_definition < 0) {
    definition->reason = RA_LOOP_DEFINITION_REASON_MISSING_ENTRY;
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=yes entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=missing_entry status=complete\n",
        function_name, join_block_index, temp_index, entry_definition,
        latch_definition, consumer_instruction, consumer_operand);
    return SUCCEEDED;
  }
  if (latch_definition < 0) {
    definition->reason = RA_LOOP_DEFINITION_REASON_MISSING_LATCH;
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=yes entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=missing_latch status=complete\n",
        function_name, join_block_index, temp_index, entry_definition,
        latch_definition, consumer_instruction, consumer_operand);
    return SUCCEEDED;
  }
  if (consumer_instruction < 0) {
    definition->reason = RA_LOOP_DEFINITION_REASON_NO_CONSUMER;
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=yes entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=no_consumer status=complete\n",
        function_name, join_block_index, temp_index, entry_definition,
        latch_definition, consumer_instruction, consumer_operand);
    return SUCCEEDED;
  }
  if (entry_definition < blocks[loop_join->entry_predecessor].start_tac ||
      entry_definition > blocks[loop_join->entry_predecessor].end_tac ||
      latch_definition < blocks[loop_join->latch_predecessor].start_tac ||
      latch_definition > blocks[loop_join->latch_predecessor].end_tac ||
      consumer_instruction < blocks[join_block_index].start_tac ||
      consumer_instruction > blocks[join_block_index].end_tac ||
      entry_definition >= consumer_instruction ||
      latch_definition <= consumer_instruction ||
      _is_valid_tac_use_operand(consumer_operand) == NO) {
    fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=yes entry=%d latch=%d consumer=%d operand=%d definition=ineligible reason=none status=invalid_definition\n",
        function_name, join_block_index, temp_index, entry_definition,
        latch_definition, consumer_instruction, consumer_operand);
    return FAILED;
  }
  definition->status = RA_LOOP_DEFINITION_READY;
  definition->entry_definition = entry_definition;
  definition->latch_definition = latch_definition;
  definition->consumer_instruction = consumer_instruction;
  definition->consumer_operand = consumer_operand;
  fprintf(stderr, "register_allocator_core: loop_join_definition function=%s block=%d temp=r%d live_in=yes entry=%d latch=%d consumer=%d operand=%d definition=ready reason=none status=complete\n",
      function_name, join_block_index, temp_index, entry_definition,
      latch_definition, consumer_instruction, consumer_operand);
  return SUCCEEDED;
}


int register_allocator_plan_loop_join_schedule(char *function_name,
    int instruction_count, int temp_index, int no_physical_register,
    struct register_allocator_loop_join *loop_join, int entry_definition,
    int entry_physical_register, int latch_definition,
    int latch_physical_register, int consumer_instruction,
    int consumer_operand, int consumer_physical_register,
    int selected_physical_register,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity, struct register_allocator_join_schedule *schedule) {

  int conflict_instruction;

  if (schedule != NULL) {
    schedule->status = RA_JOIN_SCHEDULE_INELIGIBLE;
    schedule->assignment_count = 0;
    schedule->conflict_instruction = -1;
  }
  if (function_name == NULL || instruction_count <= 0 || temp_index < 0 ||
      loop_join == NULL || consumer_instruction < 0 ||
      consumer_instruction >= instruction_count ||
      _is_valid_tac_use_operand(consumer_operand) == NO ||
      assignment_capacity < 0 || schedule == NULL ||
      (assignment_capacity > 0 && assignments == NULL)) {
    fprintf(stderr, "register_allocator_core: loop_join_schedule function=%s temp=r%d instructions=%d entry_definition=%d latch_definition=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        instruction_count, entry_definition, latch_definition,
        consumer_instruction, consumer_operand, selected_physical_register,
        assignment_capacity);
    return FAILED;
  }
  if (loop_join->status == RA_LOOP_JOIN_NOT_LOOP ||
      selected_physical_register == no_physical_register) {
    fprintf(stderr, "register_allocator_core: loop_join_schedule function=%s temp=r%d instructions=%d entry_definition=%d latch_definition=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=-1 schedule=ineligible status=complete\n",
        function_name, temp_index, instruction_count, entry_definition,
        latch_definition, consumer_instruction, consumer_operand,
        selected_physical_register, assignment_capacity);
    return SUCCEEDED;
  }
  if (loop_join->status != RA_LOOP_JOIN_READY ||
      loop_join->entry_predecessor < 0 || loop_join->latch_predecessor < 0 ||
      loop_join->entry_edge < 0 || loop_join->back_edge < 0 ||
      entry_definition < 0 || entry_definition >= consumer_instruction ||
      latch_definition <= consumer_instruction ||
      latch_definition >= instruction_count ||
      selected_physical_register == no_physical_register) {
    fprintf(stderr, "register_allocator_core: loop_join_schedule function=%s temp=r%d instructions=%d entry_definition=%d latch_definition=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=-1 status=invalid_loop\n",
        function_name, temp_index, instruction_count, entry_definition,
        latch_definition, consumer_instruction, consumer_operand,
        selected_physical_register, assignment_capacity);
    return FAILED;
  }
  conflict_instruction = -1;
  if (entry_physical_register != no_physical_register &&
      entry_physical_register != selected_physical_register)
    conflict_instruction = entry_definition;
  else if (latch_physical_register != no_physical_register &&
      latch_physical_register != selected_physical_register)
    conflict_instruction = latch_definition;
  else if (consumer_physical_register != no_physical_register &&
      consumer_physical_register != selected_physical_register)
    conflict_instruction = consumer_instruction;
  if (conflict_instruction >= 0) {
    schedule->status = RA_JOIN_SCHEDULE_CONFLICT;
    schedule->conflict_instruction = conflict_instruction;
    fprintf(stderr, "register_allocator_core: loop_join_schedule function=%s temp=r%d instructions=%d entry_definition=%d latch_definition=%d consumer=%d operand=%d selected=%d assignments=0 capacity=%d conflict=%d schedule=conflict status=complete\n",
        function_name, temp_index, instruction_count, entry_definition,
        latch_definition, consumer_instruction, consumer_operand,
        selected_physical_register, assignment_capacity,
        conflict_instruction);
    return SUCCEEDED;
  }
  if (assignment_capacity < 3) {
    fprintf(stderr, "register_allocator_core: loop_join_schedule function=%s temp=r%d instructions=%d entry_definition=%d latch_definition=%d consumer=%d operand=%d selected=%d assignments=3 capacity=%d conflict=-1 status=insufficient_capacity\n",
        function_name, temp_index, instruction_count, entry_definition,
        latch_definition, consumer_instruction, consumer_operand,
        selected_physical_register, assignment_capacity);
    return FAILED;
  }
  assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[0].instruction = entry_definition;
  assignments[0].operand = 0;
  assignments[0].physical_register = selected_physical_register;
  assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
  assignments[1].instruction = latch_definition;
  assignments[1].operand = 0;
  assignments[1].physical_register = selected_physical_register;
  assignments[2].role = RA_JOIN_ASSIGNMENT_CONSUMER;
  assignments[2].instruction = consumer_instruction;
  assignments[2].operand = consumer_operand;
  assignments[2].physical_register = selected_physical_register;
  fprintf(stderr, "register_allocator_core: loop_join_schedule_assignment function=%s temp=r%d assignment=0 role=entry_producer instruction=%d operand=0 phy=%d status=complete\n",
      function_name, temp_index, entry_definition, selected_physical_register);
  fprintf(stderr, "register_allocator_core: loop_join_schedule_assignment function=%s temp=r%d assignment=1 role=latch_producer instruction=%d operand=0 phy=%d status=complete\n",
      function_name, temp_index, latch_definition, selected_physical_register);
  fprintf(stderr, "register_allocator_core: loop_join_schedule_assignment function=%s temp=r%d assignment=2 role=consumer instruction=%d operand=%d phy=%d status=complete\n",
      function_name, temp_index, consumer_instruction, consumer_operand,
      selected_physical_register);
  schedule->status = RA_JOIN_SCHEDULE_READY;
  schedule->assignment_count = 3;
  fprintf(stderr, "register_allocator_core: loop_join_schedule function=%s temp=r%d instructions=%d entry_definition=%d latch_definition=%d consumer=%d operand=%d selected=%d assignments=3 capacity=%d conflict=-1 schedule=ready order=entry_latch_consumer status=complete\n",
      function_name, temp_index, instruction_count, entry_definition,
      latch_definition, consumer_instruction, consumer_operand,
      selected_physical_register, assignment_capacity);
  return SUCCEEDED;
}


int register_allocator_plan_loop_join_path_reservations(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int join_block_index, int temp_index, int slot_index,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity, struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation) {

  char *forward_blocks;
  char *full_blocks;
  char *reverse_blocks;
  char *selected_blocks;
  int *instruction_blocks;
  int block_index;
  int changed;
  int edge_index;
  int producer_count;
  int reservation_count;

  if (path_reservation != NULL) {
    path_reservation->status = RA_JOIN_PATH_RESERVATION_INELIGIBLE;
    path_reservation->block_count = 0;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || edge_count < 0 || (edge_count > 0 && edges == NULL) ||
      join_block_index < 0 || join_block_index >= block_count || temp_index < 0 ||
      slot_index < 0 || loop_join == NULL || assignment_capacity < 0 ||
      schedule == NULL || block_reservation_capacity < 0 ||
      path_reservation == NULL) {
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index, slot_index, block_count, instruction_count, edge_count,
        schedule != NULL ? schedule->assignment_count : -1,
        block_reservation_capacity);
    return FAILED;
  }
  if (loop_join->status == RA_LOOP_JOIN_NOT_LOOP &&
      schedule->status == RA_JOIN_SCHEDULE_INELIGIBLE &&
      schedule->assignment_count == 0 && schedule->conflict_instruction == -1) {
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d assignments=0 capacity=%d reservations=0 plan=ineligible status=complete\n",
        function_name, join_block_index, temp_index, slot_index, block_count,
        instruction_count, edge_count, block_reservation_capacity);
    return SUCCEEDED;
  }
  if (loop_join->status != RA_LOOP_JOIN_READY ||
      loop_join->entry_predecessor < 0 ||
      loop_join->entry_predecessor >= join_block_index ||
      loop_join->latch_predecessor < join_block_index ||
      loop_join->latch_predecessor >= block_count ||
      loop_join->entry_edge < 0 || loop_join->entry_edge >= edge_count ||
      loop_join->back_edge < 0 || loop_join->back_edge >= edge_count ||
      schedule->status != RA_JOIN_SCHEDULE_READY ||
      schedule->assignment_count < 3 || schedule->conflict_instruction != -1 ||
      assignment_capacity < schedule->assignment_count || assignments == NULL) {
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d status=invalid_plan\n",
        function_name, join_block_index, temp_index, slot_index, block_count,
        instruction_count, edge_count, schedule->assignment_count,
        block_reservation_capacity);
    return FAILED;
  }

  selected_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  forward_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  full_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  reverse_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  instruction_blocks = (int *)malloc((size_t)instruction_count * sizeof(int));
  if (selected_blocks == NULL || forward_blocks == NULL || full_blocks == NULL ||
      reverse_blocks == NULL || instruction_blocks == NULL)
    goto out_of_memory;
  for (block_index = 0; block_index < instruction_count; block_index++)
    instruction_blocks[block_index] = -1;
  for (block_index = 0; block_index < block_count; block_index++) {
    int instruction_index;

    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count) {
      fprintf(stderr, "register_allocator_core: loop_join_path_reservation_block function=%s temp=r%d block=%d start=%d end=%d status=invalid_block\n",
          function_name, temp_index, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      goto failed;
    }
    for (instruction_index = blocks[block_index].start_tac;
        instruction_index <= blocks[block_index].end_tac; instruction_index++) {
      if (instruction_blocks[instruction_index] != -1) {
        fprintf(stderr, "register_allocator_core: loop_join_path_reservation_block function=%s temp=r%d block=%d start=%d end=%d status=overlapping_block\n",
            function_name, temp_index, block_index,
            blocks[block_index].start_tac, blocks[block_index].end_tac);
        goto failed;
      }
      instruction_blocks[instruction_index] = block_index;
    }
  }
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 || edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 || edges[edge_index].to_block >= block_count ||
        (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
        edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      fprintf(stderr, "register_allocator_core: loop_join_path_reservation_edge function=%s temp=r%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, temp_index, edge_index, edges[edge_index].from_block,
          edges[edge_index].to_block, edges[edge_index].kind);
      goto failed;
    }
  }
  producer_count = 0;
  while (producer_count < schedule->assignment_count &&
      assignments[producer_count].role == RA_JOIN_ASSIGNMENT_PRODUCER)
    producer_count++;
  if (producer_count < 2 || producer_count >= schedule->assignment_count ||
      edges[loop_join->entry_edge].from_block != loop_join->entry_predecessor ||
      edges[loop_join->entry_edge].to_block != join_block_index ||
      edges[loop_join->back_edge].from_block != loop_join->latch_predecessor ||
      edges[loop_join->back_edge].to_block != join_block_index ||
      assignments[0].role != RA_JOIN_ASSIGNMENT_PRODUCER || assignments[0].operand != 0 ||
      assignments[1].role != RA_JOIN_ASSIGNMENT_PRODUCER || assignments[1].operand != 0 ||
      assignments[producer_count].role != RA_JOIN_ASSIGNMENT_CONSUMER ||
      _is_valid_tac_use_operand(assignments[producer_count].operand) == NO ||
      assignments[0].instruction < 0 || assignments[0].instruction >= instruction_count ||
      assignments[1].instruction < 0 || assignments[1].instruction >= instruction_count ||
      assignments[producer_count].instruction < 0 ||
      assignments[producer_count].instruction >= instruction_count ||
      instruction_blocks[assignments[0].instruction] != loop_join->entry_predecessor ||
      instruction_blocks[assignments[1].instruction] != loop_join->latch_predecessor ||
      instruction_blocks[assignments[producer_count].instruction] != join_block_index ||
      assignments[0].physical_register != assignments[1].physical_register ||
      assignments[0].physical_register !=
      assignments[producer_count].physical_register) {
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d status=invalid_assignment\n",
        function_name, join_block_index, temp_index, slot_index, block_count,
      instruction_count, edge_count, schedule->assignment_count,
      block_reservation_capacity);
    goto failed;
  }
  for (block_index = 1; block_index < schedule->assignment_count;
      block_index++) {
    int assignment_block;
    int has_back_edge;
    int previous_assignment;

    if ((block_index < producer_count &&
        assignments[block_index].role != RA_JOIN_ASSIGNMENT_PRODUCER) ||
        (block_index >= producer_count &&
        assignments[block_index].role != RA_JOIN_ASSIGNMENT_CONSUMER) ||
        assignments[block_index].instruction < 0 ||
        assignments[block_index].instruction >= instruction_count ||
        (block_index < producer_count &&
        assignments[block_index].operand != 0) ||
        (block_index >= producer_count &&
        _is_valid_tac_use_operand(assignments[block_index].operand) == NO) ||
        assignments[block_index].physical_register !=
        assignments[0].physical_register ||
        instruction_blocks[assignments[block_index].instruction] < 0) {
      fprintf(stderr, "register_allocator_core: loop_join_path_reservation_assignment function=%s temp=r%d assignment=%d status=invalid_assignment\n",
          function_name, temp_index, block_index);
      goto failed;
    }
    for (previous_assignment = 0; previous_assignment < block_index;
        previous_assignment++) {
      if (assignments[previous_assignment].instruction ==
          assignments[block_index].instruction &&
          assignments[previous_assignment].operand ==
          assignments[block_index].operand) {
        fprintf(stderr, "register_allocator_core: loop_join_path_reservation_assignment function=%s temp=r%d assignment=%d duplicate=%d status=invalid_assignment\n",
            function_name, temp_index, block_index, previous_assignment);
        goto failed;
      }
    }
    if (block_index >= producer_count)
      continue;
    assignment_block =
        instruction_blocks[assignments[block_index].instruction];
    has_back_edge = NO;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (edges[edge_index].from_block == assignment_block &&
          edges[edge_index].to_block == join_block_index) {
        has_back_edge = YES;
        break;
      }
    }
    if (has_back_edge == NO) {
      fprintf(stderr, "register_allocator_core: loop_join_path_reservation_assignment function=%s temp=r%d assignment=%d block=%d status=missing_back_edge\n",
          function_name, temp_index, block_index, assignment_block);
      goto failed;
    }
  }
  forward_blocks[join_block_index] = YES;
  do {
    changed = NO;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (forward_blocks[edges[edge_index].from_block] == YES &&
          forward_blocks[edges[edge_index].to_block] == NO) {
        forward_blocks[edges[edge_index].to_block] = YES;
        changed = YES;
      }
    }
  } while (changed == YES);
  selected_blocks[loop_join->entry_predecessor] = YES;
  full_blocks[loop_join->entry_predecessor] = YES;
  selected_blocks[join_block_index] = YES;
  for (edge_index = 1; edge_index < producer_count; edge_index++) {
    int producer_block;

    producer_block = instruction_blocks[assignments[edge_index].instruction];
    if (forward_blocks[producer_block] == NO) {
      fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d latch=%d reachable=no status=unreachable_latch\n",
          function_name, join_block_index, temp_index, slot_index,
          producer_block);
      goto failed;
    }
    memset(reverse_blocks, 0, (size_t)block_count * sizeof(char));
    reverse_blocks[producer_block] = YES;
    do {
      changed = NO;
      for (block_index = 0; block_index < edge_count; block_index++) {
        if (reverse_blocks[edges[block_index].to_block] == YES &&
            reverse_blocks[edges[block_index].from_block] == NO) {
          reverse_blocks[edges[block_index].from_block] = YES;
          changed = YES;
        }
      }
    } while (changed == YES);
    for (block_index = 0; block_index < block_count; block_index++) {
      if (forward_blocks[block_index] == YES &&
          reverse_blocks[block_index] == YES) {
        selected_blocks[block_index] = YES;
        full_blocks[block_index] = YES;
      }
    }
  }
  for (edge_index = producer_count + 1;
      edge_index < schedule->assignment_count; edge_index++) {
    int added_block_count;
    int consumer_block_index;
    int path_block_count;

    consumer_block_index =
        instruction_blocks[assignments[edge_index].instruction];
    if (forward_blocks[consumer_block_index] == NO) {
      fprintf(stderr, "register_allocator_core: loop_join_path_reservation_assignment function=%s temp=r%d assignment=%d block=%d reachable=no status=invalid_assignment\n",
          function_name, temp_index, edge_index, consumer_block_index);
      goto failed;
    }
    memset(reverse_blocks, 0, (size_t)block_count * sizeof(char));
    reverse_blocks[consumer_block_index] = YES;
    do {
      changed = NO;
      for (block_index = 0; block_index < edge_count; block_index++) {
        if (reverse_blocks[edges[block_index].to_block] == YES &&
            reverse_blocks[edges[block_index].from_block] == NO) {
          reverse_blocks[edges[block_index].from_block] = YES;
          changed = YES;
        }
      }
    } while (changed == YES);
    path_block_count = 0;
    added_block_count = 0;
    for (block_index = 0; block_index < block_count; block_index++) {
      if (forward_blocks[block_index] == NO ||
          reverse_blocks[block_index] == NO)
        continue;
      path_block_count++;
      if (selected_blocks[block_index] == NO) {
        selected_blocks[block_index] = YES;
        added_block_count++;
      }
      if (block_index != consumer_block_index)
        full_blocks[block_index] = YES;
    }
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation_assignment function=%s temp=r%d assignment=%d block=%d path_blocks=%d added_blocks=%d status=complete\n",
        function_name, temp_index, edge_index, consumer_block_index,
        path_block_count, added_block_count);
  }
  reservation_count = 0;
  for (block_index = 0; block_index < block_count; block_index++) {
    if (selected_blocks[block_index] == YES)
      reservation_count++;
  }
  if (reservation_count > block_reservation_capacity ||
      (reservation_count > 0 && block_reservations == NULL)) {
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d reservations=%d status=insufficient_capacity\n",
        function_name, join_block_index, temp_index, slot_index, block_count,
      instruction_count, edge_count, schedule->assignment_count,
      block_reservation_capacity, reservation_count);
    goto failed;
  }
  reservation_count = 0;
  for (block_index = 0; block_index < block_count; block_index++) {
    int end_instruction;
    int start_instruction;

    if (selected_blocks[block_index] == NO)
      continue;
    start_instruction = blocks[block_index].start_tac;
    end_instruction = blocks[block_index].end_tac;
    if (block_index == loop_join->entry_predecessor)
      start_instruction = assignments[0].instruction;
    if (full_blocks[block_index] == NO) {
      int assignment_index;

      end_instruction = -1;
        for (assignment_index = producer_count + 1;
          assignment_index < schedule->assignment_count; assignment_index++) {
        if (instruction_blocks[assignments[assignment_index].instruction] ==
            block_index && assignments[assignment_index].instruction >
            end_instruction)
          end_instruction = assignments[assignment_index].instruction;
      }
      if (end_instruction < start_instruction)
        end_instruction = blocks[block_index].end_tac;
    }
    block_reservations[reservation_count].block_index = block_index;
    block_reservations[reservation_count].start_instruction = start_instruction;
    block_reservations[reservation_count].end_instruction = end_instruction;
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation_block function=%s temp=r%d reservation=%d block=%d start=%d end=%d slot=%d status=complete\n",
        function_name, temp_index, reservation_count, block_index,
        start_instruction, end_instruction, slot_index);
    reservation_count++;
  }
  path_reservation->status = RA_JOIN_PATH_RESERVATION_READY;
  path_reservation->block_count = reservation_count;
    fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d reservations=%d plan=ready order=stable_block status=complete\n",
      function_name, join_block_index, temp_index, slot_index, block_count,
      instruction_count, edge_count, schedule->assignment_count,
      block_reservation_capacity, reservation_count);
  free(instruction_blocks);
  free(reverse_blocks);
  free(full_blocks);
  free(forward_blocks);
  free(selected_blocks);
  return SUCCEEDED;

out_of_memory:
  fprintf(stderr, "register_allocator_core: loop_join_path_reservation function=%s block=%d temp=r%d slot=%d blocks=%d instructions=%d edges=%d capacity=%d status=out_of_memory\n",
      function_name, join_block_index, temp_index, slot_index, block_count,
      instruction_count, edge_count, block_reservation_capacity);
failed:
  free(instruction_blocks);
  free(reverse_blocks);
  free(full_blocks);
  free(forward_blocks);
  free(selected_blocks);
  return FAILED;
}


int register_allocator_apply_join_assignments(char *function_name,
    int instruction_count, int temp_index, int no_physical_register,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity, struct register_allocator_join_schedule *schedule,
    void *context,
    register_allocator_join_assignment_transaction_applier apply_transaction,
    struct register_allocator_join_assignment_application *application) {

  int assignment_index;
  int physical_register;

  if (application != NULL) {
    application->status = RA_JOIN_ASSIGNMENT_APPLICATION_INELIGIBLE;
    application->applied_count = 0;
  }
  if (function_name == NULL || instruction_count <= 0 || temp_index < 0 ||
      assignment_capacity < 0 || schedule == NULL || application == NULL) {
    fprintf(stderr, "register_allocator_core: join_assignment_apply function=%s temp=r%d instructions=%d assignments=%d capacity=%d applied=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        instruction_count, schedule != NULL ? schedule->assignment_count : -1,
        assignment_capacity);
    return FAILED;
  }
  if (schedule->status == RA_JOIN_SCHEDULE_INELIGIBLE &&
      schedule->assignment_count == 0 &&
      schedule->conflict_instruction == -1) {
    fprintf(stderr, "register_allocator_core: join_assignment_apply function=%s temp=r%d instructions=%d assignments=0 capacity=%d applied=0 application=ineligible status=complete\n",
        function_name, temp_index, instruction_count, assignment_capacity);
    return SUCCEEDED;
  }
  if (schedule->status != RA_JOIN_SCHEDULE_READY ||
      schedule->assignment_count < 2 ||
      schedule->assignment_count > assignment_capacity ||
      assignments == NULL || schedule->conflict_instruction != -1 ||
      apply_transaction == NULL) {
    fprintf(stderr, "register_allocator_core: join_assignment_apply function=%s temp=r%d instructions=%d assignments=%d capacity=%d applied=0 status=invalid_schedule\n",
      function_name, temp_index, instruction_count,
      schedule->assignment_count, assignment_capacity);
    return FAILED;
  }
  physical_register = assignments[0].physical_register;
  for (assignment_index = 0;
      assignment_index < schedule->assignment_count; assignment_index++) {
    struct register_allocator_join_assignment *assignment;

    assignment = &assignments[assignment_index];
    if (assignment->instruction < 0 ||
        assignment->instruction >= instruction_count ||
        assignment->physical_register != physical_register ||
        (assignment_index + 1 < schedule->assignment_count &&
        (assignment->role != RA_JOIN_ASSIGNMENT_PRODUCER ||
        assignment->operand != 0)) ||
        (assignment_index + 1 == schedule->assignment_count &&
        (assignment->role != RA_JOIN_ASSIGNMENT_CONSUMER ||
        _is_valid_tac_use_operand(assignment->operand) == NO))) {
      fprintf(stderr, "register_allocator_core: join_assignment_apply function=%s temp=r%d assignment=%d role=%d instruction=%d operand=%d phy=%d status=invalid_assignment\n",
          function_name, temp_index, assignment_index, assignment->role,
          assignment->instruction, assignment->operand,
          assignment->physical_register);
      return FAILED;
    }
  }
  if (apply_transaction(context, temp_index, assignments,
      schedule->assignment_count) == FAILED) {
    application->status = RA_JOIN_ASSIGNMENT_APPLICATION_FAILED;
    fprintf(stderr, "register_allocator_core: join_assignment_apply function=%s temp=r%d instructions=%d assignments=%d capacity=%d applied=0 application=failed status=callback_failed\n",
        function_name, temp_index, instruction_count,
        schedule->assignment_count, assignment_capacity);
    return FAILED;
  }
  application->status = RA_JOIN_ASSIGNMENT_APPLICATION_APPLIED;
  application->applied_count = schedule->assignment_count;
  fprintf(stderr, "register_allocator_core: join_assignment_apply function=%s temp=r%d instructions=%d assignments=%d capacity=%d applied=%d application=applied status=complete\n",
      function_name, temp_index, instruction_count,
      schedule->assignment_count, assignment_capacity,
      application->applied_count);
  return SUCCEEDED;
}


int register_allocator_plan_join_reservation(char *function_name,
    int instruction_count, int temp_index, int no_physical_register,
    int selected_physical_register, int slot_index, int slot_count,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity,
    struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_occupied_interval *occupied_intervals,
    int occupied_interval_count,
    struct register_allocator_join_reservation *reservation) {

  int assignment_index;
  int interval_index;
  int live_start;
  int live_end;

  if (reservation != NULL) {
    reservation->status = RA_JOIN_RESERVATION_INELIGIBLE;
    reservation->slot_index = -1;
    reservation->live_start = -1;
    reservation->live_end = -1;
    reservation->conflict_temp = -1;
  }
  if (function_name == NULL || instruction_count <= 0 || temp_index < 0 ||
      slot_count <= 0 || assignment_capacity < 0 ||
      occupied_interval_count < 0 || schedule == NULL ||
      reservation == NULL ||
      (occupied_interval_count > 0 && occupied_intervals == NULL)) {
    fprintf(stderr, "register_allocator_core: join_reservation function=%s temp=r%d instructions=%d selected=%d slot=%d slots=%d assignments=%d capacity=%d occupied=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        instruction_count, selected_physical_register, slot_index, slot_count,
        schedule != NULL ? schedule->assignment_count : -1,
      assignment_capacity, occupied_interval_count);
    return FAILED;
  }
  if (schedule->status == RA_JOIN_SCHEDULE_INELIGIBLE &&
      schedule->assignment_count == 0 &&
      schedule->conflict_instruction == -1 &&
      selected_physical_register == no_physical_register) {
    fprintf(stderr, "register_allocator_core: join_reservation function=%s temp=r%d instructions=%d selected=%d slot=-1 slots=%d assignments=0 capacity=%d occupied=%d reservation=ineligible status=complete\n",
        function_name, temp_index, instruction_count,
        selected_physical_register, slot_count, assignment_capacity,
        occupied_interval_count);
    return SUCCEEDED;
  }
  if (schedule->status != RA_JOIN_SCHEDULE_READY ||
      schedule->assignment_count < 2 || assignments == NULL ||
      schedule->assignment_count > assignment_capacity ||
      selected_physical_register == no_physical_register || slot_index < 0 ||
      slot_index >= slot_count || schedule->conflict_instruction != -1) {
    fprintf(stderr, "register_allocator_core: join_reservation function=%s temp=r%d instructions=%d selected=%d slot=%d slots=%d assignments=%d capacity=%d occupied=%d status=invalid_schedule\n",
        function_name, temp_index, instruction_count,
        selected_physical_register, slot_index, slot_count,
        schedule->assignment_count, assignment_capacity,
        occupied_interval_count);
    return FAILED;
  }

  live_start = instruction_count;
  live_end = -1;
  for (assignment_index = 0;
      assignment_index < schedule->assignment_count; assignment_index++) {
    struct register_allocator_join_assignment *assignment;

    assignment = &assignments[assignment_index];
    if ((assignment->role != RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignment->role != RA_JOIN_ASSIGNMENT_CONSUMER) ||
        assignment->instruction < 0 ||
        assignment->instruction >= instruction_count ||
        assignment->physical_register != selected_physical_register ||
        (assignment->role == RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignment->operand != 0) ||
        (assignment->role == RA_JOIN_ASSIGNMENT_CONSUMER &&
        _is_valid_tac_use_operand(assignment->operand) == NO) ||
        (assignment_index + 1 < schedule->assignment_count &&
        assignment->role != RA_JOIN_ASSIGNMENT_PRODUCER) ||
        (assignment_index + 1 == schedule->assignment_count &&
        assignment->role != RA_JOIN_ASSIGNMENT_CONSUMER)) {
      fprintf(stderr, "register_allocator_core: join_reservation_assignment function=%s temp=r%d assignment=%d role=%d instruction=%d operand=%d phy=%d status=invalid_assignment\n",
          function_name, temp_index, assignment_index, assignment->role,
          assignment->instruction, assignment->operand,
          assignment->physical_register);
      return FAILED;
    }
    if (assignment->role == RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignment->instruction < live_start)
      live_start = assignment->instruction;
    if (assignment->role == RA_JOIN_ASSIGNMENT_CONSUMER)
      live_end = assignment->instruction;
  }
  if (live_start < 0 || live_start >= live_end) {
    fprintf(stderr, "register_allocator_core: join_reservation function=%s temp=r%d instructions=%d selected=%d slot=%d slots=%d assignments=%d occupied=%d start=%d end=%d status=invalid_span\n",
        function_name, temp_index, instruction_count,
        selected_physical_register, slot_index, slot_count,
        schedule->assignment_count, occupied_interval_count, live_start,
        live_end);
    return FAILED;
  }

  for (interval_index = 0; interval_index < occupied_interval_count;
      interval_index++) {
    struct register_allocator_join_occupied_interval *interval;

    interval = &occupied_intervals[interval_index];
    if (interval->temp_index < 0 ||
        interval->physical_register == no_physical_register ||
        interval->live_start < 0 || interval->live_end < interval->live_start ||
        interval->live_end >= instruction_count ||
        (interval->overlaps_selected_register != NO &&
        interval->overlaps_selected_register != YES)) {
      fprintf(stderr, "register_allocator_core: join_reservation_interval function=%s temp=r%d interval=%d occupied_temp=r%d phy=%d start=%d end=%d overlap=%d status=invalid_interval\n",
          function_name, temp_index, interval_index, interval->temp_index,
          interval->physical_register, interval->live_start,
          interval->live_end, interval->overlaps_selected_register);
      return FAILED;
    }
  }
  for (interval_index = 0; interval_index < occupied_interval_count;
      interval_index++) {
    struct register_allocator_join_occupied_interval *interval;

    interval = &occupied_intervals[interval_index];
    if (interval->temp_index != temp_index &&
        interval->overlaps_selected_register == YES &&
        interval->live_start <= live_end && interval->live_end >= live_start) {
      reservation->status = RA_JOIN_RESERVATION_CONFLICT;
      reservation->conflict_temp = interval->temp_index;
      fprintf(stderr, "register_allocator_core: join_reservation function=%s temp=r%d instructions=%d selected=%d slot=%d slots=%d assignments=%d occupied=%d start=%d end=%d conflict=r%d reservation=conflict status=complete\n",
          function_name, temp_index, instruction_count,
          selected_physical_register, slot_index, slot_count,
          schedule->assignment_count, occupied_interval_count, live_start,
          live_end, interval->temp_index);
      return SUCCEEDED;
    }
  }

  reservation->status = RA_JOIN_RESERVATION_READY;
  reservation->slot_index = slot_index;
  reservation->live_start = live_start;
  reservation->live_end = live_end;
  fprintf(stderr, "register_allocator_core: join_reservation function=%s temp=r%d instructions=%d selected=%d slot=%d slots=%d assignments=%d occupied=%d start=%d end=%d conflict=none reservation=ready status=complete\n",
      function_name, temp_index, instruction_count, selected_physical_register,
      slot_index, slot_count, schedule->assignment_count,
      occupied_interval_count, live_start, live_end);
  return SUCCEEDED;
}


int register_allocator_plan_join_path_reservations(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count, int temp_index,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity, struct register_allocator_join_schedule *schedule,
    struct register_allocator_join_reservation *reservation,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation) {

  char *selected_blocks;
  char *forward_blocks;
  char *reverse_blocks;
  int *instruction_blocks;
  int assignment_index;
  int block_index;
  int edge_index;
  int consumer_block;
  int consumer_instruction;
  int reservation_count;
  int changed;

  if (path_reservation != NULL) {
    path_reservation->status = RA_JOIN_PATH_RESERVATION_INELIGIBLE;
    path_reservation->block_count = 0;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || temp_index < 0 ||
      assignment_capacity < 0 || block_reservation_capacity < 0 ||
      schedule == NULL || reservation == NULL || path_reservation == NULL) {
    fprintf(stderr, "register_allocator_core: join_path_reservation function=%s temp=r%d blocks=%d instructions=%d edges=%d assignments=%d assignment_capacity=%d capacity=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        block_count, instruction_count, edge_count,
        schedule != NULL ? schedule->assignment_count : -1,
        assignment_capacity, block_reservation_capacity);
    return FAILED;
  }
  if (schedule->status == RA_JOIN_SCHEDULE_INELIGIBLE &&
      schedule->assignment_count == 0 &&
      schedule->conflict_instruction == -1 &&
      reservation->status == RA_JOIN_RESERVATION_INELIGIBLE &&
      reservation->slot_index == -1 && reservation->live_start == -1 &&
      reservation->live_end == -1 && reservation->conflict_temp == -1) {
    fprintf(stderr, "register_allocator_core: join_path_reservation function=%s temp=r%d blocks=%d instructions=%d edges=%d assignments=0 capacity=%d reservations=0 plan=ineligible status=complete\n",
        function_name, temp_index, block_count, instruction_count, edge_count,
        block_reservation_capacity);
    return SUCCEEDED;
  }
  if (schedule->status != RA_JOIN_SCHEDULE_READY ||
      schedule->assignment_count < 2 ||
      schedule->assignment_count > assignment_capacity || assignments == NULL ||
      schedule->conflict_instruction != -1 ||
      reservation->status != RA_JOIN_RESERVATION_READY ||
      reservation->slot_index < 0 || reservation->live_start < 0 ||
      reservation->live_end <= reservation->live_start ||
      reservation->live_end >= instruction_count ||
      reservation->conflict_temp != -1) {
    fprintf(stderr, "register_allocator_core: join_path_reservation function=%s temp=r%d blocks=%d instructions=%d edges=%d assignments=%d assignment_capacity=%d capacity=%d status=invalid_plan\n",
        function_name, temp_index, block_count, instruction_count, edge_count,
        schedule->assignment_count, assignment_capacity,
        block_reservation_capacity);
    return FAILED;
  }

  selected_blocks = NULL;
  forward_blocks = NULL;
  reverse_blocks = NULL;
  instruction_blocks = NULL;
  if ((size_t)block_count > ((size_t)-1) / sizeof(int) ||
      (size_t)instruction_count > ((size_t)-1) / sizeof(int))
    goto out_of_memory;
  selected_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  forward_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  reverse_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  instruction_blocks = (int *)malloc((size_t)instruction_count * sizeof(int));
  if (selected_blocks == NULL || forward_blocks == NULL ||
      reverse_blocks == NULL || instruction_blocks == NULL)
    goto out_of_memory;
  for (assignment_index = 0; assignment_index < instruction_count;
      assignment_index++)
    instruction_blocks[assignment_index] = -1;
  for (block_index = 0; block_index < block_count; block_index++) {
    int instruction_index;

    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count) {
      fprintf(stderr, "register_allocator_core: join_path_reservation_block function=%s temp=r%d block=%d start=%d end=%d status=invalid_block\n",
          function_name, temp_index, block_index, blocks[block_index].start_tac,
          blocks[block_index].end_tac);
      goto failed;
    }
    for (instruction_index = blocks[block_index].start_tac;
        instruction_index <= blocks[block_index].end_tac;
        instruction_index++) {
      if (instruction_blocks[instruction_index] != -1) {
        fprintf(stderr, "register_allocator_core: join_path_reservation_block function=%s temp=r%d block=%d start=%d end=%d status=overlapping_block\n",
            function_name, temp_index, block_index,
            blocks[block_index].start_tac, blocks[block_index].end_tac);
        goto failed;
      }
      instruction_blocks[instruction_index] = block_index;
    }
  }
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count ||
        (edges[edge_index].kind != RA_CFG_EDGE_FALLTHROUGH &&
        edges[edge_index].kind != RA_CFG_EDGE_JUMP &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_TRUE &&
        edges[edge_index].kind != RA_CFG_EDGE_BRANCH_FALSE)) {
      fprintf(stderr, "register_allocator_core: join_path_reservation_edge function=%s temp=r%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, temp_index, edge_index, edges[edge_index].from_block,
          edges[edge_index].to_block, edges[edge_index].kind);
      goto failed;
    }
  }
  for (assignment_index = 0;
      assignment_index < schedule->assignment_count; assignment_index++) {
    struct register_allocator_join_assignment *assignment;

    assignment = &assignments[assignment_index];
    if ((assignment_index + 1 < schedule->assignment_count &&
        assignment->role != RA_JOIN_ASSIGNMENT_PRODUCER) ||
        (assignment_index + 1 == schedule->assignment_count &&
        assignment->role != RA_JOIN_ASSIGNMENT_CONSUMER) ||
        assignment->instruction < 0 ||
        assignment->instruction >= instruction_count ||
        instruction_blocks[assignment->instruction] < 0 ||
        assignment->physical_register != assignments[0].physical_register ||
        (assignment->role == RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignment->operand != 0) ||
        (assignment->role == RA_JOIN_ASSIGNMENT_CONSUMER &&
        _is_valid_tac_use_operand(assignment->operand) == NO)) {
      fprintf(stderr, "register_allocator_core: join_path_reservation_assignment function=%s temp=r%d assignment=%d role=%d instruction=%d operand=%d phy=%d status=invalid_assignment\n",
          function_name, temp_index, assignment_index, assignment->role,
          assignment->instruction, assignment->operand,
          assignment->physical_register);
      goto failed;
    }
  }
  consumer_instruction =
      assignments[schedule->assignment_count - 1].instruction;
  consumer_block = instruction_blocks[consumer_instruction];
  reverse_blocks[consumer_block] = YES;
  do {
    changed = NO;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (reverse_blocks[edges[edge_index].to_block] == YES &&
          reverse_blocks[edges[edge_index].from_block] == NO) {
        reverse_blocks[edges[edge_index].from_block] = YES;
        changed = YES;
      }
    }
  } while (changed == YES);
  for (assignment_index = 0;
      assignment_index + 1 < schedule->assignment_count; assignment_index++) {
    int producer_block;

    memset(forward_blocks, 0, (size_t)block_count * sizeof(char));
    producer_block = instruction_blocks[assignments[assignment_index].instruction];
    forward_blocks[producer_block] = YES;
    do {
      changed = NO;
      for (edge_index = 0; edge_index < edge_count; edge_index++) {
        if (forward_blocks[edges[edge_index].from_block] == YES &&
            forward_blocks[edges[edge_index].to_block] == NO) {
          forward_blocks[edges[edge_index].to_block] = YES;
          changed = YES;
        }
      }
    } while (changed == YES);
    if (forward_blocks[consumer_block] == NO) {
      fprintf(stderr, "register_allocator_core: join_path_reservation_route function=%s temp=r%d producer=%d producer_block=%d consumer=%d consumer_block=%d reachable=no status=unreachable_consumer\n",
          function_name, temp_index,
          assignments[assignment_index].instruction, producer_block,
          consumer_instruction, consumer_block);
      goto failed;
    }
    for (block_index = 0; block_index < block_count; block_index++) {
      if (forward_blocks[block_index] == YES &&
          reverse_blocks[block_index] == YES)
        selected_blocks[block_index] = YES;
    }
    fprintf(stderr, "register_allocator_core: join_path_reservation_route function=%s temp=r%d producer=%d producer_block=%d consumer=%d consumer_block=%d reachable=yes status=complete\n",
        function_name, temp_index,
        assignments[assignment_index].instruction, producer_block,
        consumer_instruction, consumer_block);
  }
  reservation_count = 0;
  for (block_index = 0; block_index < block_count; block_index++) {
    if (selected_blocks[block_index] == YES)
      reservation_count++;
  }
  if (reservation_count > block_reservation_capacity ||
      (reservation_count > 0 && block_reservations == NULL)) {
    fprintf(stderr, "register_allocator_core: join_path_reservation function=%s temp=r%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d reservations=%d status=insufficient_capacity\n",
        function_name, temp_index, block_count, instruction_count, edge_count,
        schedule->assignment_count, block_reservation_capacity,
        reservation_count);
    goto failed;
  }
  reservation_count = 0;
  for (block_index = 0; block_index < block_count; block_index++) {
    int start_instruction;
    int end_instruction;
    int producer_instruction;

    if (selected_blocks[block_index] == NO)
      continue;
    start_instruction = blocks[block_index].start_tac;
    end_instruction = blocks[block_index].end_tac;
    producer_instruction = -1;
    for (assignment_index = 0;
        assignment_index + 1 < schedule->assignment_count;
        assignment_index++) {
      if (instruction_blocks[assignments[assignment_index].instruction] ==
          block_index && (producer_instruction < 0 ||
          assignments[assignment_index].instruction < producer_instruction))
        producer_instruction = assignments[assignment_index].instruction;
    }
    if (producer_instruction >= 0)
      start_instruction = producer_instruction;
    if (block_index == consumer_block)
      end_instruction = consumer_instruction;
    block_reservations[reservation_count].block_index = block_index;
    block_reservations[reservation_count].start_instruction = start_instruction;
    block_reservations[reservation_count].end_instruction = end_instruction;
    fprintf(stderr, "register_allocator_core: join_path_reservation_block function=%s temp=r%d reservation=%d block=%d start=%d end=%d slot=%d status=complete\n",
        function_name, temp_index, reservation_count, block_index,
        start_instruction, end_instruction, reservation->slot_index);
    reservation_count++;
  }
  path_reservation->status = RA_JOIN_PATH_RESERVATION_READY;
  path_reservation->block_count = reservation_count;
  fprintf(stderr, "register_allocator_core: join_path_reservation function=%s temp=r%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d reservations=%d plan=ready status=complete\n",
      function_name, temp_index, block_count, instruction_count, edge_count,
      schedule->assignment_count, block_reservation_capacity,
      reservation_count);
  free(instruction_blocks);
  free(reverse_blocks);
  free(forward_blocks);
  free(selected_blocks);
  return SUCCEEDED;

out_of_memory:
  fprintf(stderr, "register_allocator_core: join_path_reservation function=%s temp=r%d blocks=%d instructions=%d edges=%d assignments=%d capacity=%d status=out_of_memory\n",
      function_name, temp_index, block_count, instruction_count, edge_count,
      schedule->assignment_count, block_reservation_capacity);
failed:
  free(instruction_blocks);
  free(reverse_blocks);
  free(forward_blocks);
  free(selected_blocks);
  return FAILED;
}


int register_allocator_apply_join_path_reservations(char *function_name,
    int block_count, int instruction_count, int temp_index,
    struct register_allocator_join_reservation *reservation,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation,
    void *context,
    register_allocator_join_block_reservation_applier apply_reservation,
    struct register_allocator_join_path_application *application) {

  int reservation_index;
  int previous_block;

  if (application != NULL) {
    application->status = RA_JOIN_PATH_APPLICATION_INELIGIBLE;
    application->applied_count = 0;
    application->failed_block = -1;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      temp_index < 0 || block_reservation_capacity < 0 ||
      reservation == NULL || path_reservation == NULL || application == NULL) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_apply function=%s temp=r%d blocks=%d instructions=%d reservations=%d capacity=%d applied=0 failed_block=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        block_count, instruction_count,
        path_reservation != NULL ? path_reservation->block_count : -1,
        block_reservation_capacity);
    return FAILED;
  }
  if (path_reservation->status == RA_JOIN_PATH_RESERVATION_INELIGIBLE &&
      path_reservation->block_count == 0 &&
      reservation->status == RA_JOIN_RESERVATION_INELIGIBLE &&
      reservation->slot_index == -1 && reservation->live_start == -1 &&
      reservation->live_end == -1 && reservation->conflict_temp == -1) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_apply function=%s temp=r%d blocks=%d instructions=%d reservations=0 capacity=%d applied=0 failed_block=-1 application=ineligible status=complete\n",
        function_name, temp_index, block_count, instruction_count,
        block_reservation_capacity);
    return SUCCEEDED;
  }
  if (path_reservation->status != RA_JOIN_PATH_RESERVATION_READY ||
      path_reservation->block_count <= 0 ||
      path_reservation->block_count > block_reservation_capacity ||
      block_reservations == NULL ||
      reservation->status != RA_JOIN_RESERVATION_READY ||
      reservation->slot_index < 0 || reservation->live_start < 0 ||
      reservation->live_end <= reservation->live_start ||
      reservation->live_end >= instruction_count ||
      reservation->conflict_temp != -1 || apply_reservation == NULL) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_apply function=%s temp=r%d blocks=%d instructions=%d reservations=%d capacity=%d applied=0 failed_block=-1 status=invalid_plan\n",
        function_name, temp_index, block_count, instruction_count,
        path_reservation->block_count, block_reservation_capacity);
    return FAILED;
  }
  previous_block = -1;
  for (reservation_index = 0;
      reservation_index < path_reservation->block_count;
      reservation_index++) {
    struct register_allocator_join_block_reservation *block_reservation;

    block_reservation = &block_reservations[reservation_index];
    if (block_reservation->block_index <= previous_block ||
        block_reservation->block_index < 0 ||
        block_reservation->block_index >= block_count ||
        block_reservation->start_instruction < 0 ||
        block_reservation->end_instruction <
        block_reservation->start_instruction ||
        block_reservation->end_instruction >= instruction_count ||
        block_reservation->start_instruction < reservation->live_start ||
        block_reservation->end_instruction > reservation->live_end) {
      fprintf(stderr, "register_allocator_core: join_path_reservation_apply_block function=%s temp=r%d reservation=%d block=%d start=%d end=%d slot=%d status=invalid_reservation\n",
          function_name, temp_index, reservation_index,
          block_reservation->block_index,
          block_reservation->start_instruction,
          block_reservation->end_instruction, reservation->slot_index);
      return FAILED;
    }
    previous_block = block_reservation->block_index;
  }
  for (reservation_index = 0;
      reservation_index < path_reservation->block_count;
      reservation_index++) {
    struct register_allocator_join_block_reservation *block_reservation;

    block_reservation = &block_reservations[reservation_index];
    if (apply_reservation(context, temp_index, reservation->slot_index,
        block_reservation) == FAILED) {
      application->status = RA_JOIN_PATH_APPLICATION_PARTIAL;
      application->applied_count = reservation_index;
      application->failed_block = block_reservation->block_index;
      fprintf(stderr, "register_allocator_core: join_path_reservation_apply function=%s temp=r%d blocks=%d instructions=%d reservations=%d capacity=%d applied=%d failed_block=%d application=partial status=callback_failed\n",
          function_name, temp_index, block_count, instruction_count,
          path_reservation->block_count, block_reservation_capacity,
          application->applied_count, application->failed_block);
      return FAILED;
    }
    fprintf(stderr, "register_allocator_core: join_path_reservation_apply_block function=%s temp=r%d reservation=%d block=%d start=%d end=%d slot=%d status=complete\n",
        function_name, temp_index, reservation_index,
        block_reservation->block_index,
        block_reservation->start_instruction,
        block_reservation->end_instruction, reservation->slot_index);
  }
  application->status = RA_JOIN_PATH_APPLICATION_APPLIED;
  application->applied_count = path_reservation->block_count;
  fprintf(stderr, "register_allocator_core: join_path_reservation_apply function=%s temp=r%d blocks=%d instructions=%d reservations=%d capacity=%d applied=%d failed_block=-1 application=applied status=complete\n",
      function_name, temp_index, block_count, instruction_count,
      path_reservation->block_count, block_reservation_capacity,
      application->applied_count);
  return SUCCEEDED;
}


int register_allocator_commit_join_path_reservations(char *function_name,
    int block_count, int instruction_count, int temp_index,
    struct register_allocator_join_reservation *reservation,
    struct register_allocator_join_block_reservation *block_reservations,
    int block_reservation_capacity,
    struct register_allocator_join_path_reservation *path_reservation,
    struct register_allocator_join_path_state_entry *state_entries,
    int state_capacity, int *state_count,
    struct register_allocator_join_path_commit *commit) {

  int reservation_index;
  int current_count;
  int previous_block;

  if (commit != NULL) {
    commit->status = RA_JOIN_PATH_COMMIT_INELIGIBLE;
    commit->committed_count = 0;
    commit->conflict_entry = -1;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      temp_index < 0 || block_reservation_capacity < 0 ||
      state_capacity < 0 || reservation == NULL || path_reservation == NULL ||
      state_count == NULL || commit == NULL) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=%d reservation_capacity=%d state_count=%d state_capacity=%d committed=0 conflict=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        block_count, instruction_count,
        path_reservation != NULL ? path_reservation->block_count : -1,
        block_reservation_capacity, state_count != NULL ? *state_count : -1,
        state_capacity);
    return FAILED;
  }
  current_count = *state_count;
  if (current_count < 0 || current_count > state_capacity ||
      (state_capacity > 0 && state_entries == NULL)) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=%d reservation_capacity=%d state_count=%d state_capacity=%d committed=0 conflict=-1 status=invalid_state\n",
        function_name, temp_index, block_count, instruction_count,
        path_reservation->block_count, block_reservation_capacity,
        current_count, state_capacity);
    return FAILED;
  }
  if (path_reservation->status == RA_JOIN_PATH_RESERVATION_INELIGIBLE &&
      path_reservation->block_count == 0 &&
      reservation->status == RA_JOIN_RESERVATION_INELIGIBLE &&
      reservation->slot_index == -1 && reservation->live_start == -1 &&
      reservation->live_end == -1 && reservation->conflict_temp == -1) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=0 reservation_capacity=%d state_count=%d state_capacity=%d committed=0 conflict=-1 commit=ineligible status=complete\n",
        function_name, temp_index, block_count, instruction_count,
        block_reservation_capacity, current_count, state_capacity);
    return SUCCEEDED;
  }
  if (path_reservation->status != RA_JOIN_PATH_RESERVATION_READY ||
      path_reservation->block_count <= 0 ||
      path_reservation->block_count > block_reservation_capacity ||
      block_reservations == NULL ||
      reservation->status != RA_JOIN_RESERVATION_READY ||
      reservation->slot_index < 0 || reservation->live_start < 0 ||
      reservation->live_end <= reservation->live_start ||
      reservation->live_end >= instruction_count ||
      reservation->conflict_temp != -1) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=%d reservation_capacity=%d state_count=%d state_capacity=%d committed=0 conflict=-1 status=invalid_plan\n",
        function_name, temp_index, block_count, instruction_count,
        path_reservation->block_count, block_reservation_capacity,
        current_count, state_capacity);
    return FAILED;
  }
  if (path_reservation->block_count > state_capacity - current_count) {
    fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=%d reservation_capacity=%d state_count=%d state_capacity=%d committed=0 conflict=-1 status=insufficient_capacity\n",
        function_name, temp_index, block_count, instruction_count,
        path_reservation->block_count, block_reservation_capacity,
        current_count, state_capacity);
    return FAILED;
  }
  previous_block = -1;
  for (reservation_index = 0;
      reservation_index < path_reservation->block_count;
      reservation_index++) {
    struct register_allocator_join_block_reservation *block_reservation;
    int state_index;

    block_reservation = &block_reservations[reservation_index];
    if (block_reservation->block_index <= previous_block ||
        block_reservation->block_index < 0 ||
        block_reservation->block_index >= block_count ||
        block_reservation->start_instruction < reservation->live_start ||
        block_reservation->end_instruction <
        block_reservation->start_instruction ||
        block_reservation->end_instruction > reservation->live_end ||
        block_reservation->end_instruction >= instruction_count) {
      fprintf(stderr, "register_allocator_core: join_path_reservation_commit_block function=%s temp=r%d reservation=%d block=%d start=%d end=%d slot=%d status=invalid_reservation\n",
          function_name, temp_index, reservation_index,
          block_reservation->block_index,
          block_reservation->start_instruction,
          block_reservation->end_instruction, reservation->slot_index);
      return FAILED;
    }
    previous_block = block_reservation->block_index;
    for (state_index = 0; state_index < current_count; state_index++) {
      struct register_allocator_join_path_state_entry *state_entry;

      state_entry = &state_entries[state_index];
      if (state_entry->block_index < 0 ||
          state_entry->block_index >= block_count ||
          state_entry->slot_index < 0 || state_entry->temp_index < 0 ||
          state_entry->start_instruction < 0 ||
          state_entry->end_instruction < state_entry->start_instruction ||
          state_entry->end_instruction >= instruction_count) {
        fprintf(stderr, "register_allocator_core: join_path_reservation_commit_state function=%s temp=r%d entry=%d block=%d slot=%d occupied_temp=r%d start=%d end=%d status=invalid_entry\n",
            function_name, temp_index, state_index, state_entry->block_index,
            state_entry->slot_index, state_entry->temp_index,
            state_entry->start_instruction, state_entry->end_instruction);
        return FAILED;
      }
      if (state_entry->block_index == block_reservation->block_index &&
          state_entry->slot_index == reservation->slot_index &&
          state_entry->start_instruction <= block_reservation->end_instruction &&
          state_entry->end_instruction >= block_reservation->start_instruction) {
        commit->status = RA_JOIN_PATH_COMMIT_CONFLICT;
        commit->conflict_entry = state_index;
        fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=%d reservation_capacity=%d state_count=%d state_capacity=%d committed=0 conflict=%d commit=conflict status=complete\n",
            function_name, temp_index, block_count, instruction_count,
            path_reservation->block_count, block_reservation_capacity,
            current_count, state_capacity, state_index);
        return SUCCEEDED;
      }
    }
  }
  for (reservation_index = 0;
      reservation_index < path_reservation->block_count;
      reservation_index++) {
    struct register_allocator_join_path_state_entry *state_entry;

    state_entry = &state_entries[current_count + reservation_index];
    state_entry->block_index = block_reservations[reservation_index].block_index;
    state_entry->slot_index = reservation->slot_index;
    state_entry->temp_index = temp_index;
    state_entry->start_instruction =
        block_reservations[reservation_index].start_instruction;
    state_entry->end_instruction =
        block_reservations[reservation_index].end_instruction;
    fprintf(stderr, "register_allocator_core: join_path_reservation_commit_entry function=%s temp=r%d entry=%d block=%d slot=%d start=%d end=%d status=complete\n",
        function_name, temp_index, current_count + reservation_index,
        state_entry->block_index, state_entry->slot_index,
        state_entry->start_instruction, state_entry->end_instruction);
  }
  *state_count = current_count + path_reservation->block_count;
  commit->status = RA_JOIN_PATH_COMMIT_COMMITTED;
  commit->committed_count = path_reservation->block_count;
  fprintf(stderr, "register_allocator_core: join_path_reservation_commit function=%s temp=r%d blocks=%d instructions=%d reservations=%d reservation_capacity=%d state_count=%d state_capacity=%d committed=%d conflict=-1 commit=committed status=complete\n",
      function_name, temp_index, block_count, instruction_count,
      path_reservation->block_count, block_reservation_capacity, *state_count,
      state_capacity, commit->committed_count);
  return SUCCEEDED;
}


int register_allocator_plan_join_path_state_storage(char *function_name,
    int block_count, int temp_count,
    struct register_allocator_join_path_state_storage *storage) {

  int entry_capacity;

  if (storage != NULL) {
    storage->entry_capacity = 0;
    storage->entry_bytes = 0;
  }
  if (function_name == NULL || block_count <= 0 || temp_count <= 0 ||
      storage == NULL || block_count > INT_MAX / temp_count) {
    fprintf(stderr, "register_allocator_core: join_path_state_storage function=%s blocks=%d temps=%d capacity=0 bytes=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_count,
        temp_count);
    return FAILED;
  }
  entry_capacity = block_count * temp_count;
  if ((size_t)entry_capacity > ((size_t)-1) /
      sizeof(struct register_allocator_join_path_state_entry) ||
      (size_t)entry_capacity > (size_t)ULONG_MAX /
      sizeof(struct register_allocator_join_path_state_entry)) {
    fprintf(stderr, "register_allocator_core: join_path_state_storage function=%s blocks=%d temps=%d capacity=0 bytes=0 status=invalid_input\n",
        function_name, block_count, temp_count);
    return FAILED;
  }
  storage->entry_capacity = entry_capacity;
  storage->entry_bytes = (size_t)entry_capacity *
      sizeof(struct register_allocator_join_path_state_entry);
  fprintf(stderr, "register_allocator_core: join_path_state_storage function=%s blocks=%d temps=%d capacity=%d bytes=%lu status=complete\n",
      function_name, block_count, temp_count, storage->entry_capacity,
      (unsigned long)storage->entry_bytes);
  return SUCCEEDED;
}


int register_allocator_query_join_path_state(char *function_name,
    int block_count, int instruction_count, int slot_count, int block_index,
    int slot_index, int temp_index, int start_instruction,
    int end_instruction,
    struct register_allocator_join_path_state_entry *state_entries,
    int state_count,
    struct register_allocator_join_path_state_query *query) {

  int state_index;

  if (query != NULL) {
    query->status = RA_JOIN_PATH_STATE_AVAILABLE;
    query->entry_index = -1;
    query->owner_temp = -1;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      slot_count <= 0 || block_index < 0 || block_index >= block_count ||
      slot_index < 0 || slot_index >= slot_count || temp_index < 0 ||
      start_instruction < 0 || end_instruction < start_instruction ||
      end_instruction >= instruction_count || state_count < 0 ||
      (state_count > 0 && state_entries == NULL) || query == NULL) {
    fprintf(stderr, "register_allocator_core: join_path_state_query function=%s block=%d slot=%d temp=r%d start=%d end=%d entries=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        slot_index, temp_index, start_instruction, end_instruction,
        state_count);
    return FAILED;
  }
  for (state_index = 0; state_index < state_count; state_index++) {
    struct register_allocator_join_path_state_entry *entry;

    entry = &state_entries[state_index];
    if (entry->block_index < 0 || entry->block_index >= block_count ||
        entry->slot_index < 0 || entry->slot_index >= slot_count ||
        entry->temp_index < 0 || entry->start_instruction < 0 ||
        entry->end_instruction < entry->start_instruction ||
        entry->end_instruction >= instruction_count) {
      fprintf(stderr, "register_allocator_core: join_path_state_query_entry function=%s entry=%d block=%d slot=%d temp=r%d start=%d end=%d status=invalid_entry\n",
          function_name, state_index, entry->block_index, entry->slot_index,
          entry->temp_index, entry->start_instruction,
          entry->end_instruction);
      return FAILED;
    }
  }
  for (state_index = 0; state_index < state_count; state_index++) {
    struct register_allocator_join_path_state_entry *entry;

    entry = &state_entries[state_index];
    if (entry->block_index != block_index ||
        entry->slot_index != slot_index ||
        entry->start_instruction > end_instruction ||
        entry->end_instruction < start_instruction)
      continue;
    query->status = entry->temp_index == temp_index ?
        RA_JOIN_PATH_STATE_OWNED : RA_JOIN_PATH_STATE_CONFLICT;
    query->entry_index = state_index;
    query->owner_temp = entry->temp_index;
    fprintf(stderr, "register_allocator_core: join_path_state_query function=%s block=%d slot=%d temp=r%d start=%d end=%d entries=%d entry=%d owner=r%d reservation=%s status=complete\n",
        function_name, block_index, slot_index, temp_index,
        start_instruction, end_instruction, state_count, state_index,
        entry->temp_index, query->status == RA_JOIN_PATH_STATE_OWNED ?
        "owned" : "conflict");
    return SUCCEEDED;
  }
  fprintf(stderr, "register_allocator_core: join_path_state_query function=%s block=%d slot=%d temp=r%d start=%d end=%d entries=%d entry=-1 owner=r-1 reservation=available status=complete\n",
      function_name, block_index, slot_index, temp_index,
      start_instruction, end_instruction, state_count);
  return SUCCEEDED;
}


int register_allocator_plan_block_exit_spill(char *function_name,
    int block_count, int instruction_count, int slot_count, int block_index,
    int end_instruction, int temp_index,
    struct register_allocator_join_path_state_entry *state_entries,
    int state_count, struct register_allocator_block_exit_spill *spill) {

  int state_index;

  if (spill != NULL) {
    spill->status = RA_BLOCK_EXIT_SPILL_REQUIRED;
    spill->reservation_entry = -1;
    spill->slot_index = -1;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      slot_count <= 0 || block_index < 0 || block_index >= block_count ||
      end_instruction < 0 || end_instruction >= instruction_count ||
      temp_index < 0 || state_count < 0 ||
      (state_count > 0 && state_entries == NULL) || spill == NULL) {
    fprintf(stderr, "register_allocator_core: block_exit_spill_plan function=%s block=%d end=%d temp=r%d entries=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        end_instruction, temp_index, state_count);
    return FAILED;
  }
  for (state_index = 0; state_index < state_count; state_index++) {
    struct register_allocator_join_path_state_entry *entry;

    entry = &state_entries[state_index];
    if (entry->block_index < 0 || entry->block_index >= block_count ||
        entry->slot_index < 0 || entry->slot_index >= slot_count ||
        entry->temp_index < 0 || entry->start_instruction < 0 ||
        entry->end_instruction < entry->start_instruction ||
        entry->end_instruction >= instruction_count) {
      fprintf(stderr, "register_allocator_core: block_exit_spill_plan_entry function=%s entry=%d block=%d slot=%d temp=r%d start=%d end=%d status=invalid_entry\n",
          function_name, state_index, entry->block_index, entry->slot_index,
          entry->temp_index, entry->start_instruction,
          entry->end_instruction);
      return FAILED;
    }
  }
  for (state_index = 0; state_index < state_count; state_index++) {
    struct register_allocator_join_path_state_entry *entry;

    entry = &state_entries[state_index];
    if (entry->block_index != block_index ||
        entry->temp_index != temp_index ||
        entry->start_instruction > end_instruction ||
        entry->end_instruction < end_instruction)
      continue;
    if (spill->status == RA_BLOCK_EXIT_SPILL_EXEMPT) {
      fprintf(stderr, "register_allocator_core: block_exit_spill_plan function=%s block=%d end=%d temp=r%d entries=%d status=ambiguous_reservation\n",
          function_name, block_index, end_instruction, temp_index,
          state_count);
      spill->status = RA_BLOCK_EXIT_SPILL_REQUIRED;
      spill->reservation_entry = -1;
      spill->slot_index = -1;
      return FAILED;
    }
    spill->status = RA_BLOCK_EXIT_SPILL_EXEMPT;
    spill->reservation_entry = state_index;
    spill->slot_index = entry->slot_index;
  }
  fprintf(stderr, "register_allocator_core: block_exit_spill_plan function=%s block=%d end=%d temp=r%d entries=%d entry=%d slot=%d spill=%s status=complete\n",
      function_name, block_index, end_instruction, temp_index, state_count,
      spill->reservation_entry, spill->slot_index,
      spill->status == RA_BLOCK_EXIT_SPILL_EXEMPT ? "exempt" : "required");
  return SUCCEEDED;
}


int register_allocator_apply_join_spill_emission(char *function_name,
    int join_block_index, int no_physical_register, void *context,
    struct register_allocator_join_spill_emission *emission,
    register_allocator_join_spill_emission_applier apply_emission) {

  if (function_name == NULL || join_block_index < 0 || emission == NULL ||
      (emission != NULL && emission->emit != NO && emission->emit != YES) ||
      (emission != NULL && emission->emit == NO &&
      (emission->site.predecessor_block != -1 ||
      emission->site.edge_kind != 0 ||
      emission->site.anchor_instruction != -1 ||
      emission->site.placement != 0 || emission->temp_index != -1 ||
      emission->physical_register != no_physical_register ||
      emission->destination_offset != 0 || emission->byte_count != 0)) ||
      (emission != NULL && emission->emit == YES &&
      (emission->site.predecessor_block < 0 ||
      emission->site.anchor_instruction < 0 || emission->temp_index < 0 ||
      emission->physical_register == no_physical_register ||
      emission->byte_count <= 0 ||
      (emission->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      emission->site.edge_kind != RA_CFG_EDGE_JUMP &&
      emission->site.edge_kind != RA_CFG_EDGE_BRANCH_TRUE &&
      emission->site.edge_kind != RA_CFG_EDGE_BRANCH_FALSE) ||
      (emission->site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR &&
      emission->site.placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (emission->site.edge_kind == RA_CFG_EDGE_FALLTHROUGH &&
      emission->site.placement != RA_JOIN_SPILL_AFTER_ANCHOR) ||
      (emission->site.edge_kind != RA_CFG_EDGE_FALLTHROUGH &&
      emission->site.placement != RA_JOIN_SPILL_BEFORE_ANCHOR) ||
      apply_emission == NULL))) {
    fprintf(stderr, "register_allocator_core: join_spill_emission_apply function=%s block=%d emit=%s predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d destination_offset=%d bytes=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        emission != NULL && emission->emit == YES ? "yes" :
        (emission != NULL && emission->emit == NO ? "no" : "invalid"),
        emission != NULL ? emission->site.predecessor_block : -1,
        emission != NULL ? _register_allocator_join_edge_name(
        emission->site.edge_kind) : "invalid",
        emission != NULL ? emission->site.anchor_instruction : -1,
        emission != NULL ? _register_allocator_join_spill_placement_name(
        emission->site.placement) : "invalid",
        emission != NULL ? emission->temp_index : -1,
        emission != NULL ? emission->physical_register : no_physical_register,
        emission != NULL ? emission->destination_offset : 0,
        emission != NULL ? emission->byte_count : 0);
    return FAILED;
  }

  if (emission->emit == NO) {
    fprintf(stderr, "register_allocator_core: join_spill_emission_apply function=%s block=%d emit=no status=skipped reason=empty_plan\n",
        function_name, join_block_index);
    return SUCCEEDED;
  }

  if (apply_emission(context, join_block_index, &emission->site,
      emission->temp_index, emission->physical_register,
      emission->destination_offset, emission->byte_count) == FAILED) {
    fprintf(stderr, "register_allocator_core: join_spill_emission_apply function=%s block=%d emit=yes predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d destination_offset=%d bytes=%d status=callback_failed\n",
        function_name, join_block_index, emission->site.predecessor_block,
        _register_allocator_join_edge_name(emission->site.edge_kind),
        emission->site.anchor_instruction,
        _register_allocator_join_spill_placement_name(
        emission->site.placement), emission->temp_index,
        emission->physical_register, emission->destination_offset,
        emission->byte_count);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_core: join_spill_emission_apply function=%s block=%d emit=yes predecessor=%d edge=%s anchor=%d placement=%s temp=r%d phy=%d destination_offset=%d bytes=%d status=complete\n",
      function_name, join_block_index, emission->site.predecessor_block,
      _register_allocator_join_edge_name(emission->site.edge_kind),
      emission->site.anchor_instruction,
      _register_allocator_join_spill_placement_name(emission->site.placement),
      emission->temp_index, emission->physical_register,
      emission->destination_offset, emission->byte_count);
  return SUCCEEDED;
}


int register_allocator_plan_split_action(char *function_name, int block_index, int temp_index, int producer_instruction, int consumer_instruction, int has_single_read, int preservation_supported, int no_physical_register, int preservation_physical_register, int selected_physical_register) {

  int action;
  char *reason;

  if (function_name == NULL || block_index < 0 || temp_index < 0 || producer_instruction < 0 ||
      consumer_instruction <= producer_instruction ||
      (has_single_read != NO && has_single_read != YES) ||
      (preservation_supported != NO && preservation_supported != YES) ||
      selected_physical_register == no_physical_register ||
      (has_single_read == YES && preservation_supported != NO) ||
      (preservation_supported == YES && preservation_physical_register == no_physical_register) ||
      (preservation_supported == NO && preservation_physical_register != no_physical_register)) {
    fprintf(stderr, "register_allocator_core: split_action function=%s block=%d temp=r%d producer=%d consumer=%d single_read=%s preservation_supported=%s preservation_phy=%d selected_phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, producer_instruction, consumer_instruction,
        has_single_read == YES ? "yes" : (has_single_read == NO ? "no" : "invalid"),
        preservation_supported == YES ? "yes" : (preservation_supported == NO ? "no" : "invalid"),
        preservation_physical_register, selected_physical_register);
    return FAILED;
  }

  if (has_single_read == YES) {
    action = RA_SPLIT_ACTION_NONE;
    reason = "single_read";
  }
  else if (preservation_supported == NO) {
    action = RA_SPLIT_ACTION_REJECT_UNSUPPORTED;
    reason = "preservation_unsupported";
  }
  else if (selected_physical_register != preservation_physical_register) {
    action = RA_SPLIT_ACTION_REJECT_REGISTER;
    reason = "preservation_register_changed";
  }
  else {
    action = RA_SPLIT_ACTION_PRESERVE;
    reason = "multi_read_interval";
  }

  fprintf(stderr, "register_allocator_core: split_action function=%s block=%d temp=r%d producer=%d consumer=%d single_read=%s preservation_supported=%s preservation_phy=%d selected_phy=%d action=%s reason=%s status=complete\n",
      function_name, block_index, temp_index, producer_instruction, consumer_instruction,
      has_single_read == YES ? "yes" : "no", preservation_supported == YES ? "yes" : "no",
      preservation_physical_register, selected_physical_register,
      action == RA_SPLIT_ACTION_NONE ? "none" :
      (action == RA_SPLIT_ACTION_PRESERVE ? "preserve" :
      (action == RA_SPLIT_ACTION_REJECT_UNSUPPORTED ? "reject_unsupported" : "reject_register")), reason);
  return action;
}


static void _clear_reload_site(struct register_allocator_reload_site *reload_site) {

  reload_site->found = NO;
  reload_site->instruction = -1;
}


static void _clear_reload_chain(struct register_allocator_reload_chain *reload_chain) {

  reload_chain->count = 0;
  reload_chain->truncated = NO;
  reload_chain->stop_instruction = -1;
}


int register_allocator_plan_reload_scan_range(char *function_name, int block_count,
    int instruction_count, struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count, int start_block,
    struct register_allocator_reload_scan_range *range) {

  char *reason_name;
  char *visited;
  int block_index;
  int current_block;

  if (range != NULL) {
    range->end_block = -1;
    range->end_instruction = -1;
    range->extended_block_count = 0;
    range->stop_reason = RA_RELOAD_RANGE_BLOCK_END;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || edge_count < 0 || (edge_count > 0 && edges == NULL) ||
      start_block < 0 || start_block >= block_count || range == NULL) {
    fprintf(stderr, "register_allocator_core: reload_scan_range function=%s start_block=%d blocks=%d edges=%d end_block=-1 end=-1 extended=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", start_block,
        block_count, edge_count);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
        blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: reload_scan_range function=%s start_block=%d block=%d start=%d end=%d status=invalid_block\n",
          function_name, start_block, block_index,
          blocks[block_index].start_tac, blocks[block_index].end_tac);
      return FAILED;
    }
  }
  for (block_index = 0; block_index < edge_count; block_index++) {
    if (edges[block_index].from_block < 0 ||
        edges[block_index].from_block >= block_count ||
        edges[block_index].to_block < 0 ||
        edges[block_index].to_block >= block_count ||
        edges[block_index].kind < RA_CFG_EDGE_FALLTHROUGH ||
        edges[block_index].kind > RA_CFG_EDGE_BRANCH_FALSE) {
      fprintf(stderr, "register_allocator_core: reload_scan_range function=%s start_block=%d edge=%d from=%d to=%d kind=%d status=invalid_edge\n",
          function_name, start_block, block_index,
          edges[block_index].from_block, edges[block_index].to_block,
          edges[block_index].kind);
      return FAILED;
    }
  }
  visited = (char *)calloc((size_t)block_count, sizeof(char));
  if (visited == NULL) {
    fprintf(stderr, "register_allocator_core: reload_scan_range function=%s start_block=%d blocks=%d status=out_of_memory\n",
        function_name, start_block, block_count);
    return FAILED;
  }

  current_block = start_block;
  range->end_block = start_block;
  range->end_instruction = blocks[start_block].end_tac;
  while (1) {
    int edge_index;
    int incoming_count;
    int outgoing_count;
    int successor;
    int successor_edge_kind;

    if (visited[current_block] == YES) {
      range->stop_reason = RA_RELOAD_RANGE_CYCLE;
      break;
    }
    visited[current_block] = YES;
    incoming_count = 0;
    outgoing_count = 0;
    successor = -1;
    successor_edge_kind = -1;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (edges[edge_index].from_block == current_block) {
        outgoing_count++;
        successor = edges[edge_index].to_block;
        successor_edge_kind = edges[edge_index].kind;
      }
    }
    if (outgoing_count == 0) {
      range->stop_reason = RA_RELOAD_RANGE_FUNCTION_END;
      break;
    }
    if (outgoing_count != 1 || successor_edge_kind != RA_CFG_EDGE_FALLTHROUGH) {
      range->stop_reason = RA_RELOAD_RANGE_BRANCH;
      break;
    }
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      if (edges[edge_index].to_block == successor)
        incoming_count++;
    }
    if (incoming_count != 1) {
      range->stop_reason = RA_RELOAD_RANGE_JOIN;
      break;
    }
    if (visited[successor] == YES) {
      range->stop_reason = RA_RELOAD_RANGE_CYCLE;
      break;
    }
    if (blocks[successor].start_tac != blocks[current_block].end_tac + 1) {
      range->stop_reason = RA_RELOAD_RANGE_NONCONTIGUOUS;
      break;
    }
    current_block = successor;
    range->end_block = successor;
    range->end_instruction = blocks[successor].end_tac;
    range->extended_block_count++;
  }
  free(visited);
  reason_name = range->stop_reason == RA_RELOAD_RANGE_FUNCTION_END ?
      "function_end" :
      (range->stop_reason == RA_RELOAD_RANGE_BRANCH ? "branch" :
      (range->stop_reason == RA_RELOAD_RANGE_JOIN ? "join" :
      (range->stop_reason == RA_RELOAD_RANGE_CYCLE ? "cycle" :
      (range->stop_reason == RA_RELOAD_RANGE_NONCONTIGUOUS ?
      "noncontiguous" : "block_end"))));
  fprintf(stderr, "register_allocator_core: reload_scan_range function=%s start_block=%d end_block=%d start=%d end=%d extended=%d reason=%s status=complete\n",
      function_name, start_block, range->end_block,
      blocks[start_block].start_tac, range->end_instruction,
      range->extended_block_count, reason_name);
  return SUCCEEDED;
}


int register_allocator_discover_reload_chain(char *function_name, int block_index, int temp_index, int start_instruction, int end_instruction, int instruction_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, int *reload_instructions, int max_reload_instructions, struct register_allocator_reload_chain *reload_chain) {

  int instruction;
  int reload_index;
  char *reason;

  if (reload_chain != NULL)
    _clear_reload_chain(reload_chain);
  if (reload_instructions != NULL && max_reload_instructions > 0) {
    for (reload_index = 0; reload_index < max_reload_instructions; reload_index++)
      reload_instructions[reload_index] = -1;
  }

  if (function_name == NULL || block_index < 0 || temp_index < 0 || start_instruction < 0 ||
      start_instruction > instruction_count || end_instruction >= instruction_count || instruction_count <= 0 ||
      is_active == NULL || reads_temp == NULL || writes_temp == NULL || is_reload_eligible == NULL ||
      reload_instructions == NULL || max_reload_instructions <= 0 || reload_chain == NULL) {
    fprintf(stderr, "register_allocator_core: reload_chain function=%s block=%d temp=r%d start=%d end=%d instructions=%d capacity=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, start_instruction,
        end_instruction, instruction_count, max_reload_instructions);
    return FAILED;
  }

  reason = "block_end";
  for (instruction = start_instruction; instruction <= end_instruction; instruction++) {
    if (is_active(context, instruction, temp_index) == NO)
      continue;

    if (reads_temp(context, instruction, temp_index) == YES) {
      if (is_reload_eligible(context, instruction, temp_index) != YES) {
        reload_chain->stop_instruction = instruction;
        reason = "read_ineligible";
        break;
      }
      if (reload_chain->count >= max_reload_instructions) {
        reload_chain->truncated = YES;
        reload_chain->stop_instruction = instruction;
        reason = "capacity";
        break;
      }

      reload_instructions[reload_chain->count] = instruction;
      reload_chain->count++;
      fprintf(stderr, "register_allocator_core: reload_chain_site function=%s block=%d temp=r%d ordinal=%d instruction=%d status=found\n",
          function_name, block_index, temp_index, reload_chain->count, instruction);

      if (writes_temp(context, instruction, temp_index) == YES) {
        reload_chain->stop_instruction = instruction;
        reason = "redefined_after_read";
        break;
      }
      continue;
    }

    if (writes_temp(context, instruction, temp_index) == YES) {
      reload_chain->stop_instruction = instruction;
      reason = "redefined";
      break;
    }
  }

  fprintf(stderr, "register_allocator_core: reload_chain function=%s block=%d temp=r%d start=%d end=%d count=%d capacity=%d truncated=%s stop=%d reason=%s status=complete\n",
      function_name, block_index, temp_index, start_instruction, end_instruction, reload_chain->count,
      max_reload_instructions, reload_chain->truncated == YES ? "yes" : "no",
      reload_chain->stop_instruction, reason);
  if (reload_chain->count > 0)
    fprintf(stderr, "register_allocator_core: reload_site function=%s block=%d temp=r%d start=%d end=%d instruction=%d selection=first_next_read status=found\n",
        function_name, block_index, temp_index, start_instruction, end_instruction, reload_instructions[0]);
  return SUCCEEDED;
}


int register_allocator_discover_reload_site(char *function_name, int block_index, int temp_index, int start_instruction, int end_instruction, int instruction_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, register_allocator_instruction_predicate is_reload_eligible, struct register_allocator_reload_site *reload_site) {

  int instruction;

  if (reload_site != NULL)
    _clear_reload_site(reload_site);

  if (function_name == NULL || block_index < 0 || temp_index < 0 || start_instruction < 0 ||
      start_instruction > instruction_count || end_instruction >= instruction_count || instruction_count <= 0 ||
      is_active == NULL || reads_temp == NULL || writes_temp == NULL || is_reload_eligible == NULL || reload_site == NULL) {
    fprintf(stderr, "register_allocator_core: reload_site function=%s block=%d temp=r%d start=%d end=%d instructions=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, start_instruction, end_instruction, instruction_count);
    return FAILED;
  }

  instruction = register_allocator_find_next_use(function_name, temp_index, start_instruction, end_instruction,
      instruction_count, context, is_active, reads_temp, writes_temp);
  if (instruction < 0) {
    fprintf(stderr, "register_allocator_core: reload_site function=%s block=%d temp=r%d start=%d end=%d status=not_found reason=no_next_read\n",
        function_name, block_index, temp_index, start_instruction, end_instruction);
    return SUCCEEDED;
  }

  if (is_reload_eligible(context, instruction, temp_index) != YES) {
    fprintf(stderr, "register_allocator_core: reload_site function=%s block=%d temp=r%d start=%d end=%d instruction=%d status=not_found reason=first_read_ineligible\n",
        function_name, block_index, temp_index, start_instruction, end_instruction, instruction);
    return SUCCEEDED;
  }

  reload_site->found = YES;
  reload_site->instruction = instruction;
  fprintf(stderr, "register_allocator_core: reload_site function=%s block=%d temp=r%d start=%d end=%d instruction=%d selection=first_next_read status=found\n",
      function_name, block_index, temp_index, start_instruction, end_instruction, instruction);
  return SUCCEEDED;
}


int register_allocator_plan_reload_action(char *function_name, int block_index, int temp_index, int spill_instruction, int reload_instruction, int no_physical_register, int reload_physical_register) {

  int action;
  char *reason;

  if (function_name == NULL || block_index < 0 || temp_index < 0 || spill_instruction < 0 ||
      reload_instruction <= spill_instruction) {
    fprintf(stderr, "register_allocator_core: reload_action function=%s block=%d temp=r%d spill=%d reload=%d reload_phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, spill_instruction,
        reload_instruction, reload_physical_register);
    return FAILED;
  }

  if (reload_physical_register == no_physical_register) {
    action = RA_RELOAD_ACTION_NONE;
    reason = "target_unsupported";
  }
  else {
    action = RA_RELOAD_ACTION_INSERT;
    reason = "target_register_selected";
  }

  fprintf(stderr, "register_allocator_core: reload_action function=%s block=%d temp=r%d spill=%d reload=%d reload_phy=%d action=%s reason=%s status=complete\n",
      function_name, block_index, temp_index, spill_instruction, reload_instruction, reload_physical_register,
      action == RA_RELOAD_ACTION_INSERT ? "insert" : "none", reason);
  return action;
}


static void _clear_reload_mutation(struct register_allocator_reload_mutation *mutation, int no_physical_register) {

  mutation->apply = NO;
  mutation->instruction = -1;
  mutation->temp_index = -1;
  mutation->operand = -1;
  mutation->physical_register = no_physical_register;
}


int register_allocator_prepare_reload_operand_mutation(char *function_name,
    int block_index, int temp_index, int reload_instruction,
    int reload_operand, int reload_action, int no_physical_register,
    int reload_physical_register,
    struct register_allocator_reload_mutation *mutation) {

  if (mutation != NULL)
    _clear_reload_mutation(mutation, no_physical_register);

  if (function_name == NULL || block_index < 0 || temp_index < 0 || reload_instruction < 0 ||
      (reload_operand != TAC_USE_ARG1 && reload_operand != TAC_USE_ARG2 &&
      reload_operand != TAC_USE_RESULT) ||
      (reload_action != RA_RELOAD_ACTION_NONE && reload_action != RA_RELOAD_ACTION_INSERT) ||
      (reload_action == RA_RELOAD_ACTION_NONE && reload_physical_register != no_physical_register) ||
      (reload_action == RA_RELOAD_ACTION_INSERT && reload_physical_register == no_physical_register) || mutation == NULL) {
    fprintf(stderr, "register_allocator_core: reload_mutation function=%s block=%d temp=r%d reload=%d operand=%d action=%s reload_phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, reload_instruction,
      reload_operand, reload_action == RA_RELOAD_ACTION_NONE ? "none" :
        (reload_action == RA_RELOAD_ACTION_INSERT ? "insert" : "invalid"), reload_physical_register);
    return FAILED;
  }

  if (reload_action == RA_RELOAD_ACTION_NONE) {
    fprintf(stderr, "register_allocator_core: reload_mutation function=%s block=%d temp=r%d reload=%d operand=%d action=none reload_phy=%d apply=no status=complete\n",
      function_name, block_index, temp_index, reload_instruction,
      reload_operand, reload_physical_register);
    return SUCCEEDED;
  }

  mutation->apply = YES;
  mutation->instruction = reload_instruction;
  mutation->temp_index = temp_index;
  mutation->operand = reload_operand;
  mutation->physical_register = reload_physical_register;
  fprintf(stderr, "register_allocator_core: reload_mutation function=%s block=%d temp=r%d reload=%d operand=%d action=insert reload_phy=%d apply=yes status=complete\n",
      function_name, block_index, temp_index, reload_instruction,
      reload_operand, reload_physical_register);
  return SUCCEEDED;
}


int register_allocator_prepare_reload_mutation(char *function_name,
    int block_index, int temp_index, int reload_instruction,
    int reload_action, int no_physical_register,
    int reload_physical_register,
    struct register_allocator_reload_mutation *mutation) {

  return register_allocator_prepare_reload_operand_mutation(function_name,
      block_index, temp_index, reload_instruction, TAC_USE_RESULT,
      reload_action, no_physical_register, reload_physical_register,
      mutation);
}


static void _clear_reload_emission(struct register_allocator_reload_emission *emission, int no_physical_register) {

  emission->emit = NO;
  emission->temp_index = -1;
  emission->operand = -1;
  emission->physical_register = no_physical_register;
  emission->source_offset = 0;
  emission->byte_count = 0;
}


int register_allocator_prepare_reload_operand_emission(char *function_name,
    int reload_requested, int temp_index, int operand,
    int no_physical_register, int physical_register, int spill_available,
    int source_offset, int byte_count,
    struct register_allocator_reload_emission *emission) {

  if (emission != NULL)
    _clear_reload_emission(emission, no_physical_register);

  if (function_name == NULL || (reload_requested != NO && reload_requested != YES) ||
      (spill_available != NO && spill_available != YES) || emission == NULL ||
    (reload_requested == NO &&
     (temp_index != -1 || operand != -1 || physical_register != no_physical_register || spill_available != NO ||
      source_offset != 0 || byte_count != 0)) ||
      (reload_requested == YES &&
       (temp_index < 0 || (operand != TAC_USE_ARG1 && operand != TAC_USE_ARG2 &&
        operand != TAC_USE_RESULT) || physical_register == no_physical_register ||
        spill_available != YES || byte_count <= 0))) {
    fprintf(stderr, "register_allocator_core: reload_emission function=%s requested=%s temp=r%d operand=%d phy=%d spill=%s source_offset=%d bytes=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        reload_requested == YES ? "yes" : (reload_requested == NO ? "no" : "invalid"), temp_index,
        operand, physical_register, spill_available == YES ? "yes" : (spill_available == NO ? "no" : "invalid"),
        source_offset, byte_count);
    return FAILED;
  }

  if (reload_requested == NO) {
    fprintf(stderr, "register_allocator_core: reload_emission function=%s requested=no emit=no status=complete\n",
        function_name);
    return SUCCEEDED;
  }

  emission->emit = YES;
  emission->temp_index = temp_index;
  emission->operand = operand;
  emission->physical_register = physical_register;
  emission->source_offset = source_offset;
  emission->byte_count = byte_count;
  fprintf(stderr, "register_allocator_core: reload_emission function=%s requested=yes temp=r%d operand=%d phy=%d spill=yes source_offset=%d bytes=%d emit=yes status=complete\n",
      function_name, temp_index, operand, physical_register, source_offset, byte_count);
  return SUCCEEDED;
}


int register_allocator_prepare_reload_emission(char *function_name,
    int reload_requested, int temp_index, int no_physical_register,
    int physical_register, int spill_available, int source_offset,
    int byte_count, struct register_allocator_reload_emission *emission) {

  return register_allocator_prepare_reload_operand_emission(function_name,
      reload_requested, temp_index,
      reload_requested == YES ? TAC_USE_RESULT : -1,
      no_physical_register, physical_register, spill_available,
      source_offset, byte_count, emission);
}


static void _clear_spill_emission(struct register_allocator_spill_emission *emission, int no_physical_register) {

  emission->emit = NO;
  emission->temp_index = -1;
  emission->physical_register = no_physical_register;
  emission->destination_offset = 0;
  emission->byte_count = 0;
}


int register_allocator_prepare_spill_emission(char *function_name, int spill_requested, int temp_index, int no_physical_register, int physical_register, int spill_available, int destination_offset, int byte_count, struct register_allocator_spill_emission *emission) {

  if (emission != NULL)
    _clear_spill_emission(emission, no_physical_register);

  if (function_name == NULL || (spill_requested != NO && spill_requested != YES) ||
      (spill_available != NO && spill_available != YES) || emission == NULL ||
      (spill_requested == NO &&
       (temp_index != -1 || physical_register != no_physical_register || spill_available != NO ||
        destination_offset != 0 || byte_count != 0)) ||
      (spill_requested == YES &&
       (temp_index < 0 || physical_register == no_physical_register || spill_available != YES || byte_count <= 0))) {
    fprintf(stderr, "register_allocator_core: spill_emission function=%s requested=%s temp=r%d phy=%d spill=%s destination_offset=%d bytes=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        spill_requested == YES ? "yes" : (spill_requested == NO ? "no" : "invalid"), temp_index,
        physical_register, spill_available == YES ? "yes" : (spill_available == NO ? "no" : "invalid"),
        destination_offset, byte_count);
    return FAILED;
  }

  if (spill_requested == NO) {
    fprintf(stderr, "register_allocator_core: spill_emission function=%s requested=no emit=no status=complete\n",
        function_name);
    return SUCCEEDED;
  }

  emission->emit = YES;
  emission->temp_index = temp_index;
  emission->physical_register = physical_register;
  emission->destination_offset = destination_offset;
  emission->byte_count = byte_count;
  fprintf(stderr, "register_allocator_core: spill_emission function=%s requested=yes temp=r%d phy=%d spill=yes destination_offset=%d bytes=%d emit=yes status=complete\n",
      function_name, temp_index, physical_register, destination_offset, byte_count);
  return SUCCEEDED;
}


static void _clear_spill_mutation(struct register_allocator_spill_mutation *mutation, int no_physical_register) {

  mutation->apply = NO;
  mutation->instruction = -1;
  mutation->temp_index = -1;
  mutation->operand = -1;
  mutation->physical_register = no_physical_register;
}


int register_allocator_prepare_spill_mutation(char *function_name, int block_index, int temp_index, int consumer_instruction, int consumer_operand, int split_action, int retained_interval, int no_physical_register, int physical_register, struct register_allocator_spill_mutation *mutation) {

  char *reason;

  if (mutation != NULL)
    _clear_spill_mutation(mutation, no_physical_register);

  if (function_name == NULL || block_index < 0 || temp_index < 0 || consumer_instruction < 0 ||
      consumer_operand < 0 ||
      (split_action != RA_SPLIT_ACTION_NONE && split_action != RA_SPLIT_ACTION_PRESERVE) ||
      (retained_interval != NO && retained_interval != YES) || physical_register == no_physical_register ||
      mutation == NULL) {
    fprintf(stderr, "register_allocator_core: spill_mutation function=%s block=%d temp=r%d consumer=%d operand=%d split_action=%s retained=%s phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index, consumer_instruction,
        consumer_operand, split_action == RA_SPLIT_ACTION_NONE ? "none" :
        (split_action == RA_SPLIT_ACTION_PRESERVE ? "preserve" : "invalid"),
        retained_interval == YES ? "yes" : (retained_interval == NO ? "no" : "invalid"), physical_register);
    return FAILED;
  }

  if (split_action == RA_SPLIT_ACTION_NONE)
    reason = "split_not_required";
  else if (retained_interval == NO)
    reason = "candidate_not_retained";
  else {
    mutation->apply = YES;
    mutation->instruction = consumer_instruction;
    mutation->temp_index = temp_index;
    mutation->operand = consumer_operand;
    mutation->physical_register = physical_register;
    fprintf(stderr, "register_allocator_core: spill_mutation function=%s block=%d temp=r%d consumer=%d operand=%d split_action=preserve retained=yes phy=%d apply=yes reason=preserved_interval status=complete\n",
        function_name, block_index, temp_index, consumer_instruction, consumer_operand, physical_register);
    return SUCCEEDED;
  }

  fprintf(stderr, "register_allocator_core: spill_mutation function=%s block=%d temp=r%d consumer=%d operand=%d split_action=%s retained=%s phy=%d apply=no reason=%s status=complete\n",
      function_name, block_index, temp_index, consumer_instruction, consumer_operand,
      split_action == RA_SPLIT_ACTION_PRESERVE ? "preserve" : "none", retained_interval == YES ? "yes" : "no",
      physical_register, reason);
  return SUCCEEDED;
}


int register_allocator_apply_spill_mutation(char *function_name, int block_index, int no_physical_register, void *context, struct register_allocator_spill_mutation *mutation, register_allocator_spill_mutation_applier apply_mutation) {

  if (function_name == NULL || block_index < 0 || mutation == NULL ||
      (mutation != NULL && mutation->apply != NO && mutation->apply != YES) ||
      (mutation != NULL && mutation->apply == NO &&
       (mutation->instruction != -1 || mutation->temp_index != -1 || mutation->operand != -1 ||
        mutation->physical_register != no_physical_register)) ||
      (mutation != NULL && mutation->apply == YES &&
       (mutation->instruction < 0 || mutation->temp_index < 0 || mutation->operand < 0 ||
        mutation->physical_register == no_physical_register || apply_mutation == NULL))) {
    fprintf(stderr, "register_allocator_core: spill_mutation_apply function=%s block=%d apply=%s instruction=%d temp=r%d operand=%d phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        mutation != NULL && mutation->apply == YES ? "yes" :
        (mutation != NULL && mutation->apply == NO ? "no" : "invalid"),
        mutation != NULL ? mutation->instruction : -1, mutation != NULL ? mutation->temp_index : -1,
        mutation != NULL ? mutation->operand : -1,
        mutation != NULL ? mutation->physical_register : no_physical_register);
    return FAILED;
  }

  if (mutation->apply == NO) {
    fprintf(stderr, "register_allocator_core: spill_mutation_apply function=%s block=%d apply=no status=skipped reason=empty_plan\n",
        function_name, block_index);
    return SUCCEEDED;
  }

  if (apply_mutation(context, mutation->instruction, mutation->temp_index, mutation->operand,
      mutation->physical_register) == FAILED) {
    fprintf(stderr, "register_allocator_core: spill_mutation_apply function=%s block=%d apply=yes instruction=%d temp=r%d operand=%d phy=%d status=callback_failed\n",
        function_name, block_index, mutation->instruction, mutation->temp_index, mutation->operand,
        mutation->physical_register);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_core: spill_mutation_apply function=%s block=%d apply=yes instruction=%d temp=r%d operand=%d phy=%d status=complete\n",
      function_name, block_index, mutation->instruction, mutation->temp_index, mutation->operand,
      mutation->physical_register);
  return SUCCEEDED;
}


static void _clear_candidate_transition_application(
    struct register_allocator_candidate_transition_application *application,
    int no_physical_register) {

  application->retained_interval = NO;
  _clear_spill_mutation(&application->spill_mutation, no_physical_register);
}


int register_allocator_apply_candidate_transition(char *function_name, int block_index,
    int slot_index, struct register_allocator_active_slot *slots, int slot_count,
    int physical_register, int no_physical_register, int candidate_temp,
    int candidate_next_use, int producer_instruction, int consumer_operand, int split_action,
    void *transition_context, struct register_allocator_slot_transition *transition,
    register_allocator_clear_interval_callback clear_interval,
    register_allocator_spill_temp_callback spill_temp,
    register_allocator_retain_candidate_callback retain_candidate, void *spill_context,
    register_allocator_spill_mutation_applier apply_spill_mutation,
    struct register_allocator_candidate_transition_application *application) {

  char *failure_stage;

  if (application != NULL)
    _clear_candidate_transition_application(application, no_physical_register);
  if (function_name == NULL || block_index < 0 ||
      (split_action != RA_SPLIT_ACTION_NONE && split_action != RA_SPLIT_ACTION_PRESERVE) ||
      application == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_transition_apply function=%s block=%d slot=%d candidate=r%d split_action=%s retained=no spill=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, slot_index,
        candidate_temp, split_action == RA_SPLIT_ACTION_NONE ? "none" :
        (split_action == RA_SPLIT_ACTION_PRESERVE ? "preserve" : "invalid"));
    return FAILED;
  }

  failure_stage = "slot_transition";
  if (register_allocator_apply_slot_transition(function_name, block_index, slot_index,
      slots, slot_count, physical_register, no_physical_register, candidate_temp,
      candidate_next_use, producer_instruction, consumer_operand, transition_context,
      transition, clear_interval, spill_temp, retain_candidate,
      &application->retained_interval) == FAILED)
    goto dependency_failed;

  failure_stage = "spill_prepare";
  if (register_allocator_prepare_spill_mutation(function_name, block_index, candidate_temp,
      candidate_next_use, consumer_operand, split_action, application->retained_interval,
      no_physical_register, physical_register, &application->spill_mutation) == FAILED)
    goto dependency_failed;

  failure_stage = "spill_apply";
  if (register_allocator_apply_spill_mutation(function_name, block_index,
      no_physical_register, spill_context, &application->spill_mutation,
      apply_spill_mutation) == FAILED)
    goto dependency_failed;

  fprintf(stderr, "register_allocator_core: candidate_transition_apply function=%s block=%d slot=%d candidate=r%d split_action=%s retained=%s spill=%s order=slot_prepare_spill status=complete\n",
      function_name, block_index, slot_index, candidate_temp,
      split_action == RA_SPLIT_ACTION_PRESERVE ? "preserve" : "none",
      application->retained_interval == YES ? "yes" : "no",
      application->spill_mutation.apply == YES ? "yes" : "no");
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_transition_application(application, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_transition_apply function=%s block=%d slot=%d candidate=r%d split_action=%s retained=no spill=no stage=%s status=dependency_failed\n",
      function_name, block_index, slot_index, candidate_temp,
      split_action == RA_SPLIT_ACTION_PRESERVE ? "preserve" : "none", failure_stage);
  return FAILED;
}


static void _clear_candidate_transition_execution(
    struct register_allocator_candidate_transition_execution *execution,
    int no_physical_register) {

  _clear_candidate_transition_preparation(&execution->preparation);
  _clear_candidate_transition_application(&execution->application,
      no_physical_register);
}


int register_allocator_execute_candidate_transition(char *function_name, int block_index,
    int instruction_index, int candidate_temp, int candidate_next_use, int consumer_op,
    int candidate_operand, int physical_register, int no_physical_register,
    int split_action, struct register_allocator_active_slot *active_slots,
    int active_slot_count, struct register_allocator_target_policy *policy,
    void *planning_context, register_allocator_temp_predicate has_temp_metadata,
    void *observer_context,
    register_allocator_candidate_transition_observer observe_preparation,
    void *transition_context, register_allocator_clear_interval_callback clear_interval,
    register_allocator_spill_temp_callback spill_temp,
    register_allocator_retain_candidate_callback retain_candidate, void *spill_context,
    register_allocator_spill_mutation_applier apply_spill_mutation,
    struct register_allocator_candidate_transition_execution *execution) {

  char *failure_stage;

  if (execution != NULL)
    _clear_candidate_transition_execution(execution, no_physical_register);
  if (function_name == NULL || block_index < 0 || instruction_index < 0 ||
      candidate_temp < 0 || candidate_next_use <= instruction_index || consumer_op < 0 ||
      candidate_operand < 0 || physical_register == no_physical_register ||
      (split_action != RA_SPLIT_ACTION_NONE && split_action != RA_SPLIT_ACTION_PRESERVE) ||
      active_slots == NULL || active_slot_count <= 0 || policy == NULL ||
      planning_context == NULL || has_temp_metadata == NULL ||
      transition_context == NULL || clear_interval == NULL || spill_temp == NULL ||
      retain_candidate == NULL || execution == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_transition_execute function=%s block=%d instruction=%d candidate=r%d next=%d phy=%d slot=-1 decision=failed retained=no spill=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        instruction_index, candidate_temp, candidate_next_use, physical_register);
    return FAILED;
  }

  failure_stage = "preparation";
  if (register_allocator_prepare_candidate_transition(function_name, block_index,
      instruction_index, candidate_temp, candidate_next_use, consumer_op,
      candidate_operand, physical_register, no_physical_register, active_slots,
      active_slot_count, policy, planning_context, has_temp_metadata,
      &execution->preparation) == FAILED)
    goto dependency_failed;

  if (observe_preparation != NULL)
    observe_preparation(observer_context, &execution->preparation);

  failure_stage = "application";
  if (register_allocator_apply_candidate_transition(function_name, block_index,
      execution->preparation.slot_index, active_slots, active_slot_count,
      physical_register, no_physical_register, candidate_temp, candidate_next_use,
      instruction_index, candidate_operand, split_action, transition_context,
      &execution->preparation.plan.transition, clear_interval, spill_temp,
      retain_candidate, spill_context, apply_spill_mutation,
      &execution->application) == FAILED)
    goto dependency_failed;

  fprintf(stderr, "register_allocator_core: candidate_transition_execute function=%s block=%d instruction=%d candidate=r%d next=%d active=r%d active_next=%d phy=%d slot=%d name=%s decision=%d retained=%s spill=%s order=prepare_apply status=complete\n",
      function_name, block_index, instruction_index, candidate_temp,
      candidate_next_use, execution->preparation.plan.active_temp,
      execution->preparation.plan.active_next_use, physical_register,
      execution->preparation.slot_index,
      execution->preparation.slot_name,
      execution->preparation.plan.linear_scan_decision,
      execution->application.retained_interval == YES ? "yes" : "no",
      execution->application.spill_mutation.apply == YES ? "yes" : "no");
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_transition_execution(execution, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_transition_execute function=%s block=%d instruction=%d candidate=r%d next=%d phy=%d slot=-1 decision=failed retained=no spill=no stage=%s status=dependency_failed\n",
      function_name, block_index, instruction_index, candidate_temp,
      candidate_next_use, physical_register, failure_stage);
  return FAILED;
}


int register_allocator_apply_reload_mutation(char *function_name, int block_index, int no_physical_register, void *context, struct register_allocator_reload_mutation *mutation, register_allocator_reload_mutation_applier apply_mutation) {

  if (function_name == NULL || block_index < 0 || mutation == NULL ||
      (mutation != NULL && mutation->apply != NO && mutation->apply != YES) ||
      (mutation != NULL && mutation->apply == NO &&
       (mutation->instruction != -1 || mutation->temp_index != -1 ||
        mutation->operand != -1 || mutation->physical_register != no_physical_register)) ||
      (mutation != NULL && mutation->apply == YES &&
       (mutation->instruction < 0 || mutation->temp_index < 0 ||
        (mutation->operand != TAC_USE_ARG1 && mutation->operand != TAC_USE_ARG2 &&
         mutation->operand != TAC_USE_RESULT) ||
        mutation->physical_register == no_physical_register || apply_mutation == NULL))) {
    fprintf(stderr, "register_allocator_core: reload_mutation_apply function=%s block=%d apply=%s instruction=%d temp=r%d operand=%d phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        mutation != NULL && mutation->apply == YES ? "yes" :
        (mutation != NULL && mutation->apply == NO ? "no" : "invalid"),
        mutation != NULL ? mutation->instruction : -1, mutation != NULL ? mutation->temp_index : -1,
        mutation != NULL ? mutation->operand : -1,
        mutation != NULL ? mutation->physical_register : no_physical_register);
    return FAILED;
  }

  if (mutation->apply == NO) {
    fprintf(stderr, "register_allocator_core: reload_mutation_apply function=%s block=%d apply=no status=skipped reason=empty_plan\n",
        function_name, block_index);
    return SUCCEEDED;
  }

  if (apply_mutation(context, mutation->instruction, mutation->temp_index, mutation->operand,
      mutation->physical_register) == FAILED) {
    fprintf(stderr, "register_allocator_core: reload_mutation_apply function=%s block=%d apply=yes instruction=%d temp=r%d operand=%d phy=%d status=callback_failed\n",
        function_name, block_index, mutation->instruction, mutation->temp_index,
        mutation->operand, mutation->physical_register);
    return FAILED;
  }

    fprintf(stderr, "register_allocator_core: reload_mutation_apply function=%s block=%d apply=yes instruction=%d temp=r%d operand=%d phy=%d status=complete\n",
      function_name, block_index, mutation->instruction, mutation->temp_index,
      mutation->operand, mutation->physical_register);
  return SUCCEEDED;
}


static void _clear_reload_execution(struct register_allocator_reload_execution *execution,
    int no_physical_register) {

  execution->action = FAILED;
  _clear_reload_mutation(&execution->mutation, no_physical_register);
}


int register_allocator_execute_reload_operand(char *function_name,
    int block_index, int temp_index, int spill_instruction,
    int reload_instruction, int reload_operand, int no_physical_register,
    int reload_physical_register, void *context,
    register_allocator_reload_mutation_applier apply_mutation,
    struct register_allocator_reload_execution *execution) {

  char *failure_stage;

  if (execution != NULL)
    _clear_reload_execution(execution, no_physical_register);
  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      spill_instruction < 0 || reload_instruction <= spill_instruction ||
      (reload_operand != TAC_USE_ARG1 && reload_operand != TAC_USE_ARG2 &&
       reload_operand != TAC_USE_RESULT) ||
      execution == NULL) {
    fprintf(stderr, "register_allocator_core: reload_execute function=%s block=%d temp=r%d spill=%d reload=%d operand=%d reload_phy=%d action=failed apply=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index,
      spill_instruction, reload_instruction, reload_operand,
      reload_physical_register);
    return FAILED;
  }

  failure_stage = "action";
  execution->action = register_allocator_plan_reload_action(function_name, block_index,
      temp_index, spill_instruction, reload_instruction, no_physical_register,
      reload_physical_register);
  if (execution->action == FAILED)
    goto dependency_failed;

  failure_stage = "preparation";
  if (register_allocator_prepare_reload_operand_mutation(function_name,
      block_index, temp_index, reload_instruction, reload_operand,
      execution->action, no_physical_register,
      reload_physical_register, &execution->mutation) == FAILED)
    goto dependency_failed;

  failure_stage = "application";
  if (register_allocator_apply_reload_mutation(function_name, block_index,
      no_physical_register, context, &execution->mutation, apply_mutation) == FAILED)
    goto dependency_failed;

  fprintf(stderr, "register_allocator_core: reload_execute function=%s block=%d temp=r%d spill=%d reload=%d operand=%d reload_phy=%d action=%s apply=%s order=action_prepare_apply status=complete\n",
      function_name, block_index, temp_index, spill_instruction, reload_instruction,
      reload_operand, reload_physical_register,
      execution->action == RA_RELOAD_ACTION_INSERT ? "insert" : "none",
      execution->mutation.apply == YES ? "yes" : "no");
  return SUCCEEDED;

dependency_failed:
  _clear_reload_execution(execution, no_physical_register);
  fprintf(stderr, "register_allocator_core: reload_execute function=%s block=%d temp=r%d spill=%d reload=%d operand=%d reload_phy=%d action=failed apply=no stage=%s status=dependency_failed\n",
      function_name, block_index, temp_index, spill_instruction, reload_instruction,
      reload_operand, reload_physical_register, failure_stage);
  return FAILED;
}


int register_allocator_execute_reload(char *function_name, int block_index,
    int temp_index, int spill_instruction, int reload_instruction,
    int no_physical_register, int reload_physical_register, void *context,
    register_allocator_reload_mutation_applier apply_mutation,
    struct register_allocator_reload_execution *execution) {

  return register_allocator_execute_reload_operand(function_name, block_index,
      temp_index, spill_instruction, reload_instruction, TAC_USE_RESULT,
      no_physical_register, reload_physical_register, context, apply_mutation,
      execution);
}


static void _clear_reload_chain_execution(struct register_allocator_reload_chain_execution *chain_execution) {

  _clear_reload_chain(&chain_execution->chain);
  chain_execution->processed_count = 0;
  chain_execution->applied_count = 0;
}


static void _clear_reload_executions(struct register_allocator_reload_execution *reload_executions,
    int reload_capacity, int no_physical_register) {

  int reload_index;

  if (reload_executions == NULL || reload_capacity <= 0)
    return;
  for (reload_index = 0; reload_index < reload_capacity; reload_index++)
    _clear_reload_execution(&reload_executions[reload_index], no_physical_register);
}


static void _clear_reload_chain_storage(struct register_allocator_reload_chain_storage *storage) {

  storage->instruction_bytes = 0;
  storage->execution_bytes = 0;
}


int register_allocator_plan_reload_chain_storage(char *function_name, int reload_capacity,
    struct register_allocator_reload_chain_storage *storage) {

  if (storage != NULL)
    _clear_reload_chain_storage(storage);
  if (function_name == NULL || reload_capacity <= 0 || storage == NULL ||
      (size_t)reload_capacity > ((size_t)-1) / sizeof(int) ||
      (size_t)reload_capacity > ((size_t)-1) / sizeof(struct register_allocator_reload_execution) ||
      (size_t)reload_capacity > (size_t)ULONG_MAX / sizeof(int) ||
      (size_t)reload_capacity > (size_t)ULONG_MAX / sizeof(struct register_allocator_reload_execution)) {
    fprintf(stderr, "register_allocator_core: reload_chain_storage function=%s capacity=%d instruction_bytes=0 execution_bytes=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", reload_capacity);
    return FAILED;
  }

  storage->instruction_bytes = (size_t)reload_capacity * sizeof(int);
  storage->execution_bytes = (size_t)reload_capacity * sizeof(struct register_allocator_reload_execution);
  fprintf(stderr, "register_allocator_core: reload_chain_storage function=%s capacity=%d instruction_bytes=%lu execution_bytes=%lu status=complete\n",
      function_name, reload_capacity, (unsigned long)storage->instruction_bytes,
      (unsigned long)storage->execution_bytes);
  return SUCCEEDED;
}


int register_allocator_execute_reload_operand_chain(char *function_name,
    int block_index, int temp_index,
    int spill_instruction, int start_instruction, int end_instruction, int instruction_count,
    int no_physical_register, void *discovery_context,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp,
    register_allocator_instruction_predicate is_reload_eligible,
    void *selection_context, register_allocator_instruction_query get_reload_operand,
    register_allocator_reload_operand_register_selector select_register,
    void *mutation_context, register_allocator_reload_mutation_applier apply_mutation,
    int *reload_instructions, struct register_allocator_reload_execution *reload_executions,
    int reload_capacity, struct register_allocator_reload_chain_execution *chain_execution) {

  char *failure_stage;
  int reload_index;
  int reload_instruction;
  int reload_operand;
  int reload_physical_register;

  if (chain_execution != NULL)
    _clear_reload_chain_execution(chain_execution);
  _clear_reload_executions(reload_executions, reload_capacity, no_physical_register);
  if (reload_instructions != NULL && reload_capacity > 0) {
    for (reload_index = 0; reload_index < reload_capacity; reload_index++)
      reload_instructions[reload_index] = -1;
  }
  if (function_name == NULL || block_index < 0 || temp_index < 0 || spill_instruction < 0 ||
      start_instruction <= spill_instruction || start_instruction < 0 ||
      start_instruction > instruction_count || end_instruction >= instruction_count ||
      instruction_count <= 0 || is_active == NULL || reads_temp == NULL ||
      writes_temp == NULL || is_reload_eligible == NULL ||
      get_reload_operand == NULL || select_register == NULL ||
      reload_instructions == NULL || reload_executions == NULL ||
      reload_capacity <= 0 || chain_execution == NULL) {
    fprintf(stderr, "register_allocator_core: reload_chain_execute function=%s block=%d temp=r%d spill=%d start=%d end=%d instructions=%d capacity=%d processed=0 applied=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index,
        spill_instruction, start_instruction, end_instruction, instruction_count, reload_capacity);
    return FAILED;
  }

  failure_stage = "discovery";
  if (register_allocator_discover_reload_chain(function_name, block_index, temp_index,
      start_instruction, end_instruction, instruction_count, discovery_context, is_active,
      reads_temp, writes_temp, is_reload_eligible, reload_instructions, reload_capacity,
      &chain_execution->chain) == FAILED)
    goto dependency_failed;

  failure_stage = "execution";
  for (reload_index = 0; reload_index < chain_execution->chain.count; reload_index++) {
    reload_instruction = reload_instructions[reload_index];
    reload_operand = get_reload_operand(selection_context, reload_instruction, temp_index);
    if (reload_operand != TAC_USE_ARG1 && reload_operand != TAC_USE_ARG2 &&
      reload_operand != TAC_USE_RESULT) {
      failure_stage = "operand";
      goto dependency_failed;
    }
    reload_physical_register = select_register(selection_context,
      reload_instruction, temp_index, reload_operand);
    fprintf(stderr, "register_allocator_core: reload_chain_select function=%s block=%d temp=r%d ordinal=%d instruction=%d operand=%d reload_phy=%d status=complete\n",
        function_name, block_index, temp_index, reload_index + 1, reload_instruction,
      reload_operand, reload_physical_register);
    if (register_allocator_execute_reload_operand(function_name, block_index,
      temp_index, spill_instruction, reload_instruction, reload_operand,
      no_physical_register,
        reload_physical_register, mutation_context, apply_mutation,
        &reload_executions[reload_index]) == FAILED)
      goto dependency_failed;
    chain_execution->processed_count++;
    if (reload_executions[reload_index].mutation.apply == YES)
      chain_execution->applied_count++;
  }

  fprintf(stderr, "register_allocator_core: reload_chain_execute function=%s block=%d temp=r%d spill=%d start=%d end=%d discovered=%d processed=%d applied=%d capacity=%d truncated=%s order=discover_select_execute status=complete\n",
      function_name, block_index, temp_index, spill_instruction, start_instruction,
      end_instruction, chain_execution->chain.count, chain_execution->processed_count,
      chain_execution->applied_count, reload_capacity,
      chain_execution->chain.truncated == YES ? "yes" : "no");
  return SUCCEEDED;

dependency_failed:
  _clear_reload_chain_execution(chain_execution);
  _clear_reload_executions(reload_executions, reload_capacity, no_physical_register);
  for (reload_index = 0; reload_index < reload_capacity; reload_index++)
    reload_instructions[reload_index] = -1;
  fprintf(stderr, "register_allocator_core: reload_chain_execute function=%s block=%d temp=r%d spill=%d start=%d end=%d discovered=0 processed=0 applied=0 capacity=%d stage=%s status=dependency_failed\n",
      function_name, block_index, temp_index, spill_instruction, start_instruction,
      end_instruction, reload_capacity, failure_stage);
  return FAILED;
}


int register_allocator_execute_reload_operand_chain_atomic(char *function_name,
    int block_index, int temp_index,
    int spill_instruction, int start_instruction, int end_instruction,
    int instruction_count, int no_physical_register, void *discovery_context,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp,
    register_allocator_instruction_predicate is_reload_eligible,
    void *selection_context,
    register_allocator_instruction_query get_reload_operand,
    register_allocator_reload_operand_register_selector select_register,
    void *transaction_context,
    register_allocator_reload_transaction_applier apply_transaction,
    int *reload_instructions,
    struct register_allocator_reload_execution *reload_executions,
    int reload_capacity,
    struct register_allocator_reload_chain_execution *chain_execution) {

  char *failure_stage;
  int mutation_count;
  int reload_index;
  int reload_instruction;
  int reload_operand;
  int reload_physical_register;
  struct register_allocator_reload_mutation *mutations;

  if (chain_execution != NULL)
    _clear_reload_chain_execution(chain_execution);
  _clear_reload_executions(reload_executions, reload_capacity,
      no_physical_register);
  if (reload_instructions != NULL && reload_capacity > 0) {
    for (reload_index = 0; reload_index < reload_capacity; reload_index++)
      reload_instructions[reload_index] = -1;
  }
  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      spill_instruction < 0 || start_instruction <= spill_instruction ||
      start_instruction < 0 || start_instruction > instruction_count ||
      end_instruction >= instruction_count || instruction_count <= 0 ||
      is_active == NULL || reads_temp == NULL || writes_temp == NULL ||
      is_reload_eligible == NULL || get_reload_operand == NULL ||
      select_register == NULL || apply_transaction == NULL ||
      reload_instructions == NULL || reload_executions == NULL ||
      reload_capacity <= 0 || chain_execution == NULL) {
    fprintf(stderr, "register_allocator_core: reload_chain_transaction function=%s block=%d temp=r%d spill=%d start=%d end=%d capacity=%d mutations=0 transaction=rejected atomic=yes status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, spill_instruction, start_instruction, end_instruction,
        reload_capacity);
    return FAILED;
  }

  mutations = (struct register_allocator_reload_mutation *)calloc(
      (size_t)reload_capacity, sizeof(struct register_allocator_reload_mutation));
  if (mutations == NULL) {
    fprintf(stderr, "register_allocator_core: reload_chain_transaction function=%s block=%d temp=r%d spill=%d start=%d end=%d capacity=%d mutations=0 transaction=rejected atomic=yes status=out_of_memory\n",
        function_name, block_index, temp_index, spill_instruction,
        start_instruction, end_instruction, reload_capacity);
    return FAILED;
  }

  failure_stage = "discovery";
  if (register_allocator_discover_reload_chain(function_name, block_index,
      temp_index, start_instruction, end_instruction, instruction_count,
      discovery_context, is_active, reads_temp, writes_temp,
      is_reload_eligible, reload_instructions, reload_capacity,
      &chain_execution->chain) == FAILED)
    goto dependency_failed;

  mutation_count = 0;
  failure_stage = "preparation";
  for (reload_index = 0; reload_index < chain_execution->chain.count;
      reload_index++) {
    reload_instruction = reload_instructions[reload_index];
    reload_operand = get_reload_operand(selection_context, reload_instruction,
        temp_index);
    if (reload_operand != TAC_USE_ARG1 && reload_operand != TAC_USE_ARG2 &&
        reload_operand != TAC_USE_RESULT) {
      failure_stage = "operand";
      goto dependency_failed;
    }
    reload_physical_register = select_register(selection_context,
        reload_instruction, temp_index, reload_operand);
    reload_executions[reload_index].action = register_allocator_plan_reload_action(
        function_name, block_index, temp_index, spill_instruction,
        reload_instruction, no_physical_register, reload_physical_register);
    if (reload_executions[reload_index].action == FAILED ||
        register_allocator_prepare_reload_operand_mutation(function_name,
        block_index, temp_index, reload_instruction, reload_operand,
        reload_executions[reload_index].action, no_physical_register,
        reload_physical_register,
        &reload_executions[reload_index].mutation) == FAILED)
      goto dependency_failed;
    chain_execution->processed_count++;
    if (reload_executions[reload_index].mutation.apply == YES) {
      mutations[mutation_count] = reload_executions[reload_index].mutation;
      mutation_count++;
    }
    fprintf(stderr, "register_allocator_core: reload_chain_transaction_stage function=%s block=%d temp=r%d ordinal=%d instruction=%d operand=%d phy=%d apply=%s transaction=staged status=complete\n",
        function_name, block_index, temp_index, reload_index + 1,
        reload_instruction, reload_operand, reload_physical_register,
        reload_executions[reload_index].mutation.apply == YES ? "yes" : "no");
  }

  failure_stage = "transaction";
  if (mutation_count > 0 && apply_transaction(transaction_context, mutations,
      mutation_count) == FAILED)
    goto dependency_failed;
  chain_execution->applied_count = mutation_count;
  fprintf(stderr, "register_allocator_core: reload_chain_transaction function=%s block=%d temp=r%d spill=%d start=%d end=%d discovered=%d processed=%d mutations=%d transaction=committed atomic=yes status=complete\n",
      function_name, block_index, temp_index, spill_instruction,
      start_instruction, end_instruction, chain_execution->chain.count,
      chain_execution->processed_count, mutation_count);
  free(mutations);
  return SUCCEEDED;

dependency_failed:
  free(mutations);
  _clear_reload_chain_execution(chain_execution);
  _clear_reload_executions(reload_executions, reload_capacity,
      no_physical_register);
  for (reload_index = 0; reload_index < reload_capacity; reload_index++)
    reload_instructions[reload_index] = -1;
  fprintf(stderr, "register_allocator_core: reload_chain_transaction function=%s block=%d temp=r%d spill=%d start=%d end=%d capacity=%d mutations=0 transaction=rejected atomic=yes stage=%s status=dependency_failed\n",
      function_name, block_index, temp_index, spill_instruction,
      start_instruction, end_instruction, reload_capacity, failure_stage);
  return FAILED;
}


int register_allocator_execute_reload_graph_transaction(char *function_name,
    int block_index, int temp_index, int spill_instruction,
    int no_physical_register, void *selection_context,
    register_allocator_instruction_query get_reload_operand,
    register_allocator_reload_operand_register_selector select_register,
    void *transaction_context,
    register_allocator_reload_transaction_applier apply_transaction,
    int *reload_instructions, int reload_count,
    struct register_allocator_reload_execution *reload_executions,
    int reload_capacity,
    struct register_allocator_reload_chain_execution *chain_execution) {

  int mutation_count;
  int reload_index;
  struct register_allocator_reload_mutation *mutations;

  if (chain_execution != NULL)
    _clear_reload_chain_execution(chain_execution);
  _clear_reload_executions(reload_executions, reload_capacity,
      no_physical_register);
  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      spill_instruction < 0 || get_reload_operand == NULL ||
      select_register == NULL || apply_transaction == NULL ||
      reload_instructions == NULL || reload_count <= 0 ||
      reload_count > reload_capacity || reload_executions == NULL ||
      reload_capacity <= 0 || chain_execution == NULL) {
    fprintf(stderr, "register_allocator_core: reload_graph_transaction function=%s block=%d temp=r%d spill=%d reloads=%d capacity=%d mutations=0 transaction=rejected atomic=yes status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, spill_instruction, reload_count, reload_capacity);
    return FAILED;
  }
  mutations = (struct register_allocator_reload_mutation *)calloc(
      (size_t)reload_count, sizeof(struct register_allocator_reload_mutation));
  if (mutations == NULL) {
    fprintf(stderr, "register_allocator_core: reload_graph_transaction function=%s block=%d temp=r%d spill=%d reloads=%d capacity=%d mutations=0 transaction=rejected atomic=yes status=out_of_memory\n",
        function_name, block_index, temp_index, spill_instruction,
        reload_count, reload_capacity);
    return FAILED;
  }
  mutation_count = 0;
  for (reload_index = 0; reload_index < reload_count; reload_index++) {
    int action;
    int operand;
    int physical_register;
    int reload_instruction;

    reload_instruction = reload_instructions[reload_index];
    if (reload_instruction <= spill_instruction ||
        (reload_index > 0 && reload_instruction <=
         reload_instructions[reload_index - 1]))
      goto rejected;
    operand = get_reload_operand(selection_context, reload_instruction,
        temp_index);
    if (operand != TAC_USE_RESULT && operand != TAC_USE_ARG1 &&
        operand != TAC_USE_ARG2)
      goto rejected;
    physical_register = select_register(selection_context, reload_instruction,
        temp_index, operand);
    action = physical_register == no_physical_register ?
        RA_RELOAD_ACTION_NONE : RA_RELOAD_ACTION_INSERT;
    reload_executions[reload_index].action = action;
    if (register_allocator_prepare_reload_operand_mutation(function_name,
        block_index, temp_index, reload_instruction, operand, action,
        no_physical_register, physical_register,
        &reload_executions[reload_index].mutation) == FAILED)
      goto rejected;
    chain_execution->processed_count++;
    if (reload_executions[reload_index].mutation.apply == YES) {
      mutations[mutation_count] = reload_executions[reload_index].mutation;
      mutation_count++;
    }
    fprintf(stderr, "register_allocator_core: reload_graph_transaction_stage function=%s block=%d temp=r%d ordinal=%d instruction=%d operand=%d phy=%d apply=%s transaction=staged status=complete\n",
        function_name, block_index, temp_index, reload_index + 1,
        reload_instruction, operand, physical_register,
        reload_executions[reload_index].mutation.apply == YES ? "yes" : "no");
  }
  if (mutation_count > 0 && apply_transaction(transaction_context, mutations,
      mutation_count) == FAILED)
    goto rejected;
  chain_execution->chain.count = reload_count;
  chain_execution->applied_count = mutation_count;
  fprintf(stderr, "register_allocator_core: reload_graph_transaction function=%s block=%d temp=r%d spill=%d reloads=%d processed=%d mutations=%d transaction=committed atomic=yes status=complete\n",
      function_name, block_index, temp_index, spill_instruction, reload_count,
      chain_execution->processed_count, mutation_count);
  free(mutations);
  return SUCCEEDED;

rejected:
  free(mutations);
  _clear_reload_chain_execution(chain_execution);
  _clear_reload_executions(reload_executions, reload_capacity,
      no_physical_register);
  fprintf(stderr, "register_allocator_core: reload_graph_transaction function=%s block=%d temp=r%d spill=%d reloads=%d capacity=%d mutations=0 transaction=rejected atomic=yes status=dependency_failed\n",
      function_name, block_index, temp_index, spill_instruction, reload_count,
      reload_capacity);
  return FAILED;
}


static int _register_allocator_liveness_index(int block_index, int temp_index, int temp_count) {

  return block_index * temp_count + temp_index;
}


int register_allocator_resolve_liveness_index(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, int block_index, int temp_index, int *set_index) {

  if (set_index != NULL)
    *set_index = -1;
  if (function_name == NULL || block_count <= 0 || temp_count <= 0 ||
      storage == NULL || block_index < 0 || block_index >= block_count ||
      temp_index < 0 || temp_index >= temp_count || set_index == NULL ||
      storage->set_count <= 0 ||
      storage->set_count / temp_count != block_count ||
      storage->set_count % temp_count != 0 ||
      storage->buffer_bytes != (size_t)storage->set_count * sizeof(char) ||
      storage->buffer_bytes > ((size_t)-1) / 4 ||
      storage->total_bytes != storage->buffer_bytes * 4) {
    fprintf(stderr, "register_allocator_core: liveness_index function=%s blocks=%d temps=%d block=%d temp=%d cells=%d index=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_count,
        temp_count, block_index, temp_index,
        storage != NULL ? storage->set_count : 0);
    return FAILED;
  }

  *set_index = _register_allocator_liveness_index(block_index, temp_index,
      temp_count);
  fprintf(stderr, "register_allocator_core: liveness_index function=%s blocks=%d temps=%d block=%d temp=%d cells=%d index=%d status=complete\n",
      function_name, block_count, temp_count, block_index, temp_index,
      storage->set_count, *set_index);
  return SUCCEEDED;
}


int register_allocator_collect_liveness_use_def(char *function_name, int block_index, int start_instruction, int end_instruction, int instruction_count, int temp_count, void *context, register_allocator_instruction_predicate is_active, register_allocator_instruction_predicate reads_temp, register_allocator_instruction_predicate writes_temp, char *use_set, char *def_set) {

  int definition_count;
  char *invalid_predicate;
  int instruction_index;
  int predicate_result;
  int temp_index;
  int use_count;

  if (function_name == NULL || block_index < 0 || start_instruction < 0 ||
      end_instruction < start_instruction || end_instruction >= instruction_count ||
      instruction_count <= 0 || temp_count <= 0 || context == NULL ||
      is_active == NULL ||
      reads_temp == NULL || writes_temp == NULL || use_set == NULL ||
      def_set == NULL || use_set == def_set) {
    fprintf(stderr, "register_allocator_core: liveness_use_def function=%s block=%d start=%d end=%d instructions=%d temps=%d uses=0 defs=0 order=read_before_write status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        start_instruction, end_instruction, instruction_count, temp_count);
    return FAILED;
  }

  memset(use_set, 0, (size_t)temp_count * sizeof(char));
  memset(def_set, 0, (size_t)temp_count * sizeof(char));
  for (instruction_index = start_instruction; instruction_index <= end_instruction;
      instruction_index++) {
    predicate_result = is_active(context, instruction_index, -1);
    if (predicate_result != NO && predicate_result != YES) {
      invalid_predicate = "is_active";
      temp_index = -1;
      goto invalid_callback;
    }
    if (predicate_result == NO)
      continue;
    for (temp_index = 0; temp_index < temp_count; temp_index++) {
      predicate_result = reads_temp(context, instruction_index, temp_index);
      if (predicate_result != NO && predicate_result != YES) {
        invalid_predicate = "reads_temp";
        goto invalid_callback;
      }
      if (predicate_result == YES &&
          def_set[temp_index] == NO)
        use_set[temp_index] = YES;
    }
    for (temp_index = 0; temp_index < temp_count; temp_index++) {
      predicate_result = writes_temp(context, instruction_index, temp_index);
      if (predicate_result != NO && predicate_result != YES) {
        invalid_predicate = "writes_temp";
        goto invalid_callback;
      }
      if (predicate_result == YES)
        def_set[temp_index] = YES;
    }
  }

  use_count = 0;
  definition_count = 0;
  for (temp_index = 0; temp_index < temp_count; temp_index++) {
    if (use_set[temp_index] == YES)
      use_count++;
    if (def_set[temp_index] == YES)
      definition_count++;
  }
  fprintf(stderr, "register_allocator_core: liveness_use_def function=%s block=%d start=%d end=%d instructions=%d temps=%d uses=%d defs=%d order=read_before_write status=complete\n",
      function_name, block_index, start_instruction, end_instruction,
      instruction_count, temp_count, use_count, definition_count);
  return SUCCEEDED;

invalid_callback:
  memset(use_set, 0, (size_t)temp_count * sizeof(char));
  memset(def_set, 0, (size_t)temp_count * sizeof(char));
  fprintf(stderr, "register_allocator_core: liveness_use_def function=%s block=%d start=%d end=%d instructions=%d temps=%d uses=0 defs=0 order=read_before_write instruction=%d temp=%d invalid_predicate=%s status=invalid_callback\n",
      function_name, block_index, start_instruction, end_instruction,
      instruction_count, temp_count, instruction_index, temp_index,
      invalid_predicate);
  return FAILED;
}


int register_allocator_reconcile_stack_only_joins(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, struct register_allocator_cfg_edge *edges, int edge_count, char *live_in, void *context, register_allocator_join_predicate is_stack_resident) {

  int block_index;
  int edge_index;
  int join_count;
  int live_temp_count;
  int predecessor_count;
  int action;
  int result;
  int temp_index;

  if (block_count <= 0 || temp_count <= 0) {
    fprintf(stderr, "register_allocator_core: join_reconcile function=%s blocks=%d edges=%d temps=%d joins=0 live_temps=0 policy=stack_only status=empty\n",
        function_name != NULL ? function_name : "<null>", block_count,
        edge_count, temp_count);
    return SUCCEEDED;
  }

  if (function_name == NULL || storage == NULL || edge_count < 0 ||
      (edge_count > 0 && edges == NULL) || live_in == NULL || context == NULL ||
      is_stack_resident == NULL || storage->set_count <= 0 ||
      storage->set_count / temp_count != block_count ||
      storage->set_count % temp_count != 0 ||
      storage->buffer_bytes != (size_t)storage->set_count * sizeof(char) ||
      storage->buffer_bytes > ((size_t)-1) / 4 ||
      storage->total_bytes != storage->buffer_bytes * 4) {
    fprintf(stderr, "register_allocator_core: join_reconcile function=%s blocks=%d edges=%d temps=%d joins=0 live_temps=0 policy=stack_only status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_count,
        edge_count, temp_count);
    return FAILED;
  }

  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count) {
      fprintf(stderr, "register_allocator_core: join_reconcile function=%s blocks=%d edges=%d temps=%d joins=0 live_temps=0 policy=stack_only edge=%d from_block=%d to_block=%d status=invalid_edge\n",
          function_name, block_count, edge_count, temp_count, edge_index,
          edges[edge_index].from_block, edges[edge_index].to_block);
      return FAILED;
    }
  }

  join_count = 0;
  live_temp_count = 0;
  for (block_index = 0; block_index < block_count; block_index++) {
    if (register_allocator_collect_join_predecessors(function_name,
        block_count, block_index, edges, edge_count, NULL, 0,
        &predecessor_count) == FAILED)
      return FAILED;
    if (predecessor_count <= 1)
      continue;

    join_count++;
    result = is_stack_resident(context, block_index, predecessor_count, -1);
    if (result != NO && result != YES) {
      fprintf(stderr, "register_allocator_core: join_reconcile function=%s block=%d predecessors=%d temp=-1 policy=stack_only status=invalid_callback\n",
          function_name, block_index, predecessor_count);
      return FAILED;
    }
    for (temp_index = 0; temp_index < temp_count; temp_index++) {
      int set_index;

      set_index = _register_allocator_liveness_index(block_index, temp_index,
          temp_count);
      if (live_in[set_index] == NO)
        continue;
      if (live_in[set_index] != YES) {
        fprintf(stderr, "register_allocator_core: join_reconcile function=%s block=%d predecessors=%d temp=%d live_in=invalid policy=stack_only status=invalid_liveness\n",
            function_name, block_index, predecessor_count, temp_index);
        return FAILED;
      }
      live_temp_count++;
      result = is_stack_resident(context, block_index, predecessor_count,
          temp_index);
      if (result != NO && result != YES) {
        fprintf(stderr, "register_allocator_core: join_reconcile function=%s block=%d predecessors=%d temp=%d policy=stack_only status=invalid_callback\n",
            function_name, block_index, predecessor_count, temp_index);
        return FAILED;
      }
      if (register_allocator_plan_join_action(function_name, block_index,
          predecessor_count, temp_index, result, &action) == FAILED)
        return FAILED;
      fprintf(stderr, "register_allocator_core: join_reconcile function=%s block=%d predecessors=%d temp=%d live_in=yes state=%s action=%s policy=stack_only status=%s\n",
          function_name, block_index, predecessor_count, temp_index,
          result == YES ? "stack" : "retained",
          action == RA_JOIN_ACTION_RELOAD_ON_DEMAND ? "reload_on_demand" :
          "spill_predecessors",
          action == RA_JOIN_ACTION_RELOAD_ON_DEMAND ? "complete" :
          "retained_live_in");
      if (action == RA_JOIN_ACTION_SPILL_PREDECESSORS)
        return FAILED;
    }
  }

  fprintf(stderr, "register_allocator_core: join_reconcile function=%s blocks=%d edges=%d temps=%d joins=%d live_temps=%d policy=stack_only status=complete\n",
      function_name, block_count, edge_count, temp_count, join_count,
      live_temp_count);
  return SUCCEEDED;
}


int register_allocator_solve_liveness(char *function_name, int block_count, int temp_count, struct register_allocator_liveness_storage *storage, struct register_allocator_cfg_edge *edges, int edge_count, char *live_use, char *live_def, char *live_in, char *live_out, int *iterations) {

  int block_index, edge_index, temp_index;
  int changed;
  int max_iterations;

  if (iterations != NULL)
    *iterations = 0;

  if (block_count <= 0 || temp_count <= 0) {
    fprintf(stderr, "register_allocator_core: liveness_solve function=%s blocks=%d edges=%d temps=%d iterations=0 algorithm=backward_fixed_point status=empty\n", function_name, block_count, edge_count, temp_count);
    return SUCCEEDED;
  }

  if (function_name == NULL || storage == NULL || iterations == NULL ||
      live_use == NULL || live_def == NULL || live_in == NULL || live_out == NULL ||
      edge_count < 0 || (edge_count > 0 && edges == NULL) ||
      storage->set_count <= 0 || storage->set_count == INT_MAX ||
      storage->set_count / temp_count != block_count ||
      storage->set_count % temp_count != 0 ||
      storage->buffer_bytes != (size_t)storage->set_count * sizeof(char) ||
      storage->buffer_bytes > ((size_t)-1) / 4 ||
      storage->total_bytes != storage->buffer_bytes * 4) {
    fprintf(stderr, "register_allocator_core: liveness_solve function=%s blocks=%d edges=%d temps=%d cells=%d buffer_bytes=%lu total_bytes=%lu iterations=0 algorithm=backward_fixed_point status=invalid_storage\n",
        function_name != NULL ? function_name : "<null>", block_count, edge_count,
        temp_count, storage != NULL ? storage->set_count : 0,
        storage != NULL ? (unsigned long)storage->buffer_bytes : 0,
        storage != NULL ? (unsigned long)storage->total_bytes : 0);
    return FAILED;
  }

  memset(live_in, 0, storage->buffer_bytes);
  memset(live_out, 0, storage->buffer_bytes);

  changed = YES;
  max_iterations = storage->set_count + 1;
  while (changed == YES) {
    changed = NO;
    (*iterations)++;

    if (*iterations > max_iterations) {
      fprintf(stderr, "register_allocator_core: liveness_solve function=%s blocks=%d edges=%d temps=%d iterations=%d algorithm=backward_fixed_point status=failed_to_converge\n", function_name, block_count, edge_count, temp_count, *iterations);
      return FAILED;
    }

    for (block_index = block_count - 1; block_index >= 0; block_index--) {
      for (temp_index = 0; temp_index < temp_count; temp_index++) {
        int set_index;
        int out_value;
        int in_value;

        set_index = _register_allocator_liveness_index(block_index, temp_index, temp_count);
        out_value = NO;

        for (edge_index = 0; edge_index < edge_count; edge_index++) {
          if (edges[edge_index].from_block != block_index)
            continue;
          if (edges[edge_index].to_block < 0 || edges[edge_index].to_block >= block_count) {
            fprintf(stderr, "register_allocator_core: liveness_solve function=%s edge=%d from_block=%d to_block=%d status=invalid_edge\n", function_name, edge_index, edges[edge_index].from_block, edges[edge_index].to_block);
            return FAILED;
          }
          if (live_in[_register_allocator_liveness_index(edges[edge_index].to_block, temp_index, temp_count)] == YES) {
            out_value = YES;
            break;
          }
        }

        in_value = live_use[set_index];
        if (in_value == NO && out_value == YES && live_def[set_index] == NO)
          in_value = YES;

        if (live_out[set_index] != out_value) {
          live_out[set_index] = (char)out_value;
          changed = YES;
        }
        if (live_in[set_index] != in_value) {
          live_in[set_index] = (char)in_value;
          changed = YES;
        }
      }
    }
  }

  fprintf(stderr, "register_allocator_core: liveness_solve function=%s blocks=%d edges=%d temps=%d iterations=%d algorithm=backward_fixed_point status=converged\n", function_name, block_count, edge_count, temp_count, *iterations);

  return SUCCEEDED;
}


int register_allocator_resolve_candidate_registers(char *function_name, struct register_allocator_target_policy *policy, int size, int no_physical_register, int *physical_registers, int max_registers, int *register_count) {

  int candidate_count;
  int candidate_index;
  int previous_index;

  if (register_count != NULL)
    *register_count = 0;
  if (physical_registers != NULL && max_registers > 0) {
    for (candidate_index = 0; candidate_index < max_registers; candidate_index++)
      physical_registers[candidate_index] = no_physical_register;
  }

  if (function_name == NULL || policy == NULL || policy->name == NULL || policy->name[0] == '\0' ||
      policy->get_candidate_register_count == NULL || policy->get_candidate_physical_register == NULL ||
      size <= 0 || physical_registers == NULL || max_registers <= 0 || register_count == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_registers function=%s target=%s size=%d capacity=%d count=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        policy != NULL && policy->name != NULL ? policy->name : "<null>", size, max_registers);
    return FAILED;
  }

  candidate_count = policy->get_candidate_register_count(size);
  if (candidate_count <= 0 || candidate_count > max_registers) {
    fprintf(stderr, "register_allocator_core: candidate_registers function=%s target=%s size=%d capacity=%d count=%d status=invalid_policy\n",
        function_name, policy->name, size, max_registers, candidate_count);
    return FAILED;
  }

  for (candidate_index = 0; candidate_index < candidate_count; candidate_index++) {
    physical_registers[candidate_index] = policy->get_candidate_physical_register(size, candidate_index);
    if (physical_registers[candidate_index] == no_physical_register) {
      fprintf(stderr, "register_allocator_core: candidate_register function=%s target=%s size=%d candidate=%d phy=%d status=invalid_policy\n",
          function_name, policy->name, size, candidate_index, physical_registers[candidate_index]);
      goto invalid_policy;
    }
    for (previous_index = 0; previous_index < candidate_index; previous_index++) {
      if (physical_registers[previous_index] == physical_registers[candidate_index]) {
        fprintf(stderr, "register_allocator_core: candidate_register function=%s target=%s size=%d candidate=%d phy=%d duplicate_candidate=%d status=invalid_policy\n",
            function_name, policy->name, size, candidate_index, physical_registers[candidate_index], previous_index);
        goto invalid_policy;
      }
    }
    fprintf(stderr, "register_allocator_core: candidate_register function=%s target=%s size=%d candidate=%d phy=%d status=complete\n",
        function_name, policy->name, size, candidate_index, physical_registers[candidate_index]);
  }

  *register_count = candidate_count;
  fprintf(stderr, "register_allocator_core: candidate_registers function=%s target=%s size=%d capacity=%d count=%d status=complete\n",
      function_name, policy->name, size, max_registers, candidate_count);
  return SUCCEEDED;

invalid_policy:
  for (candidate_index = 0; candidate_index < max_registers; candidate_index++)
    physical_registers[candidate_index] = no_physical_register;
  fprintf(stderr, "register_allocator_core: candidate_registers function=%s target=%s size=%d capacity=%d count=%d status=invalid_policy\n",
      function_name, policy->name, size, max_registers, candidate_count);
  return FAILED;
}


int register_allocator_evaluate_candidate_registers(char *function_name, int block_index, char *reason, int *physical_registers, int physical_register_count, int no_physical_register, void *context, register_allocator_candidate_predicate is_allowed, register_allocator_candidate_predicate is_path_safe, register_allocator_candidate_predicate is_conflict_free, struct register_allocator_candidate_evaluation *evaluations, int evaluation_capacity) {

  int candidate_index;
  char *failure_status;

  failure_status = "invalid_input";

  if (evaluations != NULL && evaluation_capacity > 0) {
    for (candidate_index = 0; candidate_index < evaluation_capacity; candidate_index++) {
      evaluations[candidate_index].physical_register = no_physical_register;
      evaluations[candidate_index].allowed = NO;
      evaluations[candidate_index].path_safe = NO;
      evaluations[candidate_index].conflict_free = NO;
    }
  }

  if (function_name == NULL || block_index < 0 || reason == NULL || reason[0] == '\0' ||
      physical_registers == NULL || physical_register_count <= 0 || context == NULL ||
      evaluations == NULL || evaluation_capacity <= 0 || physical_register_count > evaluation_capacity ||
      is_allowed == NULL || is_path_safe == NULL || is_conflict_free == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_evaluations function=%s block=%d reason=%s candidates=%d capacity=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        reason != NULL && reason[0] != '\0' ? reason : "<null>", physical_register_count,
        evaluation_capacity);
    return FAILED;
  }

  for (candidate_index = 0; candidate_index < physical_register_count; candidate_index++) {
    int previous_candidate;

    if (physical_registers[candidate_index] == no_physical_register) {
      fprintf(stderr, "register_allocator_core: candidate_evaluation function=%s block=%d reason=%s candidate=%d phy=%d status=invalid_input\n",
          function_name, block_index, reason, candidate_index, physical_registers[candidate_index]);
      goto invalid_input;
    }
    for (previous_candidate = 0; previous_candidate < candidate_index; previous_candidate++) {
      if (physical_registers[previous_candidate] == physical_registers[candidate_index]) {
        fprintf(stderr, "register_allocator_core: candidate_evaluation function=%s block=%d reason=%s candidate=%d phy=%d duplicate_candidate=%d status=invalid_input\n",
            function_name, block_index, reason, candidate_index, physical_registers[candidate_index],
            previous_candidate);
        goto invalid_input;
      }
    }
  }

  for (candidate_index = 0; candidate_index < physical_register_count; candidate_index++) {
    struct register_allocator_candidate_evaluation *evaluation;

    evaluation = &evaluations[candidate_index];
    evaluation->physical_register = physical_registers[candidate_index];
    evaluation->allowed = is_allowed(context, evaluation->physical_register);
    if (evaluation->allowed != NO && evaluation->allowed != YES) {
      failure_status = "invalid_callback";
      goto invalid_callback;
    }
    if (evaluation->allowed == YES) {
      evaluation->path_safe = is_path_safe(context, evaluation->physical_register);
      if (evaluation->path_safe != NO && evaluation->path_safe != YES) {
        failure_status = "invalid_callback";
        goto invalid_callback;
      }
    }
    if (evaluation->allowed == YES && evaluation->path_safe == YES) {
      evaluation->conflict_free = is_conflict_free(context, evaluation->physical_register);
      if (evaluation->conflict_free != NO && evaluation->conflict_free != YES) {
        failure_status = "invalid_callback";
        goto invalid_callback;
      }
    }

    fprintf(stderr, "register_allocator_core: candidate_evaluation function=%s block=%d reason=%s candidate=%d phy=%d allowed=%s path_safe=%s conflict_free=%s status=complete\n",
        function_name, block_index, reason, candidate_index, evaluation->physical_register,
        evaluation->allowed == YES ? "yes" : "no", evaluation->path_safe == YES ? "yes" : "no",
        evaluation->conflict_free == YES ? "yes" : "no");
  }

  fprintf(stderr, "register_allocator_core: candidate_evaluations function=%s block=%d reason=%s candidates=%d capacity=%d status=complete\n",
      function_name, block_index, reason, physical_register_count, evaluation_capacity);
  return SUCCEEDED;

invalid_callback:
  fprintf(stderr, "register_allocator_core: candidate_evaluation function=%s block=%d reason=%s candidate=%d phy=%d allowed=%s path_safe=%s conflict_free=%s status=%s\n",
      function_name, block_index, reason, candidate_index, evaluations[candidate_index].physical_register,
      evaluations[candidate_index].allowed == YES ? "yes" : (evaluations[candidate_index].allowed == NO ? "no" : "invalid"),
      evaluations[candidate_index].path_safe == YES ? "yes" : (evaluations[candidate_index].path_safe == NO ? "no" : "invalid"),
      evaluations[candidate_index].conflict_free == YES ? "yes" : (evaluations[candidate_index].conflict_free == NO ? "no" : "invalid"),
      failure_status);

invalid_input:
  for (candidate_index = 0; candidate_index < evaluation_capacity; candidate_index++) {
    evaluations[candidate_index].physical_register = no_physical_register;
    evaluations[candidate_index].allowed = NO;
    evaluations[candidate_index].path_safe = NO;
    evaluations[candidate_index].conflict_free = NO;
  }
  fprintf(stderr, "register_allocator_core: candidate_evaluations function=%s block=%d reason=%s candidates=%d capacity=%d status=%s\n",
      function_name, block_index, reason, physical_register_count, evaluation_capacity,
      failure_status != NULL ? failure_status : "invalid_input");
  return FAILED;
}


static void _clear_candidate_selection(struct register_allocator_candidate_selection *selection,
    int no_physical_register) {

  selection->physical_register = no_physical_register;
  selection->candidate_index = -1;
}


int register_allocator_select_candidate_register(char *function_name, int block_index, char *reason,
    int *physical_registers, int physical_register_count, int no_physical_register, void *context,
    register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_conflict_free,
    struct register_allocator_candidate_evaluation *evaluations, int evaluation_capacity,
    struct register_allocator_candidate_selection *selection) {

  int candidate_index;

  if (selection != NULL)
    _clear_candidate_selection(selection, no_physical_register);
  if (evaluations != NULL && evaluation_capacity > 0) {
    for (candidate_index = 0; candidate_index < evaluation_capacity; candidate_index++) {
      evaluations[candidate_index].physical_register = no_physical_register;
      evaluations[candidate_index].allowed = NO;
      evaluations[candidate_index].path_safe = NO;
      evaluations[candidate_index].conflict_free = NO;
    }
  }
  if (function_name == NULL || block_index < 0 || reason == NULL || reason[0] == '\0' ||
      physical_registers == NULL || physical_register_count <= 0 || context == NULL ||
      is_allowed == NULL || is_path_safe == NULL || is_conflict_free == NULL ||
      evaluations == NULL || evaluation_capacity <= 0 || selection == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_selection function=%s block=%d reason=%s candidates=%d selected=-1 phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        reason != NULL && reason[0] != '\0' ? reason : "<null>", physical_register_count,
        no_physical_register);
    return FAILED;
  }

  if (register_allocator_evaluate_candidate_registers(function_name, block_index, reason,
      physical_registers, physical_register_count, no_physical_register, context, is_allowed,
      is_path_safe, is_conflict_free, evaluations, evaluation_capacity) == FAILED)
    goto dependency_failed;
  if (register_allocator_choose_candidate_register(function_name, block_index, reason,
      evaluations, physical_register_count, no_physical_register, &selection->physical_register,
      &selection->candidate_index) == FAILED)
    goto dependency_failed;

  fprintf(stderr, "register_allocator_core: candidate_selection function=%s block=%d reason=%s candidates=%d selected=%d phy=%d status=complete\n",
      function_name, block_index, reason, physical_register_count, selection->candidate_index,
      selection->physical_register);
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_selection(selection, no_physical_register);
  for (candidate_index = 0; candidate_index < evaluation_capacity; candidate_index++) {
    evaluations[candidate_index].physical_register = no_physical_register;
    evaluations[candidate_index].allowed = NO;
    evaluations[candidate_index].path_safe = NO;
    evaluations[candidate_index].conflict_free = NO;
  }
  fprintf(stderr, "register_allocator_core: candidate_selection function=%s block=%d reason=%s candidates=%d selected=-1 phy=%d status=dependency_failed\n",
      function_name, block_index, reason, physical_register_count, no_physical_register);
  return FAILED;
}


static void _clear_candidate_dispatch(struct register_allocator_candidate_dispatch *dispatch,
    int no_physical_register) {

  dispatch->action = FAILED;
  dispatch->physical_register = no_physical_register;
  dispatch->candidate_index = -1;
  dispatch->check_path = NO;
  dispatch->check_overlap = NO;
}


int register_allocator_dispatch_candidate_plan(char *function_name, int block_index, int plan,
    int primary_physical_register, int no_physical_register, void *context,
    register_allocator_alternate_selector select_alternate,
    struct register_allocator_candidate_dispatch *dispatch) {

  char *reason;
  int alternate_required;
  int selector_status;

  if (dispatch != NULL)
    _clear_candidate_dispatch(dispatch, no_physical_register);
  if (function_name == NULL || block_index < 0 ||
      plan < RA_CANDIDATE_PLAN_USE_PRIMARY || plan > RA_CANDIDATE_PLAN_REJECT_PATH ||
      primary_physical_register == no_physical_register || context == NULL ||
      select_alternate == NULL || dispatch == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_dispatch function=%s block=%d plan=%d primary=%d action=failed phy=%d selected=-1 path=no overlap=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, plan,
        primary_physical_register, no_physical_register);
    return FAILED;
  }

  if (plan == RA_CANDIDATE_PLAN_USE_PRIMARY) {
    dispatch->action = RA_CANDIDATE_DISPATCH_USE_PRIMARY;
    dispatch->physical_register = primary_physical_register;
    reason = "primary_available";
  }
  else if (plan == RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED ||
      plan == RA_CANDIDATE_PLAN_REJECT_PATH) {
    dispatch->action = RA_CANDIDATE_DISPATCH_REJECT;
    reason = plan == RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED ?
        "unsupported_primary" : "clobber_between";
  }
  else {
    alternate_required = plan != RA_CANDIDATE_PLAN_FALLBACK_CONFLICT;
    reason = plan == RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED ?
        "unsupported_primary" : (plan == RA_CANDIDATE_PLAN_FALLBACK_PATH ?
        "clobber_between" : "active_register_conflict");
    dispatch->check_path = plan == RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED ? NO : YES;
    dispatch->check_overlap = dispatch->check_path;
    selector_status = select_alternate(context, reason, dispatch->check_path,
        dispatch->check_overlap, &dispatch->physical_register, &dispatch->candidate_index);
    if (selector_status != FAILED && selector_status != SUCCEEDED)
      goto invalid_callback;
    if (selector_status == FAILED)
      goto dependency_failed;
    if ((dispatch->physical_register == no_physical_register && dispatch->candidate_index != -1) ||
        (dispatch->physical_register != no_physical_register && dispatch->candidate_index < 0))
      goto invalid_callback;
    if (dispatch->physical_register != no_physical_register)
      dispatch->action = RA_CANDIDATE_DISPATCH_USE_ALTERNATE;
    else if (alternate_required == YES)
      dispatch->action = RA_CANDIDATE_DISPATCH_REJECT;
    else {
      dispatch->action = RA_CANDIDATE_DISPATCH_USE_PRIMARY;
      dispatch->physical_register = primary_physical_register;
    }
  }

  fprintf(stderr, "register_allocator_core: candidate_dispatch function=%s block=%d plan=%d primary=%d reason=%s action=%d phy=%d selected=%d path=%s overlap=%s status=complete\n",
      function_name, block_index, plan, primary_physical_register, reason, dispatch->action,
      dispatch->physical_register, dispatch->candidate_index,
      dispatch->check_path == YES ? "yes" : "no",
      dispatch->check_overlap == YES ? "yes" : "no");
  return SUCCEEDED;

invalid_callback:
  _clear_candidate_dispatch(dispatch, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_dispatch function=%s block=%d plan=%d primary=%d action=failed phy=%d selected=-1 path=no overlap=no status=invalid_callback\n",
      function_name, block_index, plan, primary_physical_register, no_physical_register);
  return FAILED;

dependency_failed:
  _clear_candidate_dispatch(dispatch, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_dispatch function=%s block=%d plan=%d primary=%d action=failed phy=%d selected=-1 path=no overlap=no status=dependency_failed\n",
      function_name, block_index, plan, primary_physical_register, no_physical_register);
  return FAILED;
}


int register_allocator_evaluate_primary_candidate(char *function_name, int block_index,
    int physical_register, int no_physical_register, void *context,
    register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_active,
    struct register_allocator_primary_candidate_evaluation *evaluation) {

  int allowed;
  int path_safe;
  int active;
  char *invalid_fact;

  allowed = NO;
  path_safe = NO;
  active = NO;
  invalid_fact = NULL;
  if (evaluation != NULL) {
    evaluation->physical_register = no_physical_register;
    evaluation->allowed = NO;
    evaluation->path_safe = NO;
    evaluation->active = NO;
  }

  if (function_name == NULL || block_index < 0 || physical_register == no_physical_register ||
      context == NULL || is_allowed == NULL || is_path_safe == NULL || is_active == NULL ||
      evaluation == NULL) {
    fprintf(stderr, "register_allocator_core: primary_candidate_evaluation function=%s block=%d phy=%d allowed=no path_safe=no active=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, physical_register);
    return FAILED;
  }

  allowed = is_allowed(context, physical_register);
  if (allowed != NO && allowed != YES) {
    invalid_fact = "allowed";
    goto invalid_callback;
  }
  if (allowed == YES) {
    path_safe = is_path_safe(context, physical_register);
    if (path_safe != NO && path_safe != YES) {
      invalid_fact = "path_safe";
      goto invalid_callback;
    }
  }
  active = is_active(context, physical_register);
  if (active != NO && active != YES) {
    invalid_fact = "active";
    goto invalid_callback;
  }

  evaluation->physical_register = physical_register;
  evaluation->allowed = allowed;
  evaluation->path_safe = path_safe;
  evaluation->active = active;
  fprintf(stderr, "register_allocator_core: primary_candidate_evaluation function=%s block=%d phy=%d allowed=%s path_safe=%s active=%s status=complete\n",
      function_name, block_index, physical_register, allowed == YES ? "yes" : "no",
      path_safe == YES ? "yes" : "no", active == YES ? "yes" : "no");
  return SUCCEEDED;

invalid_callback:
  fprintf(stderr, "register_allocator_core: primary_candidate_evaluation function=%s block=%d phy=%d allowed=%s path_safe=%s active=%s invalid_fact=%s status=invalid_callback\n",
      function_name, block_index, physical_register,
      allowed == YES ? "yes" : (allowed == NO ? "no" : "invalid"),
      path_safe == YES ? "yes" : (path_safe == NO ? "no" : "invalid"),
      active == YES ? "yes" : (active == NO ? "no" : "invalid"), invalid_fact);
  return FAILED;
}


static void _clear_primary_candidate_decision(struct register_allocator_primary_candidate_decision *decision, int no_physical_register) {

  decision->evaluation.physical_register = no_physical_register;
  decision->evaluation.allowed = NO;
  decision->evaluation.path_safe = NO;
  decision->evaluation.active = NO;
  decision->fallback_unsupported = NO;
  decision->fallback_path = NO;
  decision->fallback_conflict = NO;
  decision->plan = FAILED;
}


int register_allocator_decide_primary_candidate(char *function_name, int block_index,
    struct register_allocator_target_policy *policy, int size, int candidate_count,
    int physical_register, int no_physical_register, void *context,
    register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_active,
    struct register_allocator_primary_candidate_decision *decision) {

  if (decision != NULL)
    _clear_primary_candidate_decision(decision, no_physical_register);
  if (function_name == NULL || block_index < 0 || policy == NULL || size <= 0 ||
      candidate_count <= 0 || physical_register == no_physical_register || context == NULL ||
      is_allowed == NULL || is_path_safe == NULL || is_active == NULL || decision == NULL) {
    fprintf(stderr, "register_allocator_core: primary_candidate_decision function=%s block=%d size=%d candidates=%d phy=%d plan=failed status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, size, candidate_count,
        physical_register);
    return FAILED;
  }

  if (register_allocator_evaluate_primary_candidate(function_name, block_index,
      physical_register, no_physical_register, context, is_allowed, is_path_safe, is_active,
      &decision->evaluation) == FAILED)
    goto failed;
  if (register_allocator_resolve_candidate_fallbacks(function_name, policy, size,
      candidate_count, &decision->fallback_unsupported, &decision->fallback_path,
      &decision->fallback_conflict) == FAILED)
    goto failed;
  decision->plan = register_allocator_plan_candidate_fallback(function_name, block_index,
      decision->evaluation.allowed, decision->evaluation.path_safe, decision->evaluation.active,
      decision->fallback_unsupported, decision->fallback_path, decision->fallback_conflict);
  if (decision->plan == FAILED)
    goto failed;

  fprintf(stderr, "register_allocator_core: primary_candidate_decision function=%s block=%d size=%d candidates=%d phy=%d allowed=%s path_safe=%s active=%s fallback_unsupported=%s fallback_path=%s fallback_conflict=%s plan=%d status=complete\n",
      function_name, block_index, size, candidate_count, physical_register,
      decision->evaluation.allowed == YES ? "yes" : "no",
      decision->evaluation.path_safe == YES ? "yes" : "no",
      decision->evaluation.active == YES ? "yes" : "no",
      decision->fallback_unsupported == YES ? "yes" : "no",
      decision->fallback_path == YES ? "yes" : "no",
      decision->fallback_conflict == YES ? "yes" : "no", decision->plan);
  return SUCCEEDED;

failed:
  _clear_primary_candidate_decision(decision, no_physical_register);
  fprintf(stderr, "register_allocator_core: primary_candidate_decision function=%s block=%d size=%d candidates=%d phy=%d plan=failed status=dependency_failed\n",
      function_name, block_index, size, candidate_count, physical_register);
  return FAILED;
}


static void _clear_candidate_selection_finalization(
    struct register_allocator_candidate_selection_finalization *finalization,
    int no_physical_register) {

  finalization->proceed = NO;
  _clear_primary_candidate_decision(&finalization->decision, no_physical_register);
  _clear_candidate_dispatch(&finalization->dispatch, no_physical_register);
  finalization->split_action = FAILED;
}


int register_allocator_finalize_candidate_selection(char *function_name, int block_index,
    struct register_allocator_target_policy *policy, int size, int candidate_count,
    int primary_physical_register, int no_physical_register, int temp_index,
    int producer_instruction, int consumer_instruction, int has_single_read,
    int preservation_supported, int preservation_physical_register,
    void *evaluation_context, register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_active, void *dispatch_context,
    register_allocator_alternate_selector select_alternate,
    struct register_allocator_candidate_selection_finalization *finalization) {

  char *failure_stage;
  char *reason;

  if (finalization != NULL)
    _clear_candidate_selection_finalization(finalization, no_physical_register);
  if (function_name == NULL || block_index < 0 || policy == NULL || size <= 0 ||
      candidate_count <= 0 || primary_physical_register == no_physical_register ||
      temp_index < 0 || producer_instruction < 0 || consumer_instruction <= producer_instruction ||
      (has_single_read != NO && has_single_read != YES) ||
      (preservation_supported != NO && preservation_supported != YES) ||
      (has_single_read == YES && preservation_supported != NO) ||
      (preservation_supported == YES && preservation_physical_register == no_physical_register) ||
      (preservation_supported == NO && preservation_physical_register != no_physical_register) ||
      evaluation_context == NULL || is_allowed == NULL || is_path_safe == NULL ||
      is_active == NULL || dispatch_context == NULL || select_alternate == NULL ||
      finalization == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_selection_finalize function=%s block=%d temp=r%d producer=%d consumer=%d primary=%d proceed=no selected=%d split=failed status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, temp_index,
        producer_instruction, consumer_instruction, primary_physical_register,
        no_physical_register);
    return FAILED;
  }

  failure_stage = "primary_decision";
  if (register_allocator_decide_primary_candidate(function_name, block_index, policy, size,
      candidate_count, primary_physical_register, no_physical_register, evaluation_context,
      is_allowed, is_path_safe, is_active, &finalization->decision) == FAILED)
    goto dependency_failed;

  failure_stage = "candidate_dispatch";
  if (register_allocator_dispatch_candidate_plan(function_name, block_index,
      finalization->decision.plan, primary_physical_register, no_physical_register,
      dispatch_context, select_alternate, &finalization->dispatch) == FAILED)
    goto dependency_failed;

  if (finalization->dispatch.action == RA_CANDIDATE_DISPATCH_REJECT) {
    reason = finalization->decision.plan == RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED ||
        finalization->decision.plan == RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED ?
        "unsupported_physical_register" : "clobber_between";
    fprintf(stderr, "register_allocator_core: candidate_selection_finalize function=%s block=%d temp=r%d producer=%d consumer=%d plan=%d dispatch=%d proceed=no selected=%d split=failed reason=%s status=complete\n",
        function_name, block_index, temp_index, producer_instruction, consumer_instruction,
        finalization->decision.plan, finalization->dispatch.action,
        finalization->dispatch.physical_register, reason);
    return SUCCEEDED;
  }

  failure_stage = "split_action";
  finalization->split_action = register_allocator_plan_split_action(function_name, block_index,
      temp_index, producer_instruction, consumer_instruction, has_single_read,
      preservation_supported, no_physical_register, preservation_physical_register,
      finalization->dispatch.physical_register);
  if (finalization->split_action == FAILED)
    goto dependency_failed;
  if (finalization->split_action == RA_SPLIT_ACTION_REJECT_UNSUPPORTED ||
      finalization->split_action == RA_SPLIT_ACTION_REJECT_REGISTER) {
    reason = finalization->split_action == RA_SPLIT_ACTION_REJECT_UNSUPPORTED ?
        "multi_read_interval" : "multi_read_preserve_requires_register";
    fprintf(stderr, "register_allocator_core: candidate_selection_finalize function=%s block=%d temp=r%d producer=%d consumer=%d plan=%d dispatch=%d proceed=no selected=%d split=%d reason=%s status=complete\n",
        function_name, block_index, temp_index, producer_instruction, consumer_instruction,
        finalization->decision.plan, finalization->dispatch.action,
        finalization->dispatch.physical_register, finalization->split_action, reason);
    return SUCCEEDED;
  }

  finalization->proceed = YES;
  fprintf(stderr, "register_allocator_core: candidate_selection_finalize function=%s block=%d temp=r%d producer=%d consumer=%d plan=%d dispatch=%d proceed=yes selected=%d split=%d order=decide_dispatch_split status=complete\n",
      function_name, block_index, temp_index, producer_instruction, consumer_instruction,
      finalization->decision.plan, finalization->dispatch.action,
      finalization->dispatch.physical_register, finalization->split_action);
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_selection_finalization(finalization, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_selection_finalize function=%s block=%d temp=r%d producer=%d consumer=%d primary=%d proceed=no selected=%d split=failed stage=%s status=dependency_failed\n",
      function_name, block_index, temp_index, producer_instruction, consumer_instruction,
      primary_physical_register, no_physical_register, failure_stage);
  return FAILED;
}


int register_allocator_choose_candidate_register(char *function_name, int block_index, char *reason, struct register_allocator_candidate_evaluation *candidates, int candidate_count, int no_physical_register, int *selected_physical_register, int *selected_candidate_index) {

  int candidate_index;

  if (selected_physical_register != NULL)
    *selected_physical_register = no_physical_register;
  if (selected_candidate_index != NULL)
    *selected_candidate_index = -1;

  if (function_name == NULL || block_index < 0 || reason == NULL || reason[0] == '\0' ||
      candidates == NULL || candidate_count <= 0 || selected_physical_register == NULL ||
      selected_candidate_index == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_arbitration function=%s block=%d reason=%s candidates=%d selected=-1 phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        reason != NULL && reason[0] != '\0' ? reason : "<null>", candidate_count,
        no_physical_register);
    return FAILED;
  }

  for (candidate_index = 0; candidate_index < candidate_count; candidate_index++) {
    struct register_allocator_candidate_evaluation *candidate;
    int previous_candidate;

    candidate = &candidates[candidate_index];
    if (candidate->physical_register == no_physical_register ||
        (candidate->allowed != NO && candidate->allowed != YES) ||
        (candidate->path_safe != NO && candidate->path_safe != YES) ||
        (candidate->conflict_free != NO && candidate->conflict_free != YES)) {
      fprintf(stderr, "register_allocator_core: candidate_arbitrate function=%s block=%d reason=%s candidate=%d phy=%d allowed=%s path_safe=%s conflict_free=%s status=invalid_input\n",
          function_name, block_index, reason, candidate_index, candidate->physical_register,
          candidate->allowed == YES ? "yes" : (candidate->allowed == NO ? "no" : "invalid"),
          candidate->path_safe == YES ? "yes" : (candidate->path_safe == NO ? "no" : "invalid"),
          candidate->conflict_free == YES ? "yes" : (candidate->conflict_free == NO ? "no" : "invalid"));
        goto invalid_input;
    }
    for (previous_candidate = 0; previous_candidate < candidate_index; previous_candidate++) {
      if (candidates[previous_candidate].physical_register == candidate->physical_register) {
        fprintf(stderr, "register_allocator_core: candidate_arbitrate function=%s block=%d reason=%s candidate=%d phy=%d duplicate_candidate=%d status=invalid_input\n",
            function_name, block_index, reason, candidate_index, candidate->physical_register,
            previous_candidate);
        goto invalid_input;
      }
    }

    fprintf(stderr, "register_allocator_core: candidate_arbitrate function=%s block=%d reason=%s candidate=%d phy=%d allowed=%s path_safe=%s conflict_free=%s eligible=%s status=complete\n",
        function_name, block_index, reason, candidate_index, candidate->physical_register,
        candidate->allowed == YES ? "yes" : "no", candidate->path_safe == YES ? "yes" : "no",
        candidate->conflict_free == YES ? "yes" : "no",
        candidate->allowed == YES && candidate->path_safe == YES && candidate->conflict_free == YES ? "yes" : "no");
    if (*selected_candidate_index < 0 && candidate->allowed == YES &&
        candidate->path_safe == YES && candidate->conflict_free == YES) {
      *selected_physical_register = candidate->physical_register;
      *selected_candidate_index = candidate_index;
    }
  }

  fprintf(stderr, "register_allocator_core: candidate_arbitration function=%s block=%d reason=%s candidates=%d selected=%d phy=%d status=complete\n",
      function_name, block_index, reason, candidate_count, *selected_candidate_index,
      *selected_physical_register);
  return SUCCEEDED;

invalid_input:
  *selected_physical_register = no_physical_register;
  *selected_candidate_index = -1;
  fprintf(stderr, "register_allocator_core: candidate_arbitration function=%s block=%d reason=%s candidates=%d selected=-1 phy=%d status=invalid_input\n",
      function_name, block_index, reason, candidate_count, no_physical_register);
  return FAILED;
}


int register_allocator_resolve_candidate_fallbacks(char *function_name, struct register_allocator_target_policy *policy, int size, int candidate_count, int *fallback_unsupported, int *fallback_path, int *fallback_conflict) {

  int unsupported_capability;
  int path_capability;
  int conflict_capability;

  if (fallback_unsupported != NULL)
    *fallback_unsupported = NO;
  if (fallback_path != NULL)
    *fallback_path = NO;
  if (fallback_conflict != NULL)
    *fallback_conflict = NO;

  if (function_name == NULL || policy == NULL || policy->name == NULL || policy->name[0] == '\0' ||
      policy->can_fallback_candidate == NULL || size <= 0 || candidate_count <= 0 ||
      fallback_unsupported == NULL || fallback_path == NULL || fallback_conflict == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_fallbacks function=%s target=%s size=%d candidates=%d unsupported=no path=no conflict=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>",
        policy != NULL && policy->name != NULL && policy->name[0] != '\0' ? policy->name : "<null>",
        size, candidate_count);
    return FAILED;
  }

  unsupported_capability = policy->can_fallback_candidate(size, RA_CANDIDATE_FALLBACK_UNSUPPORTED);
  path_capability = policy->can_fallback_candidate(size, RA_CANDIDATE_FALLBACK_PATH);
  conflict_capability = policy->can_fallback_candidate(size, RA_CANDIDATE_FALLBACK_CONFLICT);
  if ((unsupported_capability != NO && unsupported_capability != YES) ||
      (path_capability != NO && path_capability != YES) ||
      (conflict_capability != NO && conflict_capability != YES)) {
    fprintf(stderr, "register_allocator_core: candidate_fallbacks function=%s target=%s size=%d candidates=%d unsupported=%s path=%s conflict=%s status=invalid_callback\n",
        function_name, policy->name, size, candidate_count,
        unsupported_capability == YES ? "yes" : (unsupported_capability == NO ? "no" : "invalid"),
        path_capability == YES ? "yes" : (path_capability == NO ? "no" : "invalid"),
        conflict_capability == YES ? "yes" : (conflict_capability == NO ? "no" : "invalid"));
    return FAILED;
  }

  if (candidate_count > 1) {
    *fallback_unsupported = unsupported_capability;
    *fallback_path = path_capability;
    *fallback_conflict = conflict_capability;
  }
  fprintf(stderr, "register_allocator_core: candidate_fallbacks function=%s target=%s size=%d candidates=%d unsupported=%s path=%s conflict=%s status=complete\n",
      function_name, policy->name, size, candidate_count,
      *fallback_unsupported == YES ? "yes" : "no", *fallback_path == YES ? "yes" : "no",
      *fallback_conflict == YES ? "yes" : "no");
  return SUCCEEDED;
}


int register_allocator_resolve_equal_next_use_preference(char *function_name, int block_index,
    struct register_allocator_target_policy *policy, int consumer_op, int candidate_operand,
    int active_operand, int physical_register, int no_physical_register, int candidate_next_use,
    int active_next_use, int *prefer_candidate) {

  int preference;

  if (prefer_candidate != NULL)
    *prefer_candidate = NO;
  if (function_name == NULL || block_index < 0 || policy == NULL || policy->name == NULL ||
      policy->name[0] == '\0' || policy->prefer_candidate_on_equal_next_use == NULL ||
      consumer_op < 0 || candidate_operand < 0 || active_operand < 0 ||
      physical_register == no_physical_register || candidate_next_use < 0 ||
      active_next_use < -1 || prefer_candidate == NULL) {
    fprintf(stderr, "register_allocator_core: equal_next_use_preference function=%s block=%d target=%s op=%d candidate_operand=%d active_operand=%d phy=%d candidate_next=%d active_next=%d queried=no prefer=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        policy != NULL && policy->name != NULL && policy->name[0] != '\0' ? policy->name : "<null>",
        consumer_op, candidate_operand, active_operand, physical_register, candidate_next_use,
        active_next_use);
    return FAILED;
  }

  if (candidate_next_use != active_next_use) {
    fprintf(stderr, "register_allocator_core: equal_next_use_preference function=%s block=%d target=%s op=%d candidate_operand=%d active_operand=%d phy=%d candidate_next=%d active_next=%d queried=no prefer=no reason=unequal_next_use status=complete\n",
        function_name, block_index, policy->name, consumer_op, candidate_operand, active_operand,
        physical_register, candidate_next_use, active_next_use);
    return SUCCEEDED;
  }

  preference = policy->prefer_candidate_on_equal_next_use(
      consumer_op, candidate_operand, active_operand, physical_register);
  if (preference != NO && preference != YES) {
    fprintf(stderr, "register_allocator_core: equal_next_use_preference function=%s block=%d target=%s op=%d candidate_operand=%d active_operand=%d phy=%d candidate_next=%d active_next=%d queried=yes prefer=invalid status=invalid_callback\n",
        function_name, block_index, policy->name, consumer_op, candidate_operand, active_operand,
        physical_register, candidate_next_use, active_next_use);
    return FAILED;
  }

  *prefer_candidate = preference;
  fprintf(stderr, "register_allocator_core: equal_next_use_preference function=%s block=%d target=%s op=%d candidate_operand=%d active_operand=%d phy=%d candidate_next=%d active_next=%d queried=yes prefer=%s reason=policy status=complete\n",
      function_name, block_index, policy->name, consumer_op, candidate_operand, active_operand,
      physical_register, candidate_next_use, active_next_use, preference == YES ? "yes" : "no");
  return SUCCEEDED;
}


int register_allocator_interval_is_blocked(char *function_name, int producer_instruction,
    int consumer_instruction, int spill_reason, int no_spill_reason,
    int unconditional_spill_reason, int spill_boundary_instruction, int *blocked) {

  char *reason;

  if (blocked != NULL)
    *blocked = NO;
  if (function_name == NULL || producer_instruction < 0 ||
      consumer_instruction <= producer_instruction || no_spill_reason == unconditional_spill_reason ||
      spill_boundary_instruction < -1 || blocked == NULL) {
    fprintf(stderr, "register_allocator_core: interval_blocking function=%s producer=%d consumer=%d spill_reason=%d boundary=%d blocked=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", producer_instruction,
        consumer_instruction, spill_reason, spill_boundary_instruction);
    return FAILED;
  }

  reason = "boundary_outside_interval";
  if (spill_reason == no_spill_reason)
    reason = "no_spill_reason";
  else if (spill_reason == unconditional_spill_reason) {
    *blocked = YES;
    reason = "unconditional_spill_reason";
  }
  else if (spill_boundary_instruction < 0) {
    *blocked = YES;
    reason = "missing_boundary";
  }
  else if (spill_boundary_instruction > producer_instruction &&
      spill_boundary_instruction < consumer_instruction) {
    *blocked = YES;
    reason = "boundary_between";
  }

  fprintf(stderr, "register_allocator_core: interval_blocking function=%s producer=%d consumer=%d spill_reason=%d boundary=%d blocked=%s reason=%s status=complete\n",
      function_name, producer_instruction, consumer_instruction, spill_reason,
      spill_boundary_instruction, *blocked == YES ? "yes" : "no", reason);
  return SUCCEEDED;
}


static void _clear_candidate_selection_preparation(
    struct register_allocator_candidate_selection_preparation *preparation,
    int no_physical_register) {

  preparation->proceed = NO;
  preparation->interval_blocked = NO;
  preparation->preservation_queried = NO;
  preparation->preservation_supported = NO;
  preparation->preservation_physical_register = no_physical_register;
}


int register_allocator_prepare_candidate_selection(char *function_name, int block_index,
    struct register_allocator_target_policy *policy,
    struct register_allocator_candidate *candidate, int consumer_op, int size,
    int physical_register, int no_physical_register, int spill_reason,
    int no_spill_reason, int unconditional_spill_reason, int spill_boundary_instruction,
    struct register_allocator_candidate_selection_preparation *preparation) {

  int preservation_supported;

  if (preparation != NULL)
    _clear_candidate_selection_preparation(preparation, no_physical_register);
  if (function_name == NULL || block_index < 0 || policy == NULL ||
      policy->name == NULL || policy->name[0] == '\0' ||
      _is_valid_candidate(candidate) == NO ||
      consumer_op < 0 || size <= 0 || physical_register == no_physical_register ||
      preparation == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=%s phy=%d proceed=no blocked=no preservation_queried=no preservation_supported=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        policy != NULL && policy->name != NULL ? policy->name : "<null>",
        candidate != NULL ? candidate->temp_index : -1,
        candidate != NULL ? candidate->producer_instruction : -1,
        candidate != NULL ? candidate->consumer_instruction : -1,
        candidate != NULL && candidate->has_single_read == YES ? "yes" :
        (candidate != NULL && candidate->has_single_read == NO ? "no" : "invalid"),
        physical_register);
    return FAILED;
  }

  if (register_allocator_interval_is_blocked(function_name, candidate->producer_instruction,
      candidate->consumer_instruction, spill_reason, no_spill_reason,
      unconditional_spill_reason, spill_boundary_instruction,
      &preparation->interval_blocked) == FAILED)
    goto dependency_failed;
  if (preparation->interval_blocked == YES) {
    fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=%s phy=%d proceed=no blocked=yes preservation_queried=no preservation_supported=no reason=spill_boundary status=complete\n",
        function_name, block_index, policy->name, candidate->temp_index,
        candidate->producer_instruction, candidate->consumer_instruction,
        candidate->has_single_read == YES ? "yes" : "no", physical_register);
    return SUCCEEDED;
  }

  preparation->proceed = YES;
  if (candidate->has_single_read == YES) {
    fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=yes phy=%d proceed=yes blocked=no preservation_queried=no preservation_supported=no reason=single_read status=complete\n",
        function_name, block_index, policy->name, candidate->temp_index,
        candidate->producer_instruction, candidate->consumer_instruction, physical_register);
    return SUCCEEDED;
  }

  if (policy->can_preserve_split_spill == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=no phy=%d proceed=yes blocked=no preservation_queried=no preservation_supported=no preservation_phy=%d reason=policy_unavailable status=complete\n",
        function_name, block_index, policy->name, candidate->temp_index,
        candidate->producer_instruction, candidate->consumer_instruction, physical_register,
        preparation->preservation_physical_register);
    return SUCCEEDED;
  }

  preparation->preservation_queried = YES;
  preservation_supported = policy->can_preserve_split_spill(consumer_op,
      candidate->consumer_operand, size, physical_register);
  if (preservation_supported != NO && preservation_supported != YES)
    goto invalid_callback;
  preparation->preservation_supported = preservation_supported;
  if (preservation_supported == YES)
    preparation->preservation_physical_register = physical_register;
  fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=no phy=%d proceed=yes blocked=no preservation_queried=yes preservation_supported=%s preservation_phy=%d reason=multi_read status=complete\n",
      function_name, block_index, policy->name, candidate->temp_index,
      candidate->producer_instruction, candidate->consumer_instruction, physical_register,
      preservation_supported == YES ? "yes" : "no",
      preparation->preservation_physical_register);
  return SUCCEEDED;

invalid_callback:
  _clear_candidate_selection_preparation(preparation, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=no phy=%d proceed=no blocked=no preservation_queried=no preservation_supported=no callback=can_preserve_split_spill status=invalid_callback\n",
      function_name, block_index, policy->name, candidate->temp_index,
      candidate->producer_instruction, candidate->consumer_instruction, physical_register);
  return FAILED;

dependency_failed:
  _clear_candidate_selection_preparation(preparation, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_selection_prepare function=%s block=%d target=%s temp=r%d producer=%d consumer=%d single_read=%s phy=%d proceed=no blocked=no preservation_queried=no preservation_supported=no stage=interval_blocking status=dependency_failed\n",
      function_name, block_index, policy->name, candidate->temp_index,
      candidate->producer_instruction, candidate->consumer_instruction,
      candidate->has_single_read == YES ? "yes" : "no", physical_register);
  return FAILED;
}


static void _clear_candidate_selection_orchestration(
    struct register_allocator_candidate_selection_orchestration *orchestration,
    int no_physical_register) {

  orchestration->proceed = NO;
  _clear_candidate_selection_preparation(&orchestration->preparation,
      no_physical_register);
  _clear_candidate_selection_finalization(&orchestration->finalization,
      no_physical_register);
}


int register_allocator_orchestrate_candidate_selection(char *function_name, int block_index,
    struct register_allocator_target_policy *policy,
    struct register_allocator_candidate *candidate, int consumer_op, int size,
    int candidate_count, int primary_physical_register, int no_physical_register,
    int spill_reason, int no_spill_reason, int unconditional_spill_reason,
    int spill_boundary_instruction, void *evaluation_context,
    register_allocator_candidate_predicate is_allowed,
    register_allocator_candidate_predicate is_path_safe,
    register_allocator_candidate_predicate is_active, void *dispatch_context,
    register_allocator_alternate_selector select_alternate,
    struct register_allocator_candidate_selection_orchestration *orchestration) {

  char *failure_stage;

  if (orchestration != NULL)
    _clear_candidate_selection_orchestration(orchestration, no_physical_register);
  if (function_name == NULL || block_index < 0 || policy == NULL ||
      _is_valid_candidate(candidate) == NO || consumer_op < 0 || size <= 0 ||
      candidate_count <= 0 || primary_physical_register == no_physical_register ||
      evaluation_context == NULL || is_allowed == NULL || is_path_safe == NULL ||
      is_active == NULL || dispatch_context == NULL || select_alternate == NULL ||
      orchestration == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_selection_orchestrate function=%s block=%d temp=r%d producer=%d consumer=%d primary=%d proceed=no status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        candidate != NULL ? candidate->temp_index : -1,
        candidate != NULL ? candidate->producer_instruction : -1,
        candidate != NULL ? candidate->consumer_instruction : -1,
        primary_physical_register);
    return FAILED;
  }

  failure_stage = "preparation";
  if (register_allocator_prepare_candidate_selection(function_name, block_index, policy,
      candidate, consumer_op, size, primary_physical_register, no_physical_register,
      spill_reason, no_spill_reason, unconditional_spill_reason,
      spill_boundary_instruction, &orchestration->preparation) == FAILED)
    goto dependency_failed;
  if (orchestration->preparation.proceed == NO) {
    fprintf(stderr, "register_allocator_core: candidate_selection_orchestrate function=%s block=%d temp=r%d producer=%d consumer=%d primary=%d proceed=no stage=preparation reason=spill_boundary status=complete\n",
        function_name, block_index, candidate->temp_index,
        candidate->producer_instruction, candidate->consumer_instruction,
        primary_physical_register);
    return SUCCEEDED;
  }

  failure_stage = "finalization";
  if (register_allocator_finalize_candidate_selection(function_name, block_index, policy,
      size, candidate_count, primary_physical_register, no_physical_register,
      candidate->temp_index, candidate->producer_instruction,
      candidate->consumer_instruction, candidate->has_single_read,
      orchestration->preparation.preservation_supported,
      orchestration->preparation.preservation_physical_register,
      evaluation_context, is_allowed, is_path_safe, is_active, dispatch_context,
      select_alternate, &orchestration->finalization) == FAILED)
    goto dependency_failed;

  orchestration->proceed = orchestration->finalization.proceed;
  fprintf(stderr, "register_allocator_core: candidate_selection_orchestrate function=%s block=%d temp=r%d producer=%d consumer=%d primary=%d proceed=%s selected=%d split=%d order=prepare_finalize status=complete\n",
      function_name, block_index, candidate->temp_index,
      candidate->producer_instruction, candidate->consumer_instruction,
      primary_physical_register, orchestration->proceed == YES ? "yes" : "no",
      orchestration->finalization.dispatch.physical_register,
      orchestration->finalization.split_action);
  return SUCCEEDED;

dependency_failed:
  _clear_candidate_selection_orchestration(orchestration, no_physical_register);
  fprintf(stderr, "register_allocator_core: candidate_selection_orchestrate function=%s block=%d temp=r%d producer=%d consumer=%d primary=%d proceed=no stage=%s status=dependency_failed\n",
      function_name, block_index, candidate->temp_index,
      candidate->producer_instruction, candidate->consumer_instruction,
      primary_physical_register, failure_stage);
  return FAILED;
}


int register_allocator_is_transparent_range(char *function_name, int start_instruction,
    int end_instruction, int instruction_count, int physical_register,
    int no_physical_register, void *context, register_allocator_path_predicate is_transparent,
    int *transparent, int *blocking_instruction) {

  int instruction_index;

  if (transparent != NULL)
    *transparent = NO;
  if (blocking_instruction != NULL)
    *blocking_instruction = -1;
  if (function_name == NULL || start_instruction < 0 || end_instruction <= start_instruction ||
      end_instruction >= instruction_count || physical_register == no_physical_register ||
      context == NULL || is_transparent == NULL || transparent == NULL ||
      blocking_instruction == NULL) {
    fprintf(stderr, "register_allocator_core: transparent_range function=%s start=%d end=%d instructions=%d phy=%d transparent=no blocking=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", start_instruction, end_instruction,
        instruction_count, physical_register);
    return FAILED;
  }

  for (instruction_index = start_instruction + 1; instruction_index < end_instruction;
      instruction_index++) {
    int result;

    result = is_transparent(context, instruction_index, physical_register);
    if (result != NO && result != YES) {
      fprintf(stderr, "register_allocator_core: transparent_range function=%s start=%d end=%d instructions=%d phy=%d transparent=no blocking=%d status=invalid_callback\n",
          function_name, start_instruction, end_instruction, instruction_count,
          physical_register, instruction_index);
      return FAILED;
    }
    if (result == NO) {
      *blocking_instruction = instruction_index;
      fprintf(stderr, "register_allocator_core: transparent_range function=%s start=%d end=%d instructions=%d phy=%d transparent=no blocking=%d reason=target_non_transparent status=complete\n",
          function_name, start_instruction, end_instruction, instruction_count,
          physical_register, instruction_index);
      return SUCCEEDED;
    }
  }

  *transparent = YES;
  fprintf(stderr, "register_allocator_core: transparent_range function=%s start=%d end=%d instructions=%d phy=%d transparent=yes blocking=-1 reason=all_transparent status=complete\n",
      function_name, start_instruction, end_instruction, instruction_count, physical_register);
  return SUCCEEDED;
}


int register_allocator_evaluate_candidate_path(char *function_name, int block_index,
    int start_instruction, int end_instruction, int instruction_count, int physical_register,
    int no_physical_register, void *context,
    register_allocator_path_predicate has_nearer_candidate,
    register_allocator_path_predicate has_preserving_overlap,
    register_allocator_path_predicate is_special_transparent,
    register_allocator_path_predicate is_target_transparent, int *path_safe,
    int *deciding_instruction) {

  int instruction_index;
  char *invalid_predicate;

  if (path_safe != NULL)
    *path_safe = NO;
  if (deciding_instruction != NULL)
    *deciding_instruction = -1;
  if (function_name == NULL || block_index < 0 || start_instruction < 0 ||
      end_instruction <= start_instruction || end_instruction >= instruction_count ||
      physical_register == no_physical_register || context == NULL ||
      has_nearer_candidate == NULL || has_preserving_overlap == NULL ||
      is_special_transparent == NULL || is_target_transparent == NULL || path_safe == NULL ||
      deciding_instruction == NULL) {
    fprintf(stderr, "register_allocator_core: candidate_path function=%s block=%d start=%d end=%d instructions=%d phy=%d safe=no deciding=-1 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index, start_instruction,
        end_instruction, instruction_count, physical_register);
    return FAILED;
  }

  for (instruction_index = start_instruction + 1; instruction_index < end_instruction;
      instruction_index++) {
    int nearer;
    int preserving_overlap;
    int special_transparent;
    int target_transparent;

    invalid_predicate = "nearer_candidate";
    nearer = has_nearer_candidate(context, instruction_index, physical_register);
    if (nearer != NO && nearer != YES)
      goto invalid_callback;
    if (nearer == YES) {
      *path_safe = YES;
      *deciding_instruction = instruction_index;
      fprintf(stderr, "register_allocator_core: candidate_path function=%s block=%d start=%d end=%d instructions=%d phy=%d safe=yes deciding=%d reason=nearer_candidate status=complete\n",
          function_name, block_index, start_instruction, end_instruction, instruction_count,
          physical_register, instruction_index);
      return SUCCEEDED;
    }

    invalid_predicate = "preserving_overlap";
    preserving_overlap = has_preserving_overlap(context, instruction_index, physical_register);
    if (preserving_overlap != NO && preserving_overlap != YES)
      goto invalid_callback;
    if (preserving_overlap == YES)
      continue;

    invalid_predicate = "special_transparency";
    special_transparent = is_special_transparent(context, instruction_index, physical_register);
    if (special_transparent != NO && special_transparent != YES)
      goto invalid_callback;
    if (special_transparent == YES)
      continue;

    invalid_predicate = "target_transparency";
    target_transparent = is_target_transparent(context, instruction_index, physical_register);
    if (target_transparent != NO && target_transparent != YES)
      goto invalid_callback;
    if (target_transparent == YES)
      continue;

    *deciding_instruction = instruction_index;
    fprintf(stderr, "register_allocator_core: candidate_path function=%s block=%d start=%d end=%d instructions=%d phy=%d safe=no deciding=%d reason=non_transparent status=complete\n",
        function_name, block_index, start_instruction, end_instruction, instruction_count,
        physical_register, instruction_index);
    return SUCCEEDED;

invalid_callback:
  fprintf(stderr, "register_allocator_core: candidate_path function=%s block=%d start=%d end=%d instructions=%d phy=%d safe=no deciding=%d invalid_predicate=%s status=invalid_callback\n",
        function_name, block_index, start_instruction, end_instruction, instruction_count,
    physical_register, instruction_index, invalid_predicate);
    return FAILED;
  }

  *path_safe = YES;
  fprintf(stderr, "register_allocator_core: candidate_path function=%s block=%d start=%d end=%d instructions=%d phy=%d safe=yes deciding=-1 reason=all_transparent status=complete\n",
      function_name, block_index, start_instruction, end_instruction, instruction_count,
      physical_register);
  return SUCCEEDED;
}


int register_allocator_plan_candidate_fallback(char *function_name, int block_index, int primary_allowed, int primary_path_safe, int primary_active, int fallback_unsupported, int fallback_path, int fallback_conflict) {

  int plan;
  char *reason;

  if (function_name == NULL || block_index < 0 ||
      (primary_allowed != NO && primary_allowed != YES) ||
      (primary_path_safe != NO && primary_path_safe != YES) ||
      (primary_active != NO && primary_active != YES) ||
      (fallback_unsupported != NO && fallback_unsupported != YES) ||
      (fallback_path != NO && fallback_path != YES) ||
      (fallback_conflict != NO && fallback_conflict != YES)) {
    fprintf(stderr, "register_allocator_core: candidate_fallback_plan function=%s block=%d allowed=%s path_safe=%s active=%s fallback_unsupported=%s fallback_path=%s fallback_conflict=%s status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        primary_allowed == YES ? "yes" : (primary_allowed == NO ? "no" : "invalid"),
        primary_path_safe == YES ? "yes" : (primary_path_safe == NO ? "no" : "invalid"),
        primary_active == YES ? "yes" : (primary_active == NO ? "no" : "invalid"),
        fallback_unsupported == YES ? "yes" : (fallback_unsupported == NO ? "no" : "invalid"),
        fallback_path == YES ? "yes" : (fallback_path == NO ? "no" : "invalid"),
        fallback_conflict == YES ? "yes" : (fallback_conflict == NO ? "no" : "invalid"));
    return FAILED;
  }

  if (primary_allowed == NO) {
    plan = fallback_unsupported == YES ? RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED : RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED;
    reason = fallback_unsupported == YES ? "fallback_unsupported" : "reject_unsupported";
  }
  else if (primary_path_safe == NO) {
    plan = fallback_path == YES ? RA_CANDIDATE_PLAN_FALLBACK_PATH : RA_CANDIDATE_PLAN_REJECT_PATH;
    reason = fallback_path == YES ? "fallback_path" : "reject_path";
  }
  else if (primary_active == YES && fallback_conflict == YES) {
    plan = RA_CANDIDATE_PLAN_FALLBACK_CONFLICT;
    reason = "fallback_conflict";
  }
  else {
    plan = RA_CANDIDATE_PLAN_USE_PRIMARY;
    reason = primary_active == YES ? "primary_conflict_deferred" : "primary_available";
  }

  fprintf(stderr, "register_allocator_core: candidate_fallback_plan function=%s block=%d allowed=%s path_safe=%s active=%s fallback_unsupported=%s fallback_path=%s fallback_conflict=%s plan=%d reason=%s status=complete\n",
      function_name, block_index, primary_allowed == YES ? "yes" : "no",
      primary_path_safe == YES ? "yes" : "no", primary_active == YES ? "yes" : "no",
      fallback_unsupported == YES ? "yes" : "no", fallback_path == YES ? "yes" : "no",
      fallback_conflict == YES ? "yes" : "no", plan, reason);
  return plan;
}


int register_allocator_evaluate_loop_register_paths(char *function_name,
    int block_index, int temp_index, int no_physical_register,
    int *candidate_registers, int candidate_count, void *context,
    register_allocator_candidate_predicate is_path_safe,
    struct register_allocator_loop_register_path_evaluation *evaluations,
    int evaluation_capacity) {

  int candidate_index;
  int path_safe_count;

  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      candidate_registers == NULL || candidate_count <= 0 || context == NULL ||
      is_path_safe == NULL || evaluations == NULL ||
      evaluation_capacity < candidate_count) {
    fprintf(stderr, "register_allocator_core: loop_register_path_evaluation function=%s block=%d temp=r%d candidates=%d capacity=%d safe=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, candidate_count, evaluation_capacity);
    return FAILED;
  }
  path_safe_count = 0;
  for (candidate_index = 0; candidate_index < candidate_count;
      candidate_index++) {
    int path_safe;

    if (candidate_registers[candidate_index] == no_physical_register) {
      fprintf(stderr, "register_allocator_core: loop_register_path_candidate function=%s block=%d temp=r%d candidate=%d phy=%d status=invalid_candidate\n",
          function_name, block_index, temp_index, candidate_index,
          candidate_registers[candidate_index]);
      return FAILED;
    }
    path_safe = is_path_safe(context, candidate_registers[candidate_index]);
    if (path_safe != NO && path_safe != YES) {
      fprintf(stderr, "register_allocator_core: loop_register_path_candidate function=%s block=%d temp=r%d candidate=%d phy=%d path_safe=no status=invalid_callback\n",
          function_name, block_index, temp_index, candidate_index,
          candidate_registers[candidate_index]);
      return FAILED;
    }
    evaluations[candidate_index].physical_register =
        candidate_registers[candidate_index];
    evaluations[candidate_index].path_safe = path_safe;
    if (path_safe == YES)
      path_safe_count++;
    fprintf(stderr, "register_allocator_core: loop_register_path_candidate function=%s block=%d temp=r%d candidate=%d phy=%d path_safe=%s status=complete\n",
        function_name, block_index, temp_index, candidate_index,
        candidate_registers[candidate_index], path_safe == YES ? "yes" : "no");
  }
  fprintf(stderr, "register_allocator_core: loop_register_path_evaluation function=%s block=%d temp=r%d candidates=%d capacity=%d safe=%d status=complete\n",
      function_name, block_index, temp_index, candidate_count,
      evaluation_capacity, path_safe_count);
  return SUCCEEDED;
}


int register_allocator_prepare_loop_register_exclusions(char *function_name,
    int block_index, int temp_index, int no_physical_register,
    int *candidate_registers, int candidate_count,
    struct register_allocator_loop_register_exclusion *exclusions,
    int exclusion_capacity) {

  int candidate_index;

  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      candidate_registers == NULL || candidate_count <= 0 ||
      exclusions == NULL || exclusion_capacity < candidate_count) {
    fprintf(stderr, "register_allocator_core: loop_register_exclusions function=%s block=%d temp=r%d candidates=%d capacity=%d excluded=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, candidate_count, exclusion_capacity);
    return FAILED;
  }
  for (candidate_index = 0; candidate_index < candidate_count;
      candidate_index++) {
    if (candidate_registers[candidate_index] == no_physical_register) {
      fprintf(stderr, "register_allocator_core: loop_register_exclusion_candidate function=%s block=%d temp=r%d candidate=%d phy=%d status=invalid_candidate\n",
          function_name, block_index, temp_index, candidate_index,
          candidate_registers[candidate_index]);
      return FAILED;
    }
    exclusions[candidate_index].physical_register =
        candidate_registers[candidate_index];
    exclusions[candidate_index].excluded = NO;
    exclusions[candidate_index].reason = RA_LOOP_REGISTER_EXCLUSION_NONE;
    exclusions[candidate_index].blocking_block = -1;
    exclusions[candidate_index].blocking_instruction = -1;
  }
  fprintf(stderr, "register_allocator_core: loop_register_exclusions function=%s block=%d temp=r%d candidates=%d capacity=%d excluded=0 status=complete\n",
      function_name, block_index, temp_index, candidate_count,
      exclusion_capacity);
  return SUCCEEDED;
}


int register_allocator_exclude_loop_register_candidate(char *function_name,
    int block_index, int temp_index, int candidate_index, int reason,
    int blocking_block, int blocking_instruction,
    struct register_allocator_loop_register_exclusion *exclusions,
    int exclusion_count) {

  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      candidate_index < 0 || candidate_index >= exclusion_count ||
      reason != RA_LOOP_REGISTER_EXCLUSION_RESERVATION ||
      blocking_block < 0 || blocking_instruction < 0 || exclusions == NULL) {
    fprintf(stderr, "register_allocator_core: loop_register_exclusion function=%s block=%d temp=r%d candidate=%d reason=%d blocking_block=%d blocking_instruction=%d exclusions=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, candidate_index, reason, blocking_block,
        blocking_instruction, exclusion_count);
    return FAILED;
  }
  if (exclusions[candidate_index].excluded != NO ||
      exclusions[candidate_index].reason != RA_LOOP_REGISTER_EXCLUSION_NONE ||
      exclusions[candidate_index].blocking_block != -1 ||
      exclusions[candidate_index].blocking_instruction != -1) {
    fprintf(stderr, "register_allocator_core: loop_register_exclusion function=%s block=%d temp=r%d candidate=%d phy=%d reason=%d blocking_block=%d blocking_instruction=%d status=duplicate_exclusion\n",
        function_name, block_index, temp_index, candidate_index,
        exclusions[candidate_index].physical_register, reason,
        blocking_block, blocking_instruction);
    return FAILED;
  }
  exclusions[candidate_index].excluded = YES;
  exclusions[candidate_index].reason = reason;
  exclusions[candidate_index].blocking_block = blocking_block;
  exclusions[candidate_index].blocking_instruction = blocking_instruction;
  fprintf(stderr, "register_allocator_core: loop_register_exclusion function=%s block=%d temp=r%d candidate=%d phy=%d reason=reservation blocking_block=%d blocking_instruction=%d status=complete\n",
      function_name, block_index, temp_index, candidate_index,
      exclusions[candidate_index].physical_register, blocking_block,
      blocking_instruction);
  return SUCCEEDED;
}


static int _register_allocator_select_loop_retention_register(
  char *function_name,
    int block_index, int temp_index, int no_physical_register,
    int preferred_physical_register, int *candidate_registers,
    int candidate_count, struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_loop_register_path_evaluation *path_evaluations,
    int path_evaluation_count,
  struct register_allocator_loop_register_exclusion *exclusions,
  int exclusion_count,
    struct register_allocator_loop_register_selection *selection) {

  int assignment_index;
  int bound_assignment_count;
  int candidate_index;
  int compatible_candidate_count;
  int unexcluded_candidate_count;
  int preferred_index;

  if (selection != NULL) {
    selection->status = RA_LOOP_REGISTER_SELECTION_INELIGIBLE;
    selection->physical_register = no_physical_register;
    selection->candidate_index = -1;
    selection->bound_assignment_count = 0;
    selection->conflict_assignment = -1;
  }
  if (function_name == NULL || block_index < 0 || temp_index < 0 ||
      preferred_physical_register == no_physical_register ||
      candidate_registers == NULL || candidate_count <= 0 ||
      assignments == NULL || assignment_count <= 0 ||
      path_evaluations == NULL || path_evaluation_count != candidate_count ||
      ((exclusions == NULL && exclusion_count != 0) ||
      (exclusions != NULL && exclusion_count != candidate_count)) ||
      selection == NULL) {
    fprintf(stderr, "register_allocator_core: loop_register_selection function=%s block=%d temp=r%d preferred=%d candidates=%d assignments=%d paths=%d selected=-1 phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, preferred_physical_register, candidate_count,
        assignment_count, path_evaluation_count, no_physical_register);
    return FAILED;
  }
  preferred_index = -1;
  for (candidate_index = 0; candidate_index < candidate_count;
      candidate_index++) {
    int previous_candidate;

    if (candidate_registers[candidate_index] == no_physical_register) {
      fprintf(stderr, "register_allocator_core: loop_register_candidate function=%s block=%d temp=r%d candidate=%d phy=%d status=invalid_candidate\n",
          function_name, block_index, temp_index, candidate_index,
          candidate_registers[candidate_index]);
      return FAILED;
    }
    if (path_evaluations[candidate_index].physical_register !=
        candidate_registers[candidate_index] ||
        (path_evaluations[candidate_index].path_safe != NO &&
        path_evaluations[candidate_index].path_safe != YES)) {
      fprintf(stderr, "register_allocator_core: loop_register_path_candidate function=%s block=%d temp=r%d candidate=%d phy=%d evaluated_phy=%d path_safe=%d status=invalid_evaluation\n",
          function_name, block_index, temp_index, candidate_index,
          candidate_registers[candidate_index],
          path_evaluations[candidate_index].physical_register,
          path_evaluations[candidate_index].path_safe);
      return FAILED;
    }
    if (exclusions != NULL &&
        (exclusions[candidate_index].physical_register !=
        candidate_registers[candidate_index] ||
        (exclusions[candidate_index].excluded != NO &&
        exclusions[candidate_index].excluded != YES) ||
        (exclusions[candidate_index].excluded == NO &&
        (exclusions[candidate_index].reason !=
        RA_LOOP_REGISTER_EXCLUSION_NONE ||
        exclusions[candidate_index].blocking_block != -1 ||
        exclusions[candidate_index].blocking_instruction != -1)) ||
        (exclusions[candidate_index].excluded == YES &&
        (exclusions[candidate_index].reason !=
        RA_LOOP_REGISTER_EXCLUSION_RESERVATION ||
        exclusions[candidate_index].blocking_block < 0 ||
        exclusions[candidate_index].blocking_instruction < 0)))) {
      fprintf(stderr, "register_allocator_core: loop_register_exclusion_candidate function=%s block=%d temp=r%d candidate=%d phy=%d evaluated_phy=%d excluded=%d reason=%d blocking_block=%d blocking_instruction=%d status=invalid_exclusion\n",
          function_name, block_index, temp_index, candidate_index,
          candidate_registers[candidate_index],
          exclusions[candidate_index].physical_register,
          exclusions[candidate_index].excluded,
          exclusions[candidate_index].reason,
          exclusions[candidate_index].blocking_block,
          exclusions[candidate_index].blocking_instruction);
      return FAILED;
    }
    for (previous_candidate = 0; previous_candidate < candidate_index;
        previous_candidate++) {
      if (candidate_registers[previous_candidate] ==
          candidate_registers[candidate_index]) {
        fprintf(stderr, "register_allocator_core: loop_register_candidate function=%s block=%d temp=r%d candidate=%d phy=%d duplicate=%d status=invalid_candidate\n",
            function_name, block_index, temp_index, candidate_index,
            candidate_registers[candidate_index], previous_candidate);
        return FAILED;
      }
    }
    if (candidate_registers[candidate_index] == preferred_physical_register)
      preferred_index = candidate_index;
  }
  if (preferred_index < 0) {
    fprintf(stderr, "register_allocator_core: loop_register_selection function=%s block=%d temp=r%d preferred=%d candidates=%d assignments=%d selected=-1 phy=%d status=invalid_preference\n",
        function_name, block_index, temp_index, preferred_physical_register,
        candidate_count, assignment_count, no_physical_register);
    return FAILED;
  }
  bound_assignment_count = 0;
  for (assignment_index = 0; assignment_index < assignment_count;
      assignment_index++) {
    if ((assignments[assignment_index].role != RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignments[assignment_index].role != RA_JOIN_ASSIGNMENT_CONSUMER) ||
        assignments[assignment_index].instruction < 0 ||
        (assignments[assignment_index].role == RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignments[assignment_index].operand != 0) ||
        (assignments[assignment_index].role == RA_JOIN_ASSIGNMENT_CONSUMER &&
        _is_valid_tac_use_operand(assignments[assignment_index].operand) == NO) ||
        (assignments[assignment_index].physical_register < 0 &&
        assignments[assignment_index].physical_register !=
        no_physical_register)) {
      fprintf(stderr, "register_allocator_core: loop_register_assignment function=%s block=%d temp=r%d assignment=%d role=%d instruction=%d operand=%d phy=%d status=invalid_assignment\n",
          function_name, block_index, temp_index, assignment_index,
          assignments[assignment_index].role,
          assignments[assignment_index].instruction,
          assignments[assignment_index].operand,
          assignments[assignment_index].physical_register);
      return FAILED;
    }
    if (assignments[assignment_index].physical_register !=
        no_physical_register)
      bound_assignment_count++;
  }
  compatible_candidate_count = 0;
  unexcluded_candidate_count = 0;
  for (candidate_index = -1; candidate_index < candidate_count;
      candidate_index++) {
    int actual_candidate_index;
    int conflict_assignment;
    int physical_register;

    actual_candidate_index = candidate_index < 0 ? preferred_index :
        candidate_index;
    if (candidate_index >= 0 && actual_candidate_index == preferred_index)
      continue;
    physical_register = candidate_registers[actual_candidate_index];
    if (exclusions != NULL &&
        exclusions[actual_candidate_index].excluded == YES) {
      fprintf(stderr, "register_allocator_core: loop_register_candidate function=%s block=%d temp=r%d candidate=%d phy=%d preferred=%s excluded=yes reason=reservation blocking_block=%d blocking_instruction=%d status=complete\n",
          function_name, block_index, temp_index, actual_candidate_index,
          physical_register,
          actual_candidate_index == preferred_index ? "yes" : "no",
          exclusions[actual_candidate_index].blocking_block,
          exclusions[actual_candidate_index].blocking_instruction);
      continue;
    }
    unexcluded_candidate_count++;
    conflict_assignment = -1;
    for (assignment_index = 0; assignment_index < assignment_count;
        assignment_index++) {
      if (assignments[assignment_index].physical_register !=
          no_physical_register &&
          assignments[assignment_index].physical_register !=
          physical_register) {
        conflict_assignment = assignment_index;
        break;
      }
    }
    if (conflict_assignment < 0)
      compatible_candidate_count++;
    fprintf(stderr, "register_allocator_core: loop_register_candidate function=%s block=%d temp=r%d candidate=%d phy=%d preferred=%s bound=%d compatible=%s path_safe=%s conflict_assignment=%d status=complete\n",
        function_name, block_index, temp_index, actual_candidate_index,
        physical_register,
        actual_candidate_index == preferred_index ? "yes" : "no",
        bound_assignment_count, conflict_assignment < 0 ? "yes" : "no",
      path_evaluations[actual_candidate_index].path_safe == YES ?
      "yes" : "no",
        conflict_assignment);
    if (conflict_assignment < 0 &&
      path_evaluations[actual_candidate_index].path_safe == YES) {
      selection->status = RA_LOOP_REGISTER_SELECTION_READY;
      selection->physical_register = physical_register;
      selection->candidate_index = actual_candidate_index;
      selection->bound_assignment_count = bound_assignment_count;
      fprintf(stderr, "register_allocator_core: loop_register_selection function=%s block=%d temp=r%d preferred=%d candidates=%d assignments=%d bound=%d selected=%d phy=%d selection=ready status=complete\n",
          function_name, block_index, temp_index,
          preferred_physical_register, candidate_count, assignment_count,
          bound_assignment_count, actual_candidate_index, physical_register);
      return SUCCEEDED;
    }
    if (selection->conflict_assignment < 0)
      selection->conflict_assignment = conflict_assignment;
  }
  selection->bound_assignment_count = bound_assignment_count;
    fprintf(stderr, "register_allocator_core: loop_register_selection function=%s block=%d temp=r%d preferred=%d candidates=%d assignments=%d bound=%d selected=-1 phy=%d conflict_assignment=%d selection=ineligible reason=%s status=complete\n",
      function_name, block_index, temp_index, preferred_physical_register,
      candidate_count, assignment_count, bound_assignment_count,
      no_physical_register, selection->conflict_assignment,
      compatible_candidate_count > 0 ? "no_path_safe_register" :
      (unexcluded_candidate_count == 0 ? "all_candidates_excluded" :
      "no_common_register"));
  return SUCCEEDED;
}


int register_allocator_select_loop_retention_register(char *function_name,
    int block_index, int temp_index, int no_physical_register,
    int preferred_physical_register, int *candidate_registers,
    int candidate_count, struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_loop_register_path_evaluation *path_evaluations,
    int path_evaluation_count,
    struct register_allocator_loop_register_selection *selection) {

  return _register_allocator_select_loop_retention_register(function_name,
      block_index, temp_index, no_physical_register,
      preferred_physical_register, candidate_registers, candidate_count,
      assignments, assignment_count, path_evaluations, path_evaluation_count,
      NULL, 0, selection);
}


int register_allocator_select_loop_retention_register_with_exclusions(
    char *function_name, int block_index, int temp_index,
    int no_physical_register, int preferred_physical_register,
    int *candidate_registers, int candidate_count,
    struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_loop_register_path_evaluation *path_evaluations,
    int path_evaluation_count,
    struct register_allocator_loop_register_exclusion *exclusions,
    int exclusion_count,
    struct register_allocator_loop_register_selection *selection) {

  if (exclusions == NULL) {
    fprintf(stderr, "register_allocator_core: loop_register_selection function=%s block=%d temp=r%d preferred=%d candidates=%d assignments=%d paths=%d exclusions=%d selected=-1 phy=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", block_index,
        temp_index, preferred_physical_register, candidate_count,
        assignment_count, path_evaluation_count, exclusion_count,
        no_physical_register);
    return FAILED;
  }
  return _register_allocator_select_loop_retention_register(function_name,
      block_index, temp_index, no_physical_register,
      preferred_physical_register, candidate_registers, candidate_count,
      assignments, assignment_count, path_evaluations, path_evaluation_count,
      exclusions, exclusion_count, selection);
}


int register_allocator_plan_multi_latch_retention(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int join_block_index, int temp_index, int slot_index,
    int no_physical_register, int selected_physical_register,
    int entry_physical_register, int latch_physical_register,
    int consumer_physical_register,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_loop_definition *definition,
    struct register_allocator_join_assignment *supplemental_producers,
    int supplemental_producer_count,
    struct register_allocator_join_assignment *supplemental_consumers,
    int supplemental_consumer_count,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_capacity,
    struct register_allocator_loop_retention_plan *plan) {

  struct register_allocator_join_schedule schedule;
  struct register_allocator_join_path_reservation path_reservation;
  struct register_allocator_join_assignment *staged_assignments;
  struct register_allocator_join_block_reservation *staged_reservations;
  int assignment_count;
  int assignment_index;

  staged_assignments = NULL;
  staged_reservations = NULL;

  if (plan != NULL) {
    plan->status = RA_LOOP_RETENTION_INELIGIBLE;
    plan->assignment_count = 0;
    plan->reservation_count = 0;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || edge_count < 0 || (edge_count > 0 && edges == NULL) ||
      join_block_index < 0 || join_block_index >= block_count || temp_index < 0 ||
      slot_index < 0 || loop_join == NULL || definition == NULL ||
      supplemental_producer_count < 0 || supplemental_consumer_count < 0 ||
      assignment_capacity < 0 ||
      reservation_capacity < 0 || plan == NULL ||
      (supplemental_producer_count > 0 && supplemental_producers == NULL) ||
      (supplemental_consumer_count > 0 && supplemental_consumers == NULL) ||
      (assignment_capacity > 0 && assignments == NULL) ||
      (reservation_capacity > 0 && reservations == NULL)) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", join_block_index,
        temp_index, slot_index);
    return FAILED;
  }
  if (definition->status != RA_LOOP_DEFINITION_READY &&
      definition->status != RA_LOOP_DEFINITION_INELIGIBLE) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=invalid_definition\n",
        function_name, join_block_index, temp_index, slot_index);
    return FAILED;
  }
  if (definition->status == RA_LOOP_DEFINITION_INELIGIBLE) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 plan=ineligible reason=definition status=complete\n",
        function_name, join_block_index, temp_index, slot_index);
    return SUCCEEDED;
  }
  if (supplemental_producer_count > INT_MAX - 3 ||
      supplemental_consumer_count >
      INT_MAX - 3 - supplemental_producer_count) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=invalid_input\n",
        function_name, join_block_index, temp_index, slot_index);
    return FAILED;
  }
  assignment_count = 3 + supplemental_producer_count +
      supplemental_consumer_count;
  if (assignment_capacity < assignment_count || reservation_capacity <= 0) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=insufficient_capacity\n",
        function_name, join_block_index, temp_index, slot_index);
    return FAILED;
  }
  if ((size_t)assignment_count > ((size_t)-1) /
      sizeof(struct register_allocator_join_assignment)) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=invalid_input\n",
        function_name, join_block_index, temp_index, slot_index);
    return FAILED;
  }
  if ((size_t)reservation_capacity > ((size_t)-1) /
      sizeof(struct register_allocator_join_block_reservation)) {
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=invalid_input\n",
        function_name, join_block_index, temp_index, slot_index);
    return FAILED;
  }
  staged_assignments = (struct register_allocator_join_assignment *)malloc(
      sizeof(struct register_allocator_join_assignment) *
      (size_t)assignment_count);
  staged_reservations = (struct register_allocator_join_block_reservation *)malloc(
      sizeof(struct register_allocator_join_block_reservation) *
      (size_t)reservation_capacity);
  if (staged_assignments == NULL || staged_reservations == NULL) {
    free(staged_assignments);
    free(staged_reservations);
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 status=out_of_memory\n",
        function_name, join_block_index, temp_index, slot_index);
    return FAILED;
  }
  if (register_allocator_plan_loop_join_schedule(function_name,
      instruction_count, temp_index, no_physical_register, loop_join,
      definition->entry_definition, entry_physical_register,
      definition->latch_definition, latch_physical_register,
      definition->consumer_instruction, definition->consumer_operand,
      consumer_physical_register, selected_physical_register,
      staged_assignments, 3, &schedule) == FAILED) {
    free(staged_assignments);
    free(staged_reservations);
    return FAILED;
  }
  if (schedule.status != RA_JOIN_SCHEDULE_READY) {
    free(staged_assignments);
    free(staged_reservations);
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 plan=ineligible reason=schedule status=complete\n",
        function_name, join_block_index, temp_index, slot_index);
    return SUCCEEDED;
  }
  staged_assignments[2 + supplemental_producer_count] =
      staged_assignments[2];
  for (assignment_index = 0;
      assignment_index < supplemental_producer_count; assignment_index++) {
    struct register_allocator_join_assignment *supplemental;
    int previous_assignment;

    supplemental = &supplemental_producers[assignment_index];
    if (supplemental->role != RA_JOIN_ASSIGNMENT_PRODUCER ||
        supplemental->instruction < 0 ||
        supplemental->instruction >= instruction_count ||
        supplemental->operand != 0 ||
        (supplemental->physical_register != no_physical_register &&
        supplemental->physical_register != selected_physical_register)) {
      free(staged_assignments);
      free(staged_reservations);
      fprintf(stderr, "register_allocator_core: loop_retention_plan_assignment function=%s temp=r%d assignment=%d status=invalid_assignment\n",
          function_name, temp_index, assignment_index + 2);
      return FAILED;
    }
    for (previous_assignment = 0; previous_assignment < assignment_index + 2;
        previous_assignment++) {
      if (staged_assignments[previous_assignment].instruction ==
          supplemental->instruction &&
          staged_assignments[previous_assignment].operand ==
          supplemental->operand) {
        free(staged_assignments);
        free(staged_reservations);
        fprintf(stderr, "register_allocator_core: loop_retention_plan_assignment function=%s temp=r%d assignment=%d duplicate=%d status=invalid_assignment\n",
            function_name, temp_index, assignment_index + 2,
            previous_assignment);
        return FAILED;
      }
    }
    staged_assignments[assignment_index + 2] = *supplemental;
    staged_assignments[assignment_index + 2].physical_register =
        selected_physical_register;
    fprintf(stderr, "register_allocator_core: loop_retention_plan_assignment function=%s temp=r%d assignment=%d role=supplemental_producer instruction=%d operand=0 phy=%d status=complete\n",
        function_name, temp_index, assignment_index + 2,
        supplemental->instruction, selected_physical_register);
  }
  for (assignment_index = 0;
      assignment_index < supplemental_consumer_count; assignment_index++) {
    struct register_allocator_join_assignment *supplemental;
    int previous_assignment;

    supplemental = &supplemental_consumers[assignment_index];
    if (supplemental->role != RA_JOIN_ASSIGNMENT_CONSUMER ||
        supplemental->instruction < 0 ||
        supplemental->instruction >= instruction_count ||
        _is_valid_tac_use_operand(supplemental->operand) == NO ||
        (supplemental->physical_register != no_physical_register &&
        supplemental->physical_register != selected_physical_register)) {
      free(staged_assignments);
      free(staged_reservations);
        fprintf(stderr, "register_allocator_core: loop_retention_plan_assignment function=%s temp=r%d assignment=%d status=invalid_assignment\n",
          function_name, temp_index,
          assignment_index + 3 + supplemental_producer_count);
      return FAILED;
    }
      for (previous_assignment = 0; previous_assignment <
        assignment_index + 3 + supplemental_producer_count;
        previous_assignment++) {
      if (staged_assignments[previous_assignment].instruction ==
          supplemental->instruction &&
          staged_assignments[previous_assignment].operand ==
          supplemental->operand) {
        free(staged_assignments);
        free(staged_reservations);
        fprintf(stderr, "register_allocator_core: loop_retention_plan_assignment function=%s temp=r%d assignment=%d duplicate=%d status=invalid_assignment\n",
            function_name, temp_index,
            assignment_index + 3 + supplemental_producer_count,
            previous_assignment);
        return FAILED;
      }
    }
        staged_assignments[assignment_index + 3 + supplemental_producer_count] =
          *supplemental;
        staged_assignments[assignment_index + 3 +
          supplemental_producer_count].physical_register =
        selected_physical_register;
    fprintf(stderr, "register_allocator_core: loop_retention_plan_assignment function=%s temp=r%d assignment=%d role=supplemental_consumer instruction=%d operand=%d phy=%d status=complete\n",
          function_name, temp_index,
          assignment_index + 3 + supplemental_producer_count,
        supplemental->instruction, supplemental->operand,
        selected_physical_register);
  }
  schedule.assignment_count = assignment_count;
  if (register_allocator_plan_loop_join_path_reservations(function_name,
      block_count, instruction_count, blocks, edges, edge_count,
      join_block_index, temp_index, slot_index, loop_join, staged_assignments,
      assignment_count, &schedule, staged_reservations, reservation_capacity,
      &path_reservation) == FAILED) {
    free(staged_assignments);
    free(staged_reservations);
    return FAILED;
  }
  if (path_reservation.status != RA_JOIN_PATH_RESERVATION_READY) {
    free(staged_assignments);
    free(staged_reservations);
    fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=0 reservations=0 plan=ineligible reason=reservation status=complete\n",
        function_name, join_block_index, temp_index, slot_index);
    return SUCCEEDED;
  }
  plan->status = RA_LOOP_RETENTION_READY;
  plan->assignment_count = schedule.assignment_count;
  plan->reservation_count = path_reservation.block_count;
  memcpy(assignments, staged_assignments,
      sizeof(struct register_allocator_join_assignment) *
      (size_t)assignment_count);
  memcpy(reservations, staged_reservations,
      sizeof(struct register_allocator_join_block_reservation) *
      (size_t)plan->reservation_count);
  free(staged_assignments);
  free(staged_reservations);
  fprintf(stderr, "register_allocator_core: loop_retention_plan function=%s block=%d temp=r%d slot=%d assignments=%d reservations=%d plan=ready status=complete\n",
      function_name, join_block_index, temp_index, slot_index,
      plan->assignment_count, plan->reservation_count);
  return SUCCEEDED;
}


int register_allocator_plan_loop_retention(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int join_block_index, int temp_index, int slot_index,
    int no_physical_register, int selected_physical_register,
    int entry_physical_register, int latch_physical_register,
    int consumer_physical_register,
    struct register_allocator_loop_join *loop_join,
    struct register_allocator_loop_definition *definition,
    struct register_allocator_join_assignment *supplemental_consumers,
    int supplemental_consumer_count,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_capacity,
    struct register_allocator_loop_retention_plan *plan) {

  return register_allocator_plan_multi_latch_retention(function_name,
      block_count, instruction_count, blocks, edges, edge_count,
      join_block_index, temp_index, slot_index, no_physical_register,
      selected_physical_register, entry_physical_register,
      latch_physical_register, consumer_physical_register, loop_join,
      definition, NULL, 0, supplemental_consumers,
      supplemental_consumer_count, assignments, assignment_capacity,
      reservations, reservation_capacity, plan);
}


int register_allocator_apply_loop_retention(char *function_name,
    int temp_index, int slot_index,
    struct register_allocator_join_assignment *assignments,
    int assignment_capacity,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_capacity,
    struct register_allocator_loop_retention_plan *plan, void *context,
    register_allocator_loop_retention_transaction_applier apply_transaction,
    struct register_allocator_loop_retention_application *application) {

  int assignment_index;
  int producer_count;
  int reservation_index;
  int result;

  if (application != NULL) {
    application->status = RA_LOOP_APPLICATION_INELIGIBLE;
    application->assignment_count = 0;
    application->reservation_count = 0;
  }
  if (function_name == NULL || temp_index < 0 || slot_index < 0 ||
      assignment_capacity < 0 || reservation_capacity < 0 || plan == NULL ||
      application == NULL || context == NULL || apply_transaction == NULL ||
      (assignment_capacity > 0 && assignments == NULL) ||
      (reservation_capacity > 0 && reservations == NULL)) {
    fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=0 reservations=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        slot_index);
    return FAILED;
  }
  if (plan->status == RA_LOOP_RETENTION_INELIGIBLE &&
      plan->assignment_count == 0 && plan->reservation_count == 0) {
    fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=0 reservations=0 application=ineligible status=complete\n",
        function_name, temp_index, slot_index);
    return SUCCEEDED;
  }
  if (plan->status != RA_LOOP_RETENTION_READY || plan->assignment_count < 3 ||
      plan->reservation_count <= 0 ||
      plan->assignment_count > assignment_capacity ||
      plan->reservation_count > reservation_capacity || assignments == NULL ||
      reservations == NULL) {
    fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=%d reservations=%d status=invalid_plan\n",
        function_name, temp_index, slot_index, plan->assignment_count,
        plan->reservation_count);
    return FAILED;
  }
  producer_count = 0;
  while (producer_count < plan->assignment_count &&
      assignments[producer_count].role == RA_JOIN_ASSIGNMENT_PRODUCER)
    producer_count++;
  if (producer_count < 2 || producer_count >= plan->assignment_count) {
    fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=%d reservations=%d status=invalid_plan\n",
        function_name, temp_index, slot_index, plan->assignment_count,
        plan->reservation_count);
    return FAILED;
  }
  for (assignment_index = 0; assignment_index < plan->assignment_count;
      assignment_index++) {
    int expected_role;
    int previous_assignment;

    expected_role = assignment_index < producer_count ?
      RA_JOIN_ASSIGNMENT_PRODUCER :
        RA_JOIN_ASSIGNMENT_CONSUMER;
    if (assignments[assignment_index].role != expected_role ||
        assignments[assignment_index].instruction < 0 ||
      (expected_role == RA_JOIN_ASSIGNMENT_PRODUCER &&
      assignments[assignment_index].operand != 0) ||
      (expected_role == RA_JOIN_ASSIGNMENT_CONSUMER &&
      _is_valid_tac_use_operand(assignments[assignment_index].operand) == NO) ||
        assignments[assignment_index].physical_register < 0 ||
        (assignment_index > 0 &&
        assignments[assignment_index].physical_register !=
        assignments[0].physical_register)) {
      fprintf(stderr, "register_allocator_core: loop_retention_apply_assignment function=%s temp=r%d assignment=%d status=invalid_assignment\n",
          function_name, temp_index, assignment_index);
      return FAILED;
    }
    for (previous_assignment = 0;
        previous_assignment < assignment_index; previous_assignment++) {
      if (assignments[previous_assignment].instruction ==
          assignments[assignment_index].instruction &&
          assignments[previous_assignment].operand ==
          assignments[assignment_index].operand) {
        fprintf(stderr, "register_allocator_core: loop_retention_apply_assignment function=%s temp=r%d assignment=%d duplicate=%d status=invalid_assignment\n",
            function_name, temp_index, assignment_index,
            previous_assignment);
        return FAILED;
      }
    }
  }
  for (reservation_index = 0; reservation_index < plan->reservation_count;
      reservation_index++) {
    if (reservations[reservation_index].block_index < 0 ||
        reservations[reservation_index].start_instruction < 0 ||
        reservations[reservation_index].end_instruction <
        reservations[reservation_index].start_instruction ||
        (reservation_index > 0 &&
        reservations[reservation_index].block_index <=
        reservations[reservation_index - 1].block_index)) {
      fprintf(stderr, "register_allocator_core: loop_retention_apply_reservation function=%s temp=r%d reservation=%d status=invalid_reservation\n",
          function_name, temp_index, reservation_index);
      return FAILED;
    }
  }
  result = apply_transaction(context, temp_index, slot_index, assignments,
      plan->assignment_count, reservations, plan->reservation_count);
  if (result != SUCCEEDED && result != FAILED) {
    fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=%d reservations=%d status=invalid_callback\n",
        function_name, temp_index, slot_index, plan->assignment_count,
        plan->reservation_count);
    return FAILED;
  }
  if (result == FAILED) {
    application->status = RA_LOOP_APPLICATION_FAILED;
    fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=0 reservations=0 application=failed atomic=yes status=complete\n",
        function_name, temp_index, slot_index);
    return SUCCEEDED;
  }
  application->status = RA_LOOP_APPLICATION_APPLIED;
  application->assignment_count = plan->assignment_count;
  application->reservation_count = plan->reservation_count;
  fprintf(stderr, "register_allocator_core: loop_retention_apply function=%s temp=r%d slot=%d assignments=%d reservations=%d application=applied atomic=yes status=complete\n",
      function_name, temp_index, slot_index, application->assignment_count,
      application->reservation_count);
  return SUCCEEDED;
}


int register_allocator_commit_loop_retention_transaction(char *function_name,
    int block_count, int instruction_count, int temp_index, int slot_index,
    struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_count,
    struct register_allocator_join_path_state_entry *state_entries,
    int state_capacity, int *state_count, void *context,
    register_allocator_join_assignment_transaction_applier apply_assignments) {

  struct register_allocator_join_path_state_entry *staged_entries;
  struct register_allocator_join_path_reservation path_reservation;
  struct register_allocator_join_path_commit path_commit;
  struct register_allocator_join_reservation reservation;
  int assignment_index;
  int producer_count;
  int staged_count;

  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      temp_index < 0 || slot_index < 0 || assignments == NULL ||
      assignment_count < 3 || reservations == NULL ||
      reservation_count <= 0 || state_capacity <= 0 || state_count == NULL ||
      *state_count < 0 || *state_count > state_capacity ||
      state_entries == NULL || apply_assignments == NULL) {
    fprintf(stderr, "register_allocator_core: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d state_capacity=%d transaction=rejected atomic=yes status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", temp_index,
        slot_index, assignment_count, reservation_count,
        state_count != NULL ? *state_count : -1, state_capacity);
    return FAILED;
  }
  producer_count = 0;
  while (producer_count < assignment_count &&
      assignments[producer_count].role == RA_JOIN_ASSIGNMENT_PRODUCER)
    producer_count++;
  if (producer_count < 2 || producer_count >= assignment_count) {
    fprintf(stderr, "register_allocator_core: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d state_capacity=%d transaction=rejected atomic=yes status=invalid_assignment_order\n",
        function_name, temp_index, slot_index, assignment_count,
        reservation_count, *state_count, state_capacity);
    return FAILED;
  }
  for (assignment_index = 0; assignment_index < assignment_count;
      assignment_index++) {
    int expected_role;
    int previous_assignment;

    expected_role = assignment_index < producer_count ?
      RA_JOIN_ASSIGNMENT_PRODUCER :
        RA_JOIN_ASSIGNMENT_CONSUMER;
    if (assignments[assignment_index].role != expected_role ||
        assignments[assignment_index].instruction < 0 ||
        assignments[assignment_index].instruction >= instruction_count ||
        (expected_role == RA_JOIN_ASSIGNMENT_PRODUCER &&
        assignments[assignment_index].operand != 0) ||
        (expected_role == RA_JOIN_ASSIGNMENT_CONSUMER &&
        _is_valid_tac_use_operand(assignments[assignment_index].operand) == NO) ||
        assignments[assignment_index].physical_register < 0 ||
        (assignment_index > 0 &&
        assignments[assignment_index].physical_register !=
        assignments[0].physical_register)) {
      fprintf(stderr, "register_allocator_core: loop_retention_transaction_assignment function=%s temp=r%d assignment=%d transaction=rejected status=invalid_assignment\n",
          function_name, temp_index, assignment_index);
      return FAILED;
    }
    for (previous_assignment = 0;
        previous_assignment < assignment_index; previous_assignment++) {
      if (assignments[previous_assignment].instruction ==
          assignments[assignment_index].instruction &&
          assignments[previous_assignment].operand ==
          assignments[assignment_index].operand) {
        fprintf(stderr, "register_allocator_core: loop_retention_transaction_assignment function=%s temp=r%d assignment=%d duplicate=%d transaction=rejected status=invalid_assignment\n",
            function_name, temp_index, assignment_index,
            previous_assignment);
        return FAILED;
      }
    }
  }
  staged_entries =
      (struct register_allocator_join_path_state_entry *)calloc(
      (size_t)state_capacity,
      sizeof(struct register_allocator_join_path_state_entry));
  if (staged_entries == NULL) {
    fprintf(stderr, "register_allocator_core: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d state_capacity=%d transaction=rejected atomic=yes status=out_of_memory\n",
        function_name, temp_index, slot_index, assignment_count,
        reservation_count, *state_count, state_capacity);
    return FAILED;
  }
  staged_count = *state_count;
  if (staged_count > 0)
    memcpy(staged_entries, state_entries,
        sizeof(struct register_allocator_join_path_state_entry) *
        (size_t)staged_count);
  reservation.status = RA_JOIN_RESERVATION_READY;
  reservation.slot_index = slot_index;
  reservation.live_start = reservations[0].start_instruction;
  reservation.live_end = reservations[reservation_count - 1].end_instruction;
  reservation.conflict_temp = -1;
  path_reservation.status = RA_JOIN_PATH_RESERVATION_READY;
  path_reservation.block_count = reservation_count;
  if (register_allocator_commit_join_path_reservations(function_name,
      block_count, instruction_count, temp_index, &reservation, reservations,
      reservation_count, &path_reservation, staged_entries, state_capacity,
      &staged_count, &path_commit) == FAILED ||
      path_commit.status != RA_JOIN_PATH_COMMIT_COMMITTED ||
      apply_assignments(context, temp_index, assignments,
      assignment_count) == FAILED) {
    free(staged_entries);
    fprintf(stderr, "register_allocator_core: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d state_capacity=%d transaction=rejected atomic=yes status=complete\n",
        function_name, temp_index, slot_index, assignment_count,
        reservation_count, *state_count, state_capacity);
    return FAILED;
  }
  memcpy(state_entries, staged_entries,
      sizeof(struct register_allocator_join_path_state_entry) *
      (size_t)staged_count);
  *state_count = staged_count;
  free(staged_entries);
  fprintf(stderr, "register_allocator_core: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d state_capacity=%d transaction=applied atomic=yes status=complete\n",
      function_name, temp_index, slot_index, assignment_count,
      reservation_count, staged_count, state_capacity);
  return SUCCEEDED;
}


struct _result_reload_chain_context {
  void *selection_context;
  register_allocator_reload_register_selector select_register;
};


static int _get_result_reload_operand(void *context, int instruction_index,
    int temp_index) {

  (void)context;
  (void)instruction_index;
  (void)temp_index;
  return TAC_USE_RESULT;
}


static int _select_result_reload_register(void *context, int instruction_index,
    int temp_index, int operand) {

  struct _result_reload_chain_context *result_context;

  if (context == NULL || operand != TAC_USE_RESULT)
    return -1;
  result_context = (struct _result_reload_chain_context *)context;
  return result_context->select_register(result_context->selection_context,
      instruction_index, temp_index);
}


int register_allocator_execute_reload_chain(char *function_name,
    int block_index, int temp_index, int spill_instruction,
    int start_instruction, int end_instruction, int instruction_count,
    int no_physical_register, void *discovery_context,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp,
    register_allocator_instruction_predicate is_reload_eligible,
    void *selection_context,
    register_allocator_reload_register_selector select_register,
    void *mutation_context,
    register_allocator_reload_mutation_applier apply_mutation,
    int *reload_instructions,
    struct register_allocator_reload_execution *reload_executions,
    int reload_capacity,
    struct register_allocator_reload_chain_execution *chain_execution) {

  struct _result_reload_chain_context result_context;

  result_context.selection_context = selection_context;
  result_context.select_register = select_register;
  return register_allocator_execute_reload_operand_chain(function_name,
      block_index, temp_index, spill_instruction, start_instruction,
      end_instruction, instruction_count, no_physical_register,
      discovery_context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &result_context, _get_result_reload_operand,
      select_register != NULL ? _select_result_reload_register : NULL,
      mutation_context, apply_mutation, reload_instructions,
      reload_executions, reload_capacity, chain_execution);
}


int register_allocator_discover_reload_graph(char *function_name,
    int block_count, int instruction_count,
    struct register_allocator_basic_block *blocks,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int start_block, int spill_instruction, int temp_index, void *context,
    register_allocator_instruction_predicate is_active,
    register_allocator_instruction_predicate reads_temp,
    register_allocator_instruction_predicate writes_temp,
    register_allocator_instruction_predicate is_reload_eligible,
    int *reload_instructions, int reload_capacity,
    struct register_allocator_reload_chain *reload_chain) {

  char *queued;
  int block_index;
  int edge_index;
  int queue_count;
  int queue_index;
  int *queue;
  int reload_index;
  int stopped_paths;
  int visited_count;

  if (reload_chain != NULL)
    _clear_reload_chain(reload_chain);
  if (reload_instructions != NULL && reload_capacity > 0) {
    for (reload_index = 0; reload_index < reload_capacity; reload_index++)
      reload_instructions[reload_index] = -1;
  }
  if (function_name == NULL || block_count <= 0 || instruction_count <= 0 ||
      blocks == NULL || edge_count < 0 || (edge_count > 0 && edges == NULL) ||
      start_block < 0 || start_block >= block_count || spill_instruction < 0 ||
      spill_instruction < blocks[start_block].start_tac ||
      spill_instruction > blocks[start_block].end_tac || temp_index < 0 ||
      is_active == NULL || reads_temp == NULL || writes_temp == NULL ||
      is_reload_eligible == NULL || reload_instructions == NULL ||
      reload_capacity <= 0 || reload_chain == NULL) {
    fprintf(stderr, "register_allocator_core: reload_graph function=%s start_block=%d spill=%d temp=r%d blocks=%d edges=%d capacity=%d reloads=0 visited=0 stopped=0 status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", start_block,
        spill_instruction, temp_index, block_count, edge_count,
        reload_capacity);
    return FAILED;
  }
  for (block_index = 0; block_index < block_count; block_index++) {
    if (blocks[block_index].start_tac < 0 ||
        blocks[block_index].end_tac < blocks[block_index].start_tac ||
        blocks[block_index].end_tac >= instruction_count ||
        (block_index > 0 && blocks[block_index].start_tac <=
         blocks[block_index - 1].end_tac)) {
      fprintf(stderr, "register_allocator_core: reload_graph function=%s block=%d status=invalid_block\n",
          function_name, block_index);
      return FAILED;
    }
  }
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count ||
        edges[edge_index].kind < RA_CFG_EDGE_FALLTHROUGH ||
        edges[edge_index].kind > RA_CFG_EDGE_BRANCH_FALSE) {
      fprintf(stderr, "register_allocator_core: reload_graph function=%s edge=%d status=invalid_edge\n",
          function_name, edge_index);
      return FAILED;
    }
  }

  queued = (char *)calloc((size_t)block_count, sizeof(char));
  queue = (int *)calloc((size_t)block_count, sizeof(int));
  if (queued == NULL || queue == NULL) {
    free(queue);
    free(queued);
    fprintf(stderr, "register_allocator_core: reload_graph function=%s blocks=%d status=out_of_memory\n",
        function_name, block_count);
    return FAILED;
  }
  queue[0] = start_block;
  queued[start_block] = YES;
  queue_count = 1;
  stopped_paths = 0;
  visited_count = 0;
  for (queue_index = 0; queue_index < queue_count; queue_index++) {
    int instruction;
    int scan_start;
    int stop_path;
    int ineligible_use;

    block_index = queue[queue_index];
    visited_count++;
    scan_start = block_index == start_block ? spill_instruction + 1 :
        blocks[block_index].start_tac;
    stop_path = NO;
    ineligible_use = NO;
    for (instruction = scan_start;
        instruction <= blocks[block_index].end_tac; instruction++) {
      if (is_active(context, instruction, temp_index) == NO)
        continue;
      if (reads_temp(context, instruction, temp_index) == YES) {
        if (is_reload_eligible(context, instruction, temp_index) != YES) {
          stop_path = YES;
          ineligible_use = YES;
          reload_chain->stop_instruction = instruction;
          break;
        }
        if (reload_chain->count >= reload_capacity) {
          reload_chain->truncated = YES;
          stop_path = YES;
          break;
        }
        reload_instructions[reload_chain->count] = instruction;
        reload_chain->count++;
        fprintf(stderr, "register_allocator_core: reload_graph_site function=%s block=%d temp=r%d instruction=%d ordinal=%d status=found\n",
            function_name, block_index, temp_index, instruction,
            reload_chain->count);
      }
      if (writes_temp(context, instruction, temp_index) == YES) {
        stop_path = YES;
        reload_chain->stop_instruction = instruction;
        break;
      }
    }
    if (ineligible_use == YES)
      stopped_paths++;
    if (stop_path == YES)
      continue;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      int successor;

      if (edges[edge_index].from_block != block_index)
        continue;
      successor = edges[edge_index].to_block;
      if (queued[successor] == NO) {
        queued[successor] = YES;
        queue[queue_count] = successor;
        queue_count++;
      }
    }
  }
  for (reload_index = 1; reload_index < reload_chain->count; reload_index++) {
    int insertion_index;
    int instruction;

    instruction = reload_instructions[reload_index];
    insertion_index = reload_index;
    while (insertion_index > 0 &&
        reload_instructions[insertion_index - 1] > instruction) {
      reload_instructions[insertion_index] =
          reload_instructions[insertion_index - 1];
      insertion_index--;
    }
    reload_instructions[insertion_index] = instruction;
  }
  if (stopped_paths > 0 || reload_chain->truncated == YES) {
    for (reload_index = 0; reload_index < reload_capacity; reload_index++)
      reload_instructions[reload_index] = -1;
    reload_chain->count = 0;
  }
  free(queue);
  free(queued);
  fprintf(stderr, "register_allocator_core: reload_graph function=%s start_block=%d spill=%d temp=r%d blocks=%d edges=%d capacity=%d reloads=%d visited=%d stopped=%d truncated=%s qualification=%s order=cfg_breadth_first_instruction status=complete\n",
      function_name, start_block, spill_instruction, temp_index, block_count,
      edge_count, reload_capacity, reload_chain->count, visited_count,
      stopped_paths, reload_chain->truncated == YES ? "yes" : "no",
      stopped_paths == 0 && reload_chain->truncated == NO ? "ready" : "rejected");
  return SUCCEEDED;
}
