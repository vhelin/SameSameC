#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

static int storage_is_clear(struct register_allocator_control_flow_storage *storage) {

  return storage->max_blocks == 0 && storage->max_edges == 0 &&
  storage->instruction_bytes == 0 && storage->block_bytes == 0 &&
  storage->edge_bytes == 0;
}

static int expect_storage(char *function_name, int instruction_capacity) {

  struct register_allocator_control_flow_storage storage;

  if (register_allocator_plan_control_flow_storage(function_name, instruction_capacity,
      &storage) == FAILED)
    return FAILED;
  if (storage.max_blocks != instruction_capacity ||
      storage.max_edges != instruction_capacity * 2 ||
      storage.instruction_bytes != (size_t)instruction_capacity *
      sizeof(struct register_allocator_instruction) ||
      storage.block_bytes != (size_t)instruction_capacity *
      sizeof(struct register_allocator_basic_block) ||
      storage.edge_bytes != (size_t)storage.max_edges *
      sizeof(struct register_allocator_cfg_edge))
    return FAILED;
  return SUCCEEDED;
}

static int expect_invalid_storage(char *function_name, int instruction_capacity) {

  struct register_allocator_control_flow_storage storage;

  storage.max_blocks = 1;
  storage.max_edges = 2;
  storage.instruction_bytes = 3;
  storage.block_bytes = 4;
  storage.edge_bytes = 5;
  if (register_allocator_plan_control_flow_storage(function_name, instruction_capacity,
      &storage) != FAILED)
    return FAILED;
  return storage_is_clear(&storage) == YES ? SUCCEEDED : FAILED;
}

static void clear_instruction(struct register_allocator_instruction *instruction, int tac_index) {

  instruction->active = YES;
  instruction->tac_index = tac_index;
  instruction->is_label = NO;
  instruction->label = NULL;
  instruction->end_reason = RA_BLOCK_END_NONE;
  instruction->is_jump = NO;
  instruction->is_conditional_jump = NO;
  instruction->jump_target = NULL;
}

static int test_storage_lifecycle(void) {

  struct register_allocator_control_flow_storage storage;
  struct register_allocator_instruction *instructions;
  struct register_allocator_basic_block *blocks;
  struct register_allocator_cfg_edge *edges;
  int block_count;
  int edge_count;
  int instruction_index;
  int status;

  block_count = 0;
  edge_count = 0;
  if (register_allocator_plan_control_flow_storage("coreCfgStorageLifecycle", 4,
      &storage) == FAILED)
    return FAILED;
  instructions = (struct register_allocator_instruction *)calloc(1,
      storage.instruction_bytes);
  blocks = (struct register_allocator_basic_block *)calloc(1, storage.block_bytes);
  edges = (struct register_allocator_cfg_edge *)calloc(1, storage.edge_bytes);
  if (instructions == NULL || blocks == NULL || edges == NULL) {
    free(edges);
    free(blocks);
    free(instructions);
    return FAILED;
  }

  for (instruction_index = 0; instruction_index < 4; instruction_index++)
    clear_instruction(&instructions[instruction_index], instruction_index);
  instructions[1].end_reason = RA_BLOCK_END_JUMP;
  instructions[1].is_jump = YES;
  instructions[1].is_conditional_jump = YES;
  instructions[1].jump_target = "target";
  instructions[2].is_label = YES;
  instructions[2].label = "target";
  instructions[3].end_reason = RA_BLOCK_END_RETURN;

  status = register_allocator_build_basic_blocks("coreCfgStorageLifecycle", instructions,
      4, blocks, storage.max_blocks, &block_count);
  if (status == SUCCEEDED)
    status = register_allocator_build_cfg("coreCfgStorageLifecycle", instructions, 4,
        blocks, block_count, edges, storage.max_edges, &edge_count);
  if (status == SUCCEEDED && (block_count != 2 || edge_count != 2 ||
      edges[0].from_block != 0 || edges[0].to_block != 1 ||
      edges[0].kind != RA_CFG_EDGE_BRANCH_TRUE || edges[1].from_block != 0 ||
      edges[1].to_block != 1 || edges[1].kind != RA_CFG_EDGE_BRANCH_FALSE))
    status = FAILED;

  free(edges);
  free(blocks);
  free(instructions);
  return status;
}

static int test_post_mutation_reconstruction(void) {

  struct register_allocator_control_flow_storage storage;
  struct register_allocator_instruction *instructions;
  struct register_allocator_basic_block *blocks;
  struct register_allocator_cfg_edge *edges;
  int block_count;
  int edge_count;
  int instruction_index;
  int status;

  block_count = 0;
  edge_count = 0;
  if (register_allocator_plan_control_flow_storage(
      "coreCfgPostMutation", 5, &storage) == FAILED)
    return FAILED;
  instructions = (struct register_allocator_instruction *)calloc(1,
      storage.instruction_bytes);
  blocks = (struct register_allocator_basic_block *)calloc(1,
      storage.block_bytes);
  edges = (struct register_allocator_cfg_edge *)calloc(1,
      storage.edge_bytes);
  if (instructions == NULL || blocks == NULL || edges == NULL) {
    free(edges);
    free(blocks);
    free(instructions);
    return FAILED;
  }
  for (instruction_index = 0; instruction_index < 5; instruction_index++)
    clear_instruction(&instructions[instruction_index], instruction_index);
  instructions[2].end_reason = RA_BLOCK_END_JUMP;
  instructions[2].is_jump = YES;
  instructions[2].is_conditional_jump = YES;
  instructions[2].jump_target = "shifted_target";
  instructions[3].is_label = YES;
  instructions[3].label = "shifted_target";
  instructions[4].end_reason = RA_BLOCK_END_RETURN;

  status = register_allocator_build_basic_blocks("coreCfgPostMutation",
      instructions, 5, blocks, storage.max_blocks, &block_count);
  if (status == SUCCEEDED)
    status = register_allocator_build_cfg("coreCfgPostMutation",
        instructions, 5, blocks, block_count, edges, storage.max_edges,
        &edge_count);
  if (status == SUCCEEDED && (block_count != 2 || edge_count != 2 ||
      blocks[0].start_tac != 0 || blocks[0].end_tac != 2 ||
      blocks[1].start_tac != 3 || blocks[1].end_tac != 4 ||
      edges[0].from_block != 0 || edges[0].to_block != 1 ||
      edges[0].kind != RA_CFG_EDGE_BRANCH_TRUE ||
      edges[1].from_block != 0 || edges[1].to_block != 1 ||
      edges[1].kind != RA_CFG_EDGE_BRANCH_FALSE))
    status = FAILED;
  if (status == SUCCEEDED)
    fprintf(stderr, "core_cfg_storage_test: post_mutation_reconstruction instructions=5 inserted=1 blocks=2 edges=2 shifted_jump=2 shifted_label=3 status=complete\n");

  free(edges);
  free(blocks);
  free(instructions);
  return status;
}

int main(void) {

  struct register_allocator_control_flow_storage storage;
  int maximum_capacity;

  if (expect_storage("coreCfgStorageOne", 1) == FAILED ||
      expect_storage("coreCfgStorageFour", 4) == FAILED ||
      expect_storage("coreCfgStorageForty", 40) == FAILED)
    return 1;
  if (expect_invalid_storage(NULL, 4) == FAILED ||
      expect_invalid_storage("coreCfgStorageZero", 0) == FAILED ||
      expect_invalid_storage("coreCfgStorageNegative", -1) == FAILED ||
      expect_invalid_storage("coreCfgStorageEdgeOverflow", INT_MAX) == FAILED)
    return 2;
  maximum_capacity = INT_MAX / 2;
  if ((size_t)maximum_capacity > ((size_t)-1) /
      sizeof(struct register_allocator_instruction) ||
      (size_t)maximum_capacity > (size_t)ULONG_MAX /
      sizeof(struct register_allocator_instruction) ||
      (size_t)maximum_capacity > ((size_t)-1) /
      sizeof(struct register_allocator_basic_block) ||
      (size_t)maximum_capacity > (size_t)ULONG_MAX /
      sizeof(struct register_allocator_basic_block) ||
      (size_t)(maximum_capacity * 2) > ((size_t)-1) /
      sizeof(struct register_allocator_cfg_edge) ||
      (size_t)(maximum_capacity * 2) > (size_t)ULONG_MAX /
      sizeof(struct register_allocator_cfg_edge)) {
    if (expect_invalid_storage("coreCfgStorageMaximum", maximum_capacity) == FAILED)
      return 3;
  }
  else if (expect_storage("coreCfgStorageMaximum", maximum_capacity) == FAILED)
    return 4;
  storage.max_blocks = 1;
  storage.max_edges = 2;
  storage.instruction_bytes = 3;
  storage.block_bytes = 4;
  storage.edge_bytes = 5;
  if (register_allocator_plan_control_flow_storage("coreCfgStorageNoOutput", 4,
      NULL) != FAILED || storage.max_blocks != 1)
    return 5;
  if (test_storage_lifecycle() == FAILED)
    return 6;
  if (test_post_mutation_reconstruction() == FAILED)
    return 7;
  return 0;
}
