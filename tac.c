
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


static void _print_tac_arg(int type, double d, char *s) {

  if (type == TAC_ARG_TYPE_CONSTANT)
    fprintf(stderr, "%d", (int)d);
  else if (type == TAC_ARG_TYPE_TEMP)
    fprintf(stderr, "r%d", (int)d);
  else
    fprintf(stderr, "%s", s);
}


char *generate_temp_label(int id) {

  snprintf(g_temp_label, sizeof(g_temp_label), "label_%d", id);

  return g_temp_label;
}


void print_tac(struct tac *t) {

  int j;
  
  if (t->op == TAC_OP_DEAD)
    return;
  else if (t->op == TAC_OP_LABEL)
    fprintf(stderr, "%s:\n", t->result_s);
  else if (t->op == TAC_OP_JUMP) {
    fprintf(stderr, "    ");
    fprintf(stderr, "jmp %s\n", t->result_s);
  }
  else if (t->op == TAC_OP_ADD) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " + ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_SUB) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " - ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_MUL) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " * ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_DIV) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " / ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_MOD) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " %% ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_AND) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " & ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_OR) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " | ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_SHIFT_LEFT) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " << ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_SHIFT_RIGHT) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " >> ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_ASSIGNMENT) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");      
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_ARRAY_ASSIGNMENT) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "[");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "] := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_ARRAY_READ) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "[");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, "]\n");
  }
  else if (t->op == TAC_OP_JUMP_EQ) {
    fprintf(stderr, "    ");
    fprintf(stderr, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " == ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_JUMP_NEQ) {
    fprintf(stderr, "    ");
    fprintf(stderr, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " != ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_JUMP_LT) {
    fprintf(stderr, "    ");
    fprintf(stderr, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " < ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_JUMP_GT) {
    fprintf(stderr, "    ");
    fprintf(stderr, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " > ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_JUMP_LTE) {
    fprintf(stderr, "    ");
    fprintf(stderr, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " <= ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_JUMP_GTE) {
    fprintf(stderr, "    ");
    fprintf(stderr, "if ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, " >= ");
    _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
    fprintf(stderr, " jmp ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_RETURN) {
    fprintf(stderr, "    ");
    fprintf(stderr, "return;\n");
  }
  else if (t->op == TAC_OP_RETURN_VALUE) {
    fprintf(stderr, "    ");
    fprintf(stderr, "return ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_NOT) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := !");      
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "\n");
  }
  else if (t->op == TAC_OP_FUNCTION_CALL) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "(");
    for (j = 0; j < (int)t->arg2_d; j++) {
      _print_tac_arg(TAC_ARG_TYPE_TEMP, t->registers[j], NULL);
      if (j < (int)t->arg2_d - 1)
        fprintf(stderr, ", ");
    }
    fprintf(stderr, ")\n");
  }
  else if (t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    fprintf(stderr, "    ");
    _print_tac_arg(t->result_type, t->result_d, t->result_s);
    fprintf(stderr, " := ");
    _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
    fprintf(stderr, "(");
    for (j = 0; j < (int)t->arg2_d; j++) {
      _print_tac_arg(TAC_ARG_TYPE_TEMP, t->registers[j], NULL);
      if (j < (int)t->arg2_d - 1)
        fprintf(stderr, ", ");
    }
    fprintf(stderr, ")\n");
  }
  else if (t->op == TAC_OP_CREATE_VARIABLE) {
    fprintf(stderr, "    ");
    fprintf(stderr, "variable %s TODO\n", t->result_node->children[1]->label);
  }
}


void print_tacs(void) {

  int i;

  fprintf(stderr, "///////////////////////////////////////////////////////////////////////\n");
  fprintf(stderr, "// TACS\n");
  fprintf(stderr, "///////////////////////////////////////////////////////////////////////\n");
  
  for (i = 0; i < g_tacs_count; i++)
    print_tac(&g_tacs[i]);
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
  t->arg1_type = TAC_ARG_TYPE_CONSTANT;
  t->arg1_d = 0;
  t->arg1_s = NULL;
  t->arg2_type = TAC_ARG_TYPE_CONSTANT;
  t->arg2_d = 0;
  t->arg2_s = NULL;
  t->result_type = TAC_ARG_TYPE_CONSTANT;
  t->result_d = 0;
  t->result_s = NULL;
  t->arg1_node = NULL;
  t->arg2_node = NULL;
  t->result_node = NULL;
  t->registers = NULL;
  t->is_function_start = NO;
  
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
