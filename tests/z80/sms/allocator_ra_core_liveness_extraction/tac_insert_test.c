#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/tac.h"

int g_verbose_mode = 0;
int g_input_float_mode = 0;
int g_current_filename_id = 7;
int g_current_line_number = 11;
char g_tmp[4096];
char g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
struct tree_node *g_current_statement = NULL;

extern struct tac *g_tacs;
extern int g_tacs_count;
extern int g_tacs_max;

int snprintf_(char *buffer, size_t count, const char *format, ...) {

  (void)format;
  if (buffer != NULL && count > 0)
    buffer[0] = '\0';
  return 0;
}

int print_error(char *error, int type) {

  (void)error;
  (void)type;
  return FAILED;
}

struct inline_asm *inline_asm_find(int id) {

  (void)id;
  return NULL;
}

struct symbol_table_item *symbol_table_find_symbol(char *name) {

  (void)name;
  return NULL;
}

static char *allocate_label(char *label) {

  char *copy;

  copy = (char *)calloc(strlen(label) + 1, 1);
  if (copy != NULL)
    memcpy(copy, label, strlen(label) + 1);
  return copy;
}

static int tac_is_canonical_empty(struct tac *t) {

  return t != NULL && t->op == TAC_OP_DEAD &&
      t->arg1_type == TAC_ARG_TYPE_NONE && t->arg1_d == 0 &&
      t->arg1_original_register_index == -1 && t->arg1_s == NULL &&
      t->arg1_var_type == VARIABLE_TYPE_NONE &&
      t->arg1_var_type_promoted == VARIABLE_TYPE_NONE &&
      t->arg1_node == NULL && t->arg2_type == TAC_ARG_TYPE_NONE &&
      t->arg2_d == 0 && t->arg2_original_register_index == -1 &&
      t->arg2_s == NULL && t->arg2_var_type == VARIABLE_TYPE_NONE &&
      t->arg2_var_type_promoted == VARIABLE_TYPE_NONE &&
      t->arg2_node == NULL && t->result_type == TAC_ARG_TYPE_NONE &&
      t->result_d == 0 && t->result_original_register_index == -1 &&
      t->result_s == NULL && t->result_var_type == VARIABLE_TYPE_NONE &&
      t->result_var_type_promoted == VARIABLE_TYPE_NONE &&
      t->result_node == NULL && t->result_physical_register == Z80_PHY_NONE &&
      t->arg1_physical_register == Z80_PHY_NONE &&
      t->arg2_physical_register == Z80_PHY_NONE &&
      t->store_retained_to_spill_operand == -1 &&
      t->reload_spill_to_physical_operand == -1 && t->arguments == NULL &&
      t->arguments_count == 0 && t->function_node == NULL &&
      t->is_function == NO && t->statement == g_current_statement &&
      t->file_id == g_current_filename_id &&
      t->line_number == g_current_line_number;
}

static int initialize_owned_tac(int op, char *label, int line_number) {

  struct tac *t;

  t = add_tac();
  if (t == NULL)
    return FAILED;
  t->op = (unsigned char)op;
  t->result_type = TAC_ARG_TYPE_LABEL;
  t->result_s = allocate_label(label);
  if (t->result_s == NULL)
    return FAILED;
  t->line_number = line_number;
  return SUCCEEDED;
}

static int register_spill_matches(struct tac *t,
    struct tree_node *function_node, int temp_index, int physical_register,
    int destination_offset, int byte_count) {

  return t != NULL && t->op == TAC_OP_REGISTER_SPILL &&
      t->function_node == function_node &&
      t->arg1_type == TAC_ARG_TYPE_TEMP &&
      (int)t->arg1_d == temp_index &&
      t->arg1_original_register_index == temp_index &&
      t->arg1_physical_register == physical_register &&
      t->arg2_type == TAC_ARG_TYPE_CONSTANT &&
      (int)t->arg2_d == byte_count &&
      t->result_type == TAC_ARG_TYPE_CONSTANT &&
      (int)t->result_d == destination_offset;
}

static void release_tacs(void) {

  int index;

  for (index = 0; index < g_tacs_count; index++)
    free_tac_contents(&g_tacs[index]);
  free(g_tacs);
  g_tacs = NULL;
  g_tacs_count = 0;
  g_tacs_max = 0;
}

int main(void) {

  char *first_label;
  char *second_label;
  char *third_label;
  struct tac *inserted;
  struct tac *spill_16;
  struct tac *spill_8;
  struct tree_node *function_node;
  int old_capacity;
  int old_count;

  if (initialize_owned_tac(TAC_OP_LABEL, "first", 101) == FAILED ||
      initialize_owned_tac(TAC_OP_JUMP, "second", 102) == FAILED ||
      initialize_owned_tac(TAC_OP_ASSIGNMENT, "third", 103) == FAILED) {
    release_tacs();
    return 1;
  }
  first_label = g_tacs[0].result_s;
  second_label = g_tacs[1].result_s;
  third_label = g_tacs[2].result_s;

  inserted = insert_tac(0);
  if (tac_is_canonical_empty(inserted) == NO || g_tacs_count != 4 ||
      g_tacs[1].result_s != first_label || g_tacs[1].line_number != 101 ||
      g_tacs[2].result_s != second_label || g_tacs[2].line_number != 102 ||
      g_tacs[3].result_s != third_label || g_tacs[3].line_number != 103) {
    release_tacs();
    return 2;
  }

  inserted = insert_tac(2);
  if (tac_is_canonical_empty(inserted) == NO || g_tacs_count != 5 ||
      g_tacs[1].result_s != first_label ||
      g_tacs[3].result_s != second_label ||
      g_tacs[4].result_s != third_label) {
    release_tacs();
    return 3;
  }

  inserted = insert_tac(g_tacs_count);
  if (tac_is_canonical_empty(inserted) == NO || g_tacs_count != 6 ||
      inserted != &g_tacs[5] || g_tacs[1].result_s != first_label ||
      g_tacs[3].result_s != second_label ||
      g_tacs[4].result_s != third_label) {
    release_tacs();
    return 4;
  }

  old_count = g_tacs_count;
  if (insert_tac(-1) != NULL || insert_tac(old_count + 1) != NULL ||
      g_tacs_count != old_count || g_tacs[1].result_s != first_label ||
      g_tacs[3].result_s != second_label ||
      g_tacs[4].result_s != third_label) {
    release_tacs();
    return 5;
  }

  function_node = (struct tree_node *)&old_capacity;
  spill_16 = insert_tac(g_tacs_count);
  if (spill_16 == NULL ||
      tac_set_register_spill(NULL, function_node, 2, Z80_PHY_NONE,
      Z80_PHY_HL, -6, 2) != FAILED ||
      tac_set_register_spill(spill_16, NULL, 2, Z80_PHY_NONE, Z80_PHY_HL,
      -6, 2) != FAILED ||
      tac_set_register_spill(spill_16, function_node, -1, Z80_PHY_NONE,
      Z80_PHY_HL, -6, 2) != FAILED ||
      tac_set_register_spill(spill_16, function_node, 2, Z80_PHY_NONE,
      Z80_PHY_NONE, -6, 2) != FAILED ||
      tac_set_register_spill(spill_16, function_node, 2, Z80_PHY_NONE,
      Z80_PHY_HL, -6, 0) != FAILED ||
      tac_is_canonical_empty(spill_16) == NO) {
    release_tacs();
    return 6;
  }
  spill_16->arg1_physical_register = Z80_PHY_A;
  if (tac_set_register_spill(spill_16, function_node, 2, Z80_PHY_NONE,
      Z80_PHY_HL, -6, 2) != FAILED) {
    release_tacs();
    return 7;
  }
  spill_16->arg1_physical_register = Z80_PHY_NONE;
  if (tac_set_register_spill(&g_tacs[1], function_node, 2, Z80_PHY_NONE,
      Z80_PHY_HL, -6, 2) != FAILED || g_tacs[1].result_s != first_label ||
      g_tacs[1].op != TAC_OP_LABEL) {
    release_tacs();
    return 8;
  }
  if (tac_set_register_spill(spill_16, function_node, 2, Z80_PHY_NONE,
      Z80_PHY_HL, -6, 2) == FAILED ||
      register_spill_matches(spill_16, function_node, 2, Z80_PHY_HL,
      -6, 2) == NO) {
    release_tacs();
    return 9;
  }
  spill_8 = insert_tac(g_tacs_count);
  if (spill_8 == NULL ||
      tac_set_register_spill(spill_8, function_node, 3, Z80_PHY_NONE,
      Z80_PHY_A, -8, 1) == FAILED ||
      register_spill_matches(spill_8, function_node, 3, Z80_PHY_A,
      -8, 1) == NO) {
    release_tacs();
    return 10;
  }
  print_tac(spill_16, YES, stderr);
  print_tac(spill_8, YES, stderr);

  old_capacity = g_tacs_max;
  while (g_tacs_count < old_capacity) {
    if (add_tac() == NULL) {
      release_tacs();
      return 11;
    }
  }
  inserted = insert_tac(1);
  if (tac_is_canonical_empty(inserted) == NO ||
      g_tacs_max <= old_capacity || g_tacs_count != old_capacity + 1 ||
      g_tacs[2].result_s != first_label ||
      g_tacs[4].result_s != second_label ||
      g_tacs[5].result_s != third_label) {
    release_tacs();
    return 11;
  }

  fprintf(stderr, "tac_storage_test: ownership first=%s second=%s third=%s count=%d capacity=%d status=complete\n",
      g_tacs[2].result_s, g_tacs[4].result_s, g_tacs[5].result_s,
      g_tacs_count, g_tacs_max);
  release_tacs();
  return 0;
}
