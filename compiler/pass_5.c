
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
#include "stack.h"
#include "include_file.h"
#include "pass_4.h"
#include "pass_5.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"


/* define this for DEBUG */

#define DEBUG_PASS_5 1


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char *g_variable_types[4], *g_two_char_symbols[10], g_label[MAX_NAME_LENGTH + 1];
extern double g_parsed_double;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max, g_backend;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

int *g_register_reads = NULL, *g_register_writes = NULL;

static struct label *g_removed_jump_destinations_first = NULL, *g_removed_jump_destinations_last = NULL;
static int g_register_reads_and_writes_count = 0;


int pass_5(void) {

  if (g_verbose_mode == ON)
    printf("Pass 5...\n");

#if defined(DEBUG_PASS_5)
  print_tacs();
#endif

  if (optimize_for_inc() == FAILED)
    return FAILED;  
  if (optimize_il() == FAILED)
    return FAILED;
  /* make life easier for reuse_registers() */
  if (compress_register_names() == FAILED)
    return FAILED;
  if (reuse_registers() == FAILED)
    return FAILED;
  /* reuse_registers() might have removed some registers, so let's compress again */
  if (compress_register_names() == FAILED)
    return FAILED;
  if (propagate_operand_types() == FAILED)
    return FAILED;
  if (collect_and_preprocess_local_variables_inside_functions() == FAILED)
    return FAILED;
  if (delete_function_prototype_tacs() == FAILED)
    return FAILED;
  if (reorder_global_variables() == FAILED)
    return FAILED;
  
  /* double check that this is still true */
  if (make_sure_all_tacs_have_definition_nodes() == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_5)
  fprintf(stderr, ">------------------------------ OPTIMIZED IL ------------------------------>\n");
#endif
  
#if defined(DEBUG_PASS_5)
  print_tacs();
#endif

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

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP && g_tacs[i].arguments[j].value > max)
          max = (int)g_tacs[i].arguments[j].value;
      }
    }
    else if (g_tacs[i].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP && g_tacs[i].arguments[j].value > max)
          max = (int)g_tacs[i].arguments[j].value;
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

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP)
          g_register_reads[(int)g_tacs[i].arguments[j].value]++;
      }
    }
    else if (g_tacs[i].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP)
          g_register_reads[(int)g_tacs[i].arguments[j].value]++;
      }

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


static int _remember_a_removed_jump_destination(char *label) {

  struct label *l;

  /* these labels will be processed in _optimize_il_19() */

  /* is it in memory already? */
  l = g_removed_jump_destinations_first;
  while (l != NULL) {
    if (strcmp(l->label, label) == 0)
      return SUCCEEDED;
    l = l->next;
  }

  /* add a new memory */
  l = calloc(sizeof(struct label), 1);
  if (l == NULL) {
    fprintf(stderr, "_remember_a_removed_jump_destination(): Out of memory error.\n");
    return FAILED;
  }

  strcpy(l->label, label);
  l->next = NULL;
  l->references = 0;

  if (g_removed_jump_destinations_first == NULL) {
    g_removed_jump_destinations_first = l;
    g_removed_jump_destinations_last = l;
  }
  else {
    g_removed_jump_destinations_last->next = l;
    g_removed_jump_destinations_last = l;
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

      /* remember the jump target label - perhaps it can be optimized away if there are no other jumps to it? */
      if (_remember_a_removed_jump_destination(g_tacs[current].result_s) == FAILED)
        return FAILED;
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
    rX     = ? <--- REMOVE rX if rX doesn't appear elsewhere
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


static int _optimize_il_17(int *optimizations_counter) {

  /*
    if a == b <--- REMOVE the TAC if "a" and "b" are constants and the condition fails
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
      
      if (current < 0)
        break;

      if ((g_tacs[current].op == TAC_OP_JUMP_EQ ||
           g_tacs[current].op == TAC_OP_JUMP_LT ||
           g_tacs[current].op == TAC_OP_JUMP_GT ||
           g_tacs[current].op == TAC_OP_JUMP_NEQ ||
           g_tacs[current].op == TAC_OP_JUMP_LTE ||
           g_tacs[current].op == TAC_OP_JUMP_GTE) &&
          g_tacs[current].arg1_type == TAC_ARG_TYPE_CONSTANT &&
          g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT) {
        /* found match, now let's see if the condition fails */
        int op = g_tacs[current].op, arg1 = (int)g_tacs[current].arg1_d, arg2 = (int)g_tacs[current].arg2_d;
        
        if ((op == TAC_OP_JUMP_EQ && !(arg1 == arg2)) ||
            (op == TAC_OP_JUMP_LT && !(arg1 < arg2)) ||
            (op == TAC_OP_JUMP_GT && !(arg1 > arg2)) ||
            (op == TAC_OP_JUMP_NEQ && !(arg1 != arg2)) ||
            (op == TAC_OP_JUMP_LTE && !(arg1 <= arg2)) ||
            (op == TAC_OP_JUMP_GTE && !(arg1 >= arg2))) {            
          g_tacs[current].op = TAC_OP_DEAD;
          (*optimizations_counter)++;

          /* remember the jump target label - perhaps it can be optimized away if there are no other jumps to it? */
          if (_remember_a_removed_jump_destination(g_tacs[current].result_s) == FAILED)
            return FAILED;
        }
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_18(int *optimizations_counter) {

  /*
    if a == b <--- SIMPLIFY the TAC if "a" and "b" are constants and the condition is true
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
      
      if (current < 0)
        break;

      if ((g_tacs[current].op == TAC_OP_JUMP_EQ ||
           g_tacs[current].op == TAC_OP_JUMP_LT ||
           g_tacs[current].op == TAC_OP_JUMP_GT ||
           g_tacs[current].op == TAC_OP_JUMP_NEQ ||
           g_tacs[current].op == TAC_OP_JUMP_LTE ||
           g_tacs[current].op == TAC_OP_JUMP_GTE) &&
          g_tacs[current].arg1_type == TAC_ARG_TYPE_CONSTANT &&
          g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT) {
        /* found match, now let's see if the condition fails */
        int op = g_tacs[current].op, arg1 = (int)g_tacs[current].arg1_d, arg2 = (int)g_tacs[current].arg2_d;
        
        if ((op == TAC_OP_JUMP_EQ && arg1 == arg2) ||
            (op == TAC_OP_JUMP_LT && arg1 < arg2) ||
            (op == TAC_OP_JUMP_GT && arg1 > arg2) ||
            (op == TAC_OP_JUMP_NEQ && arg1 != arg2) ||
            (op == TAC_OP_JUMP_LTE && arg1 <= arg2) ||
            (op == TAC_OP_JUMP_GTE && arg1 >= arg2)) {            
          g_tacs[current].op = TAC_OP_JUMP;
          (*optimizations_counter)++;
        }
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_19(int *optimizations_counter) {

  /* if we removed a jump to a label in _optimize_il_2() or _optimize_il_17() then here we look
     for other references to those labels and if we find none then we remove the labels as well */

  struct label *l;
  int i = 0;

  /* find references to labels */
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
      
      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_JUMP ||
          g_tacs[current].op == TAC_OP_JUMP_EQ ||
          g_tacs[current].op == TAC_OP_JUMP_LT ||
          g_tacs[current].op == TAC_OP_JUMP_GT ||
          g_tacs[current].op == TAC_OP_JUMP_NEQ ||
          g_tacs[current].op == TAC_OP_JUMP_LTE ||
          g_tacs[current].op == TAC_OP_JUMP_GTE) {
        l = g_removed_jump_destinations_first;
        while (l != NULL) {
          if (strcmp(l->label, g_tacs[current].result_s) == 0)
            l->references++;
          l = l->next;
        }
      }

      /*
          g_tacs[current].op = TAC_OP_DEAD;
          (*optimizations_counter)++;
        }
      }
      */

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  i = 0;

  /* remove unreferences labels */
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
      
      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_LABEL) {
        l = g_removed_jump_destinations_first;
        while (l != NULL) {
          if (strcmp(l->label, g_tacs[current].result_s) == 0 && l->references == 0) {
            /* no references to this label -> can be removed! */
            g_tacs[current].op = TAC_OP_DEAD;
            (*optimizations_counter)++;
            break;
          }
          l = l->next;
        }
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  /* free all jump destinations from _optimize_il_17() from memory */
  l = g_removed_jump_destinations_first;
  while (l != NULL) {
    struct label *l1;

    l1 = l->next;
    free(l);
    l = l1;
  }

  g_removed_jump_destinations_first = NULL;
  g_removed_jump_destinations_last = NULL;
  
  return SUCCEEDED;
}


static int _optimize_il_20(int *optimizations_counter) {
  
  /*
    jmp label_*
    return       <----- REMOVE if this is function's last TAC
  */

  int i = 0;

  while (i < g_tacs_count) {
    int current, next, last;

    current = _find_next_living_tac(i);
    next = _find_next_living_tac(current + 1);
    last = _find_next_living_tac(next + 1);

    if (current < 0 || next < 0)
      break;
    
    if (g_tacs[current].op == TAC_OP_JUMP &&
        g_tacs[next].op == TAC_OP_RETURN &&
        (last < 0 || (g_tacs[last].op == TAC_OP_LABEL && g_tacs[last].is_function == YES))) {
      g_tacs[next].op = TAC_OP_DEAD;
      (*optimizations_counter)++;
    }

    i = next;
  }

  return SUCCEEDED;
}


static int _optimize_il_21(int *optimizations_counter) {

  /*
    rA = ?       <--- REMOVE rA if rA doesn't appear elsewhere
    rX = rA * rB <--- REMOVE rA if rA doesn't appear elsewhere
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

      if ((g_tacs[next].op == TAC_OP_SHIFT_RIGHT ||
           g_tacs[next].op == TAC_OP_SHIFT_LEFT ||
           g_tacs[next].op == TAC_OP_OR ||
           g_tacs[next].op == TAC_OP_AND ||
           g_tacs[next].op == TAC_OP_MOD ||
           g_tacs[next].op == TAC_OP_MUL ||
           g_tacs[next].op == TAC_OP_DIV ||
           g_tacs[next].op == TAC_OP_SUB ||
           g_tacs[next].op == TAC_OP_ADD) &&
          g_tacs[current].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rA can be skipped! */
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


static int _optimize_il_22(int *optimizations_counter) {

  /*
    rB = ?       <--- REMOVE rB if rA doesn't appear elsewhere
    ...
    rX = rA * rB <--- REMOVE rB if rA doesn't appear elsewhere
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

      if ((g_tacs[next].op == TAC_OP_SHIFT_RIGHT ||
           g_tacs[next].op == TAC_OP_SHIFT_LEFT ||
           g_tacs[next].op == TAC_OP_OR ||
           g_tacs[next].op == TAC_OP_AND ||
           g_tacs[next].op == TAC_OP_MOD ||
           g_tacs[next].op == TAC_OP_MUL ||
           g_tacs[next].op == TAC_OP_DIV ||
           g_tacs[next].op == TAC_OP_SUB ||
           g_tacs[next].op == TAC_OP_ADD) &&
          g_tacs[current].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! rA can be skipped! */
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


static int _get_registers_index_in_function_call_arguments(struct tac *t, int reg) {

  int i;

  for (i = 0; i < t->arguments_count; i++) {
    if (t->arguments[i].type == TAC_ARG_TYPE_TEMP && ((int)t->arguments[i].value) == reg)
      return i;
  }

  return -1;
}


static int _optimize_il_23(int *optimizations_counter) {
  
  /*
    rX = ?       <--- REMOVE rX if rX doesn't appear elsewhere
    ...
    ?  = rX * rB <--- REMOVE rX if rX doesn't appear elsewhere
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

      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        next = current;

        while (1) {
          next = _find_next_living_tac(next + 1);

          if (next < 0)
            break;

          if (g_tacs[next].op == TAC_OP_LABEL) {
            /* we cannot search past a label */
            next = -1;
            break;
          }
          else if (g_tacs[next].op == TAC_OP_FUNCTION_CALL || g_tacs[next].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
            int index;

            index = _get_registers_index_in_function_call_arguments(&g_tacs[next], (int)g_tacs[current].result_d);
            if (index >= 0) {
              /* found match! rX can be removed, variable/constant embedded! */
              struct function_argument *arg = &(g_tacs[next].arguments[index]);

              arg->type = g_tacs[current].arg1_type;
              arg->value = g_tacs[current].arg1_d;
              arg->label = g_tacs[current].arg1_s; /* NOTE! we move the label (if there's one) */
              arg->node = g_tacs[current].arg1_node;
              arg->var_type = g_tacs[current].arg1_var_type;
              arg->var_type_promoted = g_tacs[current].arg1_var_type_promoted;

              g_tacs[current].arg1_s = NULL;
              g_tacs[current].arg1_node = NULL;
              g_tacs[current].op = TAC_OP_DEAD;
              (*optimizations_counter)++;
            }
            
            /* let's not serach past a function call */
            next = -1;
            break;
          }
          else if (g_tacs[next].op == TAC_OP_CREATE_VARIABLE) {
          }
          else if (g_tacs[next].op != TAC_OP_DEAD) {
            if (g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) {
              /* found match! rX can be removed, variable/constant embedded! */
              if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                return FAILED;
              g_tacs[next].arg1_node = g_tacs[current].arg1_node;
              g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
              g_tacs[current].op = TAC_OP_DEAD;
              (*optimizations_counter)++;
              next = -1;
              break;
            }
            if (g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) {
              /* found match! rX can be removed, constant embedded! */
              if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                return FAILED;
              g_tacs[next].arg2_node = g_tacs[current].arg1_node;
              g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
              g_tacs[current].op = TAC_OP_DEAD;
              (*optimizations_counter)++;
              next = -1;
              break;
            }
          }

          if (next < 0)
            break;
        }
      }

      i = current + 1;
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
    if (_optimize_il_17(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_18(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_19(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_20(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_21(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_22(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_23(&optimizations_counter) == FAILED)
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

    for (j = 0; j < t->arguments_count; j++) {
      if (t->arguments[j].type == TAC_ARG_TYPE_TEMP) {
        if (new_names[(int)t->arguments[j].value] < 0)
          return FAILED;
        t->arguments[j].value = new_names[(int)t->arguments[j].value];
      }
    }
  }
  else if (t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    int j;

    for (j = 0; j < t->arguments_count; j++) {
      if (t->arguments[j].type == TAC_ARG_TYPE_TEMP) {
        if (new_names[(int)t->arguments[j].value] < 0)
          return FAILED;
        t->arguments[j].value = new_names[(int)t->arguments[j].value];
      }
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


static void _find_first_and_last_register_usage(int start, int end, int reg, int *first, int *last) {

  int i, found_it = NO, item_1 = -1, item_2 = -1;

  for (i = start; i < end; i++) {
    if (g_tacs[i].op == TAC_OP_FUNCTION_CALL) {
      int j;

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP && g_tacs[i].arguments[j].value == reg) {
          if (found_it == NO) {
            found_it = YES;
            item_1 = i;
          }
          else
            item_2 = i;
        }
      }
    }
    else if (g_tacs[i].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP && g_tacs[i].arguments[j].value == reg) {
          if (found_it == NO) {
            found_it = YES;
            item_1 = i;
          }
          else
            item_2 = i;
        }
      }

      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].result_d == reg) {
          if (found_it == NO) {
            found_it = YES;
            item_1 = i;
          }
          else
            item_2 = i;
        }
      }
    }
    else if (g_tacs[i].op == TAC_OP_CREATE_VARIABLE) {
    }
    else if (g_tacs[i].op != TAC_OP_DEAD) {
      if (g_tacs[i].arg1_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].arg1_d == reg) {
          if (found_it == NO) {
            found_it = YES;
            item_1 = i;
          }
          else
            item_2 = i;
        }
      }
      if (g_tacs[i].arg2_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].arg2_d == reg) {
          if (found_it == NO) {
            found_it = YES;
            item_1 = i;
          }
          else
            item_2 = i;
        }
      }
      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].result_d == reg) {
          if (found_it == NO) {
            found_it = YES;
            item_1 = i;
          }
          else
            item_2 = i;
        }
      }
    }
  }

  *first = item_1;
  *last = item_2;
}


static void _rename_register(int start, int end, int r2, int r1) {

  int i;

  for (i = start; i < end; i++) {
    if (g_tacs[i].op == TAC_OP_FUNCTION_CALL) {
      int j;

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP && g_tacs[i].arguments[j].value == r2)
          g_tacs[i].arguments[j].value = r1;
      }
    }
    else if (g_tacs[i].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < g_tacs[i].arguments_count; j++) {
        if (g_tacs[i].arguments[j].type == TAC_ARG_TYPE_TEMP && g_tacs[i].arguments[j].value == r2)
          g_tacs[i].arguments[j].value = r1;
      }

      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].result_d == r2)
          g_tacs[i].result_d = r1;
      }
    }
    else if (g_tacs[i].op == TAC_OP_CREATE_VARIABLE) {
    }
    else if (g_tacs[i].op != TAC_OP_DEAD) {
      if (g_tacs[i].arg1_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].arg1_d == r2)
          g_tacs[i].arg1_d = r1;
      }
      if (g_tacs[i].arg2_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].arg2_d == r2)
          g_tacs[i].arg2_d = r1;
      }
      if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP) {
        if ((int)g_tacs[i].result_d == r2)
          g_tacs[i].result_d = r1;
      }
    }
  }
}


int reuse_registers(void) {

  int *register_usage;
  int i, j, r1, r2;

  i = 0;
  while (1) {
    int is_last_function = NO;
    int end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return FAILED;

    /* a function spans from "i" to "end-1" */

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return FAILED;

    for (r1 = 0; r1 < g_register_reads_and_writes_count; r1++) {
      int first1 = -1, last1 = -1, first2 = -1, last2 = -1;
      
      if (_count_and_allocate_register_usage(i, end) == FAILED)
        return FAILED;

      /* sum reads and writes */
      for (j = 0; j < g_register_reads_and_writes_count; j++)
        g_register_reads[j] += g_register_writes[j];

      /* reuse the arrays! */
      register_usage = g_register_reads;

      if (register_usage[r1] == 0)
        continue;

      /* we have a register that's used! let's find the first and last usage */
      _find_first_and_last_register_usage(i, end, r1, &first1, &last1);

      if (first1 == -1 || last1 == -1) {
        /* the register has been removed */
        continue;
      }

      /* we have a span (first, last) for a register r1. find a bigger register r2 which life span
         is after r1, and reuse this register (rename r2 to r1) */

      /* TODO: find the register r2 with the optimal span */

      for (r2 = r1 + 1; r2 < g_register_reads_and_writes_count; r2++) {
        if (register_usage[r2] == 0)
          continue;

        first2 = -1;
        last2 = -1;

        /* we have a register that's used! let's find the first and last usage */
        _find_first_and_last_register_usage(i, end, r2, &first2, &last2);

        if (first2 == -1 || last2 == -1) {
          /* the register has been removed */
          continue;
        }

        if (first2 < last1) {
          /* r2 is alive before r1 dies */
          continue;
        }

        /* we found it! */
        break;
      }

      if (r2 == g_register_reads_and_writes_count) {
        /* no r2 was found -> move to the next r1 */
        continue;
      }

      /* rename r2 to r1! */
      _rename_register(i, end, r2, r1);

      /* redo r1 */
      r1--;
    }
    
    i = end;

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

      /* RESULT from ARG1? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        if (t->result_var_type == VARIABLE_TYPE_NONE) {
          /* get the type from the operands */
          t->result_var_type = get_array_item_variable_type(t->arg1_node->children[0], t->arg1_node->children[0]->label);
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
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->arg2_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG2!\n");
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
      /* if t->arguments == NULL that means that the function was called with 0 arguments */
      if (t->arguments != NULL) {
        int j;

        for (j = 0; j < t->arguments_count; j++) {
          if (_find_operand_type(&(t->arguments[j].var_type), t->arguments[j].type, (int)t->arguments[j].value, t->arguments[j].label, t->arguments[j].node, NO) == FAILED)
            return FAILED;
        }
      }
    }
    else if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      /* if t->arguments == NULL that means that the function was called with 0 arguments */
      if (t->arguments != NULL) {
        int j;

        for (j = 0; j < t->arguments_count; j++) {
          if (_find_operand_type(&(t->arguments[j].var_type), t->arguments[j].type, (int)t->arguments[j].value, t->arguments[j].label, t->arguments[j].node, NO) == FAILED)
            return FAILED;
        }
      }

      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
    }
    else if (op == TAC_OP_CREATE_VARIABLE) {
    }
    else if (op == TAC_OP_ASM) {
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

  if (node->children[0]->value_double >= 1.0) {
    /* all pointers are two bytes in size */
    size = 16;
  }
  else {
    int type = node->children[0]->value;

    if (type == VARIABLE_TYPE_INT8)
      size = 8;
    else if (type == VARIABLE_TYPE_UINT8)
      size = 8;
    else if (type == VARIABLE_TYPE_INT16)
      size = 16;
    else if (type == VARIABLE_TYPE_UINT16)
      size = 16;
    else {
      fprintf(stderr, "_get_variable_size(): Cannot determine the variable size of variable \"%s\".\n", node->children[1]->label);
      return -1;
    }
  }

  /* is it an array? */
  if (node->value > 0) {
    /* yes */
    size *= node->value;
  }

  return size;
}


#define LOCAL_VAR_COUNT 1024
#define REG_COUNT (1024*8)


int collect_and_preprocess_local_variables_inside_functions(void) {

  int i, j, k;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES) {
      /* function start! */
      struct tree_node *function_node = t->function_node;
      struct tree_node *local_variables[LOCAL_VAR_COUNT];
      int local_variables_count = 0, register_usage[REG_COUNT], used_registers = 0, register_sizes[REG_COUNT], arguments_count = 0;
      int offset = -4; /* the first 2 bytes in the stack frame are for return address, next 2 bytes are old stack frame address */
      int return_value_type, return_value_size;

      /* NOTE: if the function returns a value we'll need to allocate space for that in the stack frame */        
      return_value_type = tree_node_get_max_var_type(function_node->children[0]);
      return_value_size = get_variable_type_size(return_value_type) / 8;
      offset -= return_value_size;

      for (j = 0; j < REG_COUNT; j++) {
        register_usage[j] = 0;
        register_sizes[j] = 0;
      }
      
      /* collect all (except consts) local variables inside this function */
      for (i = i + 1; i < g_tacs_count; i++) {
        t = &g_tacs[i];
        op = t->op;

        if (op == TAC_OP_LABEL && t->is_function == YES) {
          i--;
          break;
        }
        else if (op == TAC_OP_CREATE_VARIABLE) {
          /* skip consts as they are placed at the end of the function (later in ASM) */
          if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE &&
              ((t->result_node->children[0]->value_double == 0 && (t->result_node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
               (t->result_node->children[0]->value_double > 0 && (t->result_node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2))) {
            char local_label[MAX_NAME_LENGTH + 1];
            
            /* skipping a local const variable... */

            /* ... and here we rename it to be a local label, saves a lot of work later, all references
               pointing to the original tree_node that contains the variable creation can use the new name */
            snprintf(local_label, sizeof(local_label), "_%s", t->result_node->children[1]->label);
            strcpy(t->result_node->children[1]->label, local_label);
          }
          else {          
            if (local_variables_count >= LOCAL_VAR_COUNT) {
              fprintf(stderr, "collect_local_variables_inside_functions(): Out of space! Make variables array bigger and recompile!\n");
              return FAILED;
            }

            /* is this a function argument? */
            if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
              arguments_count++;

            local_variables[local_variables_count++] = t->result_node;
          }
        }
        else if (op == TAC_OP_DEAD) {
        }
        else {
          /* mark the used registers and collect the max sizes for the registers. NOTE! the register size might
             change during the search as we've reused some registers possibly and in some cases the register
             might be 8-bit and in some cases 16-bit... */
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
      for (j = 0; j < REG_COUNT; j++) {
        if (register_usage[j] > 0)
          used_registers++;
      }

      /* store local variables for ASM generation */
      function_node->local_variables = (struct local_variables *)calloc(sizeof(struct local_variables), 1);
      if (function_node->local_variables == NULL) {
        fprintf(stderr, "collect_local_variables_inside_functions(): Out of memory error.\n");
        return FAILED;
      }

      function_node->local_variables->arguments_count = 0;
      function_node->local_variables->local_variables_count = 0;
      function_node->local_variables->local_variables = NULL;
      function_node->local_variables->temp_registers_count = 0;
      function_node->local_variables->temp_registers = NULL;      
      function_node->local_variables->offset_to_fp_total = 0;
      
      function_node->local_variables->local_variables = (struct local_variable *)calloc(sizeof(struct local_variable) * local_variables_count, 1);
      if (function_node->local_variables->local_variables == NULL) {
        fprintf(stderr, "collect_local_variables_inside_functions(): Out of memory error.\n");
        return FAILED;
      }
      function_node->local_variables->local_variables_count = local_variables_count;
      function_node->local_variables->arguments_count = arguments_count;

      function_node->local_variables->temp_registers = (struct temp_register *)calloc(sizeof(struct temp_register) * used_registers, 1);
      if (function_node->local_variables->temp_registers == NULL) {
        fprintf(stderr, "collect_local_variables_inside_functions(): Out of memory error.\n");
        return FAILED;
      }
      function_node->local_variables->temp_registers_count = used_registers;

#if defined(DEBUG_PASS_5)
      fprintf(stderr, "*** FUNCTION \"%s\" STACK FRAME\n", function_node->children[1]->label);
      fprintf(stderr, "OFFSET %.5d SIZE %.6d \"return address\"\n", -1, 2);
      fprintf(stderr, "OFFSET %.5d SIZE %.6d \"caller's stack frame address\"\n", -3, 2);
      if (return_value_size > 0)
        fprintf(stderr, "OFFSET %.5d SIZE %.6d \"return value\"\n", -4 - (return_value_size - 1), return_value_size);
#endif

      /* calculate the offsets */
      for (j = 0; j < local_variables_count; j++) {
        int size = _get_variable_size(local_variables[j]);

        if (size < 0)
          return FAILED;

        /* note that the offset points to the very first byte of the variable, and we count _up_ from there */
        function_node->local_variables->local_variables[j].node = local_variables[j];
        function_node->local_variables->local_variables[j].offset_to_fp = offset - ((size / 8) - 1);
        function_node->local_variables->local_variables[j].size = size;

#if defined(DEBUG_PASS_5)
        fprintf(stderr, "OFFSET %.5d SIZE %.6d VARIABLE %s\n", function_node->local_variables->local_variables[j].offset_to_fp, size/8, local_variables[j]->children[1]->label);
#endif
        
        offset -= size / 8;
      }

      k = 0;
      for (j = 0; j < used_registers; j++) {
        while (k < REG_COUNT) {
          if (register_usage[k] > 0)
            break;
          k++;
        }

        function_node->local_variables->temp_registers[j].offset_to_fp = offset - ((register_sizes[k] / 8) - 1);
        function_node->local_variables->temp_registers[j].size = register_sizes[k];
        function_node->local_variables->temp_registers[j].register_index = k;

#if defined(DEBUG_PASS_5)
        fprintf(stderr, "OFFSET %.5d SIZE %.6d REGISTER r%d\n", function_node->local_variables->temp_registers[j].offset_to_fp, register_sizes[k]/8, k);
#endif
        
        offset -= register_sizes[k] / 8;
        
        k++;
      }

      function_node->local_variables->offset_to_fp_total = offset;

      /*
      fprintf(stderr, "FUNCTION %s LOCAL VARIABLES %d (%d are arguments) TEMP REGISTERS %d\n", function_node->children[1]->label, local_variables_count, arguments_count, used_registers);
      */
    }
  }
  
  return SUCCEEDED;
}


int optimize_for_inc(void) {

  int i;
  
  /* here we go through TACs and if we find TAC_OP_ADD with 1 in arg1 we'll swap arg1 and arg2 */

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    if (t->op == TAC_OP_ADD && t->arg1_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg1_d) == 1)
      tac_swap_args(t);
  }
  
  return SUCCEEDED;
}


int delete_function_prototype_tacs(void) {

  int i;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    if (t->op == TAC_OP_LABEL && t->is_function == YES && t->function_node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
      /* found a function prototype -> delete as its job is done */
      t->op = TAC_OP_DEAD;
      i++;

      while (i < g_tacs_count) {
        t = &g_tacs[i];
        if (t->op == TAC_OP_LABEL && t->is_function == YES) {
          /* process this TAC again next iteration */
          i--;
          break;
        }
        t->op = TAC_OP_DEAD;
        i++;
      }
    }
  }
  
  return SUCCEEDED;
}


int reorder_global_variables(void) {

  int i, global_variables = 0, indices[1024], index;
  struct tree_node *nodes[1024];
  
  /* we want to reorder global variables so that first come variables that are fully initialized, then come
     those that are partially initialized, and last come uninitialized variables. this ordering will make
     the global variable initializer generator to produce less code. const variables are also put last
     as they are not copied to RAM. */

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if (global_variables >= 1024) {
        fprintf(stderr, "reorder_global_variables(): You have more than 1024 global variables, and we ran out of buffer! Please submit a bug report!\n");
        return FAILED;
      }
      indices[global_variables] = i;
      global_variables++;
    }
  }

  /* 0. mark all const data */
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->children[0]->value_double == 0 && (node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
          (node->children[0]->value_double > 0 && (node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2))
        node->flags |= TREE_NODE_FLAG_DATA_IS_CONST;
    }
  }
  
  /* 1. collect fully initialized non-const variables */
  index = 0;
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        if (node->value == 0) {
          /* not an array */
          if (node->added_children > 2)
            nodes[index++] = node;
        }
        else {
          /* an array */
          if (tree_node_get_create_variable_data_items(node) == node->value)
            nodes[index++] = node;
        }
      }
    }
  }

  /* 2. collect partially initialized non-const variables */
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        if (node->value > 0) {
          /* an array */
          int items = tree_node_get_create_variable_data_items(node);
          if (items < node->value && items > 0)
            nodes[index++] = node;
        }
      }
    }
  }

  /* 3. collect uninitialized and const variables */
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if (node->value == 0) {
        /* not an array */
        if (node->added_children == 2 || (node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == TREE_NODE_FLAG_DATA_IS_CONST)
          nodes[index++] = node;
      }
      else {
        /* an array */
        int items = tree_node_get_create_variable_data_items(node);
        if (items == 0 || (node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == TREE_NODE_FLAG_DATA_IS_CONST)
          nodes[index++] = node;
      }
    }
  }

  /* sanity check */
  if (index != global_variables) {
    fprintf(stderr, "reorder_global_variables(): We counted %d global variables, but captured %d! These values should match! Please submit a bug report!\n", global_variables, index);
    return FAILED;
  }

  /* reorder the global variables */
  for (i = 0; i < global_variables; i++)
    g_global_nodes->children[indices[i]] = nodes[i];
  
  return SUCCEEDED;
}
