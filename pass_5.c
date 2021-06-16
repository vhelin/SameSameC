
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "parse.h"
#include "main.h"
#include "printf.h"
#include "definitions.h"
#include "stack.h"
#include "include_file.h"
#include "pass_4.h"
#include "pass_5.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char *g_variable_types[4], *g_two_char_symbols[10], g_label[MAX_NAME_LENGTH + 1];
extern double g_parsed_double;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

int *g_register_reads = NULL, *g_register_writes = NULL;

static int g_register_reads_and_writes_count = 0;


int pass_5(void) {

  if (g_verbose_mode == ON)
    printf("Pass 5...\n");

  if (optimize_il() == FAILED)
    return FAILED;
  if (compress_register_names() == FAILED)
    return FAILED;
  
  /* TODO: reuse old registers to decrease stack usage later */

  if (propagate_operand_types() == FAILED)
    return FAILED;
  if (collect_and_preprocess_local_variables_inside_functions() == FAILED)
    return FAILED;

  /* double check that this is still true */
  if (make_sure_all_tacs_have_definition_nodes() == FAILED)
    return FAILED;

  print_tacs();
  
  return SUCCEEDED;
}


static int _find_next_living_tac(int i) {

  if (i < 0)
    return -1;
  
  while (i < g_tacs_count) {
    if (g_tacs[i].op != TAC_OP_DEAD)
      return i;
    i++;
  }

  return -1;
}


/*
static int _is_il_block_end(struct tac *t) {

  if (t->op == TAC_OP_LABEL ||
      t->op == TAC_OP_JUMP ||
      t->op == TAC_OP_JUMP_EQ ||
      t->op == TAC_OP_JUMP_LT ||
      t->op == TAC_OP_JUMP_GT ||
      t->op == TAC_OP_JUMP_NEQ ||
      t->op == TAC_OP_JUMP_LTE ||
      t->op == TAC_OP_JUMP_GTE)
    return YES;

  return NO;      
}


static int _find_end_of_il_block(int start, int *is_last_block) {

  int j = start;

  *is_last_block = NO;

  if (start >= g_tacs_count - 1) {
    *is_last_block = YES;
    return start;
  }

  fprintf(stderr, "BLOCK\n%.3d: ", j);
  print_tac(&g_tacs[j], NO, stderr);

  j++;
  
  while (j < g_tacs_count) {
    if (_is_il_block_end(&g_tacs[j]) == YES) {
      fprintf(stderr, "%.3d: ", j);
      print_tac(&g_tacs[j], NO, stderr);
      return j;
    }
    j++;
  }

  *is_last_block = YES;

  fprintf(stderr, "%.3d: ", j);
  print_tac(&g_tacs[j], NO, stderr);
  
  return j;
}
*/


static int _find_end_of_il_function(int start, int *is_last_function) {

  int j = start;

  *is_last_function = NO;

  if (start >= g_tacs_count - 1) {
    *is_last_function = YES;
    return start;
  }

  j++;
  
  while (j < g_tacs_count) {
    if (g_tacs[j].op == TAC_OP_LABEL && g_tacs[j].is_function == YES)
      return j;
    j++;
  }

  *is_last_function = YES;

  return j;
}


static int _count_and_allocate_register_usage(int start, int end) {

  int max = -1, i = start;

  /* find the biggest register this IL block uses */
  while (i <= end) {
    if (g_tacs[i].op == TAC_OP_FUNCTION_CALL) {
      int j;

      for (j = 0; j < (int)g_tacs[i].arg2_d; j++) {
        if (g_tacs[i].registers[j] > max)
          max = g_tacs[i].registers[j];
      }
    }
    else if (g_tacs[i].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < (int)g_tacs[i].arg2_d; j++) {
        if (g_tacs[i].registers[j] > max)
          max = g_tacs[i].registers[j];
      }

      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].result_d > max)
          max = (int)g_tacs[i].result_d;
      }
    }
    else if (g_tacs[i].op == TAC_OP_CREATE_VARIABLE) {
    }
    else if (g_tacs[i].op != TAC_OP_DEAD) {
      if (g_tacs[i].arg1_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].arg1_d > max)
          max = (int)g_tacs[i].arg1_d;
      }
      if (g_tacs[i].arg2_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].arg2_d > max)
          max = (int)g_tacs[i].arg2_d;
      }
      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].result_d > max)
          max = (int)g_tacs[i].result_d;
      }
    }
    i++;
  }

  max++;

  if (g_register_reads_and_writes_count < max) {
    free(g_register_reads);
    g_register_reads = (int *)calloc(sizeof(int) * max, 1);
    if (g_register_reads == NULL) {
      fprintf(stderr, "_count_and_allocate_register_usage(): Out of memory error!\n");
      return FAILED;
    }

    free(g_register_writes);
    g_register_writes = (int *)calloc(sizeof(int) * max, 1);
    if (g_register_writes == NULL) {
      fprintf(stderr, "_count_and_allocate_register_usage(): Out of memory error!\n");
      return FAILED;
    }

    g_register_reads_and_writes_count = max;
  }

  for (i = 0; i < g_register_reads_and_writes_count; i++) {
    g_register_reads[i] = 0;
    g_register_writes[i] = 0;
  }

  /* mark register reads and writes */
  i = start;
  while (i <= end) {
    if (g_tacs[i].op == TAC_OP_FUNCTION_CALL) {
      int j;

      for (j = 0; j < (int)g_tacs[i].arg2_d; j++)
        g_register_reads[g_tacs[i].registers[j]]++;
    }
    else if (g_tacs[i].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < (int)g_tacs[i].arg2_d; j++)
        g_register_reads[g_tacs[i].registers[j]]++;

      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP)
        g_register_writes[(int)g_tacs[i].result_d]++;
    }
    else if (g_tacs[i].op == TAC_OP_CREATE_VARIABLE) {
    }
    else if (g_tacs[i].op != TAC_OP_DEAD) {
      if (g_tacs[i].arg1_type == TAC_ARG_TYPE_TEMP)
        g_register_reads[(int)g_tacs[i].arg1_d]++;
      if (g_tacs[i].arg2_type == TAC_ARG_TYPE_TEMP)
        g_register_reads[(int)g_tacs[i].arg2_d]++;
      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP)
        g_register_writes[(int)g_tacs[i].result_d]++;
    }
    i++;
  }  
  
  return SUCCEEDED;
}


static int _optimize_il_1(int *optimizations_counter) {

  /*
    jmp label_* <----- REMOVE
    label_*:
  */

  int i = 0;
  while (i < g_tacs_count) {
    int current, next;

    current = _find_next_living_tac(i);
    next = _find_next_living_tac(current + 1);

    if (current < 0 || next < 0)
      break;
    
    if (g_tacs[current].op >= TAC_OP_JUMP && g_tacs[current].op <= TAC_OP_JUMP_GTE && g_tacs[next].op == TAC_OP_LABEL &&
        strcmp(g_tacs[current].result_s, g_tacs[next].result_s) == 0) {
      g_tacs[current].op = TAC_OP_DEAD;
      (*optimizations_counter)++;
    }

    i = next;
  }

  return SUCCEEDED;
}


static int _optimize_il_2(int *optimizations_counter) {
  
  /*
    jmp label_* <----- REMOVE
    label_@:
    label_*:
  */

  int i = 0;
  while (i < g_tacs_count) {
    int current, next, last;

    current = _find_next_living_tac(i);
    next = _find_next_living_tac(current + 1);
    last = _find_next_living_tac(next + 1);

    if (current < 0 || next < 0 || last < 0)
      break;
    
    if (g_tacs[current].op >= TAC_OP_JUMP && g_tacs[current].op <= TAC_OP_JUMP_GTE &&
        g_tacs[next].op == TAC_OP_LABEL && g_tacs[last].op == TAC_OP_LABEL &&
        strcmp(g_tacs[current].result_s, g_tacs[last].result_s) == 0) {
      g_tacs[current].op = TAC_OP_DEAD;
      (*optimizations_counter)++;
    }

    i = next;
  }

  return SUCCEEDED;
}


static int _optimize_il_3(int *optimizations_counter) {
  
  /*
    rX = ?  <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg1_node = g_tacs[current].arg1_node;
        g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_4(int *optimizations_counter) {
  
  /*
    rX = &? <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_GET_ADDRESS && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_result(&g_tacs[current], g_tacs[next].result_type, g_tacs[next].result_d, g_tacs[next].result_s) == FAILED)
          return FAILED;
        g_tacs[current].result_node = g_tacs[next].result_node;
        g_tacs[current].result_var_type_promoted = g_tacs[next].result_var_type_promoted;
        g_tacs[next].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_5(int *optimizations_counter) {

  /*
    rX = &?[] <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX   <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_GET_ADDRESS_ARRAY && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_result(&g_tacs[current], g_tacs[next].result_type, g_tacs[next].result_d, g_tacs[next].result_s) == FAILED)
          return FAILED;
        g_tacs[current].result_node = g_tacs[next].result_node;
        g_tacs[current].result_var_type_promoted = g_tacs[next].result_var_type_promoted;
        g_tacs[next].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_6(int *optimizations_counter) {
  
  /*
    rX = ?[]  <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX   <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ARRAY_READ && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_result(&g_tacs[current], g_tacs[next].result_type, g_tacs[next].result_d, g_tacs[next].result_s) == FAILED)
          return FAILED;
        g_tacs[current].result_node = g_tacs[next].result_node;
        g_tacs[current].result_var_type_promoted = g_tacs[next].result_var_type_promoted;
        g_tacs[next].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_7(int *optimizations_counter) {

  /*
    rX = ?      <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = &?[rX] <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_GET_ADDRESS_ARRAY &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg2_node = g_tacs[current].arg1_node;
        g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_8(int *optimizations_counter) {

  /*
    rX = ?      <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = ?[rX]  <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ARRAY_READ &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg2_node = g_tacs[current].arg1_node;
        g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_9(int *optimizations_counter) {
  
  /*
    rX   = ?  <--- REMOVE rX if rX doesn't appear elsewhere
    ?[?] = rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ARRAY_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg1_node = g_tacs[current].arg1_node;
        g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_10(int *optimizations_counter) {

  /*
    rX    = ?  <--- REMOVE rX if rX doesn't appear elsewhere
    ?[rX] = ?  <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ARRAY_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg2_node = g_tacs[current].arg1_node;
        g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_11(int *optimizations_counter) {
  
  /*
    X = ?  <--- REMOVE the first assignment
    X = ?  <--- REMOVE the first assignment
  */

  /* PS. this is not a very good optimization, doesn't work that often */
  /* PPS. make this find two assigments with n TACs between them */
  
  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == g_tacs[next].result_type &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].result_d &&
          ((g_tacs[current].result_s == NULL && g_tacs[next].result_s == NULL) ||
           (strcmp(g_tacs[current].result_s, g_tacs[next].result_s) == 0))) {
        /* found match! the first assigment can be ignored! */
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_12(int *optimizations_counter) {
  
  /*
    rX = ?    <--- REMOVE rX if rX doesn't appear elsewhere
    return rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;
    
    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_RETURN_VALUE &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg1_node = g_tacs[current].arg1_node;
        g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_13(int *optimizations_counter) {

  /*
    rX = rA * rB <--- REMOVE rX if rX doesn't appear elsewhere
     ? = rX      <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;
    
    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if ((g_tacs[current].op == TAC_OP_SHIFT_RIGHT ||
           g_tacs[current].op == TAC_OP_SHIFT_LEFT ||
           g_tacs[current].op == TAC_OP_OR ||
           g_tacs[current].op == TAC_OP_AND ||
           g_tacs[current].op == TAC_OP_MOD ||
           g_tacs[current].op == TAC_OP_MUL ||
           g_tacs[current].op == TAC_OP_DIV ||
           g_tacs[current].op == TAC_OP_SUB ||
           g_tacs[current].op == TAC_OP_ADD) &&
          g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_result(&g_tacs[current], g_tacs[next].result_type, g_tacs[next].result_d, g_tacs[next].result_s) == FAILED)
          return FAILED;
        g_tacs[current].result_node = g_tacs[next].result_node;
        g_tacs[current].result_var_type_promoted = g_tacs[next].result_var_type_promoted;
        g_tacs[next].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_14(int *optimizations_counter) {

  /*
    rX = function_call() <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX              <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_result(&g_tacs[current], g_tacs[next].result_type, g_tacs[next].result_d, g_tacs[next].result_s) == FAILED)
          return FAILED;
        g_tacs[current].result_node = g_tacs[next].result_node;
        g_tacs[current].result_var_type_promoted = g_tacs[next].result_var_type_promoted;
        g_tacs[next].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }
  
  return SUCCEEDED;
}


static int _optimize_il_15(int *optimizations_counter) {

  /*
    rX    = ?  <--- REMOVE rX if rX doesn't appear elsewhere
    if rX == ? <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && (g_tacs[next].op == TAC_OP_JUMP_EQ ||
                                                      g_tacs[next].op == TAC_OP_JUMP_LT ||
                                                      g_tacs[next].op == TAC_OP_JUMP_GT ||
                                                      g_tacs[next].op == TAC_OP_JUMP_NEQ ||
                                                      g_tacs[next].op == TAC_OP_JUMP_LTE ||
                                                      g_tacs[next].op == TAC_OP_JUMP_GTE) &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg1_node = g_tacs[current].arg1_node;
        g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_16(int *optimizations_counter) {

  /*
    rX    = ?  <--- REMOVE rX if rX doesn't appear elsewhere
    if ? == rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  int i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    while (i < end) {
      int current, next;

      current = _find_next_living_tac(i);
      next = _find_next_living_tac(current + 1);

      if (current < 0 || next < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && (g_tacs[next].op == TAC_OP_JUMP_EQ ||
                                                      g_tacs[next].op == TAC_OP_JUMP_LT ||
                                                      g_tacs[next].op == TAC_OP_JUMP_GT ||
                                                      g_tacs[next].op == TAC_OP_JUMP_NEQ ||
                                                      g_tacs[next].op == TAC_OP_JUMP_LTE ||
                                                      g_tacs[next].op == TAC_OP_JUMP_GTE) &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rX can be skipped! */
        if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
          return FAILED;
        g_tacs[next].arg2_node = g_tacs[current].arg1_node;
        g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


int optimize_il(void) {

  int optimizations_counter, loop = 1;

  /* run all optimizations until no optimizations are done */
  while (1) {
    optimizations_counter = 0;
    
    if (_optimize_il_1(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_2(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_3(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_4(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_5(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_6(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_7(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_8(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_9(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_10(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_11(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_12(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_13(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_14(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_15(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_16(&optimizations_counter) == FAILED)
      return FAILED;

    fprintf(stderr, "optimize_il(): Loop %d managed to do %d optimizations.\n", loop, optimizations_counter);
    loop++;
    
    if (optimizations_counter == 0)
      break;
  }

  return SUCCEEDED;
}


static int _rename_registers(struct tac *t, int* new_names) {

  if (t->op == TAC_OP_FUNCTION_CALL) {
    int j;

    for (j = 0; j < (int)t->arg2_d; j++) {
      if (new_names[t->registers[j]] < 0)
        return FAILED;
      t->registers[j] = new_names[t->registers[j]];
    }
  }
  else if (t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    int j;

    for (j = 0; j < (int)t->arg2_d; j++) {
      if (new_names[t->registers[j]] < 0)
        return FAILED;
      t->registers[j] = new_names[t->registers[j]];
    }
    
    if (t->result_type == TAC_ARG_TYPE_TEMP) {
      if (new_names[(int)t->result_d] < 0)
        return FAILED;
      t->result_d = new_names[(int)t->result_d];
    }
  }
  else if (t->op == TAC_OP_CREATE_VARIABLE) {
  }
  else if (t->op != TAC_OP_DEAD) {
    if (t->arg1_type == TAC_ARG_TYPE_TEMP) {
      if (new_names[(int)t->arg1_d] < 0)
        return FAILED;
      t->arg1_d = new_names[(int)t->arg1_d];
    }
    if (t->arg2_type == TAC_ARG_TYPE_TEMP) {
      if (new_names[(int)t->arg2_d] < 0)
        return FAILED;
      t->arg2_d = new_names[(int)t->arg2_d];
    }
    if (t->result_type == TAC_ARG_TYPE_TEMP) {
      if (new_names[(int)t->result_d] < 0)
        return FAILED;
      t->result_d = new_names[(int)t->result_d];
    }
  }

  return SUCCEEDED;
}


int compress_register_names(void) {

  int *register_usage, *register_new_names;
  int i, j, new_name;

  i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    /* sum reads and writes */
    for (j = 0; j < g_register_reads_and_writes_count; j++)
      g_register_reads[j] += g_register_writes[j];

    /* reuse the arrays! */
    register_usage = g_register_reads;
    register_new_names = g_register_writes;

    /* find new names for the registers */
    new_name = 0;
    for (j = 0; j < g_register_reads_and_writes_count; j++) {
      if (register_usage[j] > 0)
        register_new_names[j] = new_name++;
      else
        register_new_names[j] = -1;
    }

    /* rename the registers */
    while (i < end) {
      int current = _find_next_living_tac(i);
      if (current < 0)
        break;

      if (_rename_registers(&g_tacs[current], register_new_names) == FAILED) {
        fprintf(stderr, "compress_register_names(): Register renaming failed! Please submit a bug report!\n");
        return FAILED;
      }
          
      i = current + 1;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static char *g_temp_register_types = NULL;
static int g_temp_register_types_count = 0;


static int _set_temp_register_type(int r, int type) {

  while (r >= g_temp_register_types_count) {
    int i;
    
    g_temp_register_types = realloc(g_temp_register_types, g_temp_register_types_count + 256);
    if (g_temp_register_types == NULL) {
      fprintf(stderr, "_set_temp_register_type(): Out of memory error.\n");
      return FAILED;
    }

    for (i = g_temp_register_types_count; i < g_temp_register_types_count + 256; i++)
      g_temp_register_types[i] = 0;
    
    g_temp_register_types_count += 256;
  }

  g_temp_register_types[r] = type;

  return SUCCEEDED;
}


static int _get_temp_register_type(int r) {

  if (r >= g_temp_register_types_count)
    return VARIABLE_TYPE_NONE;

  return g_temp_register_types[r];
}


static void _clear_temp_register_types(void) {

  int i;

  for (i = 0; i < g_temp_register_types_count; i++)
    g_temp_register_types[i] = 0;
}


static int _find_operand_type(unsigned char *type, unsigned char arg_type, int value, char *label, struct tree_node *node, int allow_register_errors) {

  if (arg_type == TAC_ARG_TYPE_CONSTANT) {
    *type = get_variable_type_constant(value);
  }
  else if (arg_type == TAC_ARG_TYPE_TEMP) {
    *type = _get_temp_register_type(value);

    if (*type == VARIABLE_TYPE_NONE) {
      if (allow_register_errors == YES)
        *type = VARIABLE_TYPE_NONE;
      else {
        fprintf(stderr, "_find_operand_type(): Register %d has no type! Please submit a bug report!\n", value);
        return FAILED;
      }
    }
  }
  else if (arg_type == TAC_ARG_TYPE_LABEL) {
    int variable_type;
    double pointer_level;
    
    if (node == NULL) {
      fprintf(stderr, "_find_operand_type(): We are missing a node in a TAC for \"%s\"! Internal error. Please submit a bug report!\n", label);
      return FAILED;
    }

    variable_type = node->children[0]->value;
    pointer_level = node->children[0]->value_double;

    /* int8 and not a pointer? */
    if (variable_type == VARIABLE_TYPE_INT8 && pointer_level == 0.0)
      *type = VARIABLE_TYPE_INT8;
    /* int8 and a pointer? */
    else if (variable_type == VARIABLE_TYPE_INT8 && pointer_level >= 1.0)
      *type = VARIABLE_TYPE_UINT16;
    /* uint8 and not a pointer? */
    else if (variable_type == VARIABLE_TYPE_UINT8 && pointer_level == 0.0)
      *type = VARIABLE_TYPE_UINT8;
    /* uint8 and a pointer? */
    else if (variable_type == VARIABLE_TYPE_UINT8 && pointer_level >= 1.0)
      *type = VARIABLE_TYPE_UINT16;
    /* int16 and not a pointer? */
    else if (variable_type == VARIABLE_TYPE_INT16 && pointer_level == 0.0)
      *type = VARIABLE_TYPE_INT16;
    /* int16 and a pointer? */
    else if (variable_type == VARIABLE_TYPE_INT16 && pointer_level >= 1.0)
      *type = VARIABLE_TYPE_UINT16;
    /* uint16 and not a pointer? */
    else if (variable_type == VARIABLE_TYPE_UINT16 && pointer_level == 0.0)
      *type = VARIABLE_TYPE_UINT16;
    /* uint16 and a pointer? */
    else if (variable_type == VARIABLE_TYPE_UINT16 && pointer_level >= 1.0)
      *type = VARIABLE_TYPE_UINT16;
    /* void and a pointer? */
    else if (variable_type == VARIABLE_TYPE_VOID && pointer_level >= 1.0)
      *type = VARIABLE_TYPE_UINT16;
    else {
      fprintf(stderr, "_find_operand_type(): Variable \"%s\" is of type %d -> Not handled here! Please submit a bug report\n", node->children[1]->label, variable_type);
      return FAILED;
    }
  }
  
  return SUCCEEDED;
}


static int _get_set_max_registers_type(struct tac *t) {

  int j, args, type_max = VARIABLE_TYPE_NONE;

  /* the number of arguments */
  args = (int)t->arg2_d;

  t->registers_types = (int *)calloc(sizeof(int) * args, 1);
  if (t->registers_types == NULL) {
    fprintf(stderr, "_get_set_max_register_type(): Out of memory error.\n");
    return VARIABLE_TYPE_NONE;
  }
  
  for (j = 0; j < args; j++) {
    int type = _get_temp_register_type(t->registers[j]);

    if (type <= VARIABLE_TYPE_NONE) {
      fprintf(stderr, "_get_set_max_register_type(): A function argument type cannot be found!\n");
      return VARIABLE_TYPE_NONE;
    }

    t->registers_types[j] = type;

    type_max = get_max_variable_type_2(type, type_max);
  }

  return type_max;
}


int propagate_operand_types(void) {
  
  int i;
  
  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_DEAD) {
    }
    else if (op == TAC_OP_LABEL) {
      /* function start? */
      if (t->is_function == YES)
        _clear_temp_register_types();
    }
    else if (op == TAC_OP_ADD ||
             op == TAC_OP_SUB ||
             op == TAC_OP_MUL ||
             op == TAC_OP_DIV ||
             op == TAC_OP_MOD ||
             op == TAC_OP_AND ||
             op == TAC_OP_OR ||
             op == TAC_OP_SHIFT_LEFT ||
             op == TAC_OP_SHIFT_RIGHT) {
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1 and ARG2? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int type_max = get_max_variable_type_4(t->arg1_var_type, t->arg2_var_type, t->arg1_var_type_promoted, t->arg2_var_type_promoted);

        if (t->result_var_type == VARIABLE_TYPE_NONE) {
          /* get the type from the operands */
          t->result_var_type = type_max;
        }

        _set_temp_register_type((int)t->result_d, t->result_var_type);
      }

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->arg2_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG2!\n");
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_GET_ADDRESS) {
      /* address is always 16-bit */
      t->arg1_var_type = VARIABLE_TYPE_UINT16;

      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        t->result_var_type = VARIABLE_TYPE_UINT16;
        _set_temp_register_type((int)t->result_d, t->result_var_type);
      }

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_GET_ADDRESS_ARRAY) {
      /* address is always 16-bit */
      t->arg1_var_type = VARIABLE_TYPE_UINT16;

      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        t->result_var_type = VARIABLE_TYPE_UINT16;
        _set_temp_register_type((int)t->result_d, t->result_var_type);
      }

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->arg2_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG2!\n");
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_ASSIGNMENT) {
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int type_max = get_max_variable_type_2(t->arg1_var_type, t->arg1_var_type_promoted);

        if (t->result_var_type == VARIABLE_TYPE_NONE) {
          /* get the type from the operands */
          t->result_var_type = type_max;
        }

        _set_temp_register_type((int)t->result_d, t->result_var_type);
      }

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_ARRAY_ASSIGNMENT) {
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->arg2_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG2!\n");
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_ARRAY_READ) {
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1 and ARG2? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int type_max = get_max_variable_type_4(t->arg1_var_type, t->arg2_var_type, t->arg1_var_type_promoted, t->arg2_var_type_promoted);
        
        if (t->result_var_type == VARIABLE_TYPE_NONE) {
          /* get the type from the operands */
          t->result_var_type = type_max;
        }

        _set_temp_register_type((int)t->result_d, t->result_var_type);
      }

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->arg2_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG2!\n");
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_JUMP) {
    }
    else if (op == TAC_OP_JUMP_EQ ||
             op == TAC_OP_JUMP_NEQ ||
             op == TAC_OP_JUMP_LT ||
             op == TAC_OP_JUMP_GT ||
             op == TAC_OP_JUMP_LTE ||
             op == TAC_OP_JUMP_GTE) {
    }
    else if (op == TAC_OP_RETURN) {
    }
    else if (op == TAC_OP_RETURN_VALUE) {
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
    }
    else if (op == TAC_OP_FUNCTION_CALL) {
      int type_max = _get_set_max_registers_type(t);

      if (type_max == VARIABLE_TYPE_NONE)
        return FAILED;
    }
    else if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int type_max = _get_set_max_registers_type(t);

      if (type_max == VARIABLE_TYPE_NONE)
        return FAILED;

      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      if (t->result_var_type == VARIABLE_TYPE_NONE)
        t->result_var_type = type_max;
    }
    else if (op == TAC_OP_CREATE_VARIABLE) {
    }
    else {
      fprintf(stderr, "propagate_operand_sizes(): Unknown TAC op %d!\n", op);
      return FAILED;
    }
  }

  return SUCCEEDED;
}


static int _get_variable_size(struct tree_node *node) {

  int size = 0;

  /* value_double - pointer_depth (0 - not a pointer, 1+ is a pointer)
     value - is_array (0 - not an array, 1+ array size) */

  fprintf(stderr, "VAR: %s: ", node->children[1]->label);
  
  if (node->children[0]->value_double >= 1.0) {
    /* all pointers are two bytes in size */
    size = 16;
    fprintf(stderr, "16 (POINTER)");
  }
  else {
    int type = node->children[0]->value;

    if (type == VARIABLE_TYPE_INT8) {
      size = 8;
      fprintf(stderr, "8 (8BIT)");
    }
    else if (type == VARIABLE_TYPE_UINT8) {
      size = 8;
      fprintf(stderr, "8 (8BIT)");
    }
    else if (type == VARIABLE_TYPE_INT16) {
      size = 16;
      fprintf(stderr, "16 (16BIT)");
    }
    else if (type == VARIABLE_TYPE_UINT16) {
      size = 16;
      fprintf(stderr, "16 (16BIT)");
    }
    else {
      fprintf(stderr, "\n_get_variable_size(): Cannot determine the variable size of variable \"%s\".\n", node->children[1]->label);
      return -1;
    }
  }

  /* is it an array? */
  if (node->value > 0) {
    /* yes */
    size *= node->value;
    fprintf(stderr, " * %d", node->value);
  }

  fprintf(stderr, "\n");
  
  return size;
}


int collect_and_preprocess_local_variables_inside_functions(void) {

  int i, j, k;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES) {
      /* function start! */
      struct tree_node *function_node = t->function_node;
      struct tree_node *local_variables[1024];
      int local_variables_count = 0, register_usage[1024], used_registers = 0, register_sizes[1024];
      int offset = 0;

      for (j = 0; j < 1024; j++) {
        register_usage[j] = 0;
        register_sizes[j] = 0;
      }
      
      /* collect all local variables inside this function */
      for (i = i + 1; i < g_tacs_count; i++) {
        t = &g_tacs[i];
        op = t->op;

        if (op == TAC_OP_LABEL && t->is_function == YES) {
          i--;
          break;
        }
        else if (op == TAC_OP_CREATE_VARIABLE) {
          if (local_variables_count >= 1024) {
            fprintf(stderr, "collect_local_variables_inside_functions(): Out of space! Make variables array bigger and recompile!\n");
            return FAILED;
          }

          local_variables[local_variables_count++] = t->result_node;
        }
        else if (op == TAC_OP_DEAD) {
        }
        else {
          /* mark the used registers */
          if (t->arg1_type == TAC_ARG_TYPE_TEMP) {
            int index = (int)t->arg1_d;
            int size = get_variable_type_size(t->arg1_var_type);

            if (size == 0)
              fprintf(stderr, "collect_local_variables_inside_functions(): Register r%d size is 0!\n", index);
            
            register_usage[index] = 1;
            if (size > register_sizes[index])
              register_sizes[index] = size;
          }
          if (t->arg2_type == TAC_ARG_TYPE_TEMP) {
            int index = (int)t->arg2_d;
            int size = get_variable_type_size(t->arg2_var_type);
            
            if (size == 0)
              fprintf(stderr, "collect_local_variables_inside_functions(): Register r%d size is 0!\n", index);

            register_usage[index] = 1;
            if (size > register_sizes[index])
              register_sizes[index] = size;
          }
          if (t->result_type == TAC_ARG_TYPE_TEMP) {
            int index = (int)t->result_d;
            int size = get_variable_type_size(t->result_var_type);
            
            if (size == 0)
              fprintf(stderr, "collect_local_variables_inside_functions(): Register r%d size is 0!\n", index);

            register_usage[index] = 1;
            if (size > register_sizes[index])
              register_sizes[index] = size;
          }
        }

        /* also propagate function_node to all TACs point to their function */
        t->function_node = function_node;
      }

      /* count the used registers */
      for (j = 0; j < 1024; j++) {
        if (register_usage[j] > 0)
          used_registers++;
      }

      /* store local variables for ASM generation */
      function_node->local_variables = (struct local_variables *)calloc(sizeof(struct local_variables), 1);
      if (function_node->local_variables == NULL) {
        fprintf(stderr, "collect_local_variables_inside_functions(): Out of memory error.\n");
        return FAILED;
      }

      function_node->local_variables->local_variables_count = 0;
      function_node->local_variables->local_variables = NULL;
      function_node->local_variables->temp_registers_count = 0;
      function_node->local_variables->temp_registers = NULL;      
      
      function_node->local_variables->local_variables = (struct local_variable *)calloc(sizeof(struct local_variable) * local_variables_count, 1);
      if (function_node->local_variables->local_variables == NULL) {
        fprintf(stderr, "collect_local_variables_inside_functions(): Out of memory error.\n");
        return FAILED;
      }
      function_node->local_variables->local_variables_count = local_variables_count;

      function_node->local_variables->temp_registers = (struct temp_register *)calloc(sizeof(struct temp_register) * used_registers, 1);
      if (function_node->local_variables->temp_registers == NULL) {
        fprintf(stderr, "collect_local_variables_inside_functions(): Out of memory error.\n");
        return FAILED;
      }
      function_node->local_variables->temp_registers_count = used_registers;
      
      /* calculate the offsets */
      for (j = 0; j < local_variables_count; j++) {
        int size = _get_variable_size(local_variables[j]);

        if (size < 0)
          return FAILED;
        
        function_node->local_variables->local_variables[j].node = local_variables[j];
        function_node->local_variables->local_variables[j].offset_to_fp = offset;
        function_node->local_variables->local_variables[j].size = size;
        
        offset -= size / 8;
      }

      k = 0;
      for (j = 0; j < used_registers; j++) {
        while (k < 1024) {
          if (register_usage[k] > 0)
            break;
          k++;
        }

        function_node->local_variables->temp_registers[j].offset_to_fp = offset;
        function_node->local_variables->temp_registers[j].size = register_sizes[k];
        function_node->local_variables->temp_registers[j].register_index = k;
        
        offset -= register_sizes[k] / 8;
        
        k++;
      }
    }
  }
  
  return SUCCEEDED;
}
