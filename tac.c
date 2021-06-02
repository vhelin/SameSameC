
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "printf.h"
#include "definitions.h"
#include "symbol_table.h"
#include "main.h"
#include "tac.h"


extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

struct tac *g_tacs = NULL;
int g_tacs_count = 0, g_tacs_max = 0;

static char g_temp_label[32];


static void _print_tac_arg(int type, double d, char *s, int var_type, int var_type_promoted, FILE *file_out) {

  if (type == TAC_ARG_TYPE_CONSTANT)
    fprintf(file_out, "%d", (int)d);
  else if (type == TAC_ARG_TYPE_TEMP)
    fprintf(file_out, "r%d", (int)d);
  else
    fprintf(file_out, "%s", s);

  /* print var type information if that's available */
  
  if (var_type == VARIABLE_TYPE_VOID)
    fprintf(file_out, ".void");
  else if (var_type == VARIABLE_TYPE_INT8)
    fprintf(file_out, ".int8");
  else if (var_type == VARIABLE_TYPE_INT16)
    fprintf(file_out, ".int16");
  else if (var_type == VARIABLE_TYPE_UINT8)
    fprintf(file_out, ".uint8");
  else if (var_type == VARIABLE_TYPE_UINT16)
    fprintf(file_out, ".uint16");

  /* print promoted var type information if that's available */

  if (var_type_promoted == VARIABLE_TYPE_VOID)
    fprintf(file_out, " (void)");
  else if (var_type_promoted == VARIABLE_TYPE_INT8)
    fprintf(file_out, " (int8)");
  else if (var_type_promoted == VARIABLE_TYPE_INT16)
    fprintf(file_out, " (int16)");
  else if (var_type_promoted == VARIABLE_TYPE_UINT8)
    fprintf(file_out, " (uint8)");
  else if (var_type_promoted == VARIABLE_TYPE_UINT16)
    fprintf(file_out, " (uint16)");
}


char *generate_temp_label(int id) {

  snprintf(g_temp_label, sizeof(g_temp_label), "label_%d", id);

  return g_temp_label;
}


void print_tac(struct tac *t, int is_comment, FILE *file_out) {

  int j;

  if (t->op == TAC_OP_DEAD)
    return;

  if (is_comment == YES)
    fprintf(file_out, "      ; TAC: ");
  else
    fprintf(file_out, "  ");
  
  if (t->op == TAC_OP_LABEL) {
    fprintf(file_out, "%s:", t->result_s);
    if (t->is_function == YES && t->function_node != NULL && t->function_node->local_variables != NULL) {
      struct local_variables *local_variables = t->function_node->local_variables;
      int k;

      for (k = 0; k < local_variables->temp_registers_count; k++) {
        if (k == 0)
          fprintf(file_out, "\n");
        fprintf(file_out, "    register %d size %d offset %d", local_variables->temp_registers[k].register_index,
                local_variables->temp_registers[k].size / 8, local_variables->temp_registers[k].offset_to_fp);
        if (k < local_variables->temp_registers_count - 1)
          fprintf(file_out, "\n");
      }
    }
  }
  else if (t->op == TAC_OP_GET_ADDRESS) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := &");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_GET_ADDRESS_ARRAY) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := &");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, "[");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, "]");
  }
  else if (t->op == TAC_OP_ADD) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " + ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_SUB) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " - ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_MUL) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " * ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_DIV) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " / ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_MOD) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " %% ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_AND) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " & ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_OR) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " | ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_SHIFT_LEFT) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " << ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_SHIFT_RIGHT) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " >> ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_ASSIGNMENT) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");      
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_ARRAY_ASSIGNMENT) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, "[");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, "] := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_ARRAY_READ) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, "[");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, "]");
  }
  else if (t->op == TAC_OP_JUMP) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "jmp %s", t->result_s);
  }
  else if (t->op == TAC_OP_JUMP_EQ) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " == ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_JUMP_NEQ) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " != ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_JUMP_LT) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " < ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_JUMP_GT) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " > ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_JUMP_LTE) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " <= ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_JUMP_GTE) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, " >= ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s, t->arg2_var_type, t->arg2_var_type_promoted, file_out);
    fprintf(file_out, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_RETURN) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "return");
  }
  else if (t->op == TAC_OP_RETURN_VALUE) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    fprintf(file_out, "return ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
  }
  else if (t->op == TAC_OP_FUNCTION_CALL) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, "(");
    for (j = 0; j < (int)t->arg2_d; j++) {
      if (t->registers_types == NULL)
        _print_tac_arg(TAC_ARG_TYPE_TEMP, t->registers[j], NULL, VARIABLE_TYPE_NONE, VARIABLE_TYPE_NONE, file_out);
      else
        _print_tac_arg(TAC_ARG_TYPE_TEMP, t->registers[j], NULL, t->registers_types[j], t->registers_types[j], file_out);
      if (j < (int)t->arg2_d - 1)
        fprintf(file_out, ", ");
    }
    fprintf(file_out, ")");
  }
  else if (t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s, t->result_var_type, t->result_var_type_promoted, file_out);
    fprintf(file_out, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s, t->arg1_var_type, t->arg1_var_type_promoted, file_out);
    fprintf(file_out, "(");
    for (j = 0; j < (int)t->arg2_d; j++) {
      if (t->registers_types == NULL)
        _print_tac_arg(TAC_ARG_TYPE_TEMP, t->registers[j], NULL, VARIABLE_TYPE_NONE, VARIABLE_TYPE_NONE, file_out);
      else
        _print_tac_arg(TAC_ARG_TYPE_TEMP, t->registers[j], NULL, t->registers_types[j], t->registers_types[j], file_out);
      if (j < (int)t->arg2_d - 1)
        fprintf(file_out, ", ");
    }
    fprintf(file_out, ")");
  }
  else if (t->op == TAC_OP_CREATE_VARIABLE) {
    if (is_comment == NO)
      fprintf(file_out, "  ");
    if (t->function_node != NULL) {
      struct local_variables *local_variables = t->function_node->local_variables;
      int size = 0, offset = 0, k;
      char type = '?';

      for (k = 0; k < local_variables->local_variables_count; k++) {
        if (local_variables->local_variables[k].node == t->result_node) {
          size = local_variables->local_variables[k].size;
          offset = local_variables->local_variables[k].offset_to_fp;
          break;
        }
      }

      if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
        type = 'a';
      else
        type = 'n';
      
      fprintf(file_out, "variable \"%s\" size %d offset %d type %c", t->result_node->children[1]->label, size / 8, offset, type);
    }
    else
      fprintf(file_out, "variable \"%s\" size ? offset ? type ?", t->result_node->children[1]->label);
  }
  else {
    fprintf(file_out, "print_tac(): Unknown TAC op %d! Please submit a bug report!", t->op);
    return;
  }

  fprintf(file_out, "\n");
}


void print_tacs(void) {

  int i;

  fprintf(stderr, "///////////////////////////////////////////////////////////////////////\n");
  fprintf(stderr, "// TACS\n");
  fprintf(stderr, "///////////////////////////////////////////////////////////////////////\n");
  
  for (i = 0; i < g_tacs_count; i++)
    print_tac(&g_tacs[i], NO, stderr);
}


int tac_set_arg1(struct tac *t, int arg_type, double d, char *s) {

  t->arg1_type = arg_type;
  
  if (arg_type == TAC_ARG_TYPE_CONSTANT || arg_type == TAC_ARG_TYPE_TEMP)
    t->arg1_d = d;
  else {
    free(t->arg1_s);

    if (s == NULL) {
      fprintf(stderr, "tac_set_arg1(): The label for TAC is NULL! Please submit a bug report!\n");
      return FAILED;
    }
    
    t->arg1_s = calloc(strlen(s) + 1, 1);
    if (t->arg1_s == NULL) {
      fprintf(stderr, "tac_set_arg1(): Out of memory while allocating a label for TACs.\n");
      return FAILED;
    }

    strcpy(t->arg1_s, s);
  }

  return SUCCEEDED;
}


int tac_set_arg2(struct tac *t, int arg_type, double d, char *s) {

  t->arg2_type = arg_type;
  
  if (arg_type == TAC_ARG_TYPE_CONSTANT || arg_type == TAC_ARG_TYPE_TEMP)
    t->arg2_d = d;
  else {
    free(t->arg2_s);

    if (s == NULL) {
      fprintf(stderr, "tac_set_arg2(): The label for TAC is NULL! Please submit a bug report!\n");
      return FAILED;
    }
    
    t->arg2_s = calloc(strlen(s) + 1, 1);
    if (t->arg2_s == NULL) {
      fprintf(stderr, "tac_set_arg2(): Out of memory while allocating a label for TACs.\n");
      return FAILED;
    }

    strcpy(t->arg2_s, s);
  }

  return SUCCEEDED;
}


int tac_set_result(struct tac *t, int arg_type, double d, char *s) {

  t->result_type = arg_type;
  
  if (arg_type == TAC_ARG_TYPE_CONSTANT || arg_type == TAC_ARG_TYPE_TEMP)
    t->result_d = d;
  else {
    free(t->result_s);

    if (s == NULL) {
      fprintf(stderr, "tac_set_result(): The label for TAC is NULL! Please submit a bug report!\n");
      return FAILED;
    }
    
    t->result_s = calloc(strlen(s) + 1, 1);
    if (t->result_s == NULL) {
      fprintf(stderr, "Out of memory while allocating a label for TACs.\n");
      return FAILED;
    }

    strcpy(t->result_s, s);
  }

  return SUCCEEDED;
}


struct tac *add_tac(void) {

  struct tac *t;
  
  if (g_tacs_count == g_tacs_max) {
    g_tacs = realloc(g_tacs, sizeof(struct tac) * (g_tacs_max + 1024));
    if (g_tacs == NULL) {
      fprintf(stderr, "Out of memory while allocating TACs.\n");
      return FAILED;
    }
    g_tacs_max += 1024;
  }

  t = &g_tacs[g_tacs_count++];

  /* reset the TAC */
  t->op = TAC_OP_DEAD;
  t->arg1_type = TAC_ARG_TYPE_NONE;
  t->arg1_d = 0;
  t->arg1_s = NULL;
  t->arg1_var_type = VARIABLE_TYPE_NONE;
  t->arg1_var_type_promoted = VARIABLE_TYPE_NONE;
  t->arg2_type = TAC_ARG_TYPE_NONE;
  t->arg2_d = 0;
  t->arg2_s = NULL;
  t->arg2_var_type = VARIABLE_TYPE_NONE;
  t->arg2_var_type_promoted = VARIABLE_TYPE_NONE;
  t->result_type = TAC_ARG_TYPE_NONE;
  t->result_d = 0;
  t->result_s = NULL;
  t->result_var_type = VARIABLE_TYPE_NONE;
  t->result_var_type_promoted = VARIABLE_TYPE_NONE;
  t->arg1_node = NULL;
  t->arg2_node = NULL;
  t->result_node = NULL;
  t->registers = NULL;
  t->registers_types = NULL;
  t->function_node = NULL;
  t->is_function = NO;
  
  return t;
}


int is_last_tac(int op) {
  
  return g_tacs[g_tacs_count - 1].op == op;
}


struct tac *add_tac_label(char *label) {

  struct tac *t = add_tac();

  if (t == NULL)
    return NULL;

  t->result_s = calloc(strlen(label) + 1, 1);
  if (t->result_s == NULL) {
    fprintf(stderr, "Out of memory while allocating a label for TACs.\n");
    return NULL;
  }

  t->op = TAC_OP_LABEL;
  strcpy(t->result_s, label);

  return t;
}


struct tac *add_tac_jump(char *label) {

  struct tac *t = add_tac();

  if (t == NULL)
    return NULL;

  t->result_s = calloc(strlen(label) + 1, 1);
  if (t->result_s == NULL) {
    fprintf(stderr, "Out of memory while allocating a label for TACs.\n");
    return NULL;
  }

  t->op = TAC_OP_JUMP;
  strcpy(t->result_s, label);

  return t;
}


struct tac *add_tac_calculation(int op, int r1, int r2, int rresult) {

  struct tac *t = add_tac();

  if (t == NULL)
    return NULL;

  t->op = op;

  tac_set_result(t, TAC_ARG_TYPE_TEMP, rresult, NULL);
  tac_set_arg1(t, TAC_ARG_TYPE_TEMP, r1, NULL);
  tac_set_arg2(t, TAC_ARG_TYPE_TEMP, r2, NULL);

  return t;
}


int tac_try_find_definition(struct tac *t, char *label, struct tree_node *node, int tac_use) {

  struct symbol_table_item *sti;

  sti = symbol_table_find_symbol(label);
  if (sti == NULL) {
    if (node != NULL) {
      g_current_filename_id = node->file_id;
      g_current_line_number = node->line_number;
    }
    else
      fprintf(stderr, "tac_try_find_definition(): Filename and line number can be wrong...\n");
    snprintf(g_error_message, sizeof(g_error_message), "tac_try_find_definition(): Cannot find \"%s\"! Please submit a bug report!\n", label);
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  if (tac_use == TAC_USE_RESULT)
    t->result_node = sti->node;
  else if (tac_use == TAC_USE_ARG1)
    t->arg1_node = sti->node;
  else
    t->arg2_node = sti->node;
  
  return SUCCEEDED;
}


int tac_promote_argument(struct tac *t, int type, int tac_use) {

  if (tac_use == TAC_USE_RESULT)
    t->result_var_type_promoted = type;
  else if (tac_use == TAC_USE_ARG1)
    t->arg1_var_type_promoted = type;
  else if (tac_use == TAC_USE_ARG2)
    t->arg2_var_type_promoted = type;

  return SUCCEEDED;
}
