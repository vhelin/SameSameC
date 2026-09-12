#include <stdio.h>
#include <limits.h>

#include "defines.h"
#include "z80_register_spill.h"


static char *_get_function_name(struct tree_node *function_node) {

  if (function_node != NULL && function_node->children != NULL &&
      function_node->children[1] != NULL &&
      function_node->children[1]->label != NULL)
    return function_node->children[1]->label;
  return "<null>";
}


int z80_emit_register_spill(struct tac *t, struct tree_node *function_node,
    FILE *file_out) {

  char *function_name;
  int byte_count;
  int destination_offset;
  int physical_register;
  int temp_index;
  int valid_numbers;
  int valid_register;

  function_name = _get_function_name(function_node);
  temp_index = -1;
  physical_register = t != NULL ? t->arg1_physical_register : Z80_PHY_NONE;
  destination_offset = 0;
  byte_count = 0;
  valid_numbers = NO;
  if (t != NULL && t->arg1_d >= INT_MIN && t->arg1_d <= INT_MAX &&
      t->arg2_d >= INT_MIN && t->arg2_d <= INT_MAX &&
      t->result_d >= INT_MIN && t->result_d <= INT_MAX) {
    temp_index = (int)t->arg1_d;
    byte_count = (int)t->arg2_d;
    destination_offset = (int)t->result_d;
    if (t->arg1_d == (double)temp_index &&
        t->arg2_d == (double)byte_count &&
        t->result_d == (double)destination_offset)
      valid_numbers = YES;
  }
  valid_register = (byte_count == 1 &&
      (physical_register == Z80_PHY_A || physical_register == Z80_PHY_B ||
      physical_register == Z80_PHY_C)) || (byte_count == 2 &&
      (physical_register == Z80_PHY_HL || physical_register == Z80_PHY_BC));

  if (t == NULL || function_node == NULL || file_out == NULL ||
      function_node->children == NULL || function_node->children[1] == NULL ||
      function_node->children[1]->label == NULL ||
      t->op != TAC_OP_REGISTER_SPILL || t->function_node != function_node ||
      valid_numbers == NO || t->arg1_type != TAC_ARG_TYPE_TEMP ||
      temp_index < 0 ||
      t->arg1_original_register_index != temp_index ||
      t->arg2_type != TAC_ARG_TYPE_CONSTANT ||
      t->result_type != TAC_ARG_TYPE_CONSTANT || valid_register == NO ||
      destination_offset < -32768 || destination_offset > 32767) {
    fprintf(stderr, "register_allocator_z80: register_spill_emit function=%s temp=r%d phy=%d destination_offset=%d bytes=%d status=invalid_input\n",
        function_name, temp_index, physical_register, destination_offset,
        byte_count);
    return FAILED;
  }

  fprintf(file_out, "      LD  IX,%d\n", destination_offset);
  fprintf(file_out, "      ADD IX,DE\n");
  if (physical_register == Z80_PHY_A)
    fprintf(file_out, "      LD  (IX+0),A\n");
  else if (physical_register == Z80_PHY_B)
    fprintf(file_out, "      LD  (IX+0),B\n");
  else if (physical_register == Z80_PHY_C)
    fprintf(file_out, "      LD  (IX+0),C\n");
  else if (physical_register == Z80_PHY_HL) {
    fprintf(file_out, "      LD  (IX+0),L\n");
    fprintf(file_out, "      LD  (IX+1),H\n");
  }
  else {
    fprintf(file_out, "      LD  (IX+0),C\n");
    fprintf(file_out, "      LD  (IX+1),B\n");
  }

  fprintf(stderr, "register_allocator_z80: register_spill_emit function=%s temp=r%d phy=%d destination_offset=%d bytes=%d stores=%d status=complete\n",
      function_name, temp_index, physical_register, destination_offset,
      byte_count, byte_count);
  return SUCCEEDED;
}


int z80_validate_array_write_reload_operand(char *function_name, int operand) {

  if (function_name == NULL ||
      (operand != TAC_USE_RESULT && operand != TAC_USE_ARG1 &&
       operand != TAC_USE_ARG2)) {
    fprintf(stderr, "register_allocator_z80: reload_operand function=%s op=ARRAY_WRITE operand_id=%d status=invalid_input\n",
        function_name != NULL ? function_name : "<null>", operand);
    return FAILED;
  }
  if (operand != TAC_USE_RESULT) {
    fprintf(stderr, "register_allocator_z80: reload_operand function=%s op=ARRAY_WRITE operand_id=%d status=unsupported_operand\n",
        function_name, operand);
    return FAILED;
  }

  fprintf(stderr, "register_allocator_z80: reload_operand function=%s op=ARRAY_WRITE operand_id=%d status=supported\n",
      function_name, operand);
  return SUCCEEDED;
}


int z80_get_split_reload_physical_register(int consumer_op, int operand,
    int size) {

  int physical_register;
  char *status;

  physical_register = Z80_PHY_NONE;
  if (consumer_op < 0 ||
      (operand != TAC_USE_RESULT && operand != TAC_USE_ARG1 &&
       operand != TAC_USE_ARG2) || size <= 0) {
    status = "invalid_input";
  }
  else if (consumer_op == TAC_OP_ARRAY_WRITE && operand == TAC_USE_RESULT &&
      size == 16) {
    physical_register = Z80_PHY_BC;
    status = "supported";
  }
  else {
    status = "unsupported";
  }

  fprintf(stderr, "register_allocator_z80: split_reload_endpoint op=%d operand=%d size=%d phy=%d status=%s\n",
      consumer_op, operand, size, physical_register, status);
  return physical_register;
}