#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/z80_register_spill.h"


static void initialize_function(struct tree_node *function_node,
  struct tree_node *name_node, struct tree_node **children, char *name) {

  memset(function_node, 0, sizeof(struct tree_node));
  memset(name_node, 0, sizeof(struct tree_node));
  memset(children, 0, 2 * sizeof(struct tree_node *));
  name_node->label = name;
  children[1] = name_node;
  function_node->children = children;
}


static void initialize_spill(struct tac *t, struct tree_node *function_node,
    int temp_index, int physical_register, double destination_offset,
    double byte_count) {

  memset(t, 0, sizeof(struct tac));
  t->op = TAC_OP_REGISTER_SPILL;
  t->arg1_type = TAC_ARG_TYPE_TEMP;
  t->arg1_d = temp_index;
  t->arg1_original_register_index = temp_index;
  t->arg1_physical_register = physical_register;
  t->arg2_type = TAC_ARG_TYPE_CONSTANT;
  t->arg2_d = byte_count;
  t->result_type = TAC_ARG_TYPE_CONSTANT;
  t->result_d = destination_offset;
  t->function_node = function_node;
}


static int file_is_empty(FILE *file) {

  long size;

  fflush(file);
  if (fseek(file, 0, SEEK_END) != 0)
    return NO;
  size = ftell(file);
  return size == 0 ? YES : NO;
}


static int file_matches(FILE *file, char *expected) {

  char buffer[1024];
  size_t count;

  fflush(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    return NO;
  count = fread(buffer, 1, sizeof(buffer) - 1, file);
  buffer[count] = 0;
  return strcmp(buffer, expected) == 0 ? YES : NO;
}


int main(void) {

  struct tac spill;
  struct tree_node function_node;
  struct tree_node name_node;
  struct tree_node other_function;
  struct tree_node *function_children[2];
  FILE *invalid_output;
  FILE *valid_output;
  char *expected;

  initialize_function(&function_node, &name_node, function_children,
      "z80SpillEmitter");
  memset(&other_function, 0, sizeof(struct tree_node));
  valid_output = tmpfile();
  invalid_output = tmpfile();
  if (valid_output == NULL || invalid_output == NULL)
    return 1;

  if (z80_validate_array_write_reload_operand("z80ReloadOperand",
      TAC_USE_RESULT) == FAILED)
    return 27;
  if (z80_validate_array_write_reload_operand("z80ReloadOperand",
      TAC_USE_ARG1) != FAILED ||
      z80_validate_array_write_reload_operand("z80ReloadOperand",
      TAC_USE_ARG2) != FAILED ||
      z80_validate_array_write_reload_operand("z80ReloadOperand", -1) != FAILED ||
      z80_validate_array_write_reload_operand("z80ReloadOperand", 99) != FAILED ||
      z80_validate_array_write_reload_operand(NULL, TAC_USE_RESULT) != FAILED)
    return 28;
  if (z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE,
      TAC_USE_RESULT, 16) != Z80_PHY_BC)
    return 29;
  if (z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE,
      TAC_USE_ARG1, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE,
      TAC_USE_ARG2, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_READ,
      TAC_USE_ARG1, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_READ,
      TAC_USE_ARG2, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_GET_ADDRESS_ARRAY,
      TAC_USE_ARG1, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_GET_ADDRESS_ARRAY,
      TAC_USE_ARG2, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE,
      TAC_USE_RESULT, 8) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE,
      TAC_USE_RESULT, 32) != Z80_PHY_NONE)
    return 30;
  if (z80_get_split_reload_physical_register(-1, TAC_USE_RESULT, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE, -1, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE, 99, 16) != Z80_PHY_NONE ||
      z80_get_split_reload_physical_register(TAC_OP_ARRAY_WRITE,
      TAC_USE_RESULT, 0) != Z80_PHY_NONE)
    return 31;

  initialize_spill(&spill, &function_node, 0, Z80_PHY_A, -4, 1);
  if (z80_emit_register_spill(&spill, &function_node, valid_output) == FAILED)
    return 2;
  initialize_spill(&spill, &function_node, 1, Z80_PHY_B, 0, 1);
  if (z80_emit_register_spill(&spill, &function_node, valid_output) == FAILED)
    return 3;
  initialize_spill(&spill, &function_node, 2, Z80_PHY_C, 32767, 1);
  if (z80_emit_register_spill(&spill, &function_node, valid_output) == FAILED)
    return 4;
  initialize_spill(&spill, &function_node, 3, Z80_PHY_HL, -32768, 2);
  if (z80_emit_register_spill(&spill, &function_node, valid_output) == FAILED)
    return 5;
  initialize_spill(&spill, &function_node, 4, Z80_PHY_BC, -9, 2);
  if (z80_emit_register_spill(&spill, &function_node, valid_output) == FAILED)
    return 6;

  expected =
      "      LD  IX,-4\n      ADD IX,DE\n      LD  (IX+0),A\n"
      "      LD  IX,0\n      ADD IX,DE\n      LD  (IX+0),B\n"
      "      LD  IX,32767\n      ADD IX,DE\n      LD  (IX+0),C\n"
      "      LD  IX,-32768\n      ADD IX,DE\n      LD  (IX+0),L\n"
      "      LD  (IX+1),H\n"
      "      LD  IX,-9\n      ADD IX,DE\n      LD  (IX+0),C\n"
      "      LD  (IX+1),B\n";
  if (file_matches(valid_output, expected) == NO)
    return 7;
  fprintf(stderr, "z80_register_spill_test: assembly_begin\n%s", expected);
  fprintf(stderr, "z80_register_spill_test: assembly_end status=complete\n");

  initialize_spill(&spill, &function_node, 5, Z80_PHY_NONE, -4, 1);
  if (z80_emit_register_spill(NULL, &function_node, invalid_output) != FAILED ||
      z80_emit_register_spill(&spill, NULL, invalid_output) != FAILED ||
      z80_emit_register_spill(&spill, &function_node, NULL) != FAILED)
    return 8;
  spill.function_node = &other_function;
  if (z80_emit_register_spill(&spill, &other_function, invalid_output) != FAILED)
    return 9;
  spill.function_node = &other_function;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 10;
  initialize_spill(&spill, &function_node, -1, Z80_PHY_A, -4, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 11;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 2);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 12;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_HL, -4, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 13;
  initialize_spill(&spill, &function_node, 5, 99, -4, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 14;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -32769, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 15;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, 32768, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 16;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 1.5);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 17;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4.5, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 18;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 1);
  spill.arg1_d = 5.5;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 19;
  spill.arg1_d = 1e30;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 20;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, 1e30, 1);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 21;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 1e30);
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 22;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 1);
  spill.arg1_original_register_index = 6;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 23;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 1);
  spill.arg2_type = TAC_ARG_TYPE_TEMP;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 24;
  initialize_spill(&spill, &function_node, 5, Z80_PHY_A, -4, 1);
  spill.result_type = TAC_ARG_TYPE_TEMP;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED)
    return 25;
  spill.op = TAC_OP_ADD;
  if (z80_emit_register_spill(&spill, &function_node, invalid_output) != FAILED ||
      file_is_empty(invalid_output) == NO)
    return 26;

  fprintf(stderr, "z80_register_spill_test: invalid_cases=21 output_bytes=0 status=complete\n");
  fprintf(stderr, "z80_register_spill_test: reload_operands supported=1 unsupported=2 invalid=3 status=complete\n");
  fprintf(stderr, "z80_register_spill_test: split_reload_endpoints supported=1 unsupported=8 invalid=4 status=complete\n");
  fclose(invalid_output);
  fclose(valid_output);
  return 0;
}