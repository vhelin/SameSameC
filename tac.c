
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "printf.h"
#include "definitions.h"
#include "tac.h"


struct tac *g_tacs = NULL;
int g_tacs_count = 0, g_tacs_max = 0;


static void _print_tac_arg(int type, double d, char *s) {

  if (type == TAC_ARG_TYPE_CONSTANT)
    fprintf(stderr, "%d", (int)d);
  else if (type == TAC_ARG_TYPE_TEMP)
    fprintf(stderr, "r%d", (int)d);
  else
    fprintf(stderr, "%s", s);
}


void print_tacs(void) {

  struct tac *t;
  int i, j;

  for (i = 0; i < g_tacs_count; i++) {
    t = &g_tacs[i];

    if (t->op == TAC_OP_DEAD)
      continue;
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
    else if (t->op == TAC_OP_LOGICAL_AND) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " && ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
    }
    else if (t->op == TAC_OP_LOGICAL_OR) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " || ");
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
    else if (t->op == TAC_OP_COMPARE_LT) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " < ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
    }
    else if (t->op == TAC_OP_COMPARE_GT) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " > ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
    }
    else if (t->op == TAC_OP_COMPARE_LTE) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " <= ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
    }
    else if (t->op == TAC_OP_COMPARE_GTE) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " >= ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
    }
    else if (t->op == TAC_OP_COMPARE_EQ) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " == ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
    }
    else if (t->op == TAC_OP_COMPARE_NEQ) {
      fprintf(stderr, "    ");
      _print_tac_arg(t->result_type, t->result_d, t->result_s);
      fprintf(stderr, " := ");
      _print_tac_arg(t->arg1_type, t->arg1_d, t->arg1_s);
      fprintf(stderr, " != ");
      _print_tac_arg(t->arg2_type, t->arg2_d, t->arg2_s);
      fprintf(stderr, "\n");
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
      fprintf(stderr, "variable %s TODO\n", t->node->children[1]->label);
    }
  }
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
  t->result_type = TAC_ARG_TYPE_TEMP;
  t->result_d = 0;
  t->result_s = NULL;
  t->node = NULL;
  t->registers = NULL;
  
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
