
#include <ctype.h>
#include <limits.h>
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
#include "struct_item.h"
#include "register_allocator.h"
#include "z80_register_spill.h"


/* define this for DEBUG */

#define DEBUG_PASS_5 1

#define REGISTER_ALLOCATOR_POISON_OFFSET (-32768)


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char *g_variable_types[9], *g_two_char_symbols[17], g_label[MAX_NAME_LENGTH + 1];
extern double g_parsed_double;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max, g_backend;
extern int g_allocator_enabled, g_allocator_force_all_spill;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

char *g_temp_register_types = NULL;
int g_temp_register_types_count = 0;
int *g_register_reads = NULL, *g_register_writes = NULL;

static struct label *g_removed_jump_destinations_first = NULL, *g_removed_jump_destinations_last = NULL;
static int g_register_reads_and_writes_count = 0, g_local_variable_running_index = 1;

#if defined(DEBUG_PASS_5)
static void _debug_register_allocator_temp_compaction_stage(char *stage);
#endif


int pass_5(void) {

  if (g_verbose_mode == ON)
    printf("Pass 5...\n");

#if defined(DEBUG_PASS_5)
  print_tacs();
#endif

  if (optimize_for_inc() == FAILED)
    return FAILED;
  if (turn_some_muls_and_divs_into_shifts() == FAILED)
    return FAILED;
  if (optimize_il() == FAILED)
    return FAILED;
  /* make life easier for reuse_registers() */
#if defined(DEBUG_PASS_5)
  _debug_register_allocator_temp_compaction_stage("before_first_compress");
#endif
  if (compress_register_names() == FAILED)
    return FAILED;
#if defined(DEBUG_PASS_5)
  _debug_register_allocator_temp_compaction_stage("after_first_compress");
#endif
  if (propagate_operand_types() == FAILED)
    return FAILED;
  if (reuse_registers() == FAILED)
    return FAILED;
#if defined(DEBUG_PASS_5)
  _debug_register_allocator_temp_compaction_stage("after_reuse");
#endif
  /* reuse_registers() might have removed some registers, so let's compress again */
  if (compress_register_names() == FAILED)
    return FAILED;
#if defined(DEBUG_PASS_5)
  _debug_register_allocator_temp_compaction_stage("after_second_compress");
#endif
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
    if (g_tacs[i].op != TAC_OP_DEAD && g_tacs[i].op != TAC_OP_CREATE_VARIABLE)
      return i;
    i++;
  }

  return -1;
}


static int _find_next_living_tac_all(int i) {

  if (i < 0)
    return -1;

  while (i < g_tacs_count) {
    if (g_tacs[i].op == TAC_OP_CREATE_VARIABLE && g_tacs[i].result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT) {
    }
    else if (g_tacs[i].op != TAC_OP_DEAD)
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
      if (g_tacs[i].op == TAC_OP_ARRAY_WRITE && g_tacs[i].result_type == TAC_ARG_TYPE_TEMP)
        g_register_reads[(int)g_tacs[i].result_d]++;
      else if (g_tacs[i].result_type == TAC_ARG_TYPE_TEMP)
        g_register_writes[(int)g_tacs[i].result_d]++;
    }
    i++;
  }

  return SUCCEEDED;
}


#if defined(DEBUG_PASS_5)
static char *_debug_get_function_name_for_range(int start, int end) {

  int i;

  for (i = start; i <= end && i < g_tacs_count; i++) {
    if (g_tacs[i].op == TAC_OP_LABEL && g_tacs[i].is_function == YES && g_tacs[i].function_node != NULL)
      return g_tacs[i].function_node->children[1]->label;
  }

  return "<global>";
}


static void _debug_register_allocator_temp_compaction_stage(char *stage) {

  int i;

  if (g_allocator_enabled == NO)
    return;

  i = 0;
  while (1) {
    int end;
    int is_last_function;
    int used_registers;
    int highest_register;
    int j;

    is_last_function = NO;
    end = _find_end_of_il_function(i, &is_last_function);
    if (end < 0)
      return;

    if (_count_and_allocate_register_usage(i, end) == FAILED)
      return;

    used_registers = 0;
    highest_register = -1;
    for (j = 0; j < g_register_reads_and_writes_count; j++) {
      if (g_register_reads[j] + g_register_writes[j] > 0) {
        used_registers++;
        highest_register = j;
      }
    }

    fprintf(stderr, "register_allocator: temp_compaction stage=%s function=%s temps=%d highest_r=%d\n", stage, _debug_get_function_name_for_range(i, end), used_registers, highest_register);

    i = end;
    if (is_last_function == YES)
      break;
  }
}
#endif


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


/* NOTE! because SAS/C on Amiga thinks that this and the next function were called the same
   prefixes "aaa" and "bbb" were added to separate them */


static int _aaa_get_argument_index_in_function_call_arguments(struct tac *t, int type, struct tree_node *label_node, int r) {

  int i;

  for (i = 0; i < t->arguments_count; i++) {
    if (t->arguments[i].type == type) {
      if (type == TAC_ARG_TYPE_TEMP && r == (int)t->arguments[i].value)
        return i;
      if (type == TAC_ARG_TYPE_LABEL && label_node == t->arguments[i].node)
        return i;
    }
  }

  return -1;
}


static int _bbb_get_argument_index_in_function_call_arguments_with_tree_node(struct tac *t, struct tree_node *node) {

  int i;

  for (i = 0; i < t->arguments_count; i++) {
    if (t->arguments[i].node == node)
        return i;
  }

  return -1;
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
#if defined(DEBUG_PASS_5)
      fprintf(stderr, "_optimize_il_1(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
      fprintf(stderr, "_optimize_il_2(): SUCCESS!\n");
#endif

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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_3(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_4(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_5(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_6(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_7(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_8(): SUCCESS!\n");
#endif
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ARRAY_WRITE &&
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_9(): SUCCESS!\n");
#endif
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[next].op == TAC_OP_ARRAY_WRITE &&
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_10(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_12(): SUCCESS!\n");
#endif
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
           g_tacs[current].op == TAC_OP_XOR ||
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_13(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_14(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_15(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_16(): SUCCESS!\n");
#endif

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
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "_optimize_il_17(): SUCCESS!\n");
#endif

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
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "_optimize_il_18(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
            fprintf(stderr, "_optimize_il_19(): SUCCESS!\n");
#endif
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
#if defined(DEBUG_PASS_5)
      fprintf(stderr, "_optimize_il_20(): SUCCESS!\n");
#endif
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
           g_tacs[next].op == TAC_OP_XOR ||
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_21(): SUCCESS!\n");
#endif
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
           g_tacs[next].op == TAC_OP_XOR ||
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_22(): SUCCESS!\n");
#endif
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_23(int *optimizations_counter) {

  /*
    var = A
    ...
    function(var) <--- embed var if possible
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT) {
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

          if (g_tacs[next].result_type == TAC_ARG_TYPE_TEMP && g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].result_d) {
            /* cannot jump over writes to the same register */
            next = -1;
            break;
          }
          if (g_tacs[next].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].result_node) {
            /* cannot jump over writes to the same variable */
            next = -1;
            break;
          }

          if (g_tacs[next].result_type == TAC_ARG_TYPE_TEMP && g_tacs[current].arg1_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].arg1_d == (int)g_tacs[next].result_d) {
            /* cannot jump over writes to A */
            next = -1;
            break;
          }
          if (g_tacs[next].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].arg1_type == TAC_ARG_TYPE_LABEL && g_tacs[current].arg1_node == g_tacs[next].result_node) {
            /* cannot jump over writes to A */
            next = -1;
            break;
          }

          if (g_tacs[next].op == TAC_OP_FUNCTION_CALL || g_tacs[next].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
            int index;

            index = _aaa_get_argument_index_in_function_call_arguments(&g_tacs[next], g_tacs[current].result_type, g_tacs[current].result_node, (int)g_tacs[current].result_d);
            if (index >= 0) {
              /* found match! rX can be removed, variable/constant embedded! */
              struct function_argument *arg = &(g_tacs[next].arguments[index]);

	      /* NOTE! we don't want to leak memory... */
	      free(arg->label);

              arg->type = g_tacs[current].arg1_type;
              arg->value = g_tacs[current].arg1_d;
              if (arg->type == TAC_ARG_TYPE_TEMP)
                arg->original_register_index = g_tacs[current].arg1_original_register_index;
              else
                arg->original_register_index = -1;
              arg->label = g_tacs[current].arg1_s; /* NOTE! we move the label (if there's one) */
              arg->node = g_tacs[current].arg1_node;
              arg->var_type = g_tacs[current].arg1_var_type;
              /* NOTE: use the target's promotion
              arg->var_type_promoted = g_tacs[current].arg1_var_type_promoted;
              */

              g_tacs[current].arg1_s = NULL;
              g_tacs[current].arg1_node = NULL;
              g_tacs[current].op = TAC_OP_DEAD;
              (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
              fprintf(stderr, "_optimize_il_23(): SUCCESS!\n");
#endif
            }

            /* let's not serach past a function call */
            next = -1;
            break;
          }
          else if (g_tacs[next].op == TAC_OP_CREATE_VARIABLE) {
          }
          else if (g_tacs[next].op != TAC_OP_DEAD) {
            if ((g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP && g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) ||
                (g_tacs[next].arg1_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg1_node)) {
              if (g_tacs[next].op == TAC_OP_GET_ADDRESS || (g_allocator_enabled == YES && (g_tacs[next].op == TAC_OP_ARRAY_READ || g_tacs[next].op == TAC_OP_GET_ADDRESS_ARRAY))) {
                /* cannot optimize past address-taking TACs */
                next = -1;
                break;
              }
              else {
                /* found match! variable/constant can be embedded! */
                if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                  return FAILED;
                g_tacs[next].arg1_node = g_tacs[current].arg1_node;
                /* NOTE: use the target's promotion
                g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
                */
                (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
                fprintf(stderr, "_optimize_il_23(): SUCCESS!\n");
#endif
                next = -1;
                break;
              }
            }
            if ((g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP && g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) ||
                (g_tacs[next].arg2_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg2_node)) {
              if (g_tacs[next].op == TAC_OP_GET_ADDRESS) {
                /* cannot optimize past TAC_OP_GET_ADDRESS */
                next = -1;
                break;
              }
              else {
                /* found match! variable/constant can be embedded! */
                if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                  return FAILED;
                g_tacs[next].arg2_node = g_tacs[current].arg1_node;
                /* NOTE: use the target's promotion
                g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
                */
                (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
                fprintf(stderr, "_optimize_il_23(): SUCCESS!\n");
#endif
                next = -1;
                break;
              }
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


static int _optimize_il_24(int *optimizations_counter) {

  /*
    rX = ~? <--- REMOVE rX if rX doesn't appear elsewhere
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

      if (g_tacs[current].op == TAC_OP_COMPLEMENT && g_tacs[next].op == TAC_OP_ASSIGNMENT &&
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
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_24(): SUCCESS!\n");
#endif
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_25(int *optimizations_counter) {

  /*
    rX = rA + C <--- COMBINE if rX doesn't appear elsewhere
     ? = rX + D <--- COMBINE if rX doesn't appear elsewhere
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

      if (g_tacs[current].op == TAC_OP_ADD &&
          g_tacs[next].op == TAC_OP_ADD &&
          g_tacs[current].arg1_type == TAC_ARG_TYPE_TEMP && g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP && g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT && g_tacs[next].arg2_type == TAC_ARG_TYPE_CONSTANT &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! COMBINE! */
        g_tacs[next].arg1_d = g_tacs[current].arg1_d;
        g_tacs[next].arg2_d = (int)g_tacs[current].arg2_d + (int)g_tacs[next].arg2_d;
        g_tacs[current].op = TAC_OP_DEAD;
        (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_25(): SUCCESS!\n");
#endif
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_26(int *optimizations_counter) {

  /*
    rX = rA + 0 <--- REDUCE into assignment
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
      int current;

      current = _find_next_living_tac(i);

      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_ADD &&
          g_tacs[current].arg1_type == TAC_ARG_TYPE_CONSTANT && (int)g_tacs[current].arg1_d == 0) {
        /* found match! REDUCE! */
        g_tacs[current].op = TAC_OP_ASSIGNMENT;
        if (tac_copy_arg(&g_tacs[current], TAC_USE_ARG2, TAC_USE_ARG1) == FAILED)
          return FAILED;
        (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_26(): SUCCESS!\n");
#endif
      }
      else if (g_tacs[current].op == TAC_OP_ADD &&
          g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT && (int)g_tacs[current].arg2_d == 0) {
        /* found match! REDUCE! */
        g_tacs[current].op = TAC_OP_ASSIGNMENT;
        (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_26(): SUCCESS!\n");
#endif
      }

      i = current + 1;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_27(int *optimizations_counter) {

  /*
    rX = rA - 0 <--- REDUCE into assignment
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
      int current;

      current = _find_next_living_tac(i);

      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_SUB &&
          g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT && (int)g_tacs[current].arg2_d == 0) {
        /* found match! REDUCE! */
        g_tacs[current].op = TAC_OP_ASSIGNMENT;
        (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_27(): SUCCESS!\n");
#endif
      }

      i = current + 1;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_28(int *optimizations_counter) {

  /*
    rX = rA * 0 <--- REDUCE into assignment
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
      int current;

      current = _find_next_living_tac(i);

      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_MUL &&
          g_tacs[current].arg1_type == TAC_ARG_TYPE_CONSTANT && (int)g_tacs[current].arg1_d == 0) {
        /* found match! REDUCE! */
        g_tacs[current].op = TAC_OP_ASSIGNMENT;
        (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_28(): SUCCESS!\n");
#endif
      }
      else if (g_tacs[current].op == TAC_OP_MUL &&
          g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT && (int)g_tacs[current].arg2_d == 0) {
        /* found match! REDUCE! */
        g_tacs[current].op = TAC_OP_ASSIGNMENT;
        if (tac_copy_arg(&g_tacs[current], TAC_USE_ARG2, TAC_USE_ARG1) == FAILED)
          return FAILED;
        (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "_optimize_il_28(): SUCCESS!\n");
#endif
      }

      i = current + 1;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_29(int *optimizations_counter) {

  /*
    rX = rA + C <--- COMBINE C+D if rX doesn't appear elsewhere
     ? = rX[D]  <--- COMBINE C+D if rX doesn't appear elsewhere
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

      if (g_tacs[current].op == TAC_OP_ADD &&
          g_tacs[next].op == TAC_OP_ARRAY_READ &&
          g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && g_tacs[current].arg1_type == TAC_ARG_TYPE_TEMP && g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT &&
          g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP && g_tacs[next].arg2_type == TAC_ARG_TYPE_CONSTANT &&
          (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d &&
          g_register_reads[(int)g_tacs[current].result_d] == 1 && g_register_writes[(int)g_tacs[current].result_d] == 1) {
        /* found match! COMBINE? */
        int is_ok = NO;
        int sum = (int)g_tacs[current].arg2_d + (int)g_tacs[next].arg2_d;

        if (g_tacs[next].arg1_var_type == VARIABLE_TYPE_INT8 || g_tacs[next].arg1_var_type == VARIABLE_TYPE_UINT8)
          is_ok = YES;
        else if (g_tacs[next].arg1_var_type == VARIABLE_TYPE_INT16 || g_tacs[next].arg1_var_type == VARIABLE_TYPE_UINT16) {
          if ((sum & 1) == 0) {
            sum = sum >> 1;
            is_ok = YES;
          }
        }

        if (is_ok == YES) {
          g_tacs[next].arg1_d = g_tacs[current].arg1_d;
          g_tacs[next].arg2_d = sum;
          g_tacs[current].op = TAC_OP_DEAD;
          (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "_optimize_il_29(): SUCCESS!\n");
#endif
        }
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_30(int *optimizations_counter) {

  /*
    ? = cA * cB <--- calculate cA*cB
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

      if (g_tacs[current].arg1_type == TAC_ARG_TYPE_CONSTANT && g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT) {
        int optimized = NO;

        if (g_tacs[current].op == TAC_OP_SHIFT_RIGHT) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) >> ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_SHIFT_LEFT) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) << ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_OR) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) | ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_XOR) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) ^ ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_AND) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) & ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_MOD) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) % ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_MUL) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) * ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_DIV) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) / ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_SUB) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) - ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }
        else if (g_tacs[current].op == TAC_OP_ADD) {
          g_tacs[current].arg1_d = ((int)g_tacs[current].arg1_d) + ((int)g_tacs[current].arg2_d);
          optimized = YES;
        }

        if (optimized == YES) {
          /* found match! cA * cB can be calculated! */
          g_tacs[current].arg1_var_type_promoted = get_max_variable_type_2(g_tacs[current].arg1_var_type_promoted, g_tacs[current].arg2_var_type_promoted);
          g_tacs[current].op = TAC_OP_ASSIGNMENT;
          (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "_optimize_il_30(): SUCCESS!\n");
#endif
        }
      }

      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_31(int *optimizations_counter) {

  /*
    A = ?     <--- MERGE instructions if A doesn't appear elsewhere
    ...
    A[?] = ?  <--- MERGE instructions if A doesn't appear elsewhere
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT) {
        next = current;

        while (1) {
          next = _find_next_living_tac(next + 1);

          if (next < 0)
            break;

          if (g_tacs[next].op != TAC_OP_ARRAY_WRITE && g_tacs[next].result_type == g_tacs[current].result_type) {
            if (g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].result_d) {
              /* cannot jump over writes to the same variable */
              next = -1;
              break;
            }
            else if (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].result_node) {
              /* cannot jump over writes to the same variable */
              next = -1;
              break;
            }
          }
          if (g_tacs[next].op != TAC_OP_ARRAY_WRITE && g_tacs[next].arg1_type == g_tacs[current].result_type) {
            if (g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
            else if (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg1_node) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
          }
          if (g_tacs[next].op != TAC_OP_ARRAY_WRITE && g_tacs[next].arg2_type == g_tacs[current].result_type) {
            if (g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
            else if (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg2_node) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
          }

          if (g_tacs[next].op == TAC_OP_FUNCTION_CALL || g_tacs[next].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
            if (_aaa_get_argument_index_in_function_call_arguments(&g_tacs[next], g_tacs[current].result_type, g_tacs[current].result_node, (int)g_tacs[current].result_d) >= 0) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
          }

          if (g_tacs[next].op == TAC_OP_LABEL) {
            /* we cannot search past a label */
            next = -1;
            break;
          }
          else if (g_tacs[next].op == TAC_OP_ARRAY_WRITE) {
            if (g_tacs[next].result_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].result_d) ||
                                                                            (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].result_node))) {
              if (g_allocator_enabled == YES && g_tacs[current].result_type == TAC_ARG_TYPE_TEMP) {
                next = -1;
                break;
              }

              /* found match! operations can be merged, variable/constant/register embedded! */
              if (tac_set_result(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                return FAILED;
              g_tacs[next].result_node = g_tacs[current].arg1_node;
              /* use the target's promotion
              g_tacs[next].result_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
              */
              /*
              g_tacs[current].op = TAC_OP_DEAD;
              */
              (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
              fprintf(stderr, "_optimize_il_31(): SUCCESS!\n");
#endif
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


static int _optimize_il_32(int *optimizations_counter) {

  /*
     cA[cB] = ?  <--- Combine cA and cB
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

      if (g_tacs[current].op == TAC_OP_ARRAY_WRITE &&
          g_tacs[current].result_type == TAC_ARG_TYPE_CONSTANT && g_tacs[current].arg2_type == TAC_ARG_TYPE_CONSTANT && (int)g_tacs[current].arg2_d != 0) {
        /* found match! COMBINE? */

        /* add index directly to the address, if possible */
        int address = (int)g_tacs[current].result_d + (int)g_tacs[current].arg2_d;

        if (g_tacs[current].result_var_type == VARIABLE_TYPE_INT16 || g_tacs[current].result_var_type == VARIABLE_TYPE_UINT16) {
          /* the array has 16-bit items -> add index twice! */
          address += (int)g_tacs[current].arg2_d;
        }

        if (address >= 0 && address <= 0xffff) {
          /* yes, we can optimize! */
          g_tacs[current].result_d = address;
          g_tacs[current].arg2_d = 0;
          (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "_optimize_il_32(): SUCCESS!\n");
#endif
        }
      }

      i = next;

      if (i < 0)
        break;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_33(int *optimizations_counter) {

  /*
    X = cA
    ...
    ? = X * ?  <--- replace X here with cA
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT && g_tacs[current].arg1_type == TAC_ARG_TYPE_CONSTANT) {
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
          else if (g_tacs[next].arg1_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg1_node))) {
            if (g_tacs[next].op == TAC_OP_GET_ADDRESS) {
              /* cannot search past TAC_OP_GET_ADDRESSes */
              next = -1;
              break;
            }
            else {
              /* found match! X can be replaced with cA */
              if (tac_set_arg1(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                return FAILED;
              g_tacs[next].arg1_node = g_tacs[current].arg1_node;
              /* use the target's promotion
              g_tacs[next].arg1_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
              */
              (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
              fprintf(stderr, "_optimize_il_33(): SUCCESS!\n");
#endif
              next = -1;
              break;
            }
          }
          else if (g_tacs[next].arg2_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg2_node))) {
            if (g_tacs[next].op == TAC_OP_GET_ADDRESS) {
              /* cannot search past TAC_OP_GET_ADDRESSes */
              next = -1;
              break;
            }
            else {
              /* found match! X can be replaced with cA */
              if (tac_set_arg2(&g_tacs[next], g_tacs[current].arg1_type, g_tacs[current].arg1_d, g_tacs[current].arg1_s) == FAILED)
                return FAILED;
              g_tacs[next].arg2_node = g_tacs[current].arg1_node;
              /* use the target's promotion
              g_tacs[next].arg2_var_type_promoted = g_tacs[current].arg1_var_type_promoted;
              */
              (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
              fprintf(stderr, "_optimize_il_33(): SUCCESS!\n");
#endif
              next = -1;
              break;
            }
          }

          if (next < 0)
            break;

          if (g_tacs[next].result_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].result_d) ||
                                                                          (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].result_node))) {
            /* cannot jump over writes to the same variable */
            next = -1;
            break;
          }
        }
      }

      i = current + 1;
    }

    if (is_last_function == YES)
      break;
  }

  return SUCCEEDED;
}


static int _optimize_il_34(int *optimizations_counter) {

  /*
    A = ?  <--- remove this assignment if A isn't read between the two TACs
    ...
    A = ?
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT) {
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
          else if (g_tacs[next].arg1_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg1_node))) {
            /* we cannot search past reads from A */
            next = -1;
            break;
          }
          else if (g_tacs[next].arg2_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg2_node))) {
            /* we cannot search past reads from A */
            next = -1;
            break;
          }
          else if (g_tacs[next].op == TAC_OP_FUNCTION_CALL || g_tacs[next].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
            if (_aaa_get_argument_index_in_function_call_arguments(&g_tacs[next], g_tacs[current].result_type, g_tacs[current].result_node, (int)g_tacs[current].result_d) >= 0) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
          }

          if (g_tacs[next].result_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].result_d) ||
                                                                          (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].result_node))) {
            if (g_backend == BACKEND_Z80) {
              if (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && strcmp(g_tacs[current].result_s, "__z80_out") == 0) {
                /* we cannot optimize writes to __z80_out away */
                next = -1;
                break;
              }
            }
            if (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && (g_tacs[current].result_node->flags & TREE_NODE_FLAG_GLOBAL) == TREE_NODE_FLAG_GLOBAL) {
              /* we cannot optimize writes to global variables away */
              next = -1;
              break;
            }
            if (g_tacs[next].op == TAC_OP_ARRAY_WRITE) {
              /* cannot go past an array write to A */
              next = -1;
              break;
            }

            /* found match! the first assignment can be removed! */
            g_tacs[current].op = TAC_OP_DEAD;
            (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
            fprintf(stderr, "_optimize_il_34(): SUCCESS!\n");
#endif
            next = -1;
            break;
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


static int _optimize_il_1000(int *optimizations_counter) {

  /*
    A = ?  <--- remove this assignment if A isn't anywhere else
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

      if (g_tacs[current].op == TAC_OP_ASSIGNMENT) {
        next = current;

        while (1) {
          next = _find_next_living_tac(next + 1);

          if (next < 0 || g_tacs[next].function_node != g_tacs[current].function_node) {
            /* found the end of the function! */
            if (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && (g_tacs[current].result_node->flags & TREE_NODE_FLAG_GLOBAL) == TREE_NODE_FLAG_GLOBAL) {
              /* we cannot optimize writes to global variables away */
              next = -1;
              break;
            }

            /* the assignment can be removed! */
            g_tacs[current].op = TAC_OP_DEAD;
            (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
            fprintf(stderr, "_optimize_il_1000(): SUCCESS!\n");
#endif
            next = -1;
            break;
          }

          if (g_tacs[next].arg1_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) ||
                                                                        (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg1_node))) {
            /* we cannot search past reads from A */
            next = -1;
            break;
          }
          else if (g_tacs[next].arg2_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg2_node))) {
            /* we cannot search past reads from A */
            next = -1;
            break;
          }
          else if (g_tacs[next].op == TAC_OP_FUNCTION_CALL || g_tacs[next].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
            if (_aaa_get_argument_index_in_function_call_arguments(&g_tacs[next], g_tacs[current].result_type, g_tacs[current].result_node, (int)g_tacs[current].result_d) >= 0) {
              /* cannot jump over reads from the same variable */
              next = -1;
              break;
            }
          }
          else if (g_tacs[next].op == TAC_OP_ARRAY_WRITE && g_tacs[next].result_type == g_tacs[current].result_type &&
                   ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].result_d) ||
                    (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].result_node))) {
            /* cannot jump over reads from A */
            next = -1;
            break;
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


static int _optimize_il_1001(int *optimizations_counter) {

  /* remove the creation of a local variable if no-one references it */

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

      current = _find_next_living_tac_all(i);

      if (current < 0)
        break;

      if (g_tacs[current].op == TAC_OP_CREATE_VARIABLE) {
        next = current;

        while (1) {
          next = _find_next_living_tac_all(next + 1);

          if (next < 0 || g_tacs[next].function_node != g_tacs[current].function_node) {
            /* found the end of the function! */

            /* the variable creation can be removed! */
            g_tacs[current].op = TAC_OP_DEAD;
            (*optimizations_counter)++;
#if defined(DEBUG_PASS_5)
            fprintf(stderr, "_optimize_il_1001(): Removed unused local variable \"%s\"\n", g_tacs[current].result_s);
            fprintf(stderr, "_optimize_il_1001(): SUCCESS!\n");
#endif
            next = -1;
            break;
          }

          if (g_tacs[next].op == TAC_OP_FUNCTION_CALL || g_tacs[next].op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
            if (_bbb_get_argument_index_in_function_call_arguments_with_tree_node(&g_tacs[next], g_tacs[current].result_node) >= 0) {
              /* we cannot search past reads from the variable */
              next = -1;
              break;
            }
          }
          else if (g_tacs[next].arg1_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg1_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg1_node))) {
            /* we cannot search past reads from the variable */
            next = -1;
            break;
          }
          else if (g_tacs[next].arg2_type == g_tacs[current].result_type && ((g_tacs[current].result_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[current].result_d == (int)g_tacs[next].arg2_d) ||
                                                                             (g_tacs[current].result_type == TAC_ARG_TYPE_LABEL && g_tacs[current].result_node == g_tacs[next].arg2_node))) {
            /* we cannot search past reads from the variable */
            next = -1;
            break;
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

  if (g_tacs_count <= 0)
    return SUCCEEDED;

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
    /*
    if (_optimize_il_11(&optimizations_counter) == FAILED)
      return FAILED;
    */
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
    /* DEBUG
    fprintf(stderr, "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN\n");
    print_tacs();
    fprintf(stderr, "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN\n");
    */
    if (_optimize_il_21(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_22(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_23(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_24(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_25(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_26(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_27(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_28(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_29(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_30(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_31(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_32(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_33(&optimizations_counter) == FAILED)
      return FAILED;
    if (_optimize_il_34(&optimizations_counter) == FAILED)
      return FAILED;

    fprintf(stderr, "optimize_il(): Loop %d managed to do %d optimizations.\n", loop, optimizations_counter);
    loop++;

    if (optimizations_counter == 0)
      break;
  }

  /* the very last optimizations */
  optimizations_counter = 0;

  if (_optimize_il_1000(&optimizations_counter) == FAILED)
      return FAILED;
  if (_optimize_il_1001(&optimizations_counter) == FAILED)
    return FAILED;

  fprintf(stderr, "optimize_il(): Final pass managed to do %d optimizations.\n", optimizations_counter);

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

  if (g_tacs_count <= 0)
    return SUCCEEDED;

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

  if (g_tacs_count <= 0)
    return SUCCEEDED;

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

  if (arg_type == TAC_ARG_TYPE_CONSTANT)
    *type = get_variable_type_constant(value);
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
    /* struct? */
    else if (variable_type == VARIABLE_TYPE_STRUCT)
      *type = VARIABLE_TYPE_UINT16;
    /* union? */
    else if (variable_type == VARIABLE_TYPE_UNION)
      *type = VARIABLE_TYPE_UINT16;
    else {
      fprintf(stderr, "_find_operand_type(): Variable \"%s\" is of type %d -> Not handled here! Please submit a bug report!\n", node->children[1]->label, variable_type);
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
             op == TAC_OP_XOR ||
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
    else if (op == TAC_OP_ASSIGNMENT || op == TAC_OP_COMPLEMENT) {
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
    else if (op == TAC_OP_ARRAY_WRITE) {
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      /*
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;
      */

      if (t->arg1_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG1!\n");
      if (t->arg2_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for ARG2!\n");
      /* NOTE: this has been set already previously in pass_4()...
      if (t->result_var_type == VARIABLE_TYPE_NONE)
        fprintf(stderr, "propagate_operand_types(): Couldn't find type for RESULT!\n");
      */
    }
    else if (op == TAC_OP_ARRAY_READ) {
      /* NOTE: this has been set already previously in pass_4()...
      if (_find_operand_type(&t->arg1_var_type, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      */
      if (_find_operand_type(&t->arg2_var_type, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_type(&t->result_var_type, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        if (t->result_var_type == VARIABLE_TYPE_NONE) {
          /* get the type from the operands */
          t->result_var_type = t->arg1_var_type;
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

      if (t->arg1_var_type_promoted == VARIABLE_TYPE_NONE)
        t->arg1_var_type_promoted = t->arg1_var_type;
      if (t->arg2_var_type_promoted == VARIABLE_TYPE_NONE)
        t->arg2_var_type_promoted = t->arg2_var_type;

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

      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        if (t->result_var_type == VARIABLE_TYPE_NONE) {
          if (t->arg1_node == NULL || t->arg1_node->children[0] == NULL) {
            fprintf(stderr, "propagate_operand_types(): Function call return value has no callee return type! Please submit a bug report!\n");
            return FAILED;
          }

          t->result_var_type = tree_node_get_max_var_type(t->arg1_node->children[0]);
        }

        _set_temp_register_type((int)t->result_d, t->result_var_type);
      }

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

  int size;

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
    else if (type == VARIABLE_TYPE_STRUCT || type == VARIABLE_TYPE_UNION) {
      struct struct_item *si;

      /* sizeof(struct x) */
      si = find_struct_item(node->children[0]->children[0]->label);
      if (si == NULL) {
        snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", node->children[0]->children[0]->label);
        print_error_using_tree_node(g_error_message, ERROR_ERR, node);
        return -1;
      }

      size = si->size * 8;
    }
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

#define Z80_TAC_CLOBBER_NONE  0
#define Z80_TAC_CLOBBER_A     1
#define Z80_TAC_CLOBBER_BC    2
#define Z80_TAC_CLOBBER_DE    4
#define Z80_TAC_CLOBBER_HL    8
#define Z80_TAC_CLOBBER_IX   16
#define Z80_TAC_CLOBBER_IY   32
#define Z80_TAC_CLOBBER_SP   64
#define Z80_TAC_CLOBBER_FLAGS 128

#define Z80_TAC_CLOBBER_ALL (Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_DE | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_IY | Z80_TAC_CLOBBER_SP | Z80_TAC_CLOBBER_FLAGS)

#define RA_Z80_UNIT_NONE 0
#define RA_Z80_UNIT_A    1
#define RA_Z80_UNIT_B    2
#define RA_Z80_UNIT_C    4
#define RA_Z80_UNIT_H    8
#define RA_Z80_UNIT_L   16

struct z80_tac_clobber_descriptor {
  int op;
  int clobbers;
};

static struct z80_tac_clobber_descriptor g_z80_tac_clobber_descriptors[] = {
  { TAC_OP_ASSIGNMENT, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_IY | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_ADD, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_SUB, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_AND, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_OR, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_XOR, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_COMPLEMENT, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_JUMP_EQ, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_JUMP_NEQ, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_JUMP_LT, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_JUMP_GT, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_JUMP_LTE, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_JUMP_GTE, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_FUNCTION_CALL, Z80_TAC_CLOBBER_ALL },
  { TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE, Z80_TAC_CLOBBER_ALL },
  { TAC_OP_RETURN, Z80_TAC_CLOBBER_SP },
  { TAC_OP_RETURN_VALUE, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_SP | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_ASM, Z80_TAC_CLOBBER_ALL },
  { TAC_OP_MUL, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_DE | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_DIV, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_DE | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_MOD, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_DE | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_SHIFT_LEFT, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_SHIFT_RIGHT, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_ARRAY_READ, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_IY | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_ARRAY_WRITE, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_IY | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_GET_ADDRESS, Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_FLAGS },
  { TAC_OP_GET_ADDRESS_ARRAY, Z80_TAC_CLOBBER_A | Z80_TAC_CLOBBER_BC | Z80_TAC_CLOBBER_HL | Z80_TAC_CLOBBER_IX | Z80_TAC_CLOBBER_IY | Z80_TAC_CLOBBER_FLAGS },
  { -1, Z80_TAC_CLOBBER_NONE }
};

static struct tree_node *g_local_variables[LOCAL_VAR_COUNT];
static struct register_allocator_live_interval g_register_live_intervals[REG_COUNT];


static int _reset_register_frame_info(char *function_name) {

  return register_allocator_reset_live_intervals(function_name, g_register_live_intervals, REG_COUNT);
}


static int _note_register_frame_usage(char *function_name, int index, int size, int tac_index, int is_write) {

  return register_allocator_note_live_interval(function_name, g_register_live_intervals, REG_COUNT, index, size, tac_index, is_write);
}


static int _get_original_register_index(int original_register_index, int register_index) {

  if (original_register_index >= 0)
    return original_register_index;

  return register_index;
}


static int _find_original_register_index_for_temp(struct tree_node *function_node, int register_index) {

  int i;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];

    if (t->function_node != function_node)
      continue;

    if (t->op == TAC_OP_FUNCTION_CALL || t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int j;

      for (j = 0; j < t->arguments_count; j++) {
        if (t->arguments[j].type == TAC_ARG_TYPE_TEMP && (int)t->arguments[j].value == register_index)
          return _get_original_register_index(t->arguments[j].original_register_index, register_index);
      }
    }

    if (t->arg1_type == TAC_ARG_TYPE_TEMP && (int)t->arg1_d == register_index)
      return _get_original_register_index(t->arg1_original_register_index, register_index);
    if (t->arg2_type == TAC_ARG_TYPE_TEMP && (int)t->arg2_d == register_index)
      return _get_original_register_index(t->arg2_original_register_index, register_index);
    if (t->result_type == TAC_ARG_TYPE_TEMP && (int)t->result_d == register_index)
      return _get_original_register_index(t->result_original_register_index, register_index);
  }

  return register_index;
}


static struct temp_register *_find_temp_register_info(struct tree_node *function_node, int register_index) {

  int i;

  if (function_node->local_variables == NULL)
    return NULL;

  for (i = 0; i < function_node->local_variables->temp_registers_count; i++) {
    if (function_node->local_variables->temp_registers[i].register_index == register_index)
      return &(function_node->local_variables->temp_registers[i]);
  }

  return NULL;
}


static int _get_z80_tac_clobbers(int op) {

  int i;

  for (i = 0; g_z80_tac_clobber_descriptors[i].op >= 0; i++) {
    if (g_z80_tac_clobber_descriptors[i].op == op)
      return g_z80_tac_clobber_descriptors[i].clobbers;
  }

  return Z80_TAC_CLOBBER_NONE;
}


static int _get_register_allocator_tac_clobbers(int op);


static int _z80_tac_may_clobber_bc(int op) {

  if ((_get_register_allocator_tac_clobbers(op) & Z80_TAC_CLOBBER_BC) != 0)
    return YES;

  return NO;
}


static int _is_first_allocator_producer(int op) {

  if (op == TAC_OP_ADD || op == TAC_OP_SUB || op == TAC_OP_AND || op == TAC_OP_OR || op == TAC_OP_XOR || op == TAC_OP_COMPLEMENT)
    return YES;

  return NO;
}


static int _is_register_allocator_linear_scan_producer(struct tac *producer) {

  if (_is_first_allocator_producer(producer->op) == YES)
    return YES;
  if (producer->op == TAC_OP_ASSIGNMENT)
    return YES;
  if (producer->op == TAC_OP_SHIFT_LEFT || producer->op == TAC_OP_SHIFT_RIGHT)
    return YES;
  if (producer->op == TAC_OP_MUL || producer->op == TAC_OP_DIV || producer->op == TAC_OP_MOD)
    return YES;
  if (producer->op == TAC_OP_GET_ADDRESS || producer->op == TAC_OP_GET_ADDRESS_ARRAY)
    return YES;
  if (producer->op == TAC_OP_ARRAY_READ)
    return YES;
  if (producer->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE)
    return YES;

  return NO;
}


static int _is_first_allocator_consumer(int op) {

  if (op == TAC_OP_ADD || op == TAC_OP_SUB || op == TAC_OP_AND || op == TAC_OP_OR || op == TAC_OP_XOR ||
      op == TAC_OP_SHIFT_LEFT || op == TAC_OP_SHIFT_RIGHT ||
      op == TAC_OP_MUL || op == TAC_OP_DIV || op == TAC_OP_MOD ||
      op == TAC_OP_JUMP_EQ || op == TAC_OP_JUMP_NEQ || op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_GT ||
      op == TAC_OP_JUMP_LTE || op == TAC_OP_JUMP_GTE || op == TAC_OP_RETURN_VALUE)
    return YES;

  return NO;
}


static int _is_first_allocator_arg2_consumer(int op) {

  if (op == TAC_OP_ADD || op == TAC_OP_AND || op == TAC_OP_OR || op == TAC_OP_XOR ||
      op == TAC_OP_SHIFT_LEFT || op == TAC_OP_SHIFT_RIGHT ||
      op == TAC_OP_MUL || op == TAC_OP_DIV || op == TAC_OP_MOD ||
      op == TAC_OP_JUMP_EQ || op == TAC_OP_JUMP_NEQ || op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_GT ||
      op == TAC_OP_JUMP_LTE || op == TAC_OP_JUMP_GTE)
    return YES;

  return NO;
}


static int _get_register_allocator_physical_register_for_size(int size) {

  if (size == 8)
    return Z80_PHY_A;
  if (size == 16)
    return Z80_PHY_HL;

  return Z80_PHY_NONE;
}


static int _is_z80_register_allocator_candidate_allowed_for_physical_register(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int physical_register, int consumer_arg);
static int _is_z80_basic_block_transparent_tac_for_physical_register(struct tac *t, int physical_register);


static int _get_z80_register_allocator_physical_register_units(int physical_register) {

  if (physical_register == Z80_PHY_A)
    return RA_Z80_UNIT_A;
  if (physical_register == Z80_PHY_HL)
    return RA_Z80_UNIT_H | RA_Z80_UNIT_L;
  if (physical_register == Z80_PHY_BC)
    return RA_Z80_UNIT_B | RA_Z80_UNIT_C;
  if (physical_register == Z80_PHY_B)
    return RA_Z80_UNIT_B;
  if (physical_register == Z80_PHY_C)
    return RA_Z80_UNIT_C;

  return RA_Z80_UNIT_NONE;
}


static int _get_z80_register_allocator_active_slot_count(void) {

  return 5;
}


static int _get_z80_register_allocator_active_slot_index(int physical_register) {

  if (physical_register == Z80_PHY_A)
    return 0;
  if (physical_register == Z80_PHY_HL)
    return 1;
  if (physical_register == Z80_PHY_BC)
    return 2;
  if (physical_register == Z80_PHY_B)
    return 3;
  if (physical_register == Z80_PHY_C)
    return 4;

  return -1;
}


static char *_get_z80_register_allocator_active_slot_name(int physical_register) {

  if (physical_register == Z80_PHY_A)
    return "A";
  if (physical_register == Z80_PHY_HL)
    return "HL";
  if (physical_register == Z80_PHY_BC)
    return "BC";
  if (physical_register == Z80_PHY_B)
    return "B";
  if (physical_register == Z80_PHY_C)
    return "C";

  return NULL;
}


static int _get_z80_register_allocator_candidate_register_count(int size) {

  if (size == 8)
    return 3;
  if (size == 16)
    return 2;

  return 0;
}


static int _get_z80_register_allocator_candidate_physical_register(int size, int candidate_index) {

  if (size == 8) {
    if (candidate_index == 0)
      return Z80_PHY_A;
    if (candidate_index == 1)
      return Z80_PHY_C;
    if (candidate_index == 2)
      return Z80_PHY_B;
  }
  if (size == 16) {
    if (candidate_index == 0)
      return Z80_PHY_HL;
    if (candidate_index == 1)
      return Z80_PHY_BC;
  }

  return Z80_PHY_NONE;
}


static int _can_z80_register_allocator_fallback_candidate(int size, int reason) {

  if (reason == RA_CANDIDATE_FALLBACK_UNSUPPORTED)
    return size == 16 ? YES : NO;
  if (reason == RA_CANDIDATE_FALLBACK_PATH)
    return size == 8 ? YES : NO;
  if (reason == RA_CANDIDATE_FALLBACK_CONFLICT)
    return size == 8 || size == 16 ? YES : NO;

  return -1;
}


static int _prefer_z80_register_allocator_candidate_on_equal_next_use(int consumer_op,
    int candidate_operand, int active_operand, int physical_register) {

  if (consumer_op == TAC_OP_ARRAY_WRITE && candidate_operand == TAC_USE_ARG1 &&
      active_operand == TAC_USE_ARG2 && physical_register == Z80_PHY_A)
    return YES;

  return NO;
}


static int _get_z80_register_allocator_call_result_physical_register(int size) {

  return _get_register_allocator_physical_register_for_size(size);
}


static int _get_z80_register_allocator_call_boundary_spill_reason(int op) {

  if (op == TAC_OP_FUNCTION_CALL || op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE)
    return Z80_SPILL_REASON_FUNCTION_CALL;
  if (op == TAC_OP_RETURN || op == TAC_OP_RETURN_VALUE)
    return Z80_SPILL_REASON_RETURN;

  return Z80_SPILL_REASON_NONE;
}


static int _get_z80_register_allocator_stack_return_value_end_offset(int return_value_size) {

  if (return_value_size < 0 || return_value_size > 2)
    return 0;

  return -4 - return_value_size;
}


static int _get_z80_register_allocator_return_value_byte_offset(int return_value_size, int byte_index) {

  if (return_value_size == 1 && byte_index == 0)
    return -4;
  if (return_value_size == 2 && byte_index >= 0 && byte_index < 2)
    return -5 + byte_index;

  return 0;
}


static int _get_z80_register_allocator_call_frame_prefix_value(int value_kind) {

  if (value_kind == RA_CALL_FRAME_PREFIX_END)
    return -4;
  if (value_kind == RA_CALL_FRAME_RETURN_SP_OFFSET)
    return -1;
  if (value_kind == RA_CALL_FRAME_SAVED_FP_LOW_OFFSET)
    return -3;
  if (value_kind == RA_CALL_FRAME_SAVED_FP_HIGH_OFFSET)
    return -2;

  return 0;
}


static int _get_z80_register_allocator_stack_argument_end_offset(int frame_end_offset, int argument_size, int argument_index) {

  if (frame_end_offset > -4 || argument_size <= 0 || argument_size > 2 || argument_index < 0)
    return 0;

  return frame_end_offset - argument_size;
}


static int _get_z80_register_allocator_argument_transport(int source_var_type, int target_var_type) {

  if (target_var_type != VARIABLE_TYPE_INT8 && target_var_type != VARIABLE_TYPE_UINT8 &&
      target_var_type != VARIABLE_TYPE_INT16 && target_var_type != VARIABLE_TYPE_UINT16)
    return RA_ARGUMENT_TRANSPORT_NONE;

  if (source_var_type == VARIABLE_TYPE_CONST) {
    if (target_var_type == VARIABLE_TYPE_INT16 || target_var_type == VARIABLE_TYPE_UINT16)
      return RA_ARGUMENT_TRANSPORT_WORD;
    return RA_ARGUMENT_TRANSPORT_BYTE;
  }

  if (source_var_type == VARIABLE_TYPE_INT16 || source_var_type == VARIABLE_TYPE_UINT16) {
    if (target_var_type == VARIABLE_TYPE_INT16 || target_var_type == VARIABLE_TYPE_UINT16)
      return RA_ARGUMENT_TRANSPORT_WORD;
    return RA_ARGUMENT_TRANSPORT_TRUNCATE;
  }

  if (source_var_type == VARIABLE_TYPE_INT8 || source_var_type == VARIABLE_TYPE_UINT8) {
    if (target_var_type == VARIABLE_TYPE_INT16 || target_var_type == VARIABLE_TYPE_UINT16) {
      if (source_var_type == VARIABLE_TYPE_INT8)
        return RA_ARGUMENT_TRANSPORT_SIGN_EXTEND;
      return RA_ARGUMENT_TRANSPORT_ZERO_EXTEND;
    }
    return RA_ARGUMENT_TRANSPORT_BYTE;
  }

  return RA_ARGUMENT_TRANSPORT_NONE;
}


static int _get_z80_register_allocator_argument_byte_offset(int transport, int access_kind, int byte_index) {

  if (access_kind != RA_ARGUMENT_BYTE_ACCESS_SOURCE && access_kind != RA_ARGUMENT_BYTE_ACCESS_TARGET)
    return -1;
  if (byte_index < 0 || byte_index > 1)
    return -1;

  if (transport == RA_ARGUMENT_TRANSPORT_BYTE)
    return byte_index == 0 ? 0 : -1;
  if (transport == RA_ARGUMENT_TRANSPORT_WORD)
    return byte_index;
  if (transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND || transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND) {
    if (access_kind == RA_ARGUMENT_BYTE_ACCESS_SOURCE)
      return byte_index == 0 ? 0 : -1;
    return byte_index;
  }
  if (transport == RA_ARGUMENT_TRANSPORT_TRUNCATE) {
    if (access_kind == RA_ARGUMENT_BYTE_ACCESS_SOURCE)
      return byte_index;
    return byte_index == 0 ? 0 : -1;
  }

  return -1;
}


static int _get_z80_register_allocator_location_kind(int source_kind, int physical_register) {

  if (source_kind == RA_LOCATION_SOURCE_CONSTANT && physical_register == Z80_PHY_NONE)
    return RA_LOCATION_CONST;
  if (source_kind == RA_LOCATION_SOURCE_GLOBAL && physical_register == Z80_PHY_NONE)
    return RA_LOCATION_GLOBAL;
  if (source_kind == RA_LOCATION_SOURCE_STACK_LOCAL && physical_register == Z80_PHY_NONE)
    return RA_LOCATION_STACK_LOCAL;
  if (source_kind == RA_LOCATION_SOURCE_STACK_SPILL && physical_register == Z80_PHY_NONE)
    return RA_LOCATION_STACK_SPILL;
  if (source_kind != RA_LOCATION_SOURCE_PHYSICAL)
    return RA_LOCATION_NONE;

  if (physical_register == Z80_PHY_A)
    return RA_LOCATION_PHY_A;
  if (physical_register == Z80_PHY_HL)
    return RA_LOCATION_PHY_HL;
  if (physical_register == Z80_PHY_BC)
    return RA_LOCATION_PHY_BC;
  if (physical_register == Z80_PHY_B)
    return RA_LOCATION_PHY_B;
  if (physical_register == Z80_PHY_C)
    return RA_LOCATION_PHY_C;

  return RA_LOCATION_NONE;
}


static int _get_z80_register_allocator_address_materialization_mode(int location_kind, int address_target, int offset) {

  if (address_target != RA_ADDRESS_TARGET_IX && address_target != RA_ADDRESS_TARGET_IY &&
      address_target != RA_ADDRESS_TARGET_HL && address_target != RA_ADDRESS_TARGET_IX_UNCACHED &&
      address_target != RA_ADDRESS_TARGET_IY_OLD_FRAME)
    return RA_ADDRESS_MODE_INVALID;

  if (location_kind == RA_LOCATION_GLOBAL)
    return RA_ADDRESS_MODE_GLOBAL_LABEL;
  if (location_kind == RA_LOCATION_STACK_LOCAL || location_kind == RA_LOCATION_STACK_SPILL) {
    if ((address_target == RA_ADDRESS_TARGET_IX || address_target == RA_ADDRESS_TARGET_IX_UNCACHED) &&
      offset >= -128 && offset <= 126)
      return RA_ADDRESS_MODE_FRAME_DISPLACEMENT;
    if (address_target == RA_ADDRESS_TARGET_IY_OLD_FRAME && offset > -127)
      return RA_ADDRESS_MODE_FRAME_DISPLACEMENT;
    return RA_ADDRESS_MODE_FRAME_ADDRESS;
  }
  if (address_target == RA_ADDRESS_TARGET_HL)
    return RA_ADDRESS_MODE_INVALID;
  if (address_target == RA_ADDRESS_TARGET_IY && location_kind == RA_LOCATION_CONST)
    return RA_ADDRESS_MODE_NONE;
  if (location_kind == RA_LOCATION_PHY_A ||
      location_kind == RA_LOCATION_PHY_HL || location_kind == RA_LOCATION_PHY_BC ||
      location_kind == RA_LOCATION_PHY_B || location_kind == RA_LOCATION_PHY_C)
    return RA_ADDRESS_MODE_NONE;

  return RA_ADDRESS_MODE_INVALID;
}


static int _can_z80_register_allocator_preserve_split_spill(int consumer_op, int consumer_arg, int size, int physical_register) {

  if (consumer_op == TAC_OP_ARRAY_READ && consumer_arg == TAC_USE_ARG1 && size == 16 && physical_register == Z80_PHY_HL)
    return YES;

  return NO;
}


static int _get_z80_register_allocator_split_reload_physical_register(struct tac *consumer, int consumer_arg, int size) {

  if (consumer == NULL)
    return Z80_PHY_NONE;
  return z80_get_split_reload_physical_register(consumer->op, consumer_arg,
      size);
}


static struct register_allocator_target_policy g_z80_register_allocator_target_policy = {
  "z80",
  _get_z80_register_allocator_physical_register_units,
  _get_z80_register_allocator_active_slot_count,
  _get_z80_register_allocator_active_slot_index,
  _get_z80_register_allocator_active_slot_name,
  _get_z80_register_allocator_candidate_register_count,
  _get_z80_register_allocator_candidate_physical_register,
  _can_z80_register_allocator_fallback_candidate,
  _prefer_z80_register_allocator_candidate_on_equal_next_use,
  _is_z80_register_allocator_candidate_allowed_for_physical_register,
  _get_z80_tac_clobbers,
  _is_z80_basic_block_transparent_tac_for_physical_register,
  _get_z80_register_allocator_call_result_physical_register,
  _get_z80_register_allocator_call_boundary_spill_reason,
  _get_z80_register_allocator_stack_return_value_end_offset,
  _get_z80_register_allocator_return_value_byte_offset,
  _get_z80_register_allocator_call_frame_prefix_value,
  _get_z80_register_allocator_stack_argument_end_offset,
  _get_z80_register_allocator_argument_transport,
  _get_z80_register_allocator_argument_byte_offset,
  _get_z80_register_allocator_location_kind,
  _get_z80_register_allocator_address_materialization_mode,
  _can_z80_register_allocator_preserve_split_spill,
  _get_z80_register_allocator_split_reload_physical_register
};


static struct register_allocator_target_policy *_get_register_allocator_target_policy(void) {

  if (g_backend == BACKEND_Z80)
    return &g_z80_register_allocator_target_policy;

  return NULL;
}


static int _get_register_allocator_physical_register_units(int physical_register) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_physical_register_units == NULL)
    return RA_Z80_UNIT_NONE;

  return target_policy->get_physical_register_units(physical_register);
}


static int _get_register_allocator_active_slot_count(void) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_active_slot_count == NULL)
    return -1;

  return target_policy->get_active_slot_count();
}


static int _get_register_allocator_tac_clobbers(int op) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_tac_clobbers == NULL)
    return Z80_TAC_CLOBBER_ALL;

  return target_policy->get_tac_clobbers(op);
}


static int _get_register_allocator_call_result_physical_register(int size) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_call_result_physical_register == NULL)
    return Z80_PHY_NONE;

  return target_policy->get_call_result_physical_register(size);
}


static int _get_register_allocator_call_boundary_spill_reason(int op) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_call_boundary_spill_reason == NULL)
    return Z80_SPILL_REASON_UNMIGRATED_EMITTER;

  return target_policy->get_call_boundary_spill_reason(op);
}


static int _get_register_allocator_stack_return_value_end_offset(int return_value_size) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_stack_return_value_end_offset == NULL)
    return 0;

  return target_policy->get_stack_return_value_end_offset(return_value_size);
}


int get_register_allocator_return_value_byte_offset(int return_value_size, int byte_index) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_return_value_byte_offset == NULL)
    return 0;

  return target_policy->get_return_value_byte_offset(return_value_size, byte_index);
}


int get_register_allocator_call_frame_prefix_value(int value_kind) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_call_frame_prefix_value == NULL)
    return 0;

  return target_policy->get_call_frame_prefix_value(value_kind);
}


static int _get_register_allocator_stack_argument_end_offset(int frame_end_offset, int argument_size, int argument_index) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_stack_argument_end_offset == NULL)
    return 0;

  return target_policy->get_stack_argument_end_offset(frame_end_offset, argument_size, argument_index);
}


int get_register_allocator_argument_transport(int source_var_type, int target_var_type) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_argument_transport == NULL)
    return RA_ARGUMENT_TRANSPORT_NONE;

  return target_policy->get_argument_transport(source_var_type, target_var_type);
}


int get_register_allocator_argument_byte_offset(int transport, int access_kind, int byte_index) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_argument_byte_offset == NULL)
    return -1;

  return target_policy->get_argument_byte_offset(transport, access_kind, byte_index);
}


int get_register_allocator_location_kind(int source_kind, int physical_register) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_location_kind == NULL)
    return RA_LOCATION_NONE;

  return target_policy->get_location_kind(source_kind, physical_register);
}


int get_register_allocator_address_materialization_mode(int location_kind, int address_target, int offset) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_address_materialization_mode == NULL)
    return RA_ADDRESS_MODE_INVALID;

  return target_policy->get_address_materialization_mode(location_kind, address_target, offset);
}


static int _get_register_allocator_split_reload_physical_register(struct tac *consumer, int consumer_arg, int size) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_split_reload_physical_register == NULL)
    return Z80_PHY_NONE;

  return target_policy->get_split_reload_physical_register(consumer, consumer_arg, size);
}


static int _register_allocator_physical_registers_overlap(char *function_name, int left_register, int right_register) {

  struct register_allocator_target_policy *target_policy;
  int overlap;

  target_policy = _get_register_allocator_target_policy();
  if (register_allocator_physical_registers_overlap(function_name, target_policy, left_register, right_register, &overlap) == FAILED)
    return YES;

  return overlap;
}


static int _active_physical_registers_overlap_candidate(char *function_name, int candidate_register, int active_a_register_index, int active_hl_register_index, int active_bc_register_index, int active_b_register_index, int active_c_register_index) {

  if (active_a_register_index >= 0 && _register_allocator_physical_registers_overlap(function_name, candidate_register, Z80_PHY_A) == YES)
    return YES;
  if (active_hl_register_index >= 0 && _register_allocator_physical_registers_overlap(function_name, candidate_register, Z80_PHY_HL) == YES)
    return YES;
  if (active_bc_register_index >= 0 && _register_allocator_physical_registers_overlap(function_name, candidate_register, Z80_PHY_BC) == YES)
    return YES;
  if (active_b_register_index >= 0 && _register_allocator_physical_registers_overlap(function_name, candidate_register, Z80_PHY_B) == YES)
    return YES;
  if (active_c_register_index >= 0 && _register_allocator_physical_registers_overlap(function_name, candidate_register, Z80_PHY_C) == YES)
    return YES;

  return NO;
}


static int _is_register_allocator_bc_candidate(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int consumer_arg) {

  if (temp_register->size != 16)
    return NO;
  if (producer->op != TAC_OP_COMPLEMENT)
    return NO;

  if (consumer->op == TAC_OP_ASSIGNMENT && consumer_arg == TAC_USE_ARG1)
    return YES;
  if ((consumer->op == TAC_OP_ADD || consumer->op == TAC_OP_AND || consumer->op == TAC_OP_OR || consumer->op == TAC_OP_XOR) && consumer_arg == TAC_USE_ARG2)
    return YES;

  return NO;
}


static int _is_register_allocator_c_candidate(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int consumer_arg) {

  if (temp_register->size != 8)
    return NO;
  if (producer->op != TAC_OP_ADD && producer->op != TAC_OP_SUB && producer->op != TAC_OP_AND && producer->op != TAC_OP_OR && producer->op != TAC_OP_XOR && producer->op != TAC_OP_COMPLEMENT && producer->op != TAC_OP_ARRAY_READ && producer->op != TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE && producer->op != TAC_OP_SHIFT_LEFT && producer->op != TAC_OP_SHIFT_RIGHT && producer->op != TAC_OP_MUL && producer->op != TAC_OP_DIV && producer->op != TAC_OP_MOD)
    return NO;
  if ((consumer->op == TAC_OP_ARRAY_WRITE || consumer->op == TAC_OP_ARRAY_READ || consumer->op == TAC_OP_GET_ADDRESS_ARRAY) && consumer_arg == TAC_USE_ARG2)
    return YES;
  if ((consumer->op == TAC_OP_SHIFT_LEFT || consumer->op == TAC_OP_SHIFT_RIGHT) && consumer_arg == TAC_USE_ARG1)
    return YES;
  if (consumer->op == TAC_OP_JUMP_EQ || consumer->op == TAC_OP_JUMP_NEQ ||
      consumer->op == TAC_OP_JUMP_LT || consumer->op == TAC_OP_JUMP_GT ||
      consumer->op == TAC_OP_JUMP_LTE || consumer->op == TAC_OP_JUMP_GTE)
    return consumer_arg == TAC_USE_ARG1 ? YES : NO;
  if (consumer->op == TAC_OP_SUB)
    return consumer_arg == TAC_USE_ARG2 ? YES : NO;
  if (consumer->op == TAC_OP_ADD || consumer->op == TAC_OP_AND || consumer->op == TAC_OP_OR || consumer->op == TAC_OP_XOR)
    return YES;

  return NO;
}


static int _is_register_allocator_b_candidate(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int consumer_arg) {

  if (temp_register->size != 8)
    return NO;
  if (producer->op != TAC_OP_ASSIGNMENT && producer->op != TAC_OP_ADD && producer->op != TAC_OP_SUB && producer->op != TAC_OP_AND && producer->op != TAC_OP_OR && producer->op != TAC_OP_XOR && producer->op != TAC_OP_COMPLEMENT && producer->op != TAC_OP_ARRAY_READ && producer->op != TAC_OP_SHIFT_LEFT && producer->op != TAC_OP_SHIFT_RIGHT)
    return NO;
  if ((consumer->op == TAC_OP_ARRAY_WRITE || consumer->op == TAC_OP_ARRAY_READ || consumer->op == TAC_OP_GET_ADDRESS_ARRAY) && consumer_arg == TAC_USE_ARG2)
    return YES;
  if (consumer->op == TAC_OP_SUB)
    return consumer_arg == TAC_USE_ARG2 ? YES : NO;
  if (consumer->op == TAC_OP_ADD || consumer->op == TAC_OP_AND || consumer->op == TAC_OP_OR || consumer->op == TAC_OP_XOR)
    return YES;

  return NO;
}


static int _is_register_allocator_c_transparent_arithmetic_tac(struct tac *t) {

  if (t->op != TAC_OP_ADD && t->op != TAC_OP_SUB && t->op != TAC_OP_AND && t->op != TAC_OP_OR && t->op != TAC_OP_XOR)
    return NO;
  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16)
    return NO;
  if (t->arg1_physical_register == Z80_PHY_C || t->arg1_physical_register == Z80_PHY_BC)
    return NO;
  if (t->arg2_physical_register == Z80_PHY_C || t->arg2_physical_register == Z80_PHY_BC)
    return NO;
  if (t->result_physical_register == Z80_PHY_C || t->result_physical_register == Z80_PHY_BC)
    return NO;

  return YES;
}


static int _is_register_allocator_b_transparent_arithmetic_tac(struct tac *t) {

  if (t->op != TAC_OP_ADD && t->op != TAC_OP_SUB && t->op != TAC_OP_AND && t->op != TAC_OP_OR && t->op != TAC_OP_XOR)
    return NO;
  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16)
    return NO;
  if (t->arg1_physical_register == Z80_PHY_B || t->arg1_physical_register == Z80_PHY_BC)
    return NO;
  if (t->arg2_physical_register == Z80_PHY_B || t->arg2_physical_register == Z80_PHY_BC)
    return NO;
  if (t->result_physical_register == Z80_PHY_B || t->result_physical_register == Z80_PHY_BC)
    return NO;

  return YES;
}


static int _is_z80_register_allocator_candidate_allowed_for_physical_register(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int physical_register, int consumer_arg) {

  if (producer->op == TAC_OP_ASSIGNMENT) {
    if (producer->arg1_type == TAC_ARG_TYPE_CONSTANT &&
        temp_register->size == 8 && physical_register == Z80_PHY_C &&
        consumer_arg == TAC_USE_ARG1 &&
        (consumer->op == TAC_OP_JUMP_EQ ||
        consumer->op == TAC_OP_JUMP_NEQ ||
        consumer->op == TAC_OP_JUMP_LT ||
        consumer->op == TAC_OP_JUMP_GT ||
        consumer->op == TAC_OP_JUMP_LTE ||
        consumer->op == TAC_OP_JUMP_GTE))
      return YES;
    if (consumer->op == TAC_OP_ARRAY_WRITE && consumer_arg == TAC_USE_RESULT && temp_register->size == 16 && physical_register == Z80_PHY_HL)
      return YES;
    if (consumer->op == TAC_OP_ARRAY_WRITE && consumer_arg == TAC_USE_RESULT && temp_register->size == 16 && physical_register == Z80_PHY_BC && consumer->arg2_physical_register == Z80_PHY_HL)
      return YES;
    if (consumer->op == TAC_OP_ARRAY_READ && consumer_arg == TAC_USE_ARG1 && temp_register->size == 16 && physical_register == Z80_PHY_HL)
      return YES;
    if (consumer->op == TAC_OP_ARRAY_READ && consumer_arg == TAC_USE_ARG1 && temp_register->size == 16 && physical_register == Z80_PHY_BC && consumer->arg2_physical_register == Z80_PHY_HL)
      return YES;
    if (consumer->op == TAC_OP_GET_ADDRESS_ARRAY && consumer_arg == TAC_USE_ARG1 && temp_register->size == 16 && physical_register == Z80_PHY_HL)
      return YES;
    if (consumer->op == TAC_OP_GET_ADDRESS_ARRAY && consumer_arg == TAC_USE_ARG1 && temp_register->size == 16 && physical_register == Z80_PHY_BC && consumer->arg2_physical_register == Z80_PHY_HL)
      return YES;
    if (physical_register == Z80_PHY_B)
      return _is_register_allocator_b_candidate(producer, consumer,
          temp_register, consumer_arg);

    return NO;
  }

  if ((consumer->op == TAC_OP_SHIFT_LEFT || consumer->op == TAC_OP_SHIFT_RIGHT) && consumer_arg == TAC_USE_ARG2) {
    if (temp_register->size == 8 && physical_register == Z80_PHY_A)
      return YES;
    if (temp_register->size == 16 && physical_register == Z80_PHY_BC)
      return YES;

    return NO;
  }

  if ((consumer->op == TAC_OP_ARRAY_READ || consumer->op == TAC_OP_GET_ADDRESS_ARRAY) && consumer_arg == TAC_USE_ARG1) {
    if (temp_register->size == 16 && physical_register == Z80_PHY_HL)
      return YES;

    return NO;
  }

  if (physical_register == Z80_PHY_BC)
    return _is_register_allocator_bc_candidate(producer, consumer, temp_register, consumer_arg);

  if (physical_register == Z80_PHY_C)
    return _is_register_allocator_c_candidate(producer, consumer, temp_register, consumer_arg);

  if (physical_register == Z80_PHY_B)
    return _is_register_allocator_b_candidate(producer, consumer, temp_register, consumer_arg);

  if (_get_register_allocator_physical_register_for_size(temp_register->size) == physical_register)
    return YES;

  return NO;
}


static int _is_register_allocator_candidate_allowed_for_physical_register(struct tac *producer, struct tac *consumer, struct temp_register *temp_register, int physical_register, int consumer_arg) {

  struct register_allocator_target_policy *target_policy;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->is_candidate_allowed_for_physical_register == NULL)
    return NO;

  return target_policy->is_candidate_allowed_for_physical_register(producer, consumer, temp_register, physical_register, consumer_arg);
}


static int _get_register_allocator_consumer_arg(struct tac *consumer, int register_index) {

  if (consumer->op == TAC_OP_ASSIGNMENT) {
    if (consumer->arg1_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg1_d == register_index)
      return TAC_USE_ARG1;
  }
  else if (consumer->op == TAC_OP_ARRAY_WRITE) {
    if (consumer->arg1_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg1_d == register_index)
      return TAC_USE_ARG1;
    if (consumer->arg2_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg2_d == register_index)
      return TAC_USE_ARG2;
    if (consumer->result_type == TAC_ARG_TYPE_TEMP && (int)consumer->result_d == register_index)
      return TAC_USE_RESULT;
  }
  else if (consumer->op == TAC_OP_ARRAY_READ) {
    if (consumer->arg1_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg1_d == register_index)
      return TAC_USE_ARG1;
    if (consumer->arg2_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg2_d == register_index)
      return TAC_USE_ARG2;
  }
  else if (consumer->op == TAC_OP_GET_ADDRESS_ARRAY) {
    if (consumer->arg1_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg1_d == register_index)
      return TAC_USE_ARG1;
    if (consumer->arg2_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg2_d == register_index)
      return TAC_USE_ARG2;
  }
  else if (consumer->op == TAC_OP_MUL || consumer->op == TAC_OP_DIV || consumer->op == TAC_OP_MOD) {
    if (consumer->arg1_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg1_d == register_index)
      return TAC_USE_ARG1;
    if (consumer->arg2_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg2_d == register_index)
      return TAC_USE_ARG2;
  }
  else if (_is_first_allocator_consumer(consumer->op) == YES) {
    if (consumer->arg1_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg1_d == register_index)
      return TAC_USE_ARG1;
    if (_is_first_allocator_arg2_consumer(consumer->op) == YES && consumer->arg2_type == TAC_ARG_TYPE_TEMP && (int)consumer->arg2_d == register_index)
      return TAC_USE_ARG2;
  }

  return -1;
}


static int _is_register_allocator_jump_op(int op) {

  if (op == TAC_OP_JUMP || op == TAC_OP_JUMP_EQ || op == TAC_OP_JUMP_NEQ ||
      op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_GT || op == TAC_OP_JUMP_LTE ||
      op == TAC_OP_JUMP_GTE)
    return YES;

  return NO;
}


static int _is_register_allocator_conditional_jump_op(int op) {

  if (op == TAC_OP_JUMP_EQ || op == TAC_OP_JUMP_NEQ ||
      op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_GT || op == TAC_OP_JUMP_LTE ||
      op == TAC_OP_JUMP_GTE)
    return YES;

  return NO;
}


static int _is_register_allocator_unmigrated_emitter(int op) {

  return NO;
}


static int _get_register_allocator_basic_block_end_reason(struct tac *t) {

  if (t->op == TAC_OP_FUNCTION_CALL)
    return RA_BLOCK_END_FUNCTION_CALL;
  if (t->op == TAC_OP_RETURN || t->op == TAC_OP_RETURN_VALUE)
    return RA_BLOCK_END_RETURN;
  if (t->op == TAC_OP_ASM)
    return RA_BLOCK_END_INLINE_ASM;
  if (_is_register_allocator_jump_op(t->op) == YES)
    return RA_BLOCK_END_JUMP;
  if (_is_register_allocator_unmigrated_emitter(t->op) == YES)
    return RA_BLOCK_END_UNMIGRATED;

  return RA_BLOCK_END_NONE;
}


static struct register_allocator_instruction *_create_register_allocator_instructions(struct tree_node *function_node, size_t instruction_bytes) {

  int i;
  struct register_allocator_instruction *instructions;

  instructions = (struct register_allocator_instruction *)calloc(1, instruction_bytes);
  if (instructions == NULL)
    return NULL;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t;

    t = &g_tacs[i];
    if (t->function_node != function_node || t->op == TAC_OP_DEAD || t->op == TAC_OP_CREATE_VARIABLE || (t->op == TAC_OP_LABEL && t->is_function == YES))
      continue;

    instructions[i].active = YES;
    instructions[i].tac_index = i;
    instructions[i].is_label = t->op == TAC_OP_LABEL ? YES : NO;
    instructions[i].label = t->result_s;
    instructions[i].end_reason = _get_register_allocator_basic_block_end_reason(t);
    instructions[i].is_jump = _is_register_allocator_jump_op(t->op);
    instructions[i].is_conditional_jump = _is_register_allocator_conditional_jump_op(t->op);
    instructions[i].jump_target = t->result_s;
  }

  return instructions;
}


static int _collect_register_allocator_basic_blocks(struct tree_node *function_node, struct register_allocator_basic_block *blocks, int max_blocks, size_t instruction_bytes, int *block_count) {

  int result;
  struct register_allocator_instruction *instructions;

  instructions = _create_register_allocator_instructions(function_node, instruction_bytes);
  if (instructions == NULL)
    return FAILED;

  result = register_allocator_build_basic_blocks(function_node->children[1]->label, instructions, g_tacs_count, blocks, max_blocks, block_count);
  free(instructions);
  return result;
}


static int _collect_register_allocator_cfg_edges(struct tree_node *function_node, struct register_allocator_basic_block *blocks, int block_count, struct register_allocator_cfg_edge *edges, int max_edges, size_t instruction_bytes, int *edge_count) {

  int result;
  struct register_allocator_instruction *instructions;

  instructions = _create_register_allocator_instructions(function_node, instruction_bytes);
  if (instructions == NULL)
    return FAILED;

  result = register_allocator_build_cfg(function_node->children[1]->label, instructions, g_tacs_count, blocks, block_count, edges, max_edges, edge_count);
  free(instructions);
  return result;
}


static int _tac_reads_temp_register(struct tac *t, int register_index) {

  int i;

  if (t->op == TAC_OP_FUNCTION_CALL || t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    for (i = 0; i < t->arguments_count; i++) {
      if (t->arguments[i].type == TAC_ARG_TYPE_TEMP && (int)t->arguments[i].value == register_index)
        return YES;
    }
  }

  if (t->arg1_type == TAC_ARG_TYPE_TEMP && (int)t->arg1_d == register_index)
    return YES;
  if (t->arg2_type == TAC_ARG_TYPE_TEMP && (int)t->arg2_d == register_index)
    return YES;
  if (t->op == TAC_OP_ARRAY_WRITE && t->result_type == TAC_ARG_TYPE_TEMP && (int)t->result_d == register_index)
    return YES;

  return NO;
}


static int _tac_writes_temp_register(struct tac *t, int register_index) {

  if (t->op == TAC_OP_ARRAY_WRITE)
    return NO;

  if (t->result_type == TAC_ARG_TYPE_TEMP && (int)t->result_d == register_index)
    return YES;

  return NO;
}


static int _register_allocator_instruction_is_active(void *context, int instruction_index, int temp_index);


static int _register_allocator_liveness_reads_temp(void *context, int instruction_index, int temp_index) {

  struct tree_node *function_node;

  function_node = (struct tree_node *)context;
  return _tac_reads_temp_register(&g_tacs[instruction_index],
      function_node->local_variables->temp_registers[temp_index].register_index);
}


static int _register_allocator_liveness_writes_temp(void *context, int instruction_index, int temp_index) {

  struct tree_node *function_node;

  function_node = (struct tree_node *)context;
  return _tac_writes_temp_register(&g_tacs[instruction_index],
      function_node->local_variables->temp_registers[temp_index].register_index);
}


static int _compute_register_allocator_live_sets(struct tree_node *function_node, struct register_allocator_basic_block *blocks, int block_count, struct register_allocator_cfg_edge *edges, int edge_count, char *live_use, char *live_def, char *live_in, char *live_out, struct register_allocator_liveness_storage *storage, int *iterations) {

  int block_index;
  int temp_count;

  temp_count = function_node->local_variables->temp_registers_count;
  *iterations = 0;

  if (block_count <= 0 || temp_count <= 0)
    return SUCCEEDED;

  memset(live_use, 0, storage->buffer_bytes);
  memset(live_def, 0, storage->buffer_bytes);
  memset(live_in, 0, storage->buffer_bytes);
  memset(live_out, 0, storage->buffer_bytes);

  for (block_index = 0; block_index < block_count; block_index++) {
    int set_index;

    if (register_allocator_resolve_liveness_index(
        function_node->children[1]->label, block_count, temp_count, storage,
        block_index, 0, &set_index) == FAILED)
      return FAILED;
    if (register_allocator_collect_liveness_use_def(
        function_node->children[1]->label, block_index,
        blocks[block_index].start_tac, blocks[block_index].end_tac, g_tacs_count,
        temp_count, function_node, _register_allocator_instruction_is_active,
        _register_allocator_liveness_reads_temp,
        _register_allocator_liveness_writes_temp, live_use + set_index,
        live_def + set_index) == FAILED)
      return FAILED;
  }

  return register_allocator_solve_liveness(function_node->children[1]->label, block_count, temp_count, storage, edges, edge_count, live_use, live_def, live_in, live_out, iterations);
}


static int _register_allocator_instruction_is_active(void *context, int instruction_index, int temp_index) {

  struct tac *t;
  struct tree_node *function_node;

  (void)temp_index;
  t = &g_tacs[instruction_index];
  function_node = (struct tree_node *)context;
  if (function_node != NULL && t->function_node != function_node)
    return NO;
  if (t->op == TAC_OP_DEAD || t->op == TAC_OP_CREATE_VARIABLE)
    return NO;
  return YES;
}


static int _register_allocator_instruction_reads_temp(void *context, int instruction_index, int temp_index) {

  (void)context;
  return _tac_reads_temp_register(&g_tacs[instruction_index], temp_index);
}


static int _register_allocator_instruction_writes_temp(void *context, int instruction_index, int temp_index) {

  (void)context;
  return _tac_writes_temp_register(&g_tacs[instruction_index], temp_index);
}


static int _register_allocator_instruction_is_split_reload_eligible(void *context, int instruction_index, int temp_index) {

  struct tac *instruction;
  struct temp_register *temp_register;
  struct tree_node *function_node;
  int operand;
  int physical_register;

  function_node = (struct tree_node *)context;
  instruction = &g_tacs[instruction_index];
  if (instruction->function_node != function_node)
    return NO;
  operand = _get_register_allocator_consumer_arg(instruction, temp_index);
  temp_register = _find_temp_register_info(function_node, temp_index);
  if (operand < 0 || temp_register == NULL)
    return NO;
  physical_register = _get_register_allocator_split_reload_physical_register(
      instruction, operand, temp_register->size);
  if (physical_register == Z80_PHY_NONE)
    return NO;
  if ((operand == TAC_USE_RESULT && instruction->result_physical_register != Z80_PHY_NONE) ||
      (operand == TAC_USE_ARG1 && instruction->arg1_physical_register != Z80_PHY_NONE) ||
      (operand == TAC_USE_ARG2 && instruction->arg2_physical_register != Z80_PHY_NONE))
    return NO;
#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: reload_operand_eligible function=%s tac=%d temp=r%d op=%d operand=%d phy=%d status=eligible\n",
      function_node->children[1]->label, instruction_index, temp_index,
      instruction->op, operand, physical_register);
#endif
  return YES;
}


static int _register_allocator_get_reload_operand(void *context,
    int instruction_index, int temp_index) {

  struct tree_node *function_node;

  function_node = (struct tree_node *)context;
  if (function_node == NULL || instruction_index < 0 ||
      instruction_index >= g_tacs_count ||
      g_tacs[instruction_index].function_node != function_node)
    return -1;
  return _get_register_allocator_consumer_arg(&g_tacs[instruction_index],
      temp_index);
}


static int _register_allocator_apply_spill_mutation(void *context, int instruction_index, int temp_index, int operand, int physical_register) {

  struct tree_node *function_node;
  struct tac *instruction;

  function_node = (struct tree_node *)context;
  if (function_node == NULL || instruction_index < 0 || instruction_index >= g_tacs_count ||
      operand != TAC_USE_ARG1 || physical_register == Z80_PHY_NONE)
    return FAILED;

  instruction = &g_tacs[instruction_index];
  if (instruction->function_node != function_node || instruction->arg1_type != TAC_ARG_TYPE_TEMP ||
      (int)instruction->arg1_d != temp_index)
    return FAILED;

  instruction->store_retained_to_spill_operand = operand;
  return SUCCEEDED;
}


static int _register_allocator_apply_reload_transaction(void *context,
    struct register_allocator_reload_mutation *mutations,
    int mutation_count) {

  struct tree_node *function_node;
  int mutation_index;

  function_node = (struct tree_node *)context;
  if (function_node == NULL || mutations == NULL || mutation_count < 0)
    return FAILED;
  for (mutation_index = 0; mutation_index < mutation_count; mutation_index++) {
    struct register_allocator_reload_mutation *mutation;
    struct tac *instruction;

    mutation = &mutations[mutation_index];
    if (mutation->apply != YES || mutation->instruction < 0 ||
        mutation->instruction >= g_tacs_count || mutation->temp_index < 0 ||
        (mutation->operand != TAC_USE_RESULT &&
         mutation->operand != TAC_USE_ARG1 &&
         mutation->operand != TAC_USE_ARG2) ||
        mutation->physical_register == Z80_PHY_NONE)
      return FAILED;
    instruction = &g_tacs[mutation->instruction];
    if (instruction->function_node != function_node ||
        _get_register_allocator_consumer_arg(instruction,
        mutation->temp_index) != mutation->operand ||
        (mutation->operand == TAC_USE_RESULT &&
         instruction->result_physical_register != Z80_PHY_NONE) ||
        (mutation->operand == TAC_USE_ARG1 &&
         instruction->arg1_physical_register != Z80_PHY_NONE) ||
        (mutation->operand == TAC_USE_ARG2 &&
         instruction->arg2_physical_register != Z80_PHY_NONE))
      return FAILED;
  }
  for (mutation_index = 0; mutation_index < mutation_count; mutation_index++) {
    struct register_allocator_reload_mutation *mutation;
    struct tac *instruction;

    mutation = &mutations[mutation_index];
    instruction = &g_tacs[mutation->instruction];
    if (mutation->operand == TAC_USE_RESULT)
      instruction->result_physical_register = mutation->physical_register;
    else if (mutation->operand == TAC_USE_ARG1)
      instruction->arg1_physical_register = mutation->physical_register;
    else
      instruction->arg2_physical_register = mutation->physical_register;
    instruction->reload_spill_to_physical_operand = mutation->operand;
  }
#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: reload_transaction_apply function=%s mutations=%d transaction=committed atomic=yes status=complete\n",
      function_node->children[1]->label, mutation_count);
#endif
  return SUCCEEDED;
}


static int _register_allocator_select_reload_register(void *context,
    int instruction_index, int temp_index, int operand) {

  struct tree_node *function_node;
  struct temp_register *temp_register;

  function_node = (struct tree_node *)context;
  temp_register = _find_temp_register_info(function_node, temp_index);
  if (temp_register == NULL)
    return Z80_PHY_NONE;
  return _get_register_allocator_split_reload_physical_register(&g_tacs[instruction_index],
      operand, temp_register->size);
}


static int _register_allocator_instruction_get_produced_temp(void *context, int instruction_index, int temp_index) {

  struct tree_node *function_node;
  struct tac *producer;

  (void)temp_index;
  function_node = (struct tree_node *)context;
  producer = &g_tacs[instruction_index];
  if (producer->function_node != function_node)
    return -1;
  if (_is_register_allocator_linear_scan_producer(producer) == NO)
    return -1;
  if (producer->result_type != TAC_ARG_TYPE_TEMP)
    return -1;

  return (int)producer->result_d;
}


static int _register_allocator_instruction_get_consumer_operand(void *context, int instruction_index, int temp_index) {

  (void)context;
  return _get_register_allocator_consumer_arg(&g_tacs[instruction_index], temp_index);
}


static int _find_next_temp_read_in_basic_block(struct tree_node *function_node, int register_index, int start_tac, int end_tac) {

  return register_allocator_find_next_use(function_node->children[1]->label, register_index, start_tac, end_tac, g_tacs_count, NULL, _register_allocator_instruction_is_active, _register_allocator_instruction_reads_temp, _register_allocator_instruction_writes_temp);
}


#if defined(DEBUG_PASS_5)
static char *_get_first_allocator_op_name(int op);
static char *_get_first_allocator_physical_register_name(int physical_register);
#endif


static int _is_z80_basic_block_transparent_tac_for_physical_register(struct tac *t, int physical_register) {

  if (t->op == TAC_OP_DEAD || t->op == TAC_OP_CREATE_VARIABLE)
    return YES;

  if (physical_register == Z80_PHY_C) {
    if (_is_register_allocator_c_transparent_arithmetic_tac(t) == YES)
      return YES;
    if ((t->op == TAC_OP_JUMP_EQ || t->op == TAC_OP_JUMP_NEQ ||
        t->op == TAC_OP_JUMP_LT || t->op == TAC_OP_JUMP_GT ||
        t->op == TAC_OP_JUMP_LTE || t->op == TAC_OP_JUMP_GTE) &&
        (t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 ||
        t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
        (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 ||
        t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
      return YES;
    if (_z80_tac_may_clobber_bc(t->op) == NO)
      return YES;
    return NO;
  }

  if (physical_register == Z80_PHY_B) {
    if (_is_register_allocator_b_transparent_arithmetic_tac(t) == YES)
      return YES;
    if (_z80_tac_may_clobber_bc(t->op) == NO)
      return YES;
    return NO;
  }

  if (physical_register == Z80_PHY_BC) {
    if (_z80_tac_may_clobber_bc(t->op) == NO)
      return YES;
    return NO;
  }

  if (t->op == TAC_OP_ASSIGNMENT) {
    if (physical_register == Z80_PHY_HL)
      return YES;
    if (physical_register == Z80_PHY_A && t->arg1_type == TAC_ARG_TYPE_CONSTANT)
      return YES;
  }

  return NO;
}


static int _is_basic_block_transparent_tac_for_physical_register(struct tac *t, int physical_register) {

  struct register_allocator_target_policy *target_policy;
  int is_transparent;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->is_tac_transparent_for_physical_register == NULL)
    return NO;

  is_transparent = target_policy->is_tac_transparent_for_physical_register(t, physical_register);

#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: target_transparency function=%s tac=%d op=%s phy=%s transparent=%s\n",
      t->function_node->children[1]->label, (int)(t - g_tacs), _get_first_allocator_op_name(t->op),
      _get_first_allocator_physical_register_name(physical_register), is_transparent == YES ? "yes" : "no");
#endif

  return is_transparent;
}


struct register_allocator_path_context {
  struct tree_node *function_node;
  int block_index;
  int end_tac;
};


static int _is_path_instruction_target_transparent(void *context, int instruction_index, int physical_register) {

  (void)context;
  return _is_basic_block_transparent_tac_for_physical_register(&g_tacs[instruction_index], physical_register);
}


struct register_allocator_loop_register_path_context {
  struct tree_node *function_node;
  struct register_allocator_join_assignment *assignments;
  struct register_allocator_join_block_reservation *reservations;
  int assignment_count;
  int reservation_count;
  int block_index;
  int preferred_physical_register;
  int temp_index;
};


static int _is_loop_register_path_safe(void *context, int physical_register) {

  struct register_allocator_loop_register_path_context *path_context;
  int reservation_index;

  path_context = (struct register_allocator_loop_register_path_context *)context;
  if (physical_register != path_context->preferred_physical_register) {
    int consumer_index;
    int producer_index;

    for (producer_index = 0;
        producer_index < path_context->assignment_count; producer_index++) {
      if (path_context->assignments[producer_index].role !=
          RA_JOIN_ASSIGNMENT_PRODUCER)
        continue;
      for (consumer_index = 0;
          consumer_index < path_context->assignment_count; consumer_index++) {
        if (path_context->assignments[consumer_index].role !=
            RA_JOIN_ASSIGNMENT_CONSUMER)
          continue;
        if (_is_register_allocator_candidate_allowed_for_physical_register(
            &g_tacs[path_context->assignments[producer_index].instruction],
            &g_tacs[path_context->assignments[consumer_index].instruction],
            &path_context->function_node->local_variables->temp_registers[
            path_context->temp_index], physical_register,
            path_context->assignments[consumer_index].operand) == NO) {
          fprintf(stderr, "register_allocator: loop_register_path_legality_preflight function=%s block=%d temp=r%d candidate=%s producer=%d consumer=%d operand=%d legal=no status=complete\n",
              path_context->function_node->children[1]->label,
              path_context->block_index, path_context->temp_index,
              _get_first_allocator_physical_register_name(physical_register),
              path_context->assignments[producer_index].instruction,
              path_context->assignments[consumer_index].instruction,
              path_context->assignments[consumer_index].operand);
          return NO;
        }
      }
    }
  }
  fprintf(stderr, "register_allocator: loop_register_path_legality_preflight function=%s block=%d temp=r%d candidate=%s producers_consumers=complete legal=yes status=complete\n",
      path_context->function_node->children[1]->label,
      path_context->block_index, path_context->temp_index,
      _get_first_allocator_physical_register_name(physical_register));
  for (reservation_index = 0;
      reservation_index < path_context->reservation_count;
      reservation_index++) {
    int blocking_instruction;
    int instruction_index;
    int transparent;

    blocking_instruction = -1;
    transparent = YES;
    for (instruction_index =
        path_context->reservations[reservation_index].start_instruction + 1;
        instruction_index <
        path_context->reservations[reservation_index].end_instruction;
        instruction_index++) {
      int assignment_index;
      int assigned;

      assigned = NO;
      for (assignment_index = 0;
          assignment_index < path_context->assignment_count;
          assignment_index++) {
        if (path_context->assignments[assignment_index].instruction ==
            instruction_index) {
          assigned = YES;
          break;
        }
      }
      if (assigned == YES)
        continue;
      if (_is_basic_block_transparent_tac_for_physical_register(
          &g_tacs[instruction_index], physical_register) == NO) {
        transparent = NO;
        blocking_instruction = instruction_index;
        break;
      }
    }
    fprintf(stderr, "register_allocator: loop_register_path_reservation_preflight function=%s block=%d temp=r%d candidate=%s reservation=%d path_block=%d start=%d end=%d transparent=%s blocking=%d status=complete\n",
        path_context->function_node->children[1]->label,
        path_context->block_index, path_context->temp_index,
        _get_first_allocator_physical_register_name(physical_register),
        reservation_index,
        path_context->reservations[reservation_index].block_index,
        path_context->reservations[reservation_index].start_instruction,
        path_context->reservations[reservation_index].end_instruction,
        transparent == YES ? "yes" : "no", blocking_instruction);
    if (transparent == NO)
      return NO;
  }
  return YES;
}


static int _is_candidate_path_transparent_for_physical_register(struct tree_node *function_node, int start_tac, int end_tac, int physical_register) {

  int blocking_instruction;
  int transparent;

  if (register_allocator_is_transparent_range(function_node->children[1]->label, start_tac,
      end_tac, g_tacs_count, physical_register, Z80_PHY_NONE, function_node,
      _is_path_instruction_target_transparent, &transparent, &blocking_instruction) == FAILED)
    return NO;
  return transparent;
}


static int _candidate_spill_reason_blocks_interval(struct tree_node *function_node, struct temp_register *temp_register, int producer_tac, int consumer_tac);


static int _competing_candidate_has_metadata(void *context,
    struct register_allocator_candidate *candidate, int physical_register) {

  struct tree_node *function_node;

  (void)physical_register;
  function_node = (struct tree_node *)context;
  return _find_temp_register_info(function_node, candidate->temp_index) != NULL ? YES : NO;
}


static int _competing_candidate_interval_is_clear(void *context,
    struct register_allocator_candidate *candidate, int physical_register) {

  struct tree_node *function_node;
  struct temp_register *temp_register;

  (void)physical_register;
  function_node = (struct tree_node *)context;
  temp_register = _find_temp_register_info(function_node, candidate->temp_index);
  if (temp_register == NULL)
    return NO;
  return _candidate_spill_reason_blocks_interval(function_node, temp_register,
      candidate->producer_instruction, candidate->consumer_instruction) == YES ? NO : YES;
}


static int _competing_candidate_is_allowed(void *context,
    struct register_allocator_candidate *candidate, int physical_register) {

  struct tree_node *function_node;
  struct temp_register *temp_register;

  function_node = (struct tree_node *)context;
  temp_register = _find_temp_register_info(function_node, candidate->temp_index);
  if (temp_register == NULL)
    return NO;
  return _is_register_allocator_candidate_allowed_for_physical_register(
      &g_tacs[candidate->producer_instruction], &g_tacs[candidate->consumer_instruction],
      temp_register, physical_register, candidate->consumer_operand);
}


static int _competing_candidate_path_is_transparent(void *context,
    struct register_allocator_candidate *candidate, int physical_register) {

  return _is_candidate_path_transparent_for_physical_register((struct tree_node *)context,
      candidate->producer_instruction, candidate->consumer_instruction, physical_register);
}


static int _probe_competing_candidate(struct tree_node *function_node, int tac_index,
    int end_tac, int range_mode, int physical_register,
    struct register_allocator_candidate_probe *probe) {

  return register_allocator_probe_competing_candidate(function_node->children[1]->label,
      tac_index, end_tac, g_tacs_count, end_tac, range_mode, physical_register,
      Z80_PHY_NONE, function_node, _register_allocator_instruction_get_produced_temp,
      _register_allocator_instruction_is_active, _register_allocator_instruction_reads_temp,
      _register_allocator_instruction_writes_temp,
      _register_allocator_instruction_get_consumer_operand,
      _competing_candidate_has_metadata, _competing_candidate_interval_is_clear,
      _competing_candidate_is_allowed, _competing_candidate_path_is_transparent, probe);
}


static int _is_nearer_competing_candidate(struct tree_node *function_node, int tac_index, int end_tac, int physical_register) {

  struct register_allocator_candidate_probe probe;

  if (_probe_competing_candidate(function_node, tac_index, end_tac,
      RA_CANDIDATE_RANGE_BEFORE_DECISION, physical_register, &probe) == FAILED ||
      probe.qualification.eligible == NO)
    return NO;

#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: linear_scan competing_candidate function=%s r%d producer_tac=%d next_tac=%d before_tac=%d\n", function_node->children[1]->label, probe.candidate.temp_index, tac_index, probe.candidate.consumer_instruction, end_tac);
#endif

  return YES;
}


static int _is_intervening_bc_candidate_transparent_for_hl(struct tree_node *function_node, int tac_index, int end_tac) {

  struct register_allocator_candidate_probe probe;

  if (_probe_competing_candidate(function_node, tac_index, end_tac,
      RA_CANDIDATE_RANGE_WITHIN_BLOCK, Z80_PHY_BC, &probe) == FAILED ||
      probe.qualification.eligible == NO)
    return NO;

#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: linear_scan bc_transparent_candidate function=%s r%d producer_tac=%d next_tac=%d preserves=HL\n", function_node->children[1]->label, probe.candidate.temp_index, tac_index, probe.candidate.consumer_instruction);
#endif

  return YES;
}


static int _candidate_spill_reason_blocks_interval(struct tree_node *function_node, struct temp_register *temp_register, int producer_tac, int consumer_tac) {

  int blocked;

  if (register_allocator_interval_is_blocked(function_node->children[1]->label, producer_tac,
      consumer_tac, temp_register->spill_reason, Z80_SPILL_REASON_NONE,
      Z80_SPILL_REASON_MAINMAIN, temp_register->spill_boundary_tac, &blocked) == FAILED)
    return YES;
  return blocked;
}


static void _mark_tac_retained_interval(int producer_tac, int consumer_tac, int consumer_arg, int physical_register) {

  g_tacs[producer_tac].result_physical_register = physical_register;
  if (consumer_arg == TAC_USE_RESULT)
    g_tacs[consumer_tac].result_physical_register = physical_register;
  else if (consumer_arg == TAC_USE_ARG1)
    g_tacs[consumer_tac].arg1_physical_register = physical_register;
  else if (consumer_arg == TAC_USE_ARG2)
    g_tacs[consumer_tac].arg2_physical_register = physical_register;
}


static void _clear_tac_retained_interval(int producer_tac, int consumer_tac, int consumer_arg) {

  if (producer_tac >= 0)
    g_tacs[producer_tac].result_physical_register = Z80_PHY_NONE;
  if (consumer_tac >= 0 && consumer_arg == TAC_USE_RESULT)
    g_tacs[consumer_tac].result_physical_register = Z80_PHY_NONE;
  else if (consumer_tac >= 0 && consumer_arg == TAC_USE_ARG1)
    g_tacs[consumer_tac].arg1_physical_register = Z80_PHY_NONE;
  else if (consumer_tac >= 0 && consumer_arg == TAC_USE_ARG2)
    g_tacs[consumer_tac].arg2_physical_register = Z80_PHY_NONE;

  if (consumer_tac >= 0 && consumer_arg == TAC_USE_ARG1)
    g_tacs[consumer_tac].store_retained_to_spill_operand = -1;
}


static char *_get_first_allocator_physical_register_name(int physical_register);
static char *_get_first_allocator_spill_reason_name(int spill_reason);


static int _is_spilled_constant_assignment_transparent_for_physical_register(struct tree_node *function_node, struct tac *t, int tac_index, int physical_register) {

  struct temp_register *temp_register;
  int register_index;

  if (physical_register != Z80_PHY_B && physical_register != Z80_PHY_C && physical_register != Z80_PHY_BC)
    return NO;
  if (t->op != TAC_OP_ASSIGNMENT || t->arg1_type != TAC_ARG_TYPE_CONSTANT || t->result_type != TAC_ARG_TYPE_TEMP)
    return NO;
  if (t->result_physical_register != Z80_PHY_NONE)
    return NO;

  register_index = (int)t->result_d;
  temp_register = _find_temp_register_info(function_node, register_index);
  if (temp_register == NULL || temp_register->spill_reason == Z80_SPILL_REASON_NONE)
    return NO;

#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: assignment_transparency function=%s tac=%d r%d source=constant value=%d destination=spill preserves=%s spill_reason=%s\n",
          function_node->children[1]->label, tac_index, register_index, (int)t->arg1_d,
      _get_first_allocator_physical_register_name(physical_register), _get_first_allocator_spill_reason_name(temp_register->spill_reason));
#endif

  return YES;
}


static int _path_has_nearer_candidate(void *context, int instruction_index, int physical_register) {

  struct register_allocator_path_context *path_context;

  path_context = (struct register_allocator_path_context *)context;
  return _is_nearer_competing_candidate(path_context->function_node, instruction_index,
      path_context->end_tac, physical_register);
}


static int _path_has_preserving_overlap(void *context, int instruction_index, int physical_register) {

  struct register_allocator_path_context *path_context;

  path_context = (struct register_allocator_path_context *)context;
  if (physical_register != Z80_PHY_HL)
    return NO;
  return _is_intervening_bc_candidate_transparent_for_hl(path_context->function_node,
      instruction_index, path_context->end_tac);
}


static int _path_is_special_transparent(void *context, int instruction_index, int physical_register) {

  struct register_allocator_path_context *path_context;

  path_context = (struct register_allocator_path_context *)context;
  return _is_spilled_constant_assignment_transparent_for_physical_register(
      path_context->function_node, &g_tacs[instruction_index], instruction_index,
      physical_register);
}


static int _is_basic_block_path_safe_for_physical_register(struct tree_node *function_node, int block_index, int start_tac, int end_tac, int physical_register) {

  struct register_allocator_path_context path_context;
  int deciding_instruction;
  int path_safe;

  path_context.function_node = function_node;
  path_context.block_index = block_index;
  path_context.end_tac = end_tac;
  if (register_allocator_evaluate_candidate_path(function_node->children[1]->label, block_index,
      start_tac, end_tac, g_tacs_count, physical_register, Z80_PHY_NONE, &path_context,
      _path_has_nearer_candidate, _path_has_preserving_overlap, _path_is_special_transparent,
      _is_path_instruction_target_transparent, &path_safe, &deciding_instruction) == FAILED)
    return NO;
  return path_safe;
}


#if defined(DEBUG_PASS_5)
static char *_get_first_allocator_op_name(int op);
static char *_get_first_allocator_physical_register_name(int physical_register);
static char *_get_first_allocator_spill_reason_name(int spill_reason);
static char *_get_register_allocator_consumer_arg_name(int consumer_arg);
#endif


static int _retain_temp_register_for_linear_scan(struct tree_node *function_node, struct temp_register *temp_register, int physical_register, int block_index, int producer_tac, int consumer_tac, int consumer_arg, int *retained_interval) {

  struct register_allocator_temp_state state;
  int retain_mode;

  if (retained_interval == NULL)
    return FAILED;

  state.spill_required = temp_register->spill_required;
  state.physical_register = temp_register->physical_register;
  retain_mode = register_allocator_retain_temp_state(function_node->children[1]->label, block_index,
      temp_register->register_index, &state, Z80_PHY_NONE, physical_register,
      temp_register->spill_reason != Z80_SPILL_REASON_NONE ? YES : NO,
      temp_register->read_count, temp_register->write_count);
  if (retain_mode == FAILED)
    return FAILED;

  temp_register->spill_required = state.spill_required;
  temp_register->physical_register = state.physical_register;
  _mark_tac_retained_interval(producer_tac, consumer_tac, consumer_arg, physical_register);
  *retained_interval = YES;

  if (retain_mode == RA_TEMP_RETAIN_REGISTER_ONLY) {

#if defined(DEBUG_PASS_5)
    fprintf(stderr, "register_allocator: linear_scan retain function=%s block=%d r%d %d-bit in %s producer_tac=%d(%s) consumer_tac=%d(%s %s)\n", function_node->children[1]->label, block_index, temp_register->register_index, temp_register->size, _get_first_allocator_physical_register_name(temp_register->physical_register), producer_tac, _get_first_allocator_op_name(g_tacs[producer_tac].op), consumer_tac, _get_first_allocator_op_name(g_tacs[consumer_tac].op), _get_register_allocator_consumer_arg_name(consumer_arg));
#endif

    return SUCCEEDED;
  }

#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: linear_scan retain_interval function=%s block=%d r%d %d-bit in %s producer_tac=%d(%s) consumer_tac=%d(%s %s) spill_slot=kept reason=%s boundary_tac=%d\n", function_node->children[1]->label, block_index, temp_register->register_index, temp_register->size, _get_first_allocator_physical_register_name(physical_register), producer_tac, _get_first_allocator_op_name(g_tacs[producer_tac].op), consumer_tac, _get_first_allocator_op_name(g_tacs[consumer_tac].op), _get_register_allocator_consumer_arg_name(consumer_arg), _get_first_allocator_spill_reason_name(temp_register->spill_reason), temp_register->spill_boundary_tac);
#endif

  return SUCCEEDED;
}


static int _spill_temp_register_for_linear_scan(struct tree_node *function_node, struct temp_register *temp_register, int block_index, char *reason) {

  struct register_allocator_temp_state state;

  state.spill_required = temp_register->spill_required;
  state.physical_register = temp_register->physical_register;
  if (register_allocator_spill_temp_state(function_node->children[1]->label, block_index,
      temp_register->register_index, &state, Z80_PHY_NONE, reason) == FAILED)
    return FAILED;

  temp_register->spill_required = state.spill_required;
  temp_register->physical_register = state.physical_register;

#if defined(DEBUG_PASS_5)
  fprintf(stderr, "register_allocator: linear_scan spill function=%s block=%d r%d reason=%s\n", function_node->children[1]->label, block_index, temp_register->register_index, reason);
#endif

  return SUCCEEDED;
}


struct register_allocator_slot_transition_context {
  struct tree_node *function_node;
  int block_index;
  int physical_register;
};


static void _observe_register_allocator_transition_preparation(void *context,
    struct register_allocator_candidate_transition_preparation *preparation) {

  struct register_allocator_slot_transition_context *transition_context;

  transition_context = (struct register_allocator_slot_transition_context *)context;
  fprintf(stderr, "register_allocator: target_active_slot function=%s block=%d target=%s phy=%s slot=%d name=%s status=complete\n",
      transition_context->function_node->children[1]->label,
      transition_context->block_index, _get_register_allocator_target_policy()->name,
      _get_first_allocator_physical_register_name(transition_context->physical_register),
      preparation->slot_index, preparation->slot_name);
}


static int _clear_register_allocator_interval(void *context, int producer_instruction, int consumer_instruction, int consumer_operand) {

  (void)context;
  _clear_tac_retained_interval(producer_instruction, consumer_instruction, consumer_operand);
  return SUCCEEDED;
}


static int _spill_register_allocator_transition_temp(void *context, int temp_index) {

  struct register_allocator_slot_transition_context *transition_context;
  struct temp_register *temp_register;

  transition_context = (struct register_allocator_slot_transition_context *)context;
  temp_register = _find_temp_register_info(transition_context->function_node, temp_index);
  if (temp_register == NULL)
    return FAILED;
  return _spill_temp_register_for_linear_scan(transition_context->function_node, temp_register,
      transition_context->block_index, "farther_next_use");
}


static int _retain_register_allocator_transition_candidate(void *context, int temp_index,
    int physical_register, int producer_instruction, int consumer_instruction,
    int consumer_operand, int *retained_interval) {

  struct register_allocator_slot_transition_context *transition_context;
  struct temp_register *temp_register;

  transition_context = (struct register_allocator_slot_transition_context *)context;
  temp_register = _find_temp_register_info(transition_context->function_node, temp_index);
  if (temp_register == NULL)
    return FAILED;
  return _retain_temp_register_for_linear_scan(transition_context->function_node, temp_register,
      physical_register, transition_context->block_index, producer_instruction,
      consumer_instruction, consumer_operand, retained_interval);
}


#if defined(DEBUG_PASS_5)
static char *_get_first_allocator_op_name(int op) {

  if (op == TAC_OP_DEAD)
    return "DEAD";
  if (op == TAC_OP_CREATE_VARIABLE)
    return "CREATE_VARIABLE";
  if (op == TAC_OP_ADD)
    return "ADD";
  if (op == TAC_OP_SUB)
    return "SUB";
  if (op == TAC_OP_AND)
    return "AND";
  if (op == TAC_OP_OR)
    return "OR";
  if (op == TAC_OP_XOR)
    return "XOR";
  if (op == TAC_OP_COMPLEMENT)
    return "COMPLEMENT";
  if (op == TAC_OP_ASSIGNMENT)
    return "ASSIGNMENT";
  if (op == TAC_OP_MUL)
    return "MUL";
  if (op == TAC_OP_DIV)
    return "DIV";
  if (op == TAC_OP_MOD)
    return "MOD";
  if (op == TAC_OP_SHIFT_LEFT)
    return "SHIFT_LEFT";
  if (op == TAC_OP_SHIFT_RIGHT)
    return "SHIFT_RIGHT";
  if (op == TAC_OP_RETURN)
    return "RETURN";
  if (op == TAC_OP_RETURN_VALUE)
    return "RETURN_VALUE";
  if (op == TAC_OP_FUNCTION_CALL)
    return "FUNCTION_CALL";
  if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE)
    return "FUNCTION_CALL_USE_RETURN_VALUE";
  if (op == TAC_OP_LABEL)
    return "LABEL";
  if (op == TAC_OP_JUMP)
    return "JUMP";
  if (op == TAC_OP_ASM)
    return "ASM";
  if (op == TAC_OP_ARRAY_READ)
    return "ARRAY_READ";
  if (op == TAC_OP_ARRAY_WRITE)
    return "ARRAY_WRITE";
  if (op == TAC_OP_GET_ADDRESS)
    return "GET_ADDRESS";
  if (op == TAC_OP_GET_ADDRESS_ARRAY)
    return "GET_ADDRESS_ARRAY";
  if (op == TAC_OP_JUMP_EQ)
    return "JUMP_EQ";
  if (op == TAC_OP_JUMP_NEQ)
    return "JUMP_NEQ";
  if (op == TAC_OP_JUMP_LT)
    return "JUMP_LT";
  if (op == TAC_OP_JUMP_GT)
    return "JUMP_GT";
  if (op == TAC_OP_JUMP_LTE)
    return "JUMP_LTE";
  if (op == TAC_OP_JUMP_GTE)
    return "JUMP_GTE";

  return "UNKNOWN";
}


static char *_get_first_allocator_physical_register_name(int physical_register) {

  if (physical_register == Z80_PHY_A)
    return "A";
  if (physical_register == Z80_PHY_HL)
    return "HL";
  if (physical_register == Z80_PHY_BC)
    return "BC";
  if (physical_register == Z80_PHY_B)
    return "B";
  if (physical_register == Z80_PHY_C)
    return "C";

  return "NONE";
}


static char *_get_register_allocator_consumer_arg_name(int consumer_arg) {

  if (consumer_arg == TAC_USE_RESULT)
    return "result";
  if (consumer_arg == TAC_USE_ARG1)
    return "arg1";
  if (consumer_arg == TAC_USE_ARG2)
    return "arg2";

  return "unknown";
}


static void _debug_print_register_allocator_units(int units) {

  int first;

  if (units == RA_Z80_UNIT_NONE) {
    fprintf(stderr, "NONE");
    return;
  }

  first = YES;
  if ((units & RA_Z80_UNIT_A) != 0) {
    fprintf(stderr, "A");
    first = NO;
  }
  if ((units & RA_Z80_UNIT_B) != 0) {
    fprintf(stderr, "%sB", first == YES ? "" : "|");
    first = NO;
  }
  if ((units & RA_Z80_UNIT_C) != 0) {
    fprintf(stderr, "%sC", first == YES ? "" : "|");
    first = NO;
  }
  if ((units & RA_Z80_UNIT_H) != 0) {
    fprintf(stderr, "%sH", first == YES ? "" : "|");
    first = NO;
  }
  if ((units & RA_Z80_UNIT_L) != 0)
    fprintf(stderr, "%sL", first == YES ? "" : "|");
}


static void _debug_register_allocator_overlap_check(struct tree_node *function_node, int block_index, int candidate_register, int active_a_register_index, int active_hl_register_index, int active_bc_register_index, int active_b_register_index, int active_c_register_index, int conflict) {

  fprintf(stderr, "register_allocator: linear_scan overlap_check function=%s block=%d candidate=%s candidate_units=", function_node->children[1]->label, block_index, _get_first_allocator_physical_register_name(candidate_register));
  _debug_print_register_allocator_units(_get_register_allocator_physical_register_units(candidate_register));
  fprintf(stderr, " active_A=%s active_HL=%s active_BC=%s active_B=%s active_C=%s conflict=%s\n",
          active_a_register_index >= 0 ? "yes" : "no",
          active_hl_register_index >= 0 ? "yes" : "no",
          active_bc_register_index >= 0 ? "yes" : "no",
          active_b_register_index >= 0 ? "yes" : "no",
          active_c_register_index >= 0 ? "yes" : "no",
          conflict == YES ? "yes" : "no");
}


static char *_get_first_allocator_spill_reason_name(int spill_reason) {

  if (spill_reason == Z80_SPILL_REASON_FUNCTION_CALL)
    return "function_call";
  if (spill_reason == Z80_SPILL_REASON_RETURN)
    return "return";
  if (spill_reason == Z80_SPILL_REASON_LABEL)
    return "label";
  if (spill_reason == Z80_SPILL_REASON_INLINE_ASM)
    return "inline_asm";
  if (spill_reason == Z80_SPILL_REASON_UNMIGRATED_EMITTER)
    return "unmigrated_emitter";
  if (spill_reason == Z80_SPILL_REASON_MAINMAIN)
    return "mainmain";
  if (spill_reason == Z80_SPILL_REASON_BLOCK_EXIT)
    return "block_exit";

  return "none";
}


static char *_get_register_allocator_basic_block_end_reason_name(int end_reason) {

  if (end_reason == RA_BLOCK_END_LABEL)
    return "label";
  if (end_reason == RA_BLOCK_END_JUMP)
    return "jump";
  if (end_reason == RA_BLOCK_END_FUNCTION_CALL)
    return "function_call";
  if (end_reason == RA_BLOCK_END_RETURN)
    return "return";
  if (end_reason == RA_BLOCK_END_INLINE_ASM)
    return "inline_asm";
  if (end_reason == RA_BLOCK_END_UNMIGRATED)
    return "unmigrated_emitter";
  if (end_reason == RA_BLOCK_END_FUNCTION_END)
    return "function_end";

  return "none";
}


static void _debug_print_z80_tac_clobber_set(int clobbers) {

  int first;

  if (clobbers == Z80_TAC_CLOBBER_NONE) {
    fprintf(stderr, "NONE");
    return;
  }

  first = YES;
  if ((clobbers & Z80_TAC_CLOBBER_A) != 0) {
    fprintf(stderr, "A");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_BC) != 0) {
    fprintf(stderr, "%sBC", first == YES ? "" : "|");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_DE) != 0) {
    fprintf(stderr, "%sDE", first == YES ? "" : "|");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_HL) != 0) {
    fprintf(stderr, "%sHL", first == YES ? "" : "|");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_IX) != 0) {
    fprintf(stderr, "%sIX", first == YES ? "" : "|");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_IY) != 0) {
    fprintf(stderr, "%sIY", first == YES ? "" : "|");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_SP) != 0) {
    fprintf(stderr, "%sSP", first == YES ? "" : "|");
    first = NO;
  }
  if ((clobbers & Z80_TAC_CLOBBER_FLAGS) != 0)
    fprintf(stderr, "%sFLAGS", first == YES ? "" : "|");
}


static void _debug_register_allocator_basic_block_clobbers(struct tree_node *function_node, struct register_allocator_basic_block *block, int block_index) {

  int i;

  for (i = block->start_tac; i <= block->end_tac; i++) {
    int clobbers;

    if (g_tacs[i].op == TAC_OP_DEAD || g_tacs[i].op == TAC_OP_CREATE_VARIABLE)
      continue;

    clobbers = _get_register_allocator_tac_clobbers(g_tacs[i].op);
    if (clobbers == Z80_TAC_CLOBBER_NONE)
      continue;

    fprintf(stderr, "register_allocator: z80_clobber function=%s block=%d tac=%d(%s) may_clobber_bc=%s clobbers=", function_node->children[1]->label, block_index, i, _get_first_allocator_op_name(g_tacs[i].op), _z80_tac_may_clobber_bc(g_tacs[i].op) == YES ? "yes" : "no");
    _debug_print_z80_tac_clobber_set(clobbers);
    fprintf(stderr, "\n");

    if (g_tacs[i].op == TAC_OP_ASM) {
      fprintf(stderr, "register_allocator: inline_asm_contract function=%s block=%d tac=%d policy=hard_barrier operand_policy=stack_location clobbers=", function_node->children[1]->label, block_index, i);
      _debug_print_z80_tac_clobber_set(clobbers);
      fprintf(stderr, "\n");
    }

    if (g_tacs[i].op == TAC_OP_ARRAY_WRITE)
      fprintf(stderr, "register_allocator: migrated_emitter function=%s block=%d tac=%d(%s) barrier=removed clobber_checked=yes\n", function_node->children[1]->label, block_index, i, _get_first_allocator_op_name(g_tacs[i].op));
  }
}


static void _debug_register_allocator_basic_block_next_uses(struct tree_node *function_node, struct register_allocator_basic_block *block, int block_index) {

  int i, j;

  for (i = block->start_tac; i <= block->end_tac; i++) {
    if (g_tacs[i].op == TAC_OP_DEAD || g_tacs[i].op == TAC_OP_CREATE_VARIABLE)
      continue;

    for (j = 0; j < function_node->local_variables->temp_registers_count; j++) {
      struct temp_register *temp_register = &(function_node->local_variables->temp_registers[j]);
      int next_use;

      if (_tac_writes_temp_register(&g_tacs[i], temp_register->register_index) == NO)
        continue;

      next_use = _find_next_temp_read_in_basic_block(function_node, temp_register->register_index, i + 1, block->end_tac);
      if (next_use >= 0)
        fprintf(stderr, "register_allocator: next_use function=%s block=%d r%d from_tac=%d next_tac=%d distance=%d\n", function_node->children[1]->label, block_index, temp_register->register_index, i, next_use, next_use - i);
      else
        fprintf(stderr, "register_allocator: next_use function=%s block=%d r%d from_tac=%d next_tac=none\n", function_node->children[1]->label, block_index, temp_register->register_index, i);
    }
  }
}


static char *_get_register_allocator_cfg_edge_kind_name(int kind) {

  if (kind == RA_CFG_EDGE_FALLTHROUGH)
    return "fallthrough";
  if (kind == RA_CFG_EDGE_JUMP)
    return "jump";
  if (kind == RA_CFG_EDGE_BRANCH_TRUE)
    return "branch_true";
  if (kind == RA_CFG_EDGE_BRANCH_FALSE)
    return "branch_false";

  return "unknown";
}


static void _debug_register_allocator_cfg_edge(struct tree_node *function_node, struct register_allocator_cfg_edge *edge) {

  fprintf(stderr, "register_allocator: cfg_edge function=%s from_block=%d to_block=%d kind=%s", function_node->children[1]->label, edge->from_block, edge->to_block, _get_register_allocator_cfg_edge_kind_name(edge->kind));
  if (edge->target != NULL)
    fprintf(stderr, " target=%s", edge->target);
  fprintf(stderr, "\n");
}


static void _debug_register_allocator_cfg_edges(struct tree_node *function_node, struct register_allocator_cfg_edge *edges, int edge_count) {

  int i;

  for (i = 0; i < edge_count; i++)
    _debug_register_allocator_cfg_edge(function_node, &edges[i]);
}


static void _debug_print_register_allocator_temp_set(struct tree_node *function_node, char *set) {

  int temp_index;
  int temp_count;
  int first;

  temp_count = function_node->local_variables->temp_registers_count;
  first = YES;
  fprintf(stderr, "{");
  for (temp_index = 0; temp_index < temp_count; temp_index++) {
    if (set[temp_index] == NO)
      continue;
    fprintf(stderr, "%sr%d", first == YES ? "" : ",", function_node->local_variables->temp_registers[temp_index].register_index);
    first = NO;
  }
  fprintf(stderr, "}");
}


static int _debug_register_allocator_liveness(struct tree_node *function_node, struct register_allocator_basic_block *blocks, int block_count, struct register_allocator_cfg_edge *edges, int edge_count) {

  struct register_allocator_liveness_storage storage;
  char *live_use;
  char *live_def;
  char *live_in;
  char *live_out;
  int temp_count;
  int iterations;
  int block_index;

  temp_count = function_node->local_variables->temp_registers_count;

  if (block_count <= 0 || temp_count <= 0)
    return SUCCEEDED;
  if (register_allocator_plan_liveness_storage(function_node->children[1]->label,
      block_count, temp_count, &storage) == FAILED)
    return FAILED;

  live_use = (char *)calloc(1, storage.buffer_bytes);
  live_def = (char *)calloc(1, storage.buffer_bytes);
  live_in = (char *)calloc(1, storage.buffer_bytes);
  live_out = (char *)calloc(1, storage.buffer_bytes);
  if (live_use == NULL || live_def == NULL || live_in == NULL || live_out == NULL) {
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    fprintf(stderr, "register_allocator: out of memory while computing liveness.\n");
    return FAILED;
  }

  if (_compute_register_allocator_live_sets(function_node, blocks, block_count, edges, edge_count, live_use, live_def, live_in, live_out, &storage, &iterations) == FAILED) {
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    return FAILED;
  }

  fprintf(stderr, "register_allocator: liveness_iterations function=%s iterations=%d\n", function_node->children[1]->label, iterations);
  for (block_index = 0; block_index < block_count; block_index++) {
    int set_index;

    if (register_allocator_resolve_liveness_index(
        function_node->children[1]->label, block_count, temp_count, &storage,
        block_index, 0, &set_index) == FAILED) {
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      return FAILED;
    }
    fprintf(stderr, "register_allocator: liveness function=%s block=%d use=", function_node->children[1]->label, block_index);
    _debug_print_register_allocator_temp_set(function_node, live_use + set_index);
    fprintf(stderr, " def=");
    _debug_print_register_allocator_temp_set(function_node, live_def + set_index);
    fprintf(stderr, " live_in=");
    _debug_print_register_allocator_temp_set(function_node, live_in + set_index);
    fprintf(stderr, " live_out=");
    _debug_print_register_allocator_temp_set(function_node, live_out + set_index);
    fprintf(stderr, "\n");
  }

  free(live_use);
  free(live_def);
  free(live_in);
  free(live_out);

  return SUCCEEDED;
}


static int _register_allocator_join_is_stack_resident(void *context,
    int block_index, int predecessor_count, int temp_index) {

  struct register_allocator_temp_state state;
  struct tree_node *function_node;
  struct temp_register *temp_register;
  int stack_resident;

  function_node = (struct tree_node *)context;
  if (temp_index < 0) {
    fprintf(stderr, "register_allocator: join_reconcile function=%s block=%d predecessors=%d policy=stack_only\n",
        function_node->children[1]->label, block_index, predecessor_count);
    return YES;
  }

  temp_register = &(function_node->local_variables->temp_registers[temp_index]);
  state.spill_required = temp_register->spill_required;
  state.physical_register = temp_register->physical_register;
  if (register_allocator_classify_join_temp_state(
      function_node->children[1]->label, block_index, predecessor_count,
      temp_index, &state, Z80_PHY_NONE, &stack_resident) == FAILED)
    return -1;
  if (stack_resident == YES) {
    fprintf(stderr, "register_allocator: join_reconcile function=%s block=%d r%d live_in=yes state=stack action=reload_on_demand\n",
        function_node->children[1]->label, block_index,
        temp_register->register_index);
    return YES;
  }

  fprintf(stderr, "register_allocator: join_reconcile function=%s block=%d r%d live_in=yes state=retained action=spill_predecessors\n",
      function_node->children[1]->label, block_index,
      temp_register->register_index);
  return NO;
}


static int _register_allocator_approve_join_spill_site(void *context,
    int join_block_index, struct register_allocator_join_spill_site *site) {

  struct tree_node *function_node;
  struct tac *anchor;
  int approved;

  function_node = (struct tree_node *)context;
  if (function_node == NULL || site == NULL || site->anchor_instruction < 0 ||
      site->anchor_instruction >= g_tacs_count)
    return -1;
  anchor = &g_tacs[site->anchor_instruction];
  approved = anchor->function_node == function_node ? YES : NO;
  if (approved == YES && site->placement == RA_JOIN_SPILL_BEFORE_ANCHOR &&
      _is_register_allocator_jump_op(anchor->op) == NO)
    approved = NO;
  if (approved == YES && site->placement == RA_JOIN_SPILL_AFTER_ANCHOR &&
      _is_register_allocator_jump_op(anchor->op) == YES)
    approved = NO;
  fprintf(stderr, "register_allocator: target_join_spill_site function=%s block=%d predecessor=%d edge=%d anchor=%d(%s) placement=%s target=z80 approved=%s\n",
      function_node->children[1]->label, join_block_index,
      site->predecessor_block, site->edge_kind, site->anchor_instruction,
      _get_first_allocator_op_name(anchor->op),
      site->placement == RA_JOIN_SPILL_BEFORE_ANCHOR ? "before" : "after",
      approved == YES ? "yes" : "no");
  return approved;
}


static int _register_allocator_insert_join_spill(void *context,
    int join_block_index, struct register_allocator_join_spill_site *site,
    int temp_index, int physical_register, int destination_offset,
    int byte_count) {

  struct tree_node *function_node;
  struct tac *anchor;
  struct tac *spill;
  struct tree_node *statement;
  int file_id;
  int insertion_index;
  int line_number;
  int valid_register;

  function_node = (struct tree_node *)context;
  valid_register = (byte_count == 1 &&
      (physical_register == Z80_PHY_A || physical_register == Z80_PHY_B ||
      physical_register == Z80_PHY_C)) || (byte_count == 2 &&
      (physical_register == Z80_PHY_HL || physical_register == Z80_PHY_BC));
  if (function_node == NULL || site == NULL || temp_index < 0 ||
      site->anchor_instruction < 0 ||
      site->anchor_instruction >= g_tacs_count || valid_register == NO) {
    fprintf(stderr, "register_allocator: join_spill_insert function=%s block=%d predecessor=%d anchor=%d placement=%d temp=r%d phy=%d destination_offset=%d bytes=%d status=invalid_input\n",
        function_node != NULL && function_node->children[1] != NULL ?
        function_node->children[1]->label : "<null>", join_block_index,
        site != NULL ? site->predecessor_block : -1,
        site != NULL ? site->anchor_instruction : -1,
        site != NULL ? site->placement : 0, temp_index, physical_register,
        destination_offset, byte_count);
    return FAILED;
  }

  anchor = &g_tacs[site->anchor_instruction];
  if (anchor->function_node != function_node)
    return FAILED;
  statement = anchor->statement;
  file_id = anchor->file_id;
  line_number = anchor->line_number;
  insertion_index = site->placement == RA_JOIN_SPILL_BEFORE_ANCHOR ?
      site->anchor_instruction : site->anchor_instruction + 1;
  spill = insert_tac(insertion_index);
  if (spill == NULL || tac_set_register_spill(spill, function_node,
      temp_index, Z80_PHY_NONE, physical_register, destination_offset,
      byte_count) == FAILED)
    return FAILED;
  spill->statement = statement;
  spill->file_id = file_id;
  spill->line_number = line_number;
  fprintf(stderr, "register_allocator: join_spill_insert function=%s block=%d predecessor=%d anchor=%d placement=%s insertion=%d temp=r%d phy=%d destination_offset=%d bytes=%d status=complete\n",
      function_node->children[1]->label, join_block_index,
      site->predecessor_block, site->anchor_instruction,
      site->placement == RA_JOIN_SPILL_BEFORE_ANCHOR ? "before" : "after",
      insertion_index, temp_index, physical_register, destination_offset,
      byte_count);
  return SUCCEEDED;
}


struct register_allocator_join_path_observer {
  struct tree_node *function_node;
  int join_block_index;
  int applied_count;
};


static int _observe_register_allocator_join_block_reservation(void *context,
    int temp_index, int slot_index,
    struct register_allocator_join_block_reservation *reservation) {

  struct register_allocator_join_path_observer *observer;

  observer = (struct register_allocator_join_path_observer *)context;
  if (observer == NULL || observer->function_node == NULL ||
      reservation == NULL || temp_index < 0 || slot_index < 0 ||
      reservation->block_index < 0 ||
      reservation->start_instruction < 0 ||
      reservation->end_instruction < reservation->start_instruction ||
      reservation->end_instruction >= g_tacs_count ||
      g_tacs[reservation->start_instruction].function_node !=
      observer->function_node ||
      g_tacs[reservation->end_instruction].function_node !=
      observer->function_node)
    return FAILED;
  fprintf(stderr, "register_allocator: join_path_reservation_observe function=%s join_block=%d temp=r%d reservation_block=%d start=%d end=%d slot=%d mode=observe_only status=complete\n",
      observer->function_node->children[1]->label,
      observer->join_block_index, temp_index, reservation->block_index,
      reservation->start_instruction, reservation->end_instruction,
      slot_index);
  observer->applied_count++;
  return SUCCEEDED;
}


static int _apply_register_allocator_join_assignment_transaction(void *context,
    int temp_index, struct register_allocator_join_assignment *assignments,
    int assignment_count) {

  struct tree_node *function_node;
  int assignment_index;
  int replayed_count;

  function_node = (struct tree_node *)context;
  if (function_node == NULL || assignments == NULL || assignment_count < 2)
    return FAILED;
  replayed_count = 0;
  for (assignment_index = 0; assignment_index < assignment_count;
      assignment_index++) {
    struct register_allocator_join_assignment *assignment;
    struct tac *instruction;
    int current_physical_register;

    assignment = &assignments[assignment_index];
    if (assignment->instruction < 0 || assignment->instruction >= g_tacs_count)
      return FAILED;
    instruction = &g_tacs[assignment->instruction];
    if (instruction->function_node != function_node)
      return FAILED;
    if (assignment->role == RA_JOIN_ASSIGNMENT_PRODUCER) {
      if (instruction->result_type != TAC_ARG_TYPE_TEMP ||
          (int)instruction->result_d != temp_index ||
          (instruction->result_physical_register != Z80_PHY_NONE &&
          instruction->result_physical_register !=
          assignment->physical_register))
        return FAILED;
      current_physical_register = instruction->result_physical_register;
    }
    else if (assignment->role == RA_JOIN_ASSIGNMENT_CONSUMER) {
      if ((assignment->operand == TAC_USE_RESULT &&
          (instruction->result_type != TAC_ARG_TYPE_TEMP ||
          (int)instruction->result_d != temp_index ||
          (instruction->result_physical_register != Z80_PHY_NONE &&
          instruction->result_physical_register !=
          assignment->physical_register))) ||
          (assignment->operand == TAC_USE_ARG1 &&
          (instruction->arg1_type != TAC_ARG_TYPE_TEMP ||
          (int)instruction->arg1_d != temp_index ||
          (instruction->arg1_physical_register != Z80_PHY_NONE &&
          instruction->arg1_physical_register !=
          assignment->physical_register))) ||
          (assignment->operand == TAC_USE_ARG2 &&
          (instruction->arg2_type != TAC_ARG_TYPE_TEMP ||
          (int)instruction->arg2_d != temp_index ||
          (instruction->arg2_physical_register != Z80_PHY_NONE &&
          instruction->arg2_physical_register !=
          assignment->physical_register))) ||
          (assignment->operand != TAC_USE_RESULT &&
          assignment->operand != TAC_USE_ARG1 &&
          assignment->operand != TAC_USE_ARG2))
        return FAILED;
      current_physical_register = assignment->operand == TAC_USE_RESULT ?
          instruction->result_physical_register :
          (assignment->operand == TAC_USE_ARG1 ?
          instruction->arg1_physical_register :
          instruction->arg2_physical_register);
    }
    else
      return FAILED;
    if (current_physical_register == assignment->physical_register)
      replayed_count++;
  }
  for (assignment_index = 0; assignment_index < assignment_count;
      assignment_index++) {
    struct register_allocator_join_assignment *assignment;
    struct tac *instruction;

    assignment = &assignments[assignment_index];
    instruction = &g_tacs[assignment->instruction];
    if (assignment->role == RA_JOIN_ASSIGNMENT_PRODUCER)
      instruction->result_physical_register = assignment->physical_register;
    else if (assignment->operand == TAC_USE_RESULT)
      instruction->result_physical_register = assignment->physical_register;
    else if (assignment->operand == TAC_USE_ARG1)
      instruction->arg1_physical_register = assignment->physical_register;
    else
      instruction->arg2_physical_register = assignment->physical_register;
    fprintf(stderr, "register_allocator: join_assignment_apply_entry function=%s temp=r%d assignment=%d role=%s instruction=%d operand=%d phy=%s status=complete\n",
        function_node->children[1]->label, temp_index, assignment_index,
        assignment->role == RA_JOIN_ASSIGNMENT_PRODUCER ? "producer" :
        "consumer", assignment->instruction, assignment->operand,
        _get_first_allocator_physical_register_name(
        assignment->physical_register));
  }
  fprintf(stderr, "register_allocator: join_assignment_apply_transaction function=%s temp=r%d assignments=%d replayed=%d status=complete\n",
      function_node->children[1]->label, temp_index, assignment_count,
      replayed_count);
  return SUCCEEDED;
}


struct register_allocator_loop_transaction_context {
  struct tree_node *function_node;
  int block_count;
  struct register_allocator_join_path_state_entry *path_state_entries;
  int path_state_capacity;
  int *path_state_count;
};


static int _apply_register_allocator_loop_retention_transaction(void *context,
    int temp_index, int slot_index,
    struct register_allocator_join_assignment *assignments,
    int assignment_count,
    struct register_allocator_join_block_reservation *reservations,
    int reservation_count) {

  struct register_allocator_loop_transaction_context *transaction;
  int result;

  transaction =
      (struct register_allocator_loop_transaction_context *)context;
  if (transaction == NULL || transaction->function_node == NULL ||
      transaction->block_count <= 0 ||
      transaction->path_state_capacity < 0 ||
      transaction->path_state_count == NULL ||
      *transaction->path_state_count < 0 ||
      *transaction->path_state_count > transaction->path_state_capacity ||
      (transaction->path_state_capacity > 0 &&
      transaction->path_state_entries == NULL) || assignments == NULL ||
      assignment_count < 3 || reservations == NULL ||
      reservation_count <= 0 || temp_index < 0 || slot_index < 0)
    return FAILED;
  result = register_allocator_commit_loop_retention_transaction(
      transaction->function_node->children[1]->label,
      transaction->block_count, g_tacs_count, temp_index, slot_index,
      assignments, assignment_count, reservations, reservation_count,
      transaction->path_state_entries, transaction->path_state_capacity,
      transaction->path_state_count, transaction->function_node,
      _apply_register_allocator_join_assignment_transaction);
    if (result == FAILED) {
    fprintf(stderr, "register_allocator: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d transaction=rejected atomic=yes status=complete\n",
        transaction->function_node->children[1]->label, temp_index,
        slot_index, assignment_count, reservation_count,
        *transaction->path_state_count);
    return FAILED;
  }
  fprintf(stderr, "register_allocator: loop_retention_transaction function=%s temp=r%d slot=%d assignments=%d reservations=%d state_count=%d transaction=applied atomic=yes ownership=function_scan status=complete\n",
      transaction->function_node->children[1]->label, temp_index, slot_index,
      assignment_count, reservation_count, *transaction->path_state_count);
  return SUCCEEDED;
}


static int _find_register_allocator_local_identity(
    struct tree_node *function_node, struct tree_node *node) {

  int identity;

  if (function_node == NULL || function_node->local_variables == NULL ||
      node == NULL)
    return -1;
  for (identity = 0;
      identity < function_node->local_variables->local_variables_count;
      identity++) {
    if (function_node->local_variables->local_variables[identity].node == node)
      return identity;
  }
  return -1;
}


static int _append_register_allocator_loop_stack_observation(
    struct tree_node *function_node, int instruction_index, char *operand,
    int operand_type, struct tree_node *node, char *label,
    struct register_allocator_loop_stack_value_observation *observations,
    int observation_capacity, int *observation_count) {

  int identity;

  if (function_node == NULL || instruction_index < 0 || operand == NULL ||
      observations == NULL || observation_capacity <= 0 ||
      observation_count == NULL || *observation_count < 0 ||
      *observation_count > observation_capacity)
    return FAILED;
  if (operand_type != TAC_ARG_TYPE_LABEL)
    return SUCCEEDED;
  identity = _find_register_allocator_local_identity(function_node, node);
  if (identity < 0)
    return SUCCEEDED;
  if (*observation_count >= observation_capacity)
    return FAILED;
  observations[*observation_count].instruction_index = instruction_index;
  observations[*observation_count].identity = identity;
  fprintf(stderr, "register_allocator: loop_stack_value_observation function=%s instruction=%d operand=%s identity=%d variable=%s status=complete\n",
      function_node->children[1]->label, instruction_index, operand, identity,
      label != NULL ? label : "<null>");
  (*observation_count)++;
  return SUCCEEDED;
}


static int _collect_register_allocator_loop_stack_observations(
    struct tree_node *function_node,
    struct register_allocator_loop_stack_value_observation *observations,
    int observation_capacity, int *observation_count) {

  int instruction_index;

  if (function_node == NULL || observations == NULL ||
      observation_capacity <= 0 || observation_count == NULL)
    return FAILED;
  *observation_count = 0;
  for (instruction_index = 0; instruction_index < g_tacs_count;
      instruction_index++) {
    struct tac *instruction;

    instruction = &g_tacs[instruction_index];
    if (instruction->function_node != function_node ||
        instruction->op == TAC_OP_CREATE_VARIABLE)
      continue;
    if (_append_register_allocator_loop_stack_observation(function_node,
        instruction_index, "result", instruction->result_type,
        instruction->result_node, instruction->result_s, observations,
        observation_capacity, observation_count) == FAILED ||
        _append_register_allocator_loop_stack_observation(function_node,
        instruction_index, "arg1", instruction->arg1_type,
        instruction->arg1_node, instruction->arg1_s, observations,
        observation_capacity, observation_count) == FAILED ||
        _append_register_allocator_loop_stack_observation(function_node,
        instruction_index, "arg2", instruction->arg2_type,
        instruction->arg2_node, instruction->arg2_s, observations,
        observation_capacity, observation_count) == FAILED)
      return FAILED;
  }
  return SUCCEEDED;
}


static int _append_register_allocator_loop_stack_rewrite_observation(
    int instruction_index, int operand, int identity, int role,
    struct register_allocator_loop_stack_rewrite_observation *observations,
    int observation_capacity, int *observation_count) {

  if (instruction_index < 0 || operand < TAC_USE_RESULT ||
      operand > TAC_USE_ARG2 || identity < 0 || role <= 0 ||
      observations == NULL || observation_capacity <= 0 ||
      observation_count == NULL || *observation_count < 0 ||
      *observation_count >= observation_capacity)
    return FAILED;
  observations[*observation_count].instruction_index = instruction_index;
  observations[*observation_count].operand = operand;
  observations[*observation_count].identity = identity;
  observations[*observation_count].role = role;
  (*observation_count)++;
  return SUCCEEDED;
}


static void _map_register_allocator_loop_stack_operand(
    struct tree_node *function_node, int type, double value,
    struct tree_node *node,
    struct register_allocator_loop_stack_operand_storage *storage) {

  int identity;

  storage->kind = 0;
  storage->identity = -1;
  storage->temp_index = -1;
  if (type == TAC_ARG_TYPE_TEMP) {
    storage->kind = RA_LOOP_STACK_STORAGE_TEMP;
    storage->temp_index = (int)value;
    return;
  }
  if (type != TAC_ARG_TYPE_LABEL)
    return;
  identity = _find_register_allocator_local_identity(function_node, node);
  if (identity < 0)
    return;
  storage->kind = RA_LOOP_STACK_STORAGE_STACK;
  storage->identity = identity;
}


static int _apply_register_allocator_loop_stack_storage_transaction(
    struct tree_node *function_node,
    struct register_allocator_loop_stack_promotion_plan *promotion,
    struct register_allocator_loop_stack_rewrite *rewrites,
    int rewrite_count, int publish, int *rewritten_operand_count,
    int *added_temp_count) {

  struct register_allocator_loop_stack_operand_storage *operands;
  struct register_allocator_loop_stack_temp_storage *temps;
  struct temp_register *staged_temp_registers;
  int instruction_index;
  int operand_count;
  int original_temp_count;
  int rewrite_index;
  int staged_temp_count;
  int temp_capacity;
  int temp_index;

  if (rewritten_operand_count != NULL)
    *rewritten_operand_count = 0;
  if (added_temp_count != NULL)
    *added_temp_count = 0;
  if (function_node == NULL || function_node->local_variables == NULL ||
      promotion == NULL || rewrites == NULL || rewrite_count <= 0 ||
      (publish != NO && publish != YES) || rewritten_operand_count == NULL ||
      added_temp_count == NULL ||
      g_tacs_count <= 0 || g_tacs_count > INT_MAX / 3 ||
      function_node->local_variables->temp_registers_count < 0 ||
      function_node->local_variables->temp_registers_count == INT_MAX)
    return FAILED;
  operand_count = g_tacs_count * 3;
  original_temp_count = function_node->local_variables->temp_registers_count;
  temp_capacity = original_temp_count + 1;
  operands =
      (struct register_allocator_loop_stack_operand_storage *)calloc(
      (size_t)operand_count,
      sizeof(struct register_allocator_loop_stack_operand_storage));
  temps = (struct register_allocator_loop_stack_temp_storage *)calloc(
      (size_t)temp_capacity,
      sizeof(struct register_allocator_loop_stack_temp_storage));
  staged_temp_registers = (struct temp_register *)calloc(
      (size_t)temp_capacity, sizeof(struct temp_register));
  if (operands == NULL || temps == NULL || staged_temp_registers == NULL) {
    free(operands);
    free(temps);
    free(staged_temp_registers);
    fprintf(stderr, "register_allocator: loop_stack_storage_adapter function=%s instructions=%d rewrites=%d publication=none mode=observe_only status=out_of_memory\n",
        function_node->children[1]->label, g_tacs_count, rewrite_count);
    return FAILED;
  }
  for (instruction_index = 0; instruction_index < g_tacs_count;
      instruction_index++) {
    struct tac *instruction;
    int operand_index;

    instruction = &g_tacs[instruction_index];
    operand_index = instruction_index * 3;
    _map_register_allocator_loop_stack_operand(function_node,
        instruction->result_type, instruction->result_d,
        instruction->result_node, &operands[operand_index + TAC_USE_RESULT]);
    _map_register_allocator_loop_stack_operand(function_node,
        instruction->arg1_type, instruction->arg1_d,
        instruction->arg1_node, &operands[operand_index + TAC_USE_ARG1]);
    _map_register_allocator_loop_stack_operand(function_node,
        instruction->arg2_type, instruction->arg2_d,
        instruction->arg2_node, &operands[operand_index + TAC_USE_ARG2]);
  }
  staged_temp_count = function_node->local_variables->temp_registers_count;
  if (staged_temp_count > 0)
    memcpy(staged_temp_registers,
        function_node->local_variables->temp_registers,
        (size_t)staged_temp_count * sizeof(struct temp_register));
  for (temp_index = 0; temp_index < staged_temp_count; temp_index++) {
    temps[temp_index].temp_index =
        function_node->local_variables->temp_registers[temp_index].register_index;
    temps[temp_index].size =
        function_node->local_variables->temp_registers[temp_index].size;
  }
  if (register_allocator_commit_loop_stack_promotion_storage(
      function_node->children[1]->label, g_tacs_count, promotion, rewrites,
      rewrite_count, operands, operand_count, temps, temp_capacity,
      &staged_temp_count) == FAILED) {
    free(operands);
    free(temps);
    free(staged_temp_registers);
    return FAILED;
  }
  if (staged_temp_count !=
      function_node->local_variables->temp_registers_count + 1 ||
      temps[staged_temp_count - 1].temp_index != promotion->temp_index ||
      temps[staged_temp_count - 1].size != promotion->size) {
    free(operands);
    free(temps);
    free(staged_temp_registers);
    return FAILED;
  }
  temp_index = staged_temp_count - 1;
  staged_temp_registers[temp_index].register_index = promotion->temp_index;
  staged_temp_registers[temp_index].offset_to_fp =
      REGISTER_ALLOCATOR_POISON_OFFSET;
  staged_temp_registers[temp_index].size = promotion->size;
  staged_temp_registers[temp_index].spill_required = YES;
  staged_temp_registers[temp_index].physical_register = Z80_PHY_NONE;
  staged_temp_registers[temp_index].original_register_index =
      promotion->temp_index;
  staged_temp_registers[temp_index].live_start = g_tacs_count;
  staged_temp_registers[temp_index].live_end = -1;
  staged_temp_registers[temp_index].read_count = 0;
  staged_temp_registers[temp_index].write_count = 0;
  staged_temp_registers[temp_index].spill_reason = Z80_SPILL_REASON_NONE;
  staged_temp_registers[temp_index].spill_boundary_tac = -1;
  for (rewrite_index = 0; rewrite_index < rewrite_count; rewrite_index++) {
    int operand_index;

    operand_index = rewrites[rewrite_index].instruction_index * 3 +
        rewrites[rewrite_index].operand;
    if (operands[operand_index].kind != RA_LOOP_STACK_STORAGE_TEMP ||
        operands[operand_index].identity != -1 ||
        operands[operand_index].temp_index != promotion->temp_index) {
      free(operands);
      free(temps);
      free(staged_temp_registers);
      return FAILED;
    }
    if (rewrites[rewrite_index].instruction_index <
        staged_temp_registers[temp_index].live_start)
      staged_temp_registers[temp_index].live_start =
          rewrites[rewrite_index].instruction_index;
    if (rewrites[rewrite_index].instruction_index >
        staged_temp_registers[temp_index].live_end)
      staged_temp_registers[temp_index].live_end =
          rewrites[rewrite_index].instruction_index;
    if ((rewrites[rewrite_index].role &
        (RA_LOOP_STACK_ROLE_ENTRY_WRITE |
        RA_LOOP_STACK_ROLE_LATCH_WRITE)) != 0)
      staged_temp_registers[temp_index].write_count++;
    if ((rewrites[rewrite_index].role &
        (RA_LOOP_STACK_ROLE_HEADER_READ |
        RA_LOOP_STACK_ROLE_LATCH_READ |
        RA_LOOP_STACK_ROLE_EXIT_READ)) != 0)
      staged_temp_registers[temp_index].read_count++;
    fprintf(stderr, "register_allocator: loop_stack_storage_adapter_rewrite function=%s rewrite=%d instruction=%d operand=%d source_identity=%d staged_temp=r%d publication=%s mode=%s status=complete\n",
        function_node->children[1]->label, rewrite_index,
        rewrites[rewrite_index].instruction_index,
        rewrites[rewrite_index].operand, promotion->identity,
      promotion->temp_index, publish == YES ? "real" : "snapshot",
      publish == YES ? "mutation" : "observe_only");
  }
  if (staged_temp_registers[temp_index].live_start >
      staged_temp_registers[temp_index].live_end ||
      staged_temp_registers[temp_index].read_count <= 0 ||
      staged_temp_registers[temp_index].write_count <= 0) {
    free(operands);
    free(temps);
    free(staged_temp_registers);
    return FAILED;
  }
  if (publish == YES) {
    for (rewrite_index = 0; rewrite_index < rewrite_count; rewrite_index++) {
      struct tac *instruction;

      instruction = &g_tacs[rewrites[rewrite_index].instruction_index];
      if (rewrites[rewrite_index].operand == TAC_USE_RESULT) {
        free(instruction->result_s);
        instruction->result_s = NULL;
        instruction->result_type = TAC_ARG_TYPE_TEMP;
        instruction->result_d = promotion->temp_index;
        instruction->result_original_register_index = promotion->temp_index;
        instruction->result_node = NULL;
        instruction->result_physical_register = Z80_PHY_NONE;
      }
      else if (rewrites[rewrite_index].operand == TAC_USE_ARG1) {
        free(instruction->arg1_s);
        instruction->arg1_s = NULL;
        instruction->arg1_type = TAC_ARG_TYPE_TEMP;
        instruction->arg1_d = promotion->temp_index;
        instruction->arg1_original_register_index = promotion->temp_index;
        instruction->arg1_node = NULL;
        instruction->arg1_physical_register = Z80_PHY_NONE;
      }
      else {
        free(instruction->arg2_s);
        instruction->arg2_s = NULL;
        instruction->arg2_type = TAC_ARG_TYPE_TEMP;
        instruction->arg2_d = promotion->temp_index;
        instruction->arg2_original_register_index = promotion->temp_index;
        instruction->arg2_node = NULL;
        instruction->arg2_physical_register = Z80_PHY_NONE;
      }
    }
    free(function_node->local_variables->temp_registers);
    function_node->local_variables->temp_registers = staged_temp_registers;
    function_node->local_variables->temp_registers_count = staged_temp_count;
    staged_temp_registers = NULL;
    *rewritten_operand_count = rewrite_count;
    *added_temp_count = 1;
  }
    fprintf(stderr, "register_allocator: loop_stack_storage_adapter function=%s instructions=%d rewrites=%d real_temps=%d staged_temps=%d publication=%s mode=%s status=complete\n",
      function_node->children[1]->label, g_tacs_count, rewrite_count,
      original_temp_count, staged_temp_count,
      publish == YES ? "real" : "snapshot",
      publish == YES ? "mutation" : "observe_only");
  free(operands);
  free(temps);
  free(staged_temp_registers);
  return SUCCEEDED;
}


static int _collect_register_allocator_loop_stack_roles(
    struct tree_node *function_node,
    struct register_allocator_basic_block *blocks, int block_count,
    int join_block_index, struct register_allocator_loop_join *loop_join,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int *identities, int identity_count,
    struct register_allocator_loop_stack_value_roles *values,
    struct register_allocator_loop_stack_rewrite_observation *observations,
    int observation_capacity, int *observation_count) {

  int changed;
  int edge_index;
  int identity_index;
  int instruction_index;
  char *exit_blocks;
  char *loop_blocks;

  if (function_node == NULL || blocks == NULL || block_count <= 0 ||
      join_block_index < 0 || join_block_index >= block_count ||
      loop_join == NULL || edges == NULL || edge_count <= 0 ||
      identity_count < 0 || (identity_count > 0 &&
      (identities == NULL || values == NULL)) || observations == NULL ||
      observation_capacity <= 0 || observation_count == NULL)
    return FAILED;
  *observation_count = 0;
  if (loop_join->status != RA_LOOP_JOIN_READY ||
      loop_join->entry_predecessor < 0 ||
      loop_join->entry_predecessor >= block_count ||
      loop_join->latch_predecessor < 0 ||
      loop_join->latch_predecessor >= block_count)
    return FAILED;
  loop_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  exit_blocks = (char *)calloc((size_t)block_count, sizeof(char));
  if (loop_blocks == NULL || exit_blocks == NULL) {
    free(loop_blocks);
    free(exit_blocks);
    return FAILED;
  }
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (edges[edge_index].from_block < 0 ||
        edges[edge_index].from_block >= block_count ||
        edges[edge_index].to_block < 0 ||
        edges[edge_index].to_block >= block_count) {
      free(loop_blocks);
      free(exit_blocks);
      return FAILED;
    }
    if (edges[edge_index].to_block == join_block_index &&
        edges[edge_index].from_block >= join_block_index)
      loop_blocks[edges[edge_index].from_block] = YES;
  }
  do {
    changed = NO;
    for (edge_index = 0; edge_index < edge_count; edge_index++) {
      int from_block;
      int to_block;

      from_block = edges[edge_index].from_block;
      to_block = edges[edge_index].to_block;
      if (from_block < 0 || from_block >= block_count || to_block < 0 ||
          to_block >= block_count) {
        free(loop_blocks);
        free(exit_blocks);
        return FAILED;
      }
      if (to_block != join_block_index && loop_blocks[to_block] == YES &&
          loop_blocks[from_block] == NO) {
        loop_blocks[from_block] = YES;
        changed = YES;
      }
    }
  } while (changed == YES);
  loop_blocks[join_block_index] = YES;
  for (edge_index = 0; edge_index < edge_count; edge_index++) {
    if (loop_blocks[edges[edge_index].from_block] == YES &&
        loop_blocks[edges[edge_index].to_block] == NO)
      exit_blocks[edges[edge_index].to_block] = YES;
  }
  for (identity_index = 0; identity_index < identity_count; identity_index++) {
    values[identity_index].identity = identities[identity_index];
    values[identity_index].role_mask = 0;
  }
  for (instruction_index = 0; instruction_index < g_tacs_count;
      instruction_index++) {
    struct tac *instruction;
    int arg1_identity;
    int arg2_identity;
    int block_role;
    int result_identity;

    instruction = &g_tacs[instruction_index];
    if (instruction->function_node != function_node ||
        instruction->op == TAC_OP_DEAD ||
        instruction->op == TAC_OP_CREATE_VARIABLE)
      continue;
    block_role = 0;
    if (instruction_index >=
        blocks[loop_join->entry_predecessor].start_tac &&
        instruction_index <= blocks[loop_join->entry_predecessor].end_tac)
      block_role = RA_LOOP_STACK_ROLE_ENTRY_WRITE;
    else if (instruction_index >= blocks[join_block_index].start_tac &&
        instruction_index <= blocks[join_block_index].end_tac)
      block_role = RA_LOOP_STACK_ROLE_HEADER_READ;
    else {
      int block_index;

      for (block_index = 0; block_index < block_count; block_index++) {
        int latch_edge_index;

        if (instruction_index < blocks[block_index].start_tac ||
            instruction_index > blocks[block_index].end_tac)
          continue;
        for (latch_edge_index = 0; latch_edge_index < edge_count;
            latch_edge_index++) {
          if (edges[latch_edge_index].from_block == block_index &&
              edges[latch_edge_index].to_block == join_block_index &&
              block_index >= join_block_index) {
            block_role = RA_LOOP_STACK_ROLE_LATCH_READ |
                RA_LOOP_STACK_ROLE_LATCH_WRITE;
            break;
          }
        }
        if (block_role != 0)
          break;
        if (exit_blocks[block_index] == YES && instruction_index >=
            blocks[block_index].start_tac && instruction_index <=
            blocks[block_index].end_tac) {
          block_role = RA_LOOP_STACK_ROLE_EXIT_READ;
          break;
        }
      }
    }
    if (block_role == 0)
      continue;
    result_identity = _find_register_allocator_local_identity(function_node,
        instruction->result_node);
    arg1_identity = _find_register_allocator_local_identity(function_node,
        instruction->arg1_node);
    arg2_identity = _find_register_allocator_local_identity(function_node,
        instruction->arg2_node);
    for (identity_index = 0; identity_index < identity_count;
        identity_index++) {
      if ((block_role & RA_LOOP_STACK_ROLE_ENTRY_WRITE) != 0 &&
          instruction->op != TAC_OP_ARRAY_WRITE &&
          instruction->result_type == TAC_ARG_TYPE_LABEL &&
          result_identity == identities[identity_index]) {
        values[identity_index].role_mask |=
            RA_LOOP_STACK_ROLE_ENTRY_WRITE;
        if (_append_register_allocator_loop_stack_rewrite_observation(
            instruction_index, TAC_USE_RESULT, identities[identity_index],
            RA_LOOP_STACK_ROLE_ENTRY_WRITE, observations,
            observation_capacity, observation_count) == FAILED) {
          free(loop_blocks);
          free(exit_blocks);
          return FAILED;
        }
      }
      if ((block_role & RA_LOOP_STACK_ROLE_HEADER_READ) != 0) {
        if (instruction->arg1_type == TAC_ARG_TYPE_LABEL &&
            arg1_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_HEADER_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_ARG1, identities[identity_index],
              RA_LOOP_STACK_ROLE_HEADER_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->arg2_type == TAC_ARG_TYPE_LABEL &&
            arg2_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_HEADER_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_ARG2, identities[identity_index],
              RA_LOOP_STACK_ROLE_HEADER_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->op == TAC_OP_ARRAY_WRITE &&
          instruction->result_type == TAC_ARG_TYPE_LABEL &&
            result_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_HEADER_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_RESULT, identities[identity_index],
              RA_LOOP_STACK_ROLE_HEADER_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
      }
      if ((block_role & RA_LOOP_STACK_ROLE_LATCH_READ) != 0) {
        if (instruction->arg1_type == TAC_ARG_TYPE_LABEL &&
            arg1_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_LATCH_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_ARG1, identities[identity_index],
              RA_LOOP_STACK_ROLE_LATCH_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->arg2_type == TAC_ARG_TYPE_LABEL &&
            arg2_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_LATCH_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_ARG2, identities[identity_index],
              RA_LOOP_STACK_ROLE_LATCH_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->op == TAC_OP_ARRAY_WRITE &&
            instruction->result_type == TAC_ARG_TYPE_LABEL &&
            result_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_LATCH_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_RESULT, identities[identity_index],
              RA_LOOP_STACK_ROLE_LATCH_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->op != TAC_OP_ARRAY_WRITE &&
            instruction->result_type == TAC_ARG_TYPE_LABEL &&
            result_identity == identities[identity_index]) {
          values[identity_index].role_mask |=
              RA_LOOP_STACK_ROLE_LATCH_WRITE;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_RESULT, identities[identity_index],
              RA_LOOP_STACK_ROLE_LATCH_WRITE, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
      }
      if ((block_role & RA_LOOP_STACK_ROLE_EXIT_READ) != 0) {
        if (instruction->arg1_type == TAC_ARG_TYPE_LABEL &&
            arg1_identity == identities[identity_index]) {
          values[identity_index].role_mask |= RA_LOOP_STACK_ROLE_EXIT_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_ARG1, identities[identity_index],
              RA_LOOP_STACK_ROLE_EXIT_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->arg2_type == TAC_ARG_TYPE_LABEL &&
            arg2_identity == identities[identity_index]) {
          values[identity_index].role_mask |= RA_LOOP_STACK_ROLE_EXIT_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_ARG2, identities[identity_index],
              RA_LOOP_STACK_ROLE_EXIT_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
        if (instruction->op == TAC_OP_ARRAY_WRITE &&
          instruction->result_type == TAC_ARG_TYPE_LABEL &&
            result_identity == identities[identity_index]) {
          values[identity_index].role_mask |= RA_LOOP_STACK_ROLE_EXIT_READ;
          if (_append_register_allocator_loop_stack_rewrite_observation(
              instruction_index, TAC_USE_RESULT, identities[identity_index],
              RA_LOOP_STACK_ROLE_EXIT_READ, observations,
              observation_capacity, observation_count) == FAILED) {
            free(loop_blocks);
            free(exit_blocks);
            return FAILED;
          }
        }
      }
    }
  }
  free(loop_blocks);
  free(exit_blocks);
  return SUCCEEDED;
}


static int _debug_register_allocator_join_retention_preflight(
    struct tree_node *function_node,
    struct register_allocator_basic_block *blocks, int block_count,
    struct register_allocator_cfg_edge *edges, int edge_count,
    int join_block_index, struct register_allocator_join_spill_site *sites,
  int site_count, int temp_index,
  struct register_allocator_join_path_state_entry *path_state_entries,
  int path_state_capacity, int *path_state_count) {

  struct register_allocator_join_retention_path *paths;
  struct register_allocator_reaching_definition_block *block_facts;
  struct register_allocator_join_assignment *assignments;
  struct register_allocator_join_block_reservation *block_reservations;
  struct register_allocator_join_occupied_interval *occupied_intervals;
  struct register_allocator_join_path_reservation path_reservation;
  struct register_allocator_join_path_application path_application;
  struct register_allocator_join_path_commit path_commit;
  struct register_allocator_join_assignment_application assignment_application;
  struct register_allocator_join_path_observer path_observer;
  struct register_allocator_join_reservation reservation;
  struct register_allocator_join_schedule schedule;
  struct temp_register *temp_register;
  struct tac *consumer;
  int *producer_physical_registers;
  int assignment_capacity;
  int consumer_physical_register;
  int consumer_arg;
  int consumer_index;
  int consumer_supported;
  int occupied_interval_count;
  int path_index;
  int physical_register;
  int selected_physical_register;
  int selected_slot_index;

  if (path_state_capacity < 0 || path_state_count == NULL ||
      *path_state_count < 0 || *path_state_count > path_state_capacity ||
      (path_state_capacity > 0 && path_state_entries == NULL))
    return FAILED;
  temp_register = &function_node->local_variables->temp_registers[temp_index];
  physical_register = temp_register->size == 8 ? Z80_PHY_C :
      (temp_register->size == 16 ? Z80_PHY_HL : Z80_PHY_NONE);
  consumer = NULL;
  consumer_arg = 0;
  for (consumer_index = blocks[join_block_index].start_tac;
      consumer_index <= blocks[join_block_index].end_tac; consumer_index++) {
    if (_register_allocator_instruction_reads_temp(function_node,
        consumer_index, temp_index) == YES) {
      consumer = &g_tacs[consumer_index];
      consumer_arg = _register_allocator_instruction_get_consumer_operand(
          function_node, consumer_index, temp_index);
      break;
    }
  }
  consumer_supported = consumer != NULL && consumer_arg > 0 &&
      physical_register != Z80_PHY_NONE ? YES : NO;
  paths = (struct register_allocator_join_retention_path *)calloc(
      (size_t)site_count, sizeof(struct register_allocator_join_retention_path));
  block_facts = (struct register_allocator_reaching_definition_block *)calloc(
      (size_t)block_count,
      sizeof(struct register_allocator_reaching_definition_block));
  if (paths == NULL || block_facts == NULL) {
    free(block_facts);
    free(paths);
    fprintf(stderr, "register_allocator: join_retention_preflight function=%s block=%d temp=r%d paths=%d status=out_of_memory\n",
        function_node->children[1]->label, join_block_index, temp_index,
        site_count);
    return FAILED;
  }

  for (path_index = 0; path_index < site_count; path_index++) {
    struct register_allocator_basic_block *predecessor;
    struct register_allocator_reaching_definition reaching_definition;
    int block_index;
    int definition_index;
    int path_end;
    int scan_index;

    predecessor = &blocks[sites[path_index].predecessor_block];
    definition_index = -1;
    for (scan_index = predecessor->end_tac;
        scan_index >= predecessor->start_tac; scan_index--) {
      if (_register_allocator_instruction_writes_temp(function_node,
          scan_index, temp_index) == YES) {
        definition_index = scan_index;
        break;
      }
    }
    if (definition_index < 0) {
      for (block_index = 0; block_index < block_count; block_index++) {
        int block_path_start;
        int block_path_end;

        block_facts[block_index].definition_instruction = -1;
        block_facts[block_index].definition_found = NO;
        block_facts[block_index].path_transparent = YES;
        block_path_start = blocks[block_index].start_tac;
        block_path_end = blocks[block_index].end_tac;
        if (block_index == sites[path_index].predecessor_block)
          block_path_end = sites[path_index].placement ==
              RA_JOIN_SPILL_BEFORE_ANCHOR ?
              sites[path_index].anchor_instruction - 1 :
              sites[path_index].anchor_instruction;
        for (scan_index = block_path_end; scan_index >= block_path_start;
            scan_index--) {
          if (_register_allocator_instruction_writes_temp(function_node,
              scan_index, temp_index) == YES) {
            block_facts[block_index].definition_instruction = scan_index;
            block_facts[block_index].definition_found = YES;
            block_path_start = scan_index + 1;
            break;
          }
        }
        for (scan_index = block_path_start; scan_index <= block_path_end;
            scan_index++) {
          if (_is_basic_block_transparent_tac_for_physical_register(
              &g_tacs[scan_index], physical_register) == NO) {
            block_facts[block_index].path_transparent = NO;
            break;
          }
        }
      }
      if (register_allocator_resolve_reaching_definition(
          function_node->children[1]->label, block_count, g_tacs_count,
          edges, edge_count, block_facts,
          sites[path_index].predecessor_block, temp_index,
          &reaching_definition) == FAILED) {
        free(block_facts);
        free(paths);
        return FAILED;
      }
      if (reaching_definition.status == RA_REACHING_DEFINITION_UNIQUE)
        definition_index = reaching_definition.definition_instruction;
    }
    paths[path_index].predecessor_block = sites[path_index].predecessor_block;
    paths[path_index].definition_instruction = definition_index;
    paths[path_index].anchor_instruction = sites[path_index].anchor_instruction;
    paths[path_index].physical_register = physical_register;
    paths[path_index].definition_found = definition_index >= 0 ? YES : NO;
    paths[path_index].definition_supported = NO;
    paths[path_index].path_transparent = NO;
    if (definition_index < 0)
      continue;
    if (consumer_supported == YES &&
        _is_register_allocator_candidate_allowed_for_physical_register(
        &g_tacs[definition_index], consumer, temp_register,
        physical_register, consumer_arg) == YES)
      paths[path_index].definition_supported = YES;
    paths[path_index].path_transparent = YES;
    if (definition_index < predecessor->start_tac) {
      paths[path_index].path_transparent =
          reaching_definition.path_transparent;
    }
    else {
      path_end = sites[path_index].placement ==
          RA_JOIN_SPILL_BEFORE_ANCHOR ?
          sites[path_index].anchor_instruction - 1 :
          sites[path_index].anchor_instruction;
      for (scan_index = definition_index + 1; scan_index <= path_end;
          scan_index++) {
        if (_is_basic_block_transparent_tac_for_physical_register(
            &g_tacs[scan_index], physical_register) == NO) {
          paths[path_index].path_transparent = NO;
          break;
        }
      }
    }
  }

  if (register_allocator_plan_join_retention(
      function_node->children[1]->label, block_count, g_tacs_count,
      join_block_index, temp_index, Z80_PHY_NONE, paths, site_count,
      consumer_supported, &selected_physical_register) == FAILED) {
    free(block_facts);
    free(paths);
    return FAILED;
  }
  if (selected_physical_register == Z80_PHY_NONE)
    fprintf(stderr, "register_allocator: join_retention_fallback function=%s block=%d temp=r%d consumer=%d operand=%d reason=%s action=skip_join_rewrite status=complete\n",
        function_node->children[1]->label, join_block_index, temp_index,
        consumer != NULL ? consumer_index : -1, consumer_arg,
        consumer_supported == NO ? "consumer_unsupported" :
        "path_ineligible");
  if (site_count >= INT_MAX ||
      (size_t)(site_count + 1) > ((size_t)-1) /
      sizeof(struct register_allocator_join_assignment) ||
      (size_t)site_count > ((size_t)-1) / sizeof(int)) {
    free(block_facts);
    free(paths);
    return FAILED;
  }
  assignment_capacity = site_count + 1;
  assignments = (struct register_allocator_join_assignment *)calloc(
      (size_t)assignment_capacity,
      sizeof(struct register_allocator_join_assignment));
  producer_physical_registers = (int *)calloc((size_t)site_count,
      sizeof(int));
  if (assignments == NULL || producer_physical_registers == NULL) {
    free(producer_physical_registers);
    free(assignments);
    free(block_facts);
    free(paths);
    fprintf(stderr, "register_allocator: join_schedule_preflight function=%s block=%d temp=r%d paths=%d status=out_of_memory\n",
        function_node->children[1]->label, join_block_index, temp_index,
        site_count);
    return FAILED;
  }
  for (path_index = 0; path_index < site_count; path_index++)
    producer_physical_registers[path_index] =
        paths[path_index].definition_found == YES ?
        g_tacs[paths[path_index].definition_instruction].result_physical_register :
        Z80_PHY_NONE;
  consumer_physical_register = Z80_PHY_NONE;
  if (consumer != NULL && consumer_arg == TAC_USE_RESULT)
    consumer_physical_register = consumer->result_physical_register;
  else if (consumer != NULL && consumer_arg == TAC_USE_ARG1)
    consumer_physical_register = consumer->arg1_physical_register;
  else if (consumer != NULL && consumer_arg == TAC_USE_ARG2)
    consumer_physical_register = consumer->arg2_physical_register;
  if (register_allocator_plan_join_schedule(
      function_node->children[1]->label, g_tacs_count, temp_index,
      Z80_PHY_NONE, paths, site_count, producer_physical_registers,
      consumer_index, consumer_arg, consumer_physical_register,
      selected_physical_register, assignments, assignment_capacity,
      &schedule) == FAILED) {
    free(producer_physical_registers);
    free(assignments);
    free(block_facts);
    free(paths);
    return FAILED;
  }
  occupied_intervals = NULL;
  occupied_interval_count = 0;
  selected_slot_index = -1;
  if (schedule.status == RA_JOIN_SCHEDULE_READY) {
    struct register_allocator_target_policy *target_policy;
    int other_temp_index;
    int temp_count;

    target_policy = _get_register_allocator_target_policy();
    temp_count = function_node->local_variables->temp_registers_count;
    if (target_policy == NULL ||
        target_policy->get_active_slot_index == NULL || temp_count < 0 ||
        (temp_count > 0 && (size_t)temp_count > ((size_t)-1) /
        sizeof(struct register_allocator_join_occupied_interval))) {
      free(producer_physical_registers);
      free(assignments);
      free(block_facts);
      free(paths);
      return FAILED;
    }
    selected_slot_index =
        target_policy->get_active_slot_index(selected_physical_register);
    if (temp_count > 0)
      occupied_intervals =
          (struct register_allocator_join_occupied_interval *)calloc(
          (size_t)temp_count,
          sizeof(struct register_allocator_join_occupied_interval));
    if (temp_count > 0 && occupied_intervals == NULL) {
      free(producer_physical_registers);
      free(assignments);
      free(block_facts);
      free(paths);
      fprintf(stderr, "register_allocator: join_reservation_preflight function=%s block=%d temp=r%d intervals=%d status=out_of_memory\n",
          function_node->children[1]->label, join_block_index, temp_index,
          temp_count);
      return FAILED;
    }
    for (other_temp_index = 0; other_temp_index < temp_count;
        other_temp_index++) {
      struct temp_register *other_temp;

      other_temp = &function_node->local_variables->temp_registers[
          other_temp_index];
      if (other_temp->physical_register == Z80_PHY_NONE ||
          other_temp->live_start < 0 ||
          other_temp->live_end < other_temp->live_start)
        continue;
      occupied_intervals[occupied_interval_count].temp_index =
          other_temp_index;
      occupied_intervals[occupied_interval_count].physical_register =
          other_temp->physical_register;
      occupied_intervals[occupied_interval_count].live_start =
          other_temp->live_start;
      occupied_intervals[occupied_interval_count].live_end =
          other_temp->live_end;
      occupied_intervals[occupied_interval_count].overlaps_selected_register =
          _register_allocator_physical_registers_overlap(
          function_node->children[1]->label, selected_physical_register,
          other_temp->physical_register);
      occupied_interval_count++;
    }
  }
  if (register_allocator_plan_join_reservation(
      function_node->children[1]->label, g_tacs_count, temp_index,
      Z80_PHY_NONE, selected_physical_register, selected_slot_index,
      _get_register_allocator_active_slot_count(), assignments,
      assignment_capacity, &schedule,
      occupied_intervals, occupied_interval_count, &reservation) == FAILED) {
    free(occupied_intervals);
    free(producer_physical_registers);
    free(assignments);
    free(block_facts);
    free(paths);
    return FAILED;
  }
  block_reservations = NULL;
  if (reservation.status == RA_JOIN_RESERVATION_READY) {
    if ((size_t)block_count > ((size_t)-1) /
        sizeof(struct register_allocator_join_block_reservation)) {
      free(occupied_intervals);
      free(producer_physical_registers);
      free(assignments);
      free(block_facts);
      free(paths);
      return FAILED;
    }
    block_reservations =
        (struct register_allocator_join_block_reservation *)calloc(
        (size_t)block_count,
        sizeof(struct register_allocator_join_block_reservation));
    if (block_reservations == NULL) {
      free(occupied_intervals);
      free(producer_physical_registers);
      free(assignments);
      free(block_facts);
      free(paths);
      fprintf(stderr, "register_allocator: join_path_reservation_preflight function=%s block=%d temp=r%d capacity=%d status=out_of_memory\n",
          function_node->children[1]->label, join_block_index, temp_index,
          block_count);
      return FAILED;
    }
  }
  if (register_allocator_plan_join_path_reservations(
      function_node->children[1]->label, block_count, g_tacs_count,
      blocks, edges, edge_count, temp_index, assignments,
      assignment_capacity, &schedule, &reservation, block_reservations,
      reservation.status == RA_JOIN_RESERVATION_READY ? block_count : 0,
      &path_reservation) == FAILED) {
    free(block_reservations);
    free(occupied_intervals);
    free(producer_physical_registers);
    free(assignments);
    free(block_facts);
    free(paths);
    return FAILED;
  }
  path_observer.function_node = function_node;
  path_observer.join_block_index = join_block_index;
  path_observer.applied_count = 0;
  if (register_allocator_apply_join_path_reservations(
      function_node->children[1]->label, block_count, g_tacs_count,
      temp_index, &reservation, block_reservations,
      reservation.status == RA_JOIN_RESERVATION_READY ? block_count : 0,
      &path_reservation, &path_observer,
      _observe_register_allocator_join_block_reservation,
      &path_application) == FAILED) {
    free(block_reservations);
    free(occupied_intervals);
    free(producer_physical_registers);
    free(assignments);
    free(block_facts);
    free(paths);
    return FAILED;
  }
  if (register_allocator_commit_join_path_reservations(
      function_node->children[1]->label, block_count, g_tacs_count,
      temp_index, &reservation, block_reservations,
      reservation.status == RA_JOIN_RESERVATION_READY ? block_count : 0,
      &path_reservation, path_state_entries, path_state_capacity,
      path_state_count, &path_commit) == FAILED) {
    free(block_reservations);
    free(occupied_intervals);
    free(producer_physical_registers);
    free(assignments);
    free(block_facts);
    free(paths);
    return FAILED;
  }
  if (path_commit.status == RA_JOIN_PATH_COMMIT_COMMITTED) {
    if (register_allocator_apply_join_assignments(
        function_node->children[1]->label, g_tacs_count, temp_index,
        Z80_PHY_NONE, assignments, assignment_capacity, &schedule,
        function_node, _apply_register_allocator_join_assignment_transaction,
        &assignment_application) == FAILED) {
      free(block_reservations);
      free(occupied_intervals);
      free(producer_physical_registers);
      free(assignments);
      free(block_facts);
      free(paths);
      return FAILED;
    }
  }
  else {
    struct register_allocator_join_schedule ineligible_schedule;

    ineligible_schedule.status = RA_JOIN_SCHEDULE_INELIGIBLE;
    ineligible_schedule.assignment_count = 0;
    ineligible_schedule.conflict_instruction = -1;
    if (register_allocator_apply_join_assignments(
        function_node->children[1]->label, g_tacs_count, temp_index,
        Z80_PHY_NONE, NULL, 0, &ineligible_schedule, function_node, NULL,
        &assignment_application) == FAILED) {
      free(block_reservations);
      free(occupied_intervals);
      free(producer_physical_registers);
      free(assignments);
      free(block_facts);
      free(paths);
      return FAILED;
    }
  }
  fprintf(stderr, "register_allocator: join_assignment_apply_preflight function=%s block=%d temp=r%d assignments=%d applied=%d application=%s ownership=function_scan status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      schedule.assignment_count, assignment_application.applied_count,
      assignment_application.status == RA_JOIN_ASSIGNMENT_APPLICATION_APPLIED ?
      "applied" : "ineligible");
  fprintf(stderr, "register_allocator: join_path_reservation_commit_preflight function=%s block=%d temp=r%d selected=%s slot=%d planned=%d committed=%d state_count=%d commit=%s ownership=function_scan mode=observe_only status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      _get_first_allocator_physical_register_name(
      selected_physical_register), reservation.slot_index,
      path_reservation.block_count, path_commit.committed_count,
      *path_state_count,
      path_commit.status == RA_JOIN_PATH_COMMIT_COMMITTED ? "committed" :
      (path_commit.status == RA_JOIN_PATH_COMMIT_CONFLICT ? "conflict" :
      "ineligible"));
  fprintf(stderr, "register_allocator: join_path_reservation_apply_preflight function=%s block=%d temp=r%d selected=%s slot=%d planned=%d observed=%d application=%s mode=observe_only status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      _get_first_allocator_physical_register_name(selected_physical_register),
      reservation.slot_index, path_reservation.block_count,
      path_observer.applied_count,
      path_application.status == RA_JOIN_PATH_APPLICATION_APPLIED ?
      "applied" : "ineligible");
  fprintf(stderr, "register_allocator: join_path_reservation_preflight function=%s block=%d temp=r%d selected=%s slot=%d blocks=%d plan=%s mode=observe_only status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      _get_first_allocator_physical_register_name(selected_physical_register),
      reservation.slot_index, path_reservation.block_count,
      path_reservation.status == RA_JOIN_PATH_RESERVATION_READY ? "ready" :
      "ineligible");
  fprintf(stderr, "register_allocator: join_reservation_preflight function=%s block=%d temp=r%d selected=%s slot=%d start=%d end=%d conflict=r%d reservation=%s mode=observe_only status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      _get_first_allocator_physical_register_name(selected_physical_register),
      reservation.slot_index, reservation.live_start, reservation.live_end,
      reservation.conflict_temp,
      reservation.status == RA_JOIN_RESERVATION_READY ? "ready" :
      (reservation.status == RA_JOIN_RESERVATION_CONFLICT ? "conflict" :
      "ineligible"));
  fprintf(stderr, "register_allocator: join_schedule_preflight function=%s block=%d temp=r%d selected=%s assignments=%d schedule=%s mode=observe_only status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      _get_first_allocator_physical_register_name(selected_physical_register),
      schedule.assignment_count,
      schedule.status == RA_JOIN_SCHEDULE_READY ? "ready" :
      (schedule.status == RA_JOIN_SCHEDULE_CONFLICT ? "conflict" :
      "ineligible"));
  fprintf(stderr, "register_allocator: join_retention_preflight function=%s block=%d temp=r%d consumer=%d operand=%d candidate=%s selected=%s eligible=%s mode=observe_only status=complete\n",
      function_node->children[1]->label, join_block_index, temp_index,
      consumer != NULL ? consumer_index : -1, consumer_arg,
      _get_first_allocator_physical_register_name(physical_register),
      _get_first_allocator_physical_register_name(selected_physical_register),
      selected_physical_register != Z80_PHY_NONE ? "yes" : "no");
  free(block_reservations);
  free(occupied_intervals);
  free(producer_physical_registers);
  free(assignments);
  free(block_facts);
  free(paths);
  return SUCCEEDED;
}


static int _debug_register_allocator_join_reconciliation(struct tree_node *function_node, struct register_allocator_basic_block *blocks, int block_count, struct register_allocator_cfg_edge *edges, int edge_count, struct register_allocator_join_path_state_entry *path_state_entries, int path_state_capacity, int *path_state_count, int *inserted_instruction_count, int *rewritten_operand_count, int *added_temp_count) {

  struct register_allocator_liveness_storage storage;
  struct register_allocator_join_spill_work_item *work_items;
  char *live_use;
  char *live_def;
  char *live_in;
  char *live_out;
  int temp_count;
  int iterations;
  int block_index;
  int instruction_count;
  int status;
  int work_item_capacity;
  int work_item_count;

  if (inserted_instruction_count == NULL || rewritten_operand_count == NULL ||
      added_temp_count == NULL)
    return FAILED;
  *inserted_instruction_count = 0;
  *rewritten_operand_count = 0;
  *added_temp_count = 0;
  temp_count = function_node->local_variables->temp_registers_count;

  if (block_count <= 0 || temp_count <= 0)
    return SUCCEEDED;
  if (register_allocator_plan_liveness_storage(function_node->children[1]->label,
      block_count, temp_count, &storage) == FAILED)
    return FAILED;

  live_use = (char *)calloc(1, storage.buffer_bytes);
  live_def = (char *)calloc(1, storage.buffer_bytes);
  live_in = (char *)calloc(1, storage.buffer_bytes);
  live_out = (char *)calloc(1, storage.buffer_bytes);
  if (live_use == NULL || live_def == NULL || live_in == NULL || live_out == NULL) {
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    fprintf(stderr, "register_allocator: out of memory while checking join reconciliation.\n");
    return FAILED;
  }

  if (_compute_register_allocator_live_sets(function_node, blocks, block_count, edges, edge_count, live_use, live_def, live_in, live_out, &storage, &iterations) == FAILED) {
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    return FAILED;
  }

  if (edge_count < 0 || (edge_count > 0 &&
      (temp_count > INT_MAX / edge_count ||
      (size_t)(temp_count * edge_count) > ((size_t)-1) /
      sizeof(struct register_allocator_join_spill_work_item)))) {
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    return FAILED;
  }
  instruction_count = g_tacs_count;
  work_item_capacity = temp_count * edge_count;
  work_item_count = 0;
  work_items = NULL;
  if (work_item_capacity > 0) {
    work_items = (struct register_allocator_join_spill_work_item *)calloc(
        (size_t)work_item_capacity,
        sizeof(struct register_allocator_join_spill_work_item));
    if (work_items == NULL) {
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      fprintf(stderr, "register_allocator: out of memory while planning global join spill emissions.\n");
      return FAILED;
    }
  }

  for (block_index = 0; block_index < block_count; block_index++) {
    int approved_count;
    int site_count;
    struct register_allocator_join_spill_site *sites;

    if (register_allocator_plan_join_spill_sites(
        function_node->children[1]->label, block_count, instruction_count, blocks,
        block_index, edges, edge_count, NULL, 0, &site_count) == FAILED) {
      free(work_items);
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      return FAILED;
    }
    if (site_count <= 1)
      continue;
    if ((size_t)site_count > ((size_t)-1) /
        sizeof(struct register_allocator_join_spill_site)) {
      free(work_items);
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      return FAILED;
    }
    sites = (struct register_allocator_join_spill_site *)calloc(
        (size_t)site_count, sizeof(struct register_allocator_join_spill_site));
    if (sites == NULL) {
      free(work_items);
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      fprintf(stderr, "register_allocator: out of memory while approving join spill sites.\n");
      return FAILED;
    }
    if (register_allocator_plan_join_spill_sites(
      function_node->children[1]->label, block_count, instruction_count, blocks,
        block_index, edges, edge_count, sites, site_count,
        &site_count) == FAILED ||
        register_allocator_approve_join_spill_sites(
        function_node->children[1]->label, block_index, instruction_count, sites,
        site_count, function_node, _register_allocator_approve_join_spill_site,
        &approved_count) == FAILED) {
      free(work_items);
      free(sites);
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      return FAILED;
    }
    if (approved_count != site_count) {
      free(work_items);
      free(sites);
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      return FAILED;
    }
    {
      struct register_allocator_loop_candidate loop_candidate;
      struct register_allocator_loop_flow_profile loop_flow_profile;
      struct register_allocator_loop_join effective_loop_join;
      struct register_allocator_loop_join loop_join;
      struct register_allocator_loop_latch_selection latch_selection;
      char *candidate_live_in;
      char *loop_candidate_reason;
      int filtered_owner_count;
      int path_state_index;

      if (register_allocator_classify_loop_join(
          function_node->children[1]->label, block_count, block_index, edges,
          edge_count, &loop_join) == FAILED ||
          register_allocator_profile_loop_flow(
          function_node->children[1]->label, block_count, blocks, block_index,
          edges, edge_count, &loop_flow_profile) == FAILED) {
        free(work_items);
        free(sites);
        free(live_use);
        free(live_def);
        free(live_in);
        free(live_out);
        return FAILED;
      }
      fprintf(stderr, "register_allocator: loop_join_preflight function=%s block=%d predecessors=%d entry=%d latch=%d entry_edge=%d back_edge=%d topology=%s mode=observe_only status=complete\n",
          function_node->children[1]->label, block_index, site_count,
          loop_join.entry_predecessor, loop_join.latch_predecessor,
          loop_join.entry_edge, loop_join.back_edge,
          loop_join.status == RA_LOOP_JOIN_READY ? "ready" :
          (loop_join.status == RA_LOOP_JOIN_AMBIGUOUS ? "ambiguous" :
          "not_loop"));
          fprintf(stderr, "register_allocator: loop_exit_profile_preflight function=%s block=%d entries=%d back_edges=%d exits=%d terminal_exits=%d topology=%s action=%s mode=observe_only status=complete\n",
            function_node->children[1]->label, block_index,
            loop_flow_profile.entry_edge_count,
            loop_flow_profile.back_edge_count,
            loop_flow_profile.exit_edge_count,
            loop_flow_profile.terminal_exit_count,
            loop_flow_profile.status == RA_LOOP_JOIN_READY ? "ready" :
            (loop_flow_profile.status == RA_LOOP_JOIN_AMBIGUOUS ?
            "ambiguous" : "not_loop"),
            loop_flow_profile.status == RA_LOOP_JOIN_READY ?
            "inspect_candidate" :
            (loop_flow_profile.entry_edge_count == 1 &&
            loop_flow_profile.back_edge_count > 1 ?
            "inspect_multi_latch" : "preserve_stack"));
      effective_loop_join = loop_join;
select_loop_candidate:
      candidate_live_in = (char *)malloc(storage.buffer_bytes);
      if (candidate_live_in == NULL) {
        free(work_items);
        free(sites);
        free(live_use);
        free(live_def);
        free(live_in);
        free(live_out);
        return FAILED;
      }
      memcpy(candidate_live_in, live_in, storage.buffer_bytes);
      filtered_owner_count = 0;
      for (path_state_index = 0; path_state_index < *path_state_count;
          path_state_index++) {
        int owner_set_index;

        if (path_state_entries[path_state_index].block_index != block_index)
          continue;
        owner_set_index = block_index * temp_count +
            path_state_entries[path_state_index].temp_index;
        if (owner_set_index < 0 || owner_set_index >= storage.set_count) {
          free(candidate_live_in);
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        if (candidate_live_in[owner_set_index] == YES) {
          candidate_live_in[owner_set_index] = NO;
          filtered_owner_count++;
        }
      }
      fprintf(stderr, "register_allocator: loop_candidate_owner_filter function=%s block=%d path_entries=%d filtered=%d action=select_unreserved status=complete\n",
          function_node->children[1]->label, block_index,
          *path_state_count, filtered_owner_count);
      if (register_allocator_plan_loop_join_candidate(
          function_node->children[1]->label, block_count, temp_count, &storage,
          block_index, &effective_loop_join, candidate_live_in,
          &loop_candidate) == FAILED) {
        free(candidate_live_in);
        free(work_items);
        free(sites);
        free(live_use);
        free(live_def);
        free(live_in);
        free(live_out);
        return FAILED;
      }
      free(candidate_live_in);
      if (loop_flow_profile.back_edge_count > 1 &&
          loop_candidate.status == RA_LOOP_CANDIDATE_READY) {
        if (register_allocator_select_loop_latch(
            function_node->children[1]->label, block_count,
            instruction_count, blocks, block_index, edges, edge_count,
            loop_candidate.temp_index, function_node,
            _register_allocator_instruction_is_active,
            _register_allocator_instruction_writes_temp,
            &latch_selection) == FAILED) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        fprintf(stderr, "register_allocator: loop_latch_selection_preflight function=%s block=%d temp=r%d entries=%d back_edges=%d defining_latches=%d latch=%d definition=%d selection=%s action=%s status=complete\n",
            function_node->children[1]->label, block_index,
            loop_candidate.temp_index, latch_selection.entry_edge_count,
            latch_selection.back_edge_count,
            latch_selection.defining_latch_count,
            latch_selection.loop_join.latch_predecessor,
            latch_selection.latch_definition,
            latch_selection.status == RA_LOOP_LATCH_SELECTION_READY ?
            "ready" : (latch_selection.status ==
            RA_LOOP_LATCH_SELECTION_AMBIGUOUS ? "ambiguous" : "ineligible"),
            latch_selection.status == RA_LOOP_LATCH_SELECTION_READY ?
            "retain" : "preserve_stack");
        if (latch_selection.status == RA_LOOP_LATCH_SELECTION_READY)
          effective_loop_join = latch_selection.loop_join;
        else if (latch_selection.status == RA_LOOP_LATCH_SELECTION_AMBIGUOUS &&
            latch_selection.entry_edge_count == 1 &&
            latch_selection.defining_latch_count > 1) {
          struct register_allocator_loop_latch_collection latch_collection;
          struct register_allocator_loop_latch_definition *latch_definitions;

          latch_definitions =
              (struct register_allocator_loop_latch_definition *)calloc(
              (size_t)block_count,
              sizeof(struct register_allocator_loop_latch_definition));
          if (latch_definitions == NULL ||
              register_allocator_collect_loop_latch_definitions(
              function_node->children[1]->label, block_count,
              instruction_count, blocks, block_index, edges, edge_count,
              loop_candidate.temp_index, function_node,
              _register_allocator_instruction_is_active,
              _register_allocator_instruction_writes_temp,
              latch_definitions, block_count, &latch_collection) == FAILED) {
            free(latch_definitions);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          if (latch_collection.status == RA_LOOP_LATCH_SELECTION_READY &&
              latch_collection.definition_count > 1) {
            effective_loop_join.status = RA_LOOP_JOIN_READY;
            effective_loop_join.entry_predecessor =
                latch_collection.entry_predecessor;
            effective_loop_join.entry_edge = latch_collection.entry_edge;
            effective_loop_join.latch_predecessor =
                latch_definitions[0].block_index;
            effective_loop_join.back_edge = latch_definitions[0].edge_index;
            fprintf(stderr, "register_allocator: loop_multi_latch_selection_preflight function=%s block=%d temp=r%d entries=%d back_edges=%d definitions=%d primary_latch=%d primary_definition=%d action=retain_multi status=complete\n",
                function_node->children[1]->label, block_index,
                loop_candidate.temp_index,
                latch_collection.entry_edge_count,
                latch_collection.back_edge_count,
                latch_collection.definition_count,
                latch_definitions[0].block_index,
                latch_definitions[0].definition_instruction);
          }
          else {
            loop_candidate.status = RA_LOOP_CANDIDATE_AMBIGUOUS;
            loop_candidate.reason = RA_LOOP_CANDIDATE_REASON_TOPOLOGY;
            loop_candidate.candidate_count = 0;
            loop_candidate.temp_index = -1;
          }
          free(latch_definitions);
        }
        else {
          loop_candidate.status = RA_LOOP_CANDIDATE_AMBIGUOUS;
          loop_candidate.reason = RA_LOOP_CANDIDATE_REASON_TOPOLOGY;
          loop_candidate.candidate_count = 0;
          loop_candidate.temp_index = -1;
        }
      }
      loop_candidate_reason =
          loop_candidate.reason == RA_LOOP_CANDIDATE_REASON_TOPOLOGY ?
          (loop_flow_profile.back_edge_count > 1 ?
          "multiple_back_edges" :
          (loop_flow_profile.entry_edge_count > 1 ?
          "multiple_entries" : "loop_topology_ambiguous")) :
          (loop_candidate.reason == RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN ?
          "no_live_in_temp" :
          (loop_candidate.reason ==
          RA_LOOP_CANDIDATE_REASON_MULTIPLE_LIVE_IN ?
          "multiple_live_in_temps" : "unique_live_in_temp"));
      fprintf(stderr, "register_allocator: loop_join_candidate_preflight function=%s block=%d candidates=%d temp=r%d candidate=%s reason=%s mode=observe_only status=complete\n",
          function_node->children[1]->label, block_index,
          loop_candidate.candidate_count, loop_candidate.temp_index,
          loop_candidate.status == RA_LOOP_CANDIDATE_READY ? "ready" :
          (loop_candidate.status == RA_LOOP_CANDIDATE_AMBIGUOUS ?
          "ambiguous" : "ineligible"), loop_candidate_reason);
        if (effective_loop_join.status == RA_LOOP_JOIN_READY &&
          loop_flow_profile.back_edge_count <= 1 &&
          ((loop_candidate.status == RA_LOOP_CANDIDATE_INELIGIBLE &&
          loop_candidate.reason == RA_LOOP_CANDIDATE_REASON_NO_LIVE_IN) ||
          loop_candidate.status == RA_LOOP_CANDIDATE_READY)) {
        struct register_allocator_loop_representation loop_representation;
        struct register_allocator_loop_stack_value_roles *role_values;
        struct register_allocator_loop_stack_value_selection stack_selection;
        struct register_allocator_loop_stack_value_observation *observations;
        struct register_allocator_loop_stack_rewrite_observation
          *rewrite_observations;
        int *identities;
        int identity_capacity;
        int identity_count;
        int identity_index;
        int observation_capacity;
        int observation_count;
        int rewrite_observation_count;
        char *representation_status;

        fprintf(stderr, "register_allocator: loop_stack_search function=%s block=%d live_in_candidates=%d action=inspect_stack_values reason=%s status=complete\n",
          function_node->children[1]->label, block_index,
          loop_candidate.candidate_count,
          loop_candidate.status == RA_LOOP_CANDIDATE_READY ?
          "retained_live_in_present" : "no_live_in_temp");

        if (g_tacs_count > INT_MAX / 3) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        observation_capacity = g_tacs_count * 3;
        identity_capacity =
            function_node->local_variables->local_variables_count;
        observations = (struct register_allocator_loop_stack_value_observation *)
            calloc((size_t)observation_capacity,
            sizeof(struct register_allocator_loop_stack_value_observation));
        rewrite_observations =
          (struct register_allocator_loop_stack_rewrite_observation *)calloc(
          (size_t)observation_capacity,
          sizeof(struct register_allocator_loop_stack_rewrite_observation));
        identities = NULL;
        role_values = NULL;
        if (identity_capacity > 0)
          identities = (int *)calloc((size_t)identity_capacity, sizeof(int));
        if (observations == NULL || rewrite_observations == NULL ||
            (identity_capacity > 0 && identities == NULL) ||
            _collect_register_allocator_loop_stack_observations(function_node,
            observations, observation_capacity, &observation_count) == FAILED ||
            register_allocator_collect_loop_stack_values(
            function_node->children[1]->label, block_count,
            instruction_count, blocks, block_index, &effective_loop_join,
            observations,
            observation_count, identities, identity_capacity,
            &identity_count) == FAILED ||
            register_allocator_classify_loop_representation(
            function_node->children[1]->label, block_index, &loop_candidate,
            identity_count, &loop_representation) == FAILED) {
          free(identities);
          free(observations);
          free(rewrite_observations);
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        if (identity_count > 0)
          role_values =
              (struct register_allocator_loop_stack_value_roles *)calloc(
              (size_t)identity_count,
              sizeof(struct register_allocator_loop_stack_value_roles));
        if ((identity_count > 0 && role_values == NULL) ||
            _collect_register_allocator_loop_stack_roles(function_node,
            blocks, block_count, block_index, &effective_loop_join, edges,
          edge_count,
          identities, identity_count, role_values, rewrite_observations,
          observation_capacity, &rewrite_observation_count) == FAILED ||
            register_allocator_select_loop_stack_value(
            function_node->children[1]->label, block_index, role_values,
            identity_count, &stack_selection) == FAILED) {
          free(role_values);
          free(identities);
          free(observations);
          free(rewrite_observations);
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        for (identity_index = 0; identity_index < identity_count;
            identity_index++) {
          struct tree_node *identity_node;

          identity_node = function_node->local_variables->local_variables[
              identities[identity_index]].node;
          fprintf(stderr, "register_allocator: loop_stack_value_identity_preflight function=%s block=%d value=%d identity=%d variable=%s mode=observe_only status=complete\n",
              function_node->children[1]->label, block_index, identity_index,
              identities[identity_index],
              identity_node != NULL && identity_node->children != NULL &&
              identity_node->children[1] != NULL ?
              identity_node->children[1]->label : "<null>");
              fprintf(stderr, "register_allocator: loop_stack_value_roles_preflight function=%s block=%d value=%d identity=%d variable=%s roles=%d required=%d mode=observe_only status=complete\n",
                function_node->children[1]->label, block_index, identity_index,
                identities[identity_index],
                identity_node != NULL && identity_node->children != NULL &&
                identity_node->children[1] != NULL ?
                identity_node->children[1]->label : "<null>",
                role_values[identity_index].role_mask,
                RA_LOOP_STACK_ROLE_REQUIRED);
        }
        representation_status = loop_representation.status ==
            RA_LOOP_REPRESENTATION_UNIQUE_STACK_VALUE ?
            "unique_stack_value" :
            (loop_representation.status ==
            RA_LOOP_REPRESENTATION_AMBIGUOUS_STACK_VALUES ?
            "ambiguous_stack_values" :
            (loop_representation.status == RA_LOOP_REPRESENTATION_NO_VALUE ?
            "no_value" : "not_applicable"));
        fprintf(stderr, "register_allocator: loop_representation_preflight function=%s block=%d observations=%d values=%d representation=%s mode=observe_only status=complete\n",
            function_node->children[1]->label, block_index,
            observation_count, identity_count, representation_status);
          fprintf(stderr, "register_allocator: loop_stack_selection_preflight function=%s block=%d values=%d candidates=%d identity=%d variable=%s selection=%s mode=observe_only status=complete\n",
            function_node->children[1]->label, block_index, identity_count,
            stack_selection.candidate_count, stack_selection.identity,
            stack_selection.identity >= 0 ?
            function_node->local_variables->local_variables[
            stack_selection.identity].node->children[1]->label : "<none>",
            stack_selection.status == RA_LOOP_STACK_SELECTION_READY ?
            "ready" : (stack_selection.status ==
            RA_LOOP_STACK_SELECTION_AMBIGUOUS ? "ambiguous" : "ineligible"));
            if (stack_selection.status == RA_LOOP_STACK_SELECTION_READY &&
              stack_selection.candidate_count > 1)
            fprintf(stderr, "register_allocator: loop_stack_selection_deferred function=%s block=%d candidates=%d selected_identity=%d deferred=%d action=preserve_stack order=first_observation status=complete\n",
              function_node->children[1]->label, block_index,
              stack_selection.candidate_count, stack_selection.identity,
              stack_selection.candidate_count - 1);
          {
            struct register_allocator_loop_stack_promotion_plan promotion_plan;
            int *existing_temp_indices;
            int proposed_temp_index;
            int selected_value_size;
            int temp_metadata_index;
            char *promotion_reason;

            existing_temp_indices = NULL;
            proposed_temp_index = 0;
            selected_value_size = 0;
            if (temp_count > 0)
              existing_temp_indices = (int *)calloc((size_t)temp_count,
                  sizeof(int));
            if (temp_count > 0 && existing_temp_indices == NULL) {
              free(role_values);
              free(identities);
              free(observations);
              free(rewrite_observations);
              free(work_items);
              free(sites);
              free(live_use);
              free(live_def);
              free(live_in);
              free(live_out);
              return FAILED;
            }
            for (temp_metadata_index = 0; temp_metadata_index < temp_count;
                temp_metadata_index++) {
              existing_temp_indices[temp_metadata_index] =
                  function_node->local_variables->temp_registers[
                  temp_metadata_index].register_index;
              if (existing_temp_indices[temp_metadata_index] < 0 ||
                  existing_temp_indices[temp_metadata_index] == INT_MAX) {
                free(existing_temp_indices);
                free(role_values);
                free(identities);
                free(observations);
                free(rewrite_observations);
                free(work_items);
                free(sites);
                free(live_use);
                free(live_def);
                free(live_in);
                free(live_out);
                return FAILED;
              }
              if (existing_temp_indices[temp_metadata_index] >=
                  proposed_temp_index)
                proposed_temp_index =
                    existing_temp_indices[temp_metadata_index] + 1;
            }
            if (stack_selection.status == RA_LOOP_STACK_SELECTION_READY) {
              if (stack_selection.identity < 0 ||
                  stack_selection.identity >= identity_capacity) {
                free(existing_temp_indices);
                free(role_values);
                free(identities);
                free(observations);
                free(rewrite_observations);
                free(work_items);
                free(sites);
                free(live_use);
                free(live_def);
                free(live_in);
                free(live_out);
                return FAILED;
              }
              selected_value_size =
                  function_node->local_variables->local_variables[
                  stack_selection.identity].size;
            }
            if (register_allocator_plan_loop_stack_promotion(
                function_node->children[1]->label, block_index,
                &stack_selection, selected_value_size, proposed_temp_index,
                existing_temp_indices, temp_count, &promotion_plan) == FAILED) {
              free(existing_temp_indices);
              free(role_values);
              free(identities);
              free(observations);
              free(rewrite_observations);
              free(work_items);
              free(sites);
              free(live_use);
              free(live_def);
              free(live_in);
              free(live_out);
              return FAILED;
            }
            promotion_reason = promotion_plan.reason ==
                RA_LOOP_STACK_PROMOTION_REASON_NONE ? "none" :
                (promotion_plan.reason ==
                RA_LOOP_STACK_PROMOTION_REASON_SIZE ? "unsupported_size" :
                (promotion_plan.reason ==
                RA_LOOP_STACK_PROMOTION_REASON_TEMP_CONFLICT ?
                "temp_conflict" : "selection"));
            fprintf(stderr, "register_allocator: loop_stack_promotion_preflight function=%s block=%d identity=%d variable=%s size=%d proposed_temp=r%d existing_temps=%d plan=%s reason=%s mode=observe_only status=complete\n",
                function_node->children[1]->label, block_index,
                promotion_plan.identity,
                promotion_plan.identity >= 0 ?
                function_node->local_variables->local_variables[
                promotion_plan.identity].node->children[1]->label : "<none>",
                promotion_plan.size, proposed_temp_index, temp_count,
                promotion_plan.status == RA_LOOP_STACK_PROMOTION_READY ?
                "ready" : "ineligible", promotion_reason);
            if (promotion_plan.status != RA_LOOP_STACK_PROMOTION_READY)
              fprintf(stderr, "register_allocator: loop_stack_promotion_fallback function=%s block=%d values=%d candidates=%d identity=%d selection=%s reason=%s action=preserve_stack status=complete\n",
                  function_node->children[1]->label, block_index,
                  identity_count, stack_selection.candidate_count,
                  stack_selection.identity,
                  stack_selection.status == RA_LOOP_STACK_SELECTION_AMBIGUOUS ?
                  "ambiguous" : "ineligible", promotion_reason);
            {
              struct register_allocator_loop_stack_rewrite *rewrites;
              struct register_allocator_loop_stack_rewrite_schedule schedule;
              int rewrite_index;

              rewrites = NULL;
              if (rewrite_observation_count > 0)
                rewrites =
                    (struct register_allocator_loop_stack_rewrite *)calloc(
                    (size_t)rewrite_observation_count,
                    sizeof(struct register_allocator_loop_stack_rewrite));
              if ((rewrite_observation_count > 0 && rewrites == NULL) ||
                  register_allocator_plan_loop_stack_rewrites(
                  function_node->children[1]->label, instruction_count,
                  &promotion_plan, rewrite_observations,
                  rewrite_observation_count, rewrites,
                  rewrite_observation_count, &schedule) == FAILED) {
                free(rewrites);
                free(existing_temp_indices);
                free(role_values);
                free(identities);
                free(observations);
                free(rewrite_observations);
                free(work_items);
                free(sites);
                free(live_use);
                free(live_def);
                free(live_in);
                free(live_out);
                return FAILED;
              }
              for (rewrite_index = 0; rewrite_index < schedule.rewrite_count;
                  rewrite_index++) {
                fprintf(stderr, "register_allocator: loop_stack_rewrite_preflight function=%s block=%d rewrite=%d instruction=%d operand=%d temp=r%d role=%d mode=observe_only status=complete\n",
                    function_node->children[1]->label, block_index,
                    rewrite_index, rewrites[rewrite_index].instruction_index,
                    rewrites[rewrite_index].operand,
                    rewrites[rewrite_index].temp_index,
                    rewrites[rewrite_index].role);
              }
              fprintf(stderr, "register_allocator: loop_stack_rewrite_schedule_preflight function=%s block=%d observations=%d rewrites=%d roles=%d schedule=%s mode=observe_only status=complete\n",
                  function_node->children[1]->label, block_index,
                  rewrite_observation_count, schedule.rewrite_count,
                  schedule.role_mask,
                  schedule.status == RA_LOOP_STACK_REWRITE_READY ?
                  "ready" : "ineligible");
              if (schedule.status == RA_LOOP_STACK_REWRITE_READY) {
                if (_apply_register_allocator_loop_stack_storage_transaction(
                    function_node, &promotion_plan, rewrites,
                  schedule.rewrite_count, YES, rewritten_operand_count,
                  added_temp_count) == FAILED ||
                  *rewritten_operand_count != schedule.rewrite_count ||
                  *added_temp_count != 1) {
                free(rewrites);
                free(existing_temp_indices);
                free(role_values);
                free(identities);
                free(observations);
                free(rewrite_observations);
                free(work_items);
                free(sites);
                free(live_use);
                free(live_def);
                free(live_in);
                free(live_out);
                return FAILED;
                }
                fprintf(stderr, "register_allocator: loop_stack_promotion_handoff function=%s block=%d rewritten=%d added_temps=%d analysis=stale next=rebuild status=complete\n",
                    function_node->children[1]->label, block_index,
                    *rewritten_operand_count, *added_temp_count);
                free(rewrites);
                free(existing_temp_indices);
                free(role_values);
                free(identities);
                free(observations);
                free(rewrite_observations);
                free(work_items);
                free(sites);
                free(live_use);
                free(live_def);
                free(live_in);
                free(live_out);
                return SUCCEEDED;
              }
              free(rewrites);
            }
            free(existing_temp_indices);
          }
          free(role_values);
        free(identities);
        free(observations);
        free(rewrite_observations);
      }
      if (loop_candidate.status == RA_LOOP_CANDIDATE_READY) {
        struct register_allocator_loop_definition loop_definition;
        struct register_allocator_loop_facts loop_facts;

        if (register_allocator_discover_loop_join_facts(
            function_node->children[1]->label, block_count,
            instruction_count, blocks, block_index,
            loop_candidate.temp_index, &effective_loop_join, function_node,
            _register_allocator_instruction_is_active,
            _register_allocator_instruction_reads_temp,
            _register_allocator_instruction_writes_temp,
            _register_allocator_instruction_get_consumer_operand,
            &loop_facts) == FAILED ||
            register_allocator_plan_loop_join_definition(
            function_node->children[1]->label, block_count,
            instruction_count, blocks, block_index,
            loop_candidate.temp_index, YES, &effective_loop_join,
            loop_facts.entry_definition, loop_facts.latch_definition,
            loop_facts.consumer_instruction, loop_facts.consumer_operand,
            &loop_definition) == FAILED) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        fprintf(stderr, "register_allocator: loop_join_fact_discovery_preflight function=%s block=%d temp=r%d entry=%d latch=%d consumer=%d operand=%d facts=%s mode=observe_only status=complete\n",
            function_node->children[1]->label, block_index,
            loop_candidate.temp_index, loop_facts.entry_definition,
            loop_facts.latch_definition, loop_facts.consumer_instruction,
            loop_facts.consumer_operand,
            loop_facts.status == RA_LOOP_FACTS_READY ? "ready" :
            "ineligible");
        fprintf(stderr, "register_allocator: loop_join_definition_preflight function=%s block=%d temp=r%d entry=%d latch=%d consumer=%d operand=%d definition=%s reason=%d mode=observe_only status=complete\n",
            function_node->children[1]->label, block_index,
            loop_candidate.temp_index, loop_definition.entry_definition,
            loop_definition.latch_definition,
            loop_definition.consumer_instruction,
            loop_definition.consumer_operand,
            loop_definition.status == RA_LOOP_DEFINITION_READY ? "ready" :
            "ineligible", loop_definition.reason);
        if (loop_definition.status == RA_LOOP_DEFINITION_READY) {
          struct register_allocator_join_assignment *loop_assignments;
          struct register_allocator_join_block_reservation *loop_reservations;
          struct register_allocator_loop_retention_application loop_application;
          struct register_allocator_loop_retention_plan loop_plan;
            struct register_allocator_loop_register_path_context path_context;
            struct register_allocator_loop_register_path_evaluation
              path_evaluations[3];
              struct register_allocator_loop_register_exclusion
                register_exclusions[3];
          struct register_allocator_loop_register_selection register_selection;
            struct register_allocator_join_path_reservation path_reservation;
            struct register_allocator_join_schedule path_schedule;
          struct register_allocator_loop_transaction_context transaction;
          struct register_allocator_target_policy *target_policy;
          struct register_allocator_loop_latch_collection latch_collection;
          struct register_allocator_loop_latch_definition *latch_definitions;
          struct tac *consumer;
          int consumer_physical_register;
          int candidate_register_count;
          int candidate_registers[3];
          int coverage_instruction;
          int coverage_operand;
          int assignment_capacity;
          int first_uncovered_instruction;
          int first_uncovered_operand;
          int conflict_block;
          int conflict_end;
          int conflict_owner;
          int conflict_start;
          int entry_physical_register;
          int latch_physical_register;
          int supplemental_producer_count;
          int planned_reservation_count;
          int preferred_physical_register;
          int preferred_slot_index;
          int reservation_index;
          int register_selection_attempt;
          int uncovered_operand_count;
          int selected_physical_register;
          int slot_index;
          int supplemental_consumer_count;
          struct temp_register *loop_temp;

          latch_definitions = NULL;
          supplemental_producer_count = 0;
          if (loop_flow_profile.back_edge_count > 1) {
            latch_definitions =
                (struct register_allocator_loop_latch_definition *)calloc(
                (size_t)block_count,
                sizeof(struct register_allocator_loop_latch_definition));
            if (latch_definitions == NULL ||
                register_allocator_collect_loop_latch_definitions(
                function_node->children[1]->label, block_count,
                instruction_count, blocks, block_index, edges, edge_count,
                loop_candidate.temp_index, function_node,
                _register_allocator_instruction_is_active,
                _register_allocator_instruction_writes_temp,
                latch_definitions, block_count, &latch_collection) == FAILED ||
                latch_collection.status != RA_LOOP_LATCH_SELECTION_READY ||
                latch_collection.definition_count <= 0 ||
                latch_definitions[0].definition_instruction !=
                loop_definition.latch_definition) {
              free(latch_definitions);
              free(work_items);
              free(sites);
              free(live_use);
              free(live_def);
              free(live_in);
              free(live_out);
              return FAILED;
            }
            supplemental_producer_count =
                latch_collection.definition_count - 1;
          }

            preferred_physical_register =
              function_node->local_variables->temp_registers[
              loop_candidate.temp_index].size == 8 ? Z80_PHY_C :
              (function_node->local_variables->temp_registers[
              loop_candidate.temp_index].size == 16 ? Z80_PHY_HL :
              Z80_PHY_NONE);
          target_policy = _get_register_allocator_target_policy();
          entry_physical_register = g_tacs[
              loop_definition.entry_definition].result_physical_register;
          latch_physical_register = g_tacs[
              loop_definition.latch_definition].result_physical_register;
          consumer = &g_tacs[loop_definition.consumer_instruction];
          consumer_physical_register =
              loop_definition.consumer_operand == TAC_USE_RESULT ?
              consumer->result_physical_register :
              (loop_definition.consumer_operand == TAC_USE_ARG1 ?
              consumer->arg1_physical_register :
              consumer->arg2_physical_register);
          loop_temp = &function_node->local_variables->temp_registers[
              loop_candidate.temp_index];
          supplemental_consumer_count = 0;
          for (coverage_instruction = loop_temp->live_start;
              coverage_instruction <= loop_temp->live_end;
              coverage_instruction++) {
            struct tac *coverage_tac;

            if (_register_allocator_instruction_is_active(function_node,
                coverage_instruction, loop_candidate.temp_index) == NO)
              continue;
            coverage_tac = &g_tacs[coverage_instruction];
            if (coverage_tac->arg1_type == TAC_ARG_TYPE_TEMP &&
                (int)coverage_tac->arg1_d == loop_candidate.temp_index &&
                (coverage_instruction != loop_definition.consumer_instruction ||
                loop_definition.consumer_operand != TAC_USE_ARG1))
              supplemental_consumer_count++;
            if (coverage_tac->arg2_type == TAC_ARG_TYPE_TEMP &&
                (int)coverage_tac->arg2_d == loop_candidate.temp_index &&
                (coverage_instruction != loop_definition.consumer_instruction ||
                loop_definition.consumer_operand != TAC_USE_ARG2))
              supplemental_consumer_count++;
          }
          if (supplemental_producer_count > INT_MAX - 3 ||
              supplemental_consumer_count >
              INT_MAX - 3 - supplemental_producer_count) {
            free(latch_definitions);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
            assignment_capacity = 3 + supplemental_producer_count +
              supplemental_consumer_count;
          loop_assignments =
              (struct register_allocator_join_assignment *)calloc(
              (size_t)assignment_capacity,
              sizeof(struct register_allocator_join_assignment));
            loop_reservations =
              (struct register_allocator_join_block_reservation *)calloc(
              (size_t)block_count,
              sizeof(struct register_allocator_join_block_reservation));
            if (loop_assignments == NULL || loop_reservations == NULL) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
            loop_assignments[0].role = RA_JOIN_ASSIGNMENT_PRODUCER;
            loop_assignments[0].instruction =
              loop_definition.entry_definition;
            loop_assignments[0].operand = 0;
            loop_assignments[0].physical_register =
              entry_physical_register;
            loop_assignments[1].role = RA_JOIN_ASSIGNMENT_PRODUCER;
            loop_assignments[1].instruction =
              loop_definition.latch_definition;
            loop_assignments[1].operand = 0;
            loop_assignments[1].physical_register =
              latch_physical_register;
            loop_assignments[2 + supplemental_producer_count].role =
              RA_JOIN_ASSIGNMENT_CONSUMER;
            loop_assignments[2 + supplemental_producer_count].instruction =
              loop_definition.consumer_instruction;
            loop_assignments[2 + supplemental_producer_count].operand =
              loop_definition.consumer_operand;
            loop_assignments[2 + supplemental_producer_count].
              physical_register = consumer_physical_register;
            supplemental_consumer_count = 0;
            for (coverage_instruction = loop_temp->live_start;
              coverage_instruction <= loop_temp->live_end;
              coverage_instruction++) {
            struct tac *coverage_tac;

            if (_register_allocator_instruction_is_active(function_node,
              coverage_instruction, loop_candidate.temp_index) == NO)
              continue;
            coverage_tac = &g_tacs[coverage_instruction];
            if (coverage_tac->arg1_type == TAC_ARG_TYPE_TEMP &&
              (int)coverage_tac->arg1_d == loop_candidate.temp_index &&
              (coverage_instruction != loop_definition.consumer_instruction ||
              loop_definition.consumer_operand != TAC_USE_ARG1)) {
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].role =
                RA_JOIN_ASSIGNMENT_CONSUMER;
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].instruction =
                coverage_instruction;
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].operand =
                TAC_USE_ARG1;
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].physical_register =
                coverage_tac->arg1_physical_register;
              supplemental_consumer_count++;
            }
            if (coverage_tac->arg2_type == TAC_ARG_TYPE_TEMP &&
              (int)coverage_tac->arg2_d == loop_candidate.temp_index &&
              (coverage_instruction != loop_definition.consumer_instruction ||
              loop_definition.consumer_operand != TAC_USE_ARG2)) {
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].role =
                RA_JOIN_ASSIGNMENT_CONSUMER;
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].instruction =
                coverage_instruction;
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].operand =
                TAC_USE_ARG2;
              loop_assignments[3 + supplemental_producer_count +
                supplemental_consumer_count].physical_register =
                coverage_tac->arg2_physical_register;
              supplemental_consumer_count++;
            }
            }
            for (coverage_instruction = 0;
              coverage_instruction < supplemental_producer_count;
              coverage_instruction++) {
              loop_assignments[2 + coverage_instruction].role =
                RA_JOIN_ASSIGNMENT_PRODUCER;
              loop_assignments[2 + coverage_instruction].instruction =
                latch_definitions[coverage_instruction + 1].
                definition_instruction;
              loop_assignments[2 + coverage_instruction].operand = 0;
              loop_assignments[2 + coverage_instruction].physical_register =
                g_tacs[latch_definitions[coverage_instruction + 1].
                definition_instruction].result_physical_register;
            }
            fprintf(stderr, "register_allocator: loop_retention_supplemental_consumers function=%s block=%d temp=r%d consumers=%d assignments=%d status=complete\n",
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index, supplemental_consumer_count,
              assignment_capacity);
          candidate_register_count = 0;
          if (target_policy == NULL || preferred_physical_register ==
              Z80_PHY_NONE || register_allocator_resolve_candidate_registers(
              function_node->children[1]->label, target_policy,
              loop_temp->size, Z80_PHY_NONE, candidate_registers, 3,
              &candidate_register_count) == FAILED ||
              target_policy->get_active_slot_index == NULL) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          if (register_allocator_prepare_loop_register_exclusions(
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index, Z80_PHY_NONE,
              candidate_registers, candidate_register_count,
              register_exclusions, 3) == FAILED) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          register_selection_attempt = 0;
          preferred_slot_index = target_policy->get_active_slot_index(
              preferred_physical_register);
          path_schedule.status = RA_JOIN_SCHEDULE_READY;
          path_schedule.assignment_count = assignment_capacity;
          path_schedule.conflict_instruction = -1;
          if (preferred_slot_index < 0 ||
              register_allocator_plan_loop_join_path_reservations(
              function_node->children[1]->label, block_count,
              instruction_count, blocks, edges, edge_count, block_index,
              loop_candidate.temp_index, preferred_slot_index,
              &effective_loop_join, loop_assignments, assignment_capacity,
              &path_schedule, loop_reservations, block_count,
              &path_reservation) == FAILED ||
              path_reservation.status != RA_JOIN_PATH_RESERVATION_READY) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          path_context.function_node = function_node;
          path_context.assignments = loop_assignments;
          path_context.reservations = loop_reservations;
          path_context.assignment_count = assignment_capacity;
          path_context.reservation_count = path_reservation.block_count;
          path_context.block_index = block_index;
            path_context.preferred_physical_register =
              preferred_physical_register;
          path_context.temp_index = loop_candidate.temp_index;
          if (register_allocator_evaluate_loop_register_paths(
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index, Z80_PHY_NONE,
              candidate_registers, candidate_register_count, &path_context,
              _is_loop_register_path_safe, path_evaluations, 3) == FAILED ||
              register_allocator_select_loop_retention_register_with_exclusions(
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index, Z80_PHY_NONE,
              preferred_physical_register, candidate_registers,
              candidate_register_count, loop_assignments,
              assignment_capacity, path_evaluations,
              candidate_register_count, register_exclusions,
              candidate_register_count, &register_selection) == FAILED) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
loop_register_candidate_retry:
          if (register_selection_attempt > 0 &&
              register_allocator_select_loop_retention_register_with_exclusions(
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index, Z80_PHY_NONE,
              preferred_physical_register, candidate_registers,
              candidate_register_count, loop_assignments,
              assignment_capacity, path_evaluations,
              candidate_register_count, register_exclusions,
              candidate_register_count, &register_selection) == FAILED) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          register_selection_attempt++;
            selected_physical_register = register_selection.physical_register;
            fprintf(stderr, "register_allocator: loop_register_selection_preflight function=%s block=%d temp=r%d preferred=%s candidates=%d bound=%d selected=%s candidate=%d attempt=%d action=%s mode=mutation status=complete\n",
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index,
              _get_first_allocator_physical_register_name(
              preferred_physical_register), candidate_register_count,
              register_selection.bound_assignment_count,
              _get_first_allocator_physical_register_name(
              selected_physical_register),
              register_selection.candidate_index,
              register_selection_attempt,
              register_selection.status ==
              RA_LOOP_REGISTER_SELECTION_READY ? "retain" :
              "preserve_stack");
            if (register_selection.status ==
              RA_LOOP_REGISTER_SELECTION_INELIGIBLE) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            goto loop_register_selection_complete;
            }
          slot_index = target_policy->get_active_slot_index != NULL ?
              target_policy->get_active_slot_index(
              selected_physical_register) : -1;
          if (slot_index < 0) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
              if (register_allocator_plan_multi_latch_retention(
              function_node->children[1]->label, block_count,
              instruction_count, blocks, edges, edge_count, block_index,
              loop_candidate.temp_index, slot_index, Z80_PHY_NONE,
              selected_physical_register, entry_physical_register,
              latch_physical_register, consumer_physical_register,
              &effective_loop_join, &loop_definition,
              loop_assignments + 2, supplemental_producer_count,
              loop_assignments + 3 + supplemental_producer_count,
              supplemental_consumer_count, loop_assignments,
              assignment_capacity,
              loop_reservations, block_count, &loop_plan) == FAILED) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          uncovered_operand_count = 0;
          first_uncovered_instruction = -1;
          first_uncovered_operand = 0;
          conflict_block = -1;
          conflict_start = -1;
          conflict_end = -1;
          conflict_owner = -1;
          planned_reservation_count = loop_plan.reservation_count;
          if (loop_plan.status == RA_LOOP_RETENTION_READY) {
            for (coverage_instruction = loop_temp->live_start;
                coverage_instruction <= loop_temp->live_end;
                coverage_instruction++) {
              if (_register_allocator_instruction_is_active(function_node,
                  coverage_instruction, loop_candidate.temp_index) == NO)
                continue;
              for (coverage_operand = TAC_USE_RESULT;
                  coverage_operand <= TAC_USE_ARG2; coverage_operand++) {
                struct tac *coverage_tac;
                int assignment_index;
                int covered;
                int operand_physical_register;
                int operand_temp;
                int operand_type;

                coverage_tac = &g_tacs[coverage_instruction];
                if (coverage_operand == TAC_USE_RESULT) {
                  operand_type = coverage_tac->result_type;
                  operand_temp = (int)coverage_tac->result_d;
                  operand_physical_register =
                      coverage_tac->result_physical_register;
                }
                else if (coverage_operand == TAC_USE_ARG1) {
                  operand_type = coverage_tac->arg1_type;
                  operand_temp = (int)coverage_tac->arg1_d;
                  operand_physical_register =
                      coverage_tac->arg1_physical_register;
                }
                else {
                  operand_type = coverage_tac->arg2_type;
                  operand_temp = (int)coverage_tac->arg2_d;
                  operand_physical_register =
                      coverage_tac->arg2_physical_register;
                }
                if (operand_type != TAC_ARG_TYPE_TEMP ||
                    operand_temp != loop_candidate.temp_index)
                  continue;
                covered = operand_physical_register ==
                    selected_physical_register ? YES : NO;
                for (assignment_index = 0;
                    assignment_index < loop_plan.assignment_count;
                    assignment_index++) {
                  if (loop_assignments[assignment_index].instruction ==
                      coverage_instruction &&
                      loop_assignments[assignment_index].operand ==
                      coverage_operand)
                    covered = YES;
                }
                if (covered == NO) {
                  if (first_uncovered_instruction < 0) {
                    first_uncovered_instruction = coverage_instruction;
                    first_uncovered_operand = coverage_operand;
                  }
                  uncovered_operand_count++;
                }
              }
            }
            fprintf(stderr, "register_allocator: loop_retention_coverage function=%s block=%d temp=r%d live_start=%d live_end=%d assignments=%d uncovered=%d first_instruction=%d first_operand=%d status=%s\n",
                function_node->children[1]->label, block_index,
                loop_candidate.temp_index, loop_temp->live_start,
                loop_temp->live_end, loop_plan.assignment_count,
                uncovered_operand_count, first_uncovered_instruction,
                first_uncovered_operand,
                uncovered_operand_count == 0 ? "complete" : "incomplete");
            if (uncovered_operand_count > 0) {
              loop_plan.status = RA_LOOP_RETENTION_INELIGIBLE;
              loop_plan.assignment_count = 0;
              loop_plan.reservation_count = 0;
            }
          }
          if (loop_plan.status == RA_LOOP_RETENTION_READY) {
            for (reservation_index = 0;
                reservation_index < loop_plan.reservation_count;
                reservation_index++) {
              struct register_allocator_join_path_state_query query;

              if (register_allocator_query_join_path_state(
                  function_node->children[1]->label, block_count,
                  instruction_count,
                  _get_register_allocator_active_slot_count(),
                  loop_reservations[reservation_index].block_index,
                  slot_index, loop_candidate.temp_index,
                  loop_reservations[reservation_index].start_instruction,
                  loop_reservations[reservation_index].end_instruction,
                  path_state_entries, *path_state_count, &query) == FAILED) {
                free(latch_definitions);
                free(loop_assignments);
                free(loop_reservations);
                free(work_items);
                free(sites);
                free(live_use);
                free(live_def);
                free(live_in);
                free(live_out);
                return FAILED;
              }
              if (query.status == RA_JOIN_PATH_STATE_CONFLICT) {
                conflict_block =
                    loop_reservations[reservation_index].block_index;
                conflict_start =
                    loop_reservations[reservation_index].start_instruction;
                conflict_end =
                    loop_reservations[reservation_index].end_instruction;
                conflict_owner = query.owner_temp;
                loop_plan.status = RA_LOOP_RETENTION_INELIGIBLE;
                loop_plan.assignment_count = 0;
                loop_plan.reservation_count = 0;
                break;
              }
            }
          }
          fprintf(stderr, "register_allocator: loop_retention_conflict_preflight function=%s block=%d temp=r%d selected=%s slot=%d reservations=%d conflict=%s conflict_block=%d conflict_start=%d conflict_end=%d owner=r%d action=%s status=complete\n",
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index,
              _get_first_allocator_physical_register_name(
              selected_physical_register), slot_index,
              planned_reservation_count,
              conflict_block >= 0 ? "yes" : "no", conflict_block,
              conflict_start, conflict_end, conflict_owner,
              loop_plan.status == RA_LOOP_RETENTION_READY ? "retain" :
              "preserve_stack");
          if (conflict_block >= 0) {
            if (register_allocator_exclude_loop_register_candidate(
                function_node->children[1]->label, block_index,
                loop_candidate.temp_index,
                register_selection.candidate_index,
                RA_LOOP_REGISTER_EXCLUSION_RESERVATION, conflict_block,
                conflict_start, register_exclusions,
                candidate_register_count) == FAILED) {
              free(latch_definitions);
              free(loop_assignments);
              free(loop_reservations);
              free(work_items);
              free(sites);
              free(live_use);
              free(live_def);
              free(live_in);
              free(live_out);
              return FAILED;
            }
            fprintf(stderr, "register_allocator: loop_register_retry function=%s block=%d temp=r%d rejected=%s candidate=%d attempt=%d reason=reservation conflict_block=%d conflict_start=%d conflict_end=%d owner=r%d action=select_next status=complete\n",
                function_node->children[1]->label, block_index,
                loop_candidate.temp_index,
                _get_first_allocator_physical_register_name(
                selected_physical_register),
                register_selection.candidate_index,
                register_selection_attempt, conflict_block, conflict_start,
                conflict_end, conflict_owner);
            goto loop_register_candidate_retry;
          }
          transaction.function_node = function_node;
          transaction.block_count = block_count;
          transaction.path_state_entries = path_state_entries;
          transaction.path_state_capacity = path_state_capacity;
          transaction.path_state_count = path_state_count;
          if (register_allocator_apply_loop_retention(
              function_node->children[1]->label,
              loop_candidate.temp_index, slot_index, loop_assignments,
              assignment_capacity,
              loop_reservations, block_count, &loop_plan, &transaction,
              _apply_register_allocator_loop_retention_transaction,
              &loop_application) == FAILED) {
            free(latch_definitions);
            free(loop_assignments);
            free(loop_reservations);
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          fprintf(stderr, "register_allocator: loop_retention_apply_preflight function=%s block=%d temp=r%d selected=%s slot=%d assignments=%d reservations=%d application=%s ownership=function_scan status=complete\n",
              function_node->children[1]->label, block_index,
              loop_candidate.temp_index,
              _get_first_allocator_physical_register_name(
              selected_physical_register), slot_index,
              loop_application.assignment_count,
              loop_application.reservation_count,
              loop_application.status == RA_LOOP_APPLICATION_APPLIED ?
              "applied" : (loop_application.status ==
              RA_LOOP_APPLICATION_FAILED ? "failed" : "ineligible"));
              free(latch_definitions);
          free(loop_assignments);
          free(loop_reservations);
          if (loop_application.status == RA_LOOP_APPLICATION_APPLIED &&
              loop_candidate.candidate_count > 1) {
            fprintf(stderr, "register_allocator: loop_candidate_iteration function=%s block=%d processed_temp=r%d remaining=%d action=select_next status=complete\n",
                function_node->children[1]->label, block_index,
                loop_candidate.temp_index,
                loop_candidate.candidate_count - 1);
            goto select_loop_candidate;
          }
loop_register_selection_complete:
          ;
        }
      }
      else {
        fprintf(stderr, "register_allocator: loop_join_fact_discovery_preflight function=%s block=%d temp=r%d entry=-1 latch=-1 consumer=-1 operand=0 facts=skipped reason=%s mode=observe_only status=complete\n",
            function_node->children[1]->label, block_index,
          loop_candidate.temp_index, loop_candidate_reason);
        fprintf(stderr, "register_allocator: loop_retention_plan_preflight function=%s block=%d temp=r%d plan=skipped reason=%s ownership=function_scan status=complete\n",
            function_node->children[1]->label, block_index,
          loop_candidate.temp_index, loop_candidate_reason);
        fprintf(stderr, "register_allocator: loop_retention_apply_preflight function=%s block=%d temp=r%d application=skipped reason=%s atomic=yes ownership=function_scan status=complete\n",
            function_node->children[1]->label, block_index,
          loop_candidate.temp_index, loop_candidate_reason);
      }
    }
    if (register_allocator_order_join_spill_sites(
        function_node->children[1]->label, block_index, instruction_count,
        sites, site_count) == FAILED) {
      free(work_items);
      free(sites);
      free(live_use);
      free(live_def);
      free(live_in);
      free(live_out);
      return FAILED;
    }
    {
      int temp_index;

      for (temp_index = 0; temp_index < temp_count; temp_index++) {
        struct register_allocator_join_spill_mutation mutation;
        struct register_allocator_temp_state state;
        struct temp_register *temp_register;
        int action;
        int set_index;
        int site_index;
        int stack_resident;

        if (register_allocator_resolve_liveness_index(
            function_node->children[1]->label, block_count, temp_count,
            &storage, block_index, temp_index, &set_index) == FAILED) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        if (live_in[set_index] == NO)
          continue;
        if (live_in[set_index] != YES) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        if (_debug_register_allocator_join_retention_preflight(function_node,
          blocks, block_count, edges, edge_count, block_index, sites,
          site_count, temp_index, path_state_entries, path_state_capacity,
          path_state_count) == FAILED) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        temp_register = &function_node->local_variables->temp_registers[temp_index];
        state.spill_required = temp_register->spill_required;
        state.physical_register = temp_register->physical_register;
        if (register_allocator_classify_join_temp_state(
            function_node->children[1]->label, block_index, site_count,
            temp_index, &state, Z80_PHY_NONE, &stack_resident) == FAILED ||
            register_allocator_plan_join_action(
            function_node->children[1]->label, block_index, site_count,
            temp_index, stack_resident, &action) == FAILED) {
          free(work_items);
          free(sites);
          free(live_use);
          free(live_def);
          free(live_in);
          free(live_out);
          return FAILED;
        }
        for (site_index = 0; site_index < site_count; site_index++) {
          struct register_allocator_join_spill_emission emission;
          int byte_count;
          int destination_offset;
          int spill_available;

          if (register_allocator_prepare_join_spill_mutation(
              function_node->children[1]->label, block_index, temp_index,
              action, Z80_PHY_NONE, state.physical_register,
              &sites[site_index], &mutation) == FAILED) {
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          if (mutation.apply == YES) {
            spill_available = temp_register->offset_to_fp !=
                REGISTER_ALLOCATOR_POISON_OFFSET ? YES : NO;
            destination_offset = temp_register->offset_to_fp;
            byte_count = temp_register->size / 8;
          }
          else {
            spill_available = NO;
            destination_offset = 0;
            byte_count = 0;
          }
          if (register_allocator_prepare_join_spill_emission(
              function_node->children[1]->label, block_index, Z80_PHY_NONE,
              &mutation, spill_available, destination_offset, byte_count,
              &emission) == FAILED) {
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
          if (emission.emit == YES) {
            if (work_item_count >= work_item_capacity) {
              free(work_items);
              free(sites);
              free(live_use);
              free(live_def);
              free(live_in);
              free(live_out);
              return FAILED;
            }
            work_items[work_item_count].join_block_index = block_index;
            work_items[work_item_count].emission = emission;
            work_item_count++;
          }
          else if (register_allocator_apply_join_spill_emission(
              function_node->children[1]->label, block_index, Z80_PHY_NONE,
              function_node, &emission,
              _register_allocator_insert_join_spill) == FAILED) {
            free(work_items);
            free(sites);
            free(live_use);
            free(live_def);
            free(live_in);
            free(live_out);
            return FAILED;
          }
        }
      }
    }
    free(sites);
  }

  if (register_allocator_order_join_spill_emissions(
      function_node->children[1]->label, block_count, instruction_count,
      Z80_PHY_NONE, work_items, work_item_count) == FAILED) {
    free(work_items);
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    return FAILED;
  }
  {
    int work_item_index;

    for (work_item_index = 0; work_item_index < work_item_count;
        work_item_index++) {
      if (register_allocator_apply_join_spill_emission(
          function_node->children[1]->label,
          work_items[work_item_index].join_block_index, Z80_PHY_NONE,
          function_node, &work_items[work_item_index].emission,
          _register_allocator_insert_join_spill) == FAILED) {
        free(work_items);
        free(live_use);
        free(live_def);
        free(live_in);
        free(live_out);
        return FAILED;
      }
    }
  }
  *inserted_instruction_count = work_item_count;
  free(work_items);

  if (*inserted_instruction_count > 0) {
    fprintf(stderr, "register_allocator: join_reconcile_handoff function=%s inserted=%d liveness=stale next=rebuild status=complete\n",
        function_node->children[1]->label, *inserted_instruction_count);
    free(live_use);
    free(live_def);
    free(live_in);
    free(live_out);
    return SUCCEEDED;
  }

  status = register_allocator_reconcile_stack_only_joins(
      function_node->children[1]->label, block_count, temp_count, &storage,
      edges, edge_count, live_in, function_node,
      _register_allocator_join_is_stack_resident);

  free(live_use);
  free(live_def);
  free(live_in);
  free(live_out);

  return status;
}
#endif


static int _rebuild_register_allocator_control_flow(
    struct tree_node *function_node,
    struct register_allocator_basic_block **blocks,
    struct register_allocator_cfg_edge **edges, int *block_count,
    int *edge_count) {

  struct register_allocator_control_flow_storage storage;
  struct register_allocator_basic_block *new_blocks;
  struct register_allocator_cfg_edge *new_edges;

  if (function_node == NULL || blocks == NULL || edges == NULL ||
      block_count == NULL || edge_count == NULL)
    return FAILED;
  if (register_allocator_plan_control_flow_storage(
      function_node->children[1]->label, g_tacs_count, &storage) == FAILED)
    return FAILED;
  new_blocks = (struct register_allocator_basic_block *)calloc(1,
      storage.block_bytes);
  new_edges = (struct register_allocator_cfg_edge *)calloc(1,
      storage.edge_bytes);
  if (new_blocks == NULL || new_edges == NULL) {
    free(new_blocks);
    free(new_edges);
    fprintf(stderr, "register_allocator: out of memory while rebuilding control flow after join spill insertion.\n");
    return FAILED;
  }
  if (_collect_register_allocator_basic_blocks(function_node, new_blocks,
      storage.max_blocks, storage.instruction_bytes, block_count) == FAILED ||
      _collect_register_allocator_cfg_edges(function_node, new_blocks,
      *block_count, new_edges, storage.max_edges, storage.instruction_bytes,
      edge_count) == FAILED) {
    free(new_blocks);
    free(new_edges);
    return FAILED;
  }
  free(*blocks);
  free(*edges);
  *blocks = new_blocks;
  *edges = new_edges;
  fprintf(stderr, "register_allocator: post_mutation_control_flow function=%s instructions=%d blocks=%d edges=%d status=complete\n",
      function_node->children[1]->label, g_tacs_count, *block_count,
      *edge_count);
  return SUCCEEDED;
}


struct register_allocator_candidate_evaluation_context {
  struct tree_node *function_node;
  struct tac *producer;
  struct tac *consumer;
  struct temp_register *temp_register;
  struct register_allocator_active_slot *active_slots;
  struct register_allocator_join_path_state_entry *path_state_entries;
  int path_state_count;
  int block_count;
  int active_slot_count;
  int block_index;
  int consumer_arg;
  int check_path;
  int check_overlap;
  int a_slot;
  int hl_slot;
  int bc_slot;
  int b_slot;
  int c_slot;
};


static int _evaluate_register_allocator_candidate_allowed(void *context, int physical_register) {

  struct register_allocator_candidate_evaluation_context *evaluation_context;

  evaluation_context = (struct register_allocator_candidate_evaluation_context *)context;
  return _is_register_allocator_candidate_allowed_for_physical_register(
      evaluation_context->producer, evaluation_context->consumer, evaluation_context->temp_register,
      physical_register, evaluation_context->consumer_arg);
}


static int _evaluate_register_allocator_candidate_path(void *context, int physical_register) {

  struct register_allocator_candidate_evaluation_context *evaluation_context;

  evaluation_context = (struct register_allocator_candidate_evaluation_context *)context;
  if (evaluation_context->check_path == NO)
    return YES;
  return _is_basic_block_path_safe_for_physical_register(evaluation_context->function_node,
      evaluation_context->block_index,
      evaluation_context->producer - g_tacs, evaluation_context->consumer - g_tacs,
      physical_register);
}


static int _evaluate_register_allocator_candidate_conflict(void *context, int physical_register) {

  struct register_allocator_candidate_evaluation_context *evaluation_context;
  struct register_allocator_join_path_state_query path_state_query;
  struct register_allocator_target_policy *target_policy;
  int slot_index;
  int overlap;

  evaluation_context = (struct register_allocator_candidate_evaluation_context *)context;
  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL || target_policy->get_active_slot_index == NULL)
    return NO;
  slot_index = target_policy->get_active_slot_index(physical_register);
  if (register_allocator_query_join_path_state(
      evaluation_context->function_node->children[1]->label,
      evaluation_context->block_count, g_tacs_count,
      evaluation_context->active_slot_count, evaluation_context->block_index,
      slot_index, evaluation_context->temp_register->register_index,
      (int)(evaluation_context->producer - g_tacs),
      (int)(evaluation_context->consumer - g_tacs),
      evaluation_context->path_state_entries,
      evaluation_context->path_state_count, &path_state_query) == FAILED)
    return NO;
  fprintf(stderr, "register_allocator: join_path_state_candidate function=%s block=%d temp=r%d slot=%d start=%d end=%d reservation=%s owner=r%d status=complete\n",
      evaluation_context->function_node->children[1]->label,
      evaluation_context->block_index,
      evaluation_context->temp_register->register_index, slot_index,
      (int)(evaluation_context->producer - g_tacs),
      (int)(evaluation_context->consumer - g_tacs),
      path_state_query.status == RA_JOIN_PATH_STATE_OWNED ? "owned" :
      (path_state_query.status == RA_JOIN_PATH_STATE_CONFLICT ? "conflict" :
      "available"), path_state_query.owner_temp);
  if (path_state_query.status == RA_JOIN_PATH_STATE_CONFLICT)
    return NO;
  if (evaluation_context->check_overlap == NO)
    return YES;

  overlap = _active_physical_registers_overlap_candidate(
      evaluation_context->function_node->children[1]->label, physical_register,
      evaluation_context->active_slots[evaluation_context->a_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->hl_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->bc_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->b_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->c_slot].temp_index);
#if defined(DEBUG_PASS_5)
  _debug_register_allocator_overlap_check(evaluation_context->function_node,
      evaluation_context->block_index, physical_register,
      evaluation_context->active_slots[evaluation_context->a_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->hl_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->bc_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->b_slot].temp_index,
      evaluation_context->active_slots[evaluation_context->c_slot].temp_index, overlap);
#endif
  return overlap == NO ? YES : NO;
}


static int _evaluate_register_allocator_candidate_active(void *context, int physical_register) {

  return _evaluate_register_allocator_candidate_conflict(context, physical_register) == YES ? NO : YES;
}


static int _register_allocator_active_temp_has_metadata(void *context, int temp_index) {

  return _find_temp_register_info((struct tree_node *)context, temp_index) != NULL ? YES : NO;
}


static int _choose_register_allocator_alternate(struct tree_node *function_node, int block_index,
    char *reason, struct tac *producer, struct tac *consumer, struct temp_register *temp_register,
    int consumer_arg, int *physical_registers, int physical_register_count, int check_path,
    int check_overlap, struct register_allocator_active_slot *active_slots, int a_slot, int hl_slot,
    int bc_slot, int b_slot, int c_slot,
    struct register_allocator_join_path_state_entry *path_state_entries,
    int path_state_count, int block_count, int active_slot_count,
    int *selected_physical_register, int *selected_candidate_index) {

  struct register_allocator_candidate_evaluation evaluations[2];
  struct register_allocator_candidate_selection selection;
  struct register_allocator_candidate_evaluation_context evaluation_context;

  if (physical_register_count <= 0 || physical_register_count > 2)
    return FAILED;

  evaluation_context.function_node = function_node;
  evaluation_context.producer = producer;
  evaluation_context.consumer = consumer;
  evaluation_context.temp_register = temp_register;
  evaluation_context.active_slots = active_slots;
  evaluation_context.path_state_entries = path_state_entries;
  evaluation_context.path_state_count = path_state_count;
  evaluation_context.block_count = block_count;
  evaluation_context.active_slot_count = active_slot_count;
  evaluation_context.block_index = block_index;
  evaluation_context.consumer_arg = consumer_arg;
  evaluation_context.check_path = check_path;
  evaluation_context.check_overlap = check_overlap;
  evaluation_context.a_slot = a_slot;
  evaluation_context.hl_slot = hl_slot;
  evaluation_context.bc_slot = bc_slot;
  evaluation_context.b_slot = b_slot;
  evaluation_context.c_slot = c_slot;

  if (register_allocator_select_candidate_register(function_node->children[1]->label, block_index,
      reason, physical_registers, physical_register_count, Z80_PHY_NONE, &evaluation_context,
      _evaluate_register_allocator_candidate_allowed, _evaluate_register_allocator_candidate_path,
      _evaluate_register_allocator_candidate_conflict, evaluations, 2, &selection) == FAILED)
    return FAILED;

  *selected_physical_register = selection.physical_register;
  *selected_candidate_index = selection.candidate_index;
  return SUCCEEDED;
}


struct register_allocator_candidate_dispatch_context {
  struct tree_node *function_node;
  struct tac *producer;
  struct tac *consumer;
  struct temp_register *temp_register;
  struct register_allocator_active_slot *active_slots;
  struct register_allocator_join_path_state_entry *path_state_entries;
  int path_state_count;
  int block_count;
  int active_slot_count;
  int *byte_candidate_registers;
  int byte_candidate_count;
  int *word_candidate_registers;
  int word_candidate_count;
  int block_index;
  int consumer_arg;
  int a_slot;
  int hl_slot;
  int bc_slot;
  int b_slot;
  int c_slot;
};


static int _select_register_allocator_alternate(void *context, char *reason, int check_path,
    int check_overlap, int *selected_physical_register, int *selected_candidate_index) {

  struct register_allocator_candidate_dispatch_context *dispatch_context;
  int *candidate_registers;
  int candidate_count;

  dispatch_context = (struct register_allocator_candidate_dispatch_context *)context;
  if (dispatch_context->temp_register->size == 16) {
    candidate_registers = &dispatch_context->word_candidate_registers[1];
    candidate_count = dispatch_context->word_candidate_count - 1;
  }
  else {
    candidate_registers = &dispatch_context->byte_candidate_registers[1];
    candidate_count = dispatch_context->byte_candidate_count - 1;
  }

  return _choose_register_allocator_alternate(dispatch_context->function_node,
      dispatch_context->block_index, reason, dispatch_context->producer,
      dispatch_context->consumer, dispatch_context->temp_register,
      dispatch_context->consumer_arg, candidate_registers, candidate_count, check_path,
      check_overlap, dispatch_context->active_slots, dispatch_context->a_slot,
      dispatch_context->hl_slot, dispatch_context->bc_slot, dispatch_context->b_slot,
      dispatch_context->c_slot, dispatch_context->path_state_entries,
      dispatch_context->path_state_count, dispatch_context->block_count,
      dispatch_context->active_slot_count, selected_physical_register,
      selected_candidate_index);
}


static int _run_basic_block_linear_scan(struct tree_node *function_node,
  struct register_allocator_basic_block *blocks, int block_count,
  struct register_allocator_join_path_state_entry *path_state_entries,
  int path_state_count) {

  struct register_allocator_active_slot *active_slots;
  int byte_candidate_registers[3];
  int word_candidate_registers[2];
  int physical_register_roles[5];
  int active_slot_roles[5];
  size_t active_slot_bytes;
  int block_index;
  int active_slot_count;
  int byte_candidate_count;
  int word_candidate_count;
  int byte_primary_register;
  int byte_first_alternate_register;
  int byte_second_alternate_register;
  int word_primary_register;
  int word_alternate_register;
  int a_slot;
  int hl_slot;
  int bc_slot;
  int b_slot;
  int c_slot;
  int status;

  active_slot_count = _get_register_allocator_active_slot_count();
  if (register_allocator_resolve_candidate_registers(function_node->children[1]->label,
      _get_register_allocator_target_policy(), 8, Z80_PHY_NONE, byte_candidate_registers, 3,
      &byte_candidate_count) == FAILED || byte_candidate_count != 3)
    return FAILED;
  if (register_allocator_resolve_candidate_registers(function_node->children[1]->label,
      _get_register_allocator_target_policy(), 16, Z80_PHY_NONE, word_candidate_registers, 2,
      &word_candidate_count) == FAILED || word_candidate_count != 2)
    return FAILED;
  byte_primary_register = byte_candidate_registers[0];
  byte_first_alternate_register = byte_candidate_registers[1];
  byte_second_alternate_register = byte_candidate_registers[2];
  word_primary_register = word_candidate_registers[0];
  word_alternate_register = word_candidate_registers[1];
  physical_register_roles[0] = byte_primary_register;
  physical_register_roles[1] = word_primary_register;
  physical_register_roles[2] = word_alternate_register;
  physical_register_roles[3] = byte_second_alternate_register;
  physical_register_roles[4] = byte_first_alternate_register;
  if (register_allocator_resolve_active_slot_roles(function_node->children[1]->label,
      _get_register_allocator_target_policy(), physical_register_roles, 5, active_slot_roles) == FAILED)
    return FAILED;
  a_slot = active_slot_roles[0];
  hl_slot = active_slot_roles[1];
  bc_slot = active_slot_roles[2];
  b_slot = active_slot_roles[3];
  c_slot = active_slot_roles[4];
  if (register_allocator_plan_active_slot_storage(function_node->children[1]->label, active_slot_count, &active_slot_bytes) == FAILED)
    return FAILED;

  active_slots = (struct register_allocator_active_slot *)calloc(1, active_slot_bytes);
  if (active_slots == NULL) {
    fprintf(stderr, "register_allocator: target_active_slots function=%s target=%s slots=%d bytes=%lu status=out_of_memory\n",
        function_node->children[1]->label, _get_register_allocator_target_policy()->name,
        active_slot_count, (unsigned long)active_slot_bytes);
    return FAILED;
  }
  status = FAILED;

  fprintf(stderr, "register_allocator: target_active_slots function=%s target=%s slots=%d status=complete bytes=%lu storage=policy_sized\n",
      function_node->children[1]->label, _get_register_allocator_target_policy()->name,
      active_slot_count, (unsigned long)active_slot_bytes);

  for (block_index = 0; block_index < block_count; block_index++) {
    int tac_index;

    if (register_allocator_reset_active_slots(function_node->children[1]->label, block_index, active_slots, active_slot_count) == FAILED)
      goto cleanup;

    for (tac_index = blocks[block_index].start_tac; tac_index <= blocks[block_index].end_tac; tac_index++) {
      struct tac *producer = &g_tacs[tac_index];
      struct tac *consumer;
      struct register_allocator_candidate candidate;
      struct temp_register *temp_register;
      int register_index;
      int next_use;
      int consumer_arg;
      int physical_register;
      int split_preservation_supported;
      int split_action;
      int fallback_plan;
      struct register_allocator_candidate_dispatch_context dispatch_context;
      struct register_allocator_candidate_selection_orchestration selection_orchestration;
      struct register_allocator_candidate_transition_execution transition_execution;
      struct register_allocator_slot_transition_context transition_context;

      split_preservation_supported = NO;
      split_action = RA_SPLIT_ACTION_NONE;

      if (register_allocator_expire_active_slots(function_node->children[1]->label, block_index, tac_index, active_slots, active_slot_count) == FAILED)
        goto cleanup;

      if (register_allocator_discover_candidate(function_node->children[1]->label, tac_index,
          blocks[block_index].end_tac, g_tacs_count, function_node,
          _register_allocator_instruction_get_produced_temp, _register_allocator_instruction_is_active,
          _register_allocator_instruction_reads_temp, _register_allocator_instruction_writes_temp,
          _register_allocator_instruction_get_consumer_operand, &candidate) == FAILED)
        goto cleanup;
      if (candidate.found == NO)
        continue;

      register_index = candidate.temp_index;
      temp_register = _find_temp_register_info(function_node, register_index);
      if (temp_register == NULL)
        continue;

      if (producer->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
        physical_register = _get_register_allocator_call_result_physical_register(temp_register->size);
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: call_result_convention function=%s tac=%d size=%d target=%s phy=%s\n",
            function_node->children[1]->label, tac_index, temp_register->size,
            _get_register_allocator_target_policy()->name, _get_first_allocator_physical_register_name(physical_register));
#endif
      }
      else if (temp_register->size == 8)
        physical_register = byte_primary_register;
      else if (temp_register->size == 16)
        physical_register = word_primary_register;
      else
        physical_register = Z80_PHY_NONE;
      if (physical_register == Z80_PHY_NONE)
        continue;

      next_use = candidate.consumer_instruction;
      consumer = &g_tacs[next_use];
      consumer_arg = candidate.consumer_operand;

      {
        struct register_allocator_candidate_evaluation_context evaluation_context;

        evaluation_context.function_node = function_node;
        evaluation_context.producer = producer;
        evaluation_context.consumer = consumer;
        evaluation_context.temp_register = temp_register;
        evaluation_context.active_slots = active_slots;
        evaluation_context.path_state_entries = path_state_entries;
        evaluation_context.path_state_count = path_state_count;
        evaluation_context.block_count = block_count;
        evaluation_context.active_slot_count = active_slot_count;
        evaluation_context.block_index = block_index;
        evaluation_context.consumer_arg = consumer_arg;
        evaluation_context.check_path = YES;
        evaluation_context.check_overlap = YES;
        evaluation_context.a_slot = a_slot;
        evaluation_context.hl_slot = hl_slot;
        evaluation_context.bc_slot = bc_slot;
        evaluation_context.b_slot = b_slot;
        evaluation_context.c_slot = c_slot;

        dispatch_context.function_node = function_node;
        dispatch_context.producer = producer;
        dispatch_context.consumer = consumer;
        dispatch_context.temp_register = temp_register;
        dispatch_context.active_slots = active_slots;
        dispatch_context.path_state_entries = path_state_entries;
        dispatch_context.path_state_count = path_state_count;
        dispatch_context.block_count = block_count;
        dispatch_context.active_slot_count = active_slot_count;
        dispatch_context.byte_candidate_registers = byte_candidate_registers;
        dispatch_context.byte_candidate_count = byte_candidate_count;
        dispatch_context.word_candidate_registers = word_candidate_registers;
        dispatch_context.word_candidate_count = word_candidate_count;
        dispatch_context.block_index = block_index;
        dispatch_context.consumer_arg = consumer_arg;
        dispatch_context.a_slot = a_slot;
        dispatch_context.hl_slot = hl_slot;
        dispatch_context.bc_slot = bc_slot;
        dispatch_context.b_slot = b_slot;
        dispatch_context.c_slot = c_slot;
        if (register_allocator_orchestrate_candidate_selection(
          function_node->children[1]->label, block_index,
          _get_register_allocator_target_policy(), &candidate, consumer->op,
          temp_register->size,
            temp_register->size == 16 ? word_candidate_count : byte_candidate_count,
          physical_register, Z80_PHY_NONE, temp_register->spill_reason,
          Z80_SPILL_REASON_NONE, Z80_SPILL_REASON_MAINMAIN,
          temp_register->spill_boundary_tac, &evaluation_context,
            _evaluate_register_allocator_candidate_allowed,
            _evaluate_register_allocator_candidate_path,
            _evaluate_register_allocator_candidate_active, &dispatch_context,
          _select_register_allocator_alternate, &selection_orchestration) == FAILED)
          goto cleanup;
      }
      if (selection_orchestration.preparation.interval_blocked == YES) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan skip function=%s block=%d r%d producer_tac=%d consumer_tac=%d reason=spill_boundary_between boundary_reason=%s boundary_tac=%d\n", function_node->children[1]->label, block_index, register_index, tac_index, next_use, _get_first_allocator_spill_reason_name(temp_register->spill_reason), temp_register->spill_boundary_tac);
#endif
        continue;
      }
      split_preservation_supported =
          selection_orchestration.preparation.preservation_supported;
      if (split_preservation_supported == YES) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: target_split_spill function=%s tac=%d op=%s operand=%s size=%d target=%s phy=%s preserve=yes\n",
            function_node->children[1]->label, next_use, _get_first_allocator_op_name(consumer->op),
            _get_register_allocator_consumer_arg_name(consumer_arg), temp_register->size,
            _get_register_allocator_target_policy()->name, _get_first_allocator_physical_register_name(physical_register));
        fprintf(stderr, "register_allocator: linear_scan split_spill function=%s block=%d r%d producer_tac=%d consumer_tac=%d phy=HL preserve=arg1_to_spill reason=multi_read_interval\n", function_node->children[1]->label, block_index, register_index, tac_index, next_use);
#endif
      }
#if defined(DEBUG_PASS_5)
      if (temp_register->spill_reason != Z80_SPILL_REASON_NONE)
        fprintf(stderr, "register_allocator: linear_scan interval_reuse function=%s block=%d r%d producer_tac=%d consumer_tac=%d ignoring_reason=%s boundary_tac=%d\n", function_node->children[1]->label, block_index, register_index, tac_index, next_use, _get_first_allocator_spill_reason_name(temp_register->spill_reason), temp_register->spill_boundary_tac);
#endif
      fallback_plan = selection_orchestration.finalization.decision.plan;
      if (selection_orchestration.finalization.dispatch.action == RA_CANDIDATE_DISPATCH_REJECT) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan skip function=%s block=%d r%d producer_tac=%d consumer_tac=%d reason=%s\n",
            function_node->children[1]->label, block_index, register_index, tac_index, next_use,
            fallback_plan == RA_CANDIDATE_PLAN_FALLBACK_UNSUPPORTED ||
            fallback_plan == RA_CANDIDATE_PLAN_REJECT_UNSUPPORTED ?
            "unsupported_physical_register" : "clobber_between");
#endif
        continue;
      }
      if (selection_orchestration.finalization.dispatch.action == RA_CANDIDATE_DISPATCH_USE_ALTERNATE) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan alternate function=%s block=%d r%d primary=%s alternate=%s reason=%s producer_tac=%d consumer_tac=%d\n",
            function_node->children[1]->label, block_index, register_index,
            _get_first_allocator_physical_register_name(physical_register),
            _get_first_allocator_physical_register_name(selection_orchestration.finalization.dispatch.physical_register),
            fallback_plan == RA_CANDIDATE_PLAN_FALLBACK_PATH ? "clobber_between" :
            (fallback_plan == RA_CANDIDATE_PLAN_FALLBACK_CONFLICT ?
            "active_register_conflict" : "unsupported_primary"), tac_index, next_use);
#endif
      }
      physical_register = selection_orchestration.finalization.dispatch.physical_register;
      split_action = selection_orchestration.finalization.split_action;
      if (split_action == RA_SPLIT_ACTION_REJECT_UNSUPPORTED) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan skip function=%s block=%d r%d producer_tac=%d consumer_tac=%d reason=multi_read_interval\n", function_node->children[1]->label, block_index, register_index, tac_index, next_use);
#endif
        continue;
      }
      if (split_action == RA_SPLIT_ACTION_REJECT_REGISTER) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan skip function=%s block=%d r%d producer_tac=%d consumer_tac=%d reason=multi_read_preserve_requires_hl\n", function_node->children[1]->label, block_index, register_index, tac_index, next_use);
#endif
        continue;
      }
      if (selection_orchestration.proceed == NO)
        continue;
      transition_context.function_node = function_node;
      transition_context.block_index = block_index;
      transition_context.physical_register = physical_register;
      if (register_allocator_execute_candidate_transition(
          function_node->children[1]->label, block_index, tac_index, register_index,
          next_use, consumer->op, consumer_arg, physical_register, Z80_PHY_NONE,
          split_action, active_slots, active_slot_count,
          _get_register_allocator_target_policy(), function_node,
          _register_allocator_active_temp_has_metadata, &transition_context,
          _observe_register_allocator_transition_preparation, &transition_context,
          _clear_register_allocator_interval,
          _spill_register_allocator_transition_temp,
          _retain_register_allocator_transition_candidate, function_node,
          _register_allocator_apply_spill_mutation, &transition_execution) == FAILED)
        goto cleanup;

      if (transition_execution.preparation.plan.linear_scan_decision == RA_LINEAR_SCAN_REPLACE_ACTIVE) {
#if defined(DEBUG_PASS_5)
        if (transition_execution.preparation.plan.prefer_candidate_on_equal == YES)
          fprintf(stderr, "register_allocator: linear_scan prefer_array_write_value function=%s block=%d r%d displaced_r%d consumer_tac=%d phy=A displaced_operand=arg2 retained_operand=arg1 reason=shared_next_use\n", function_node->children[1]->label, block_index, register_index, transition_execution.preparation.plan.transition.displaced_temp, next_use);
#endif
      }
      else if (transition_execution.preparation.plan.linear_scan_decision == RA_LINEAR_SCAN_KEEP_ACTIVE) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan skip function=%s block=%d r%d producer_tac=%d consumer_tac=%d reason=active_register_conflict active_r%d active_next_tac=%d\n", function_node->children[1]->label, block_index, register_index, tac_index, next_use, transition_execution.preparation.plan.active_temp, transition_execution.preparation.plan.active_next_use);
#endif
      }

      if (transition_execution.application.spill_mutation.apply == YES) {
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: linear_scan spill_insert function=%s block=%d r%d consumer_tac=%d operand=arg1 phy=HL destination=existing_spill_slot\n", function_node->children[1]->label, block_index, register_index, next_use);
#endif
      }
    }
  }

  status = SUCCEEDED;

cleanup:
  fprintf(stderr, "register_allocator: target_active_slots_release function=%s target=%s slots=%d bytes=%lu status=%s\n",
      function_node->children[1]->label, _get_register_allocator_target_policy()->name,
      active_slot_count, (unsigned long)active_slot_bytes, status == SUCCEEDED ? "complete" : "failed");
  free(active_slots);
  return status;
}


static int _place_split_spill_reloads(struct tree_node *function_node,
  struct register_allocator_basic_block *blocks, int block_count,
  struct register_allocator_cfg_edge *edges, int edge_count) {

  int block_index;

  for (block_index = 0; block_index < block_count; block_index++) {
    int tac_index;

    for (tac_index = blocks[block_index].start_tac; tac_index <= blocks[block_index].end_tac; tac_index++) {
      struct tac *spill_consumer;
      struct tac *reload_consumer;
      struct temp_register *temp_register;
      int register_index;
      int max_reload_instructions;
      int reload_index;
      int *reload_instructions;
      struct register_allocator_reload_execution *reload_executions;
      struct register_allocator_reload_chain_execution chain_execution;
      struct register_allocator_reload_chain graph_chain;
      struct register_allocator_reload_chain_storage chain_storage;

      spill_consumer = &g_tacs[tac_index];
      if (spill_consumer->store_retained_to_spill_operand != TAC_USE_ARG1 || spill_consumer->arg1_type != TAC_ARG_TYPE_TEMP)
        continue;

      register_index = (int)spill_consumer->arg1_d;
      temp_register = _find_temp_register_info(function_node, register_index);
      if (temp_register == NULL)
        continue;

      max_reload_instructions = g_tacs_count - tac_index - 1;
      if (max_reload_instructions <= 0)
        continue;
      if (register_allocator_plan_reload_chain_storage(function_node->children[1]->label,
          max_reload_instructions, &chain_storage) == FAILED)
        return FAILED;
      reload_instructions = (int *)calloc(1, chain_storage.instruction_bytes);
      if (reload_instructions == NULL) {
        fprintf(stderr, "register_allocator: reload_chain_apply function=%s block=%d r%d spill_tac=%d capacity=%d status=out_of_memory\n",
            function_node->children[1]->label, block_index, register_index, tac_index, max_reload_instructions);
        return FAILED;
      }
      reload_executions = (struct register_allocator_reload_execution *)calloc(1,
          chain_storage.execution_bytes);
      if (reload_executions == NULL) {
        fprintf(stderr, "register_allocator: reload_chain_apply function=%s block=%d r%d spill_tac=%d capacity=%d status=out_of_memory\n",
            function_node->children[1]->label, block_index, register_index, tac_index, max_reload_instructions);
        free(reload_instructions);
        return FAILED;
      }

      if (register_allocator_discover_reload_graph(
          function_node->children[1]->label, block_count, g_tacs_count,
          blocks, edges, edge_count, block_index, tac_index, register_index,
          function_node,
          _register_allocator_instruction_is_active, _register_allocator_instruction_reads_temp,
          _register_allocator_instruction_writes_temp, _register_allocator_instruction_is_split_reload_eligible,
          reload_instructions, max_reload_instructions, &graph_chain) == FAILED) {
        free(reload_executions);
        free(reload_instructions);
        return FAILED;
      }
#if defined(DEBUG_PASS_5)
      fprintf(stderr, "register_allocator: reload_graph_preflight function=%s spill_block=%d spill_tac=%d reloads=%d truncated=%s mode=cfg_graph status=complete\n",
          function_node->children[1]->label, block_index, tac_index,
          graph_chain.count, graph_chain.truncated == YES ? "yes" : "no");
#endif
      if (graph_chain.count == 0) {
        free(reload_executions);
        free(reload_instructions);
        continue;
      }
      if (register_allocator_execute_reload_graph_transaction(
          function_node->children[1]->label, block_index, register_index,
          tac_index, Z80_PHY_NONE, function_node,
          _register_allocator_get_reload_operand,
          _register_allocator_select_reload_register, function_node,
          _register_allocator_apply_reload_transaction, reload_instructions,
          graph_chain.count, reload_executions,
          max_reload_instructions, &chain_execution) == FAILED) {
        free(reload_executions);
        free(reload_instructions);
        return FAILED;
      }

      for (reload_index = 0; reload_index < chain_execution.chain.count; reload_index++) {
        int reload_physical_register;
        int reload_tac;

        reload_tac = reload_instructions[reload_index];
        reload_consumer = &g_tacs[reload_tac];
        reload_physical_register = reload_executions[reload_index].mutation.physical_register;
        if (reload_executions[reload_index].mutation.apply == NO)
          continue;

#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: target_split_reload function=%s tac=%d op=%s operand=result size=%d target=%s phy=%s\n",
            function_node->children[1]->label, reload_tac, _get_first_allocator_op_name(reload_consumer->op), temp_register->size,
            _get_register_allocator_target_policy()->name, _get_first_allocator_physical_register_name(reload_physical_register));
        fprintf(stderr, "register_allocator: linear_scan reload_insert function=%s block=%d r%d consumer_tac=%d operand=result source=existing_spill_slot phy=%s\n",
            function_node->children[1]->label, block_index, register_index, reload_tac,
            _get_first_allocator_physical_register_name(reload_physical_register));
#endif
      }

#if defined(DEBUG_PASS_5)
      fprintf(stderr, "register_allocator: reload_chain_apply function=%s block=%d r%d spill_tac=%d discovered=%d capacity=%d truncated=%s status=complete\n",
          function_node->children[1]->label, block_index, register_index, tac_index, chain_execution.chain.count,
          max_reload_instructions, chain_execution.chain.truncated == YES ? "yes" : "no");
#endif
  free(reload_executions);
      free(reload_instructions);
    }
  }

  return SUCCEEDED;
}


static int _force_spill_at_basic_block_exits(struct tree_node *function_node,
  struct register_allocator_basic_block *blocks, int block_count,
  struct register_allocator_join_path_state_entry *path_state_entries,
  int path_state_count);


static int _scan_register_allocator_basic_blocks(struct tree_node *function_node) {

  struct register_allocator_basic_block *blocks;
  struct register_allocator_cfg_edge *edges;
  struct register_allocator_control_flow_storage control_flow_storage;
  struct register_allocator_join_path_state_entry *path_state_entries;
  struct register_allocator_join_path_state_entry *resized_path_state_entries;
  struct register_allocator_join_path_state_storage path_state_storage;
  struct register_allocator_join_path_state_storage resized_path_state_storage;
  struct register_allocator_fixed_point_step fixed_point_step;
  struct register_allocator_target_policy *target_policy;
  int block_count;
  int edge_count;
  int max_edges;
  int i;
  int inserted_instruction_count;
  int rewritten_operand_count;
  int added_temp_count;
  int reconciliation_pass;
  int reconciliation_pass_limit;
  int reconciliation_start_instruction_count;
  int path_state_count;
  int rebuild_required;
  int status;

  if (g_allocator_enabled == NO || function_node->local_variables == NULL)
    return SUCCEEDED;

  target_policy = _get_register_allocator_target_policy();
  if (target_policy == NULL) {
    fprintf(stderr, "register_allocator: no target policy for backend %d\n", g_backend);
    return FAILED;
  }
  if (register_allocator_validate_target_policy(function_node->children[1]->label, target_policy) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: target_policy function=%s target=%s hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload,argument_transport,argument_byte_access,return_slot_access,call_frame_prefix,location_materialization,address_materialization",
      function_node->children[1]->label, target_policy->name);
        fprintf(stderr, " overlap_model=A|HL|BC(B,C) clobber_model=descriptor_table transparency_model=per_tac call_result_model=size_based call_boundary_model=call_return stack_return_model=frame_slot argument_model=contiguous_stack split_model=preserve_and_reload argument_transport_model=width_and_extension argument_byte_model=logical_offsets return_slot_model=byte_offsets call_frame_model=return_address_and_saved_fp location_model=semantic_source_to_target_kind address_model=target_register_location_and_cache_profile\n");
#endif

  edges = NULL;
  path_state_entries = NULL;
  resized_path_state_entries = NULL;
  path_state_storage.entry_capacity = 0;
  path_state_storage.entry_bytes = 0;
  path_state_count = 0;
  if (register_allocator_plan_control_flow_storage(function_node->children[1]->label,
      g_tacs_count, &control_flow_storage) == FAILED)
    return FAILED;
  blocks = (struct register_allocator_basic_block *)calloc(1, control_flow_storage.block_bytes);
  if (blocks == NULL) {
    fprintf(stderr, "register_allocator: out of memory while collecting basic blocks.\n");
    return FAILED;
  }

  if (_collect_register_allocator_basic_blocks(function_node, blocks,
      control_flow_storage.max_blocks, control_flow_storage.instruction_bytes,
      &block_count) == FAILED) {
    free(blocks);
    return FAILED;
  }

  max_edges = control_flow_storage.max_edges;
  edges = (struct register_allocator_cfg_edge *)calloc(1, control_flow_storage.edge_bytes);
  if (edges == NULL) {
    free(blocks);
    fprintf(stderr, "register_allocator: out of memory while collecting CFG edges.\n");
    return FAILED;
  }

  if (_collect_register_allocator_cfg_edges(function_node, blocks, block_count, edges,
      max_edges, control_flow_storage.instruction_bytes, &edge_count) == FAILED) {
    free(edges);
    free(blocks);
    return FAILED;
  }
  if (function_node->local_variables->temp_registers_count > 0) {
    if (register_allocator_plan_join_path_state_storage(
        function_node->children[1]->label, block_count,
        function_node->local_variables->temp_registers_count,
        &path_state_storage) == FAILED) {
      free(edges);
      free(blocks);
      return FAILED;
    }
    path_state_entries =
        (struct register_allocator_join_path_state_entry *)calloc(1,
        path_state_storage.entry_bytes);
    if (path_state_entries == NULL) {
      fprintf(stderr, "register_allocator: join_path_state_acquire function=%s capacity=%d bytes=%lu status=out_of_memory\n",
          function_node->children[1]->label,
          path_state_storage.entry_capacity,
          (unsigned long)path_state_storage.entry_bytes);
      free(edges);
      free(blocks);
      return FAILED;
    }
  }
  status = FAILED;
  fprintf(stderr, "register_allocator: join_path_state_acquire function=%s capacity=%d bytes=%lu count=0 ownership=function_scan mode=observe_only status=complete\n",
      function_node->children[1]->label, path_state_storage.entry_capacity,
      (unsigned long)path_state_storage.entry_bytes);

#if defined(DEBUG_PASS_5)
  for (i = 0; i < block_count; i++) {
    fprintf(stderr, "register_allocator: basic_block function=%s block=%d start_tac=%d(%s) end_tac=%d(%s) reason=%s\n", function_node->children[1]->label, i, blocks[i].start_tac, _get_first_allocator_op_name(g_tacs[blocks[i].start_tac].op), blocks[i].end_tac, _get_first_allocator_op_name(g_tacs[blocks[i].end_tac].op), _get_register_allocator_basic_block_end_reason_name(blocks[i].end_reason));
    _debug_register_allocator_basic_block_clobbers(function_node, &blocks[i], i);
    _debug_register_allocator_basic_block_next_uses(function_node, &blocks[i], i);
  }
  _debug_register_allocator_cfg_edges(function_node, edges, edge_count);
  if (_debug_register_allocator_liveness(function_node, blocks, block_count, edges, edge_count) == FAILED) {
    goto cleanup;
  }
  reconciliation_pass = 0;
  if (register_allocator_plan_fixed_point_limit(
      function_node->children[1]->label, g_tacs_count,
      &reconciliation_pass_limit) == FAILED)
    goto cleanup;
  fprintf(stderr, "register_allocator: fixed_point_budget function=%s instruction_capacity=%d limit=%d basis=initial_instructions status=complete\n",
      function_node->children[1]->label, g_tacs_count,
      reconciliation_pass_limit);
  do {
    reconciliation_pass++;
    reconciliation_start_instruction_count = g_tacs_count;
    if (_debug_register_allocator_join_reconciliation(function_node, blocks,
        block_count, edges, edge_count, path_state_entries,
        path_state_storage.entry_capacity, &path_state_count,
        &inserted_instruction_count, &rewritten_operand_count,
        &added_temp_count) == FAILED)
      goto cleanup;
    if (register_allocator_plan_post_mutation_rebuild(
        function_node->children[1]->label,
        reconciliation_start_instruction_count, g_tacs_count,
        inserted_instruction_count, rewritten_operand_count,
        added_temp_count, &rebuild_required) == FAILED)
      goto cleanup;
    if (register_allocator_plan_fixed_point_step(
        function_node->children[1]->label, reconciliation_pass,
        reconciliation_pass_limit, rebuild_required,
        &fixed_point_step) == FAILED)
      goto cleanup;
    fprintf(stderr, "register_allocator: fixed_point_step_preflight function=%s pass=%d limit=%d rebuild=%s action=%s status=complete\n",
        function_node->children[1]->label, fixed_point_step.pass,
        fixed_point_step.pass_limit, rebuild_required == YES ? "yes" : "no",
        fixed_point_step.status == RA_FIXED_POINT_STEP_REBUILD ? "rebuild" :
        (fixed_point_step.status == RA_FIXED_POINT_STEP_STABLE ? "stable" :
        "iteration_limit"));
    fprintf(stderr, "register_allocator: post_mutation_fixed_point function=%s pass=%d inserted=%d rewritten=%d added_temps=%d rebuild=%s status=complete\n",
        function_node->children[1]->label, reconciliation_pass,
        inserted_instruction_count, rewritten_operand_count, added_temp_count,
        rebuild_required == YES ? "yes" : "no");
    if (fixed_point_step.status == RA_FIXED_POINT_STEP_ITERATION_LIMIT) {
      fprintf(stderr, "register_allocator: post_mutation_fixed_point function=%s passes=%d limit=%d status=iteration_limit\n",
          function_node->children[1]->label, reconciliation_pass,
          reconciliation_pass_limit);
      goto cleanup;
    }
    if (fixed_point_step.status == RA_FIXED_POINT_STEP_STABLE)
      break;
    if (_rebuild_register_allocator_control_flow(function_node, &blocks,
        &edges, &block_count, &edge_count) == FAILED ||
        _debug_register_allocator_liveness(function_node, blocks, block_count,
        edges, edge_count) == FAILED) {
      goto cleanup;
    }
    fprintf(stderr, "register_allocator: post_mutation_validation function=%s inserted=%d rewritten=%d added_temps=%d blocks=%d edges=%d liveness=recomputed status=complete\n",
      function_node->children[1]->label, inserted_instruction_count,
      rewritten_operand_count, added_temp_count, block_count, edge_count);
    path_state_count = 0;
    if (function_node->local_variables->temp_registers_count > 0) {
      if (register_allocator_plan_join_path_state_storage(
          function_node->children[1]->label, block_count,
          function_node->local_variables->temp_registers_count,
          &resized_path_state_storage) == FAILED) {
        goto cleanup;
      }
      resized_path_state_entries =
          (struct register_allocator_join_path_state_entry *)calloc(1,
          resized_path_state_storage.entry_bytes);
      if (resized_path_state_entries == NULL) {
        fprintf(stderr, "register_allocator: join_path_state_resize function=%s old_capacity=%d new_capacity=%d state_count=%d status=out_of_memory\n",
            function_node->children[1]->label,
            path_state_storage.entry_capacity,
            resized_path_state_storage.entry_capacity, path_state_count);
        goto cleanup;
      }
      if (path_state_count > 0)
        memcpy(resized_path_state_entries, path_state_entries,
            (size_t)path_state_count *
            sizeof(struct register_allocator_join_path_state_entry));
      free(path_state_entries);
      path_state_entries = resized_path_state_entries;
      resized_path_state_entries = NULL;
      fprintf(stderr, "register_allocator: join_path_state_resize function=%s old_capacity=%d new_capacity=%d state_count=0 old_bytes=%lu new_bytes=%lu status=complete\n",
          function_node->children[1]->label,
          path_state_storage.entry_capacity,
          resized_path_state_storage.entry_capacity,
          (unsigned long)path_state_storage.entry_bytes,
          (unsigned long)resized_path_state_storage.entry_bytes);
      path_state_storage = resized_path_state_storage;
    }
    if (reconciliation_pass > 1 || added_temp_count > 0)
      fprintf(stderr, "register_allocator: post_promotion_reconciliation function=%s inserted=%d rewritten=%d added_temps=%d state_count=%d status=additional_mutation\n",
          function_node->children[1]->label, inserted_instruction_count,
          rewritten_operand_count, added_temp_count, path_state_count);
  } while (rebuild_required == YES);
  if (reconciliation_pass > 1)
    fprintf(stderr, "register_allocator: post_promotion_reconciliation function=%s inserted=0 rewritten=0 added_temps=0 state_count=%d status=complete\n",
        function_node->children[1]->label, path_state_count);
#endif

  if (_force_spill_at_basic_block_exits(function_node, blocks, block_count,
      path_state_entries, path_state_count) == FAILED)
    goto cleanup;
  fprintf(stderr, "register_allocator: join_path_state_scan function=%s capacity=%d count=%d ownership=function_scan consumption=enabled status=complete\n",
      function_node->children[1]->label, path_state_storage.entry_capacity,
      path_state_count);
  if (_run_basic_block_linear_scan(function_node, blocks, block_count,
      path_state_entries, path_state_count) == FAILED) {
    goto cleanup;
  }
  if (_place_split_spill_reloads(function_node, blocks, block_count,
      edges, edge_count) == FAILED) {
    goto cleanup;
  }

  status = SUCCEEDED;

cleanup:
  fprintf(stderr, "register_allocator: join_path_state_release function=%s capacity=%d count=%d bytes=%lu ownership=function_scan mode=observe_only status=%s\n",
      function_node->children[1]->label, path_state_storage.entry_capacity,
      path_state_count, (unsigned long)path_state_storage.entry_bytes,
      status == SUCCEEDED ? "complete" : "failed");
  free(path_state_entries);
  free(resized_path_state_entries);
  free(edges);
  free(blocks);

  return status;
}


static int _is_register_allocator_block_exit_spill_reason(int end_reason) {

  if (end_reason == RA_BLOCK_END_LABEL || end_reason == RA_BLOCK_END_JUMP || end_reason == RA_BLOCK_END_FUNCTION_END)
    return YES;

  return NO;
}


static int _force_spill_at_basic_block_exits(struct tree_node *function_node,
    struct register_allocator_basic_block *blocks, int block_count,
    struct register_allocator_join_path_state_entry *path_state_entries,
    int path_state_count) {

  int block_index;
  int temp_index;

  if (g_allocator_enabled == NO || function_node->local_variables == NULL)
    return SUCCEEDED;

  for (block_index = 0; block_index < block_count; block_index++) {
    if (_is_register_allocator_block_exit_spill_reason(blocks[block_index].end_reason) == NO)
      continue;

    for (temp_index = 0; temp_index < function_node->local_variables->temp_registers_count; temp_index++) {
      struct temp_register *temp_register = &(function_node->local_variables->temp_registers[temp_index]);

      if (temp_register->live_start <= blocks[block_index].end_tac && temp_register->live_end > blocks[block_index].end_tac) {
        struct register_allocator_block_exit_spill spill;

        if (register_allocator_plan_block_exit_spill(
            function_node->children[1]->label, block_count, g_tacs_count,
            _get_register_allocator_active_slot_count(), block_index,
            blocks[block_index].end_tac, temp_register->register_index,
            path_state_entries, path_state_count, &spill) == FAILED)
          return FAILED;
        fprintf(stderr, "register_allocator: block_exit_spill_decision function=%s block=%d temp=r%d end=%d entry=%d slot=%d spill=%s ownership=function_scan status=complete\n",
            function_node->children[1]->label, block_index,
            temp_register->register_index, blocks[block_index].end_tac,
            spill.reservation_entry, spill.slot_index,
            spill.status == RA_BLOCK_EXIT_SPILL_EXEMPT ? "exempt" :
            "required");
        if (spill.status == RA_BLOCK_EXIT_SPILL_EXEMPT)
          continue;
        temp_register->spill_required = YES;
        temp_register->physical_register = Z80_PHY_NONE;

        if (temp_register->spill_reason == Z80_SPILL_REASON_NONE ||
            temp_register->spill_reason == Z80_SPILL_REASON_LABEL) {
          int replaced_spill_reason;

          replaced_spill_reason = temp_register->spill_reason;
          temp_register->spill_reason = Z80_SPILL_REASON_BLOCK_EXIT;
          temp_register->spill_boundary_tac = blocks[block_index].end_tac;
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "register_allocator: block_exit spill function=%s block=%d r%d end_tac=%d(%s) reason=%s replaced=%s live=%d..%d\n", function_node->children[1]->label, block_index, temp_register->register_index, blocks[block_index].end_tac, _get_first_allocator_op_name(g_tacs[blocks[block_index].end_tac].op), _get_first_allocator_spill_reason_name(temp_register->spill_reason), replaced_spill_reason == Z80_SPILL_REASON_LABEL ? "label" : "none", temp_register->live_start, temp_register->live_end);
#endif
        }
      }
    }
  }

  return SUCCEEDED;
}


static int _get_hard_boundary_spill_reason(struct tac *t) {

  if (t->op == TAC_OP_FUNCTION_CALL || t->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE ||
      t->op == TAC_OP_RETURN || t->op == TAC_OP_RETURN_VALUE) {
    int spill_reason;

    spill_reason = _get_register_allocator_call_boundary_spill_reason(t->op);
#if defined(DEBUG_PASS_5)
    {
      struct register_allocator_target_policy *target_policy;

      target_policy = _get_register_allocator_target_policy();
      fprintf(stderr, "register_allocator: target_call_boundary function=%s tac=%d op=%s target=%s spill_reason=%s\n",
          t->function_node->children[1]->label, (int)(t - g_tacs), _get_first_allocator_op_name(t->op),
          target_policy != NULL ? target_policy->name : "none", _get_first_allocator_spill_reason_name(spill_reason));
    }
#endif
    return spill_reason;
  }
  if (t->op == TAC_OP_LABEL)
    return Z80_SPILL_REASON_LABEL;
  if (t->op == TAC_OP_ASM)
    return Z80_SPILL_REASON_INLINE_ASM;
  if (_is_register_allocator_unmigrated_emitter(t->op) == YES)
    return Z80_SPILL_REASON_UNMIGRATED_EMITTER;

  return Z80_SPILL_REASON_NONE;
}


static void _force_spill_at_hard_boundaries(struct tree_node *function_node) {

  int i, j;

  if (g_allocator_enabled == NO || function_node->local_variables == NULL)
    return;

  if (strcmp(function_node->children[1]->label, "mainmain") == 0) {
    for (j = 0; j < function_node->local_variables->temp_registers_count; j++) {
      struct temp_register *temp_register = &(function_node->local_variables->temp_registers[j]);

      temp_register->spill_required = YES;
      temp_register->physical_register = Z80_PHY_NONE;
      if (temp_register->spill_reason == Z80_SPILL_REASON_NONE) {
        temp_register->spill_reason = Z80_SPILL_REASON_MAINMAIN;
        temp_register->spill_boundary_tac = -1;
#if defined(DEBUG_PASS_5)
        fprintf(stderr, "register_allocator: spill function=%s r%d reason=%s boundary_tac=-1\n", function_node->children[1]->label, temp_register->register_index, _get_first_allocator_spill_reason_name(temp_register->spill_reason));
#endif
      }
    }

    return;
  }

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *boundary = &g_tacs[i];
    int spill_reason;

    if (boundary->function_node != function_node)
      continue;

    spill_reason = _get_hard_boundary_spill_reason(boundary);
    if (spill_reason == Z80_SPILL_REASON_NONE)
      continue;

    for (j = 0; j < function_node->local_variables->temp_registers_count; j++) {
      struct temp_register *temp_register = &(function_node->local_variables->temp_registers[j]);

      if (temp_register->live_start < i && temp_register->live_end >= i) {
        if (boundary->op == TAC_OP_RETURN_VALUE && temp_register->live_end == i &&
            boundary->arg1_type == TAC_ARG_TYPE_TEMP && (int)boundary->arg1_d == temp_register->register_index) {
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "register_allocator: return_boundary_consumer function=%s r%d boundary_tac=%d(RETURN_VALUE) action=defer_to_candidate_policy\n",
                  function_node->children[1]->label, temp_register->register_index, i);
#endif
          continue;
        }

        temp_register->spill_required = YES;
        temp_register->physical_register = Z80_PHY_NONE;

        if (temp_register->spill_reason == Z80_SPILL_REASON_NONE) {
          temp_register->spill_reason = spill_reason;
          temp_register->spill_boundary_tac = i;
#if defined(DEBUG_PASS_5)
          fprintf(stderr, "register_allocator: spill function=%s r%d reason=%s boundary_tac=%d(%s) live=%d..%d\n", function_node->children[1]->label, temp_register->register_index, _get_first_allocator_spill_reason_name(spill_reason), i, _get_first_allocator_op_name(boundary->op), temp_register->live_start, temp_register->live_end);
#endif
        }
      }
    }
  }
}


static int collect_local_variables_and_temps(struct tree_node *function_node, int *tac_index, int *local_variables_count, int *arguments_count, int *used_registers) {

  int i;

  *local_variables_count = 0;
  *arguments_count = 0;
  *used_registers = 0;

  if (_reset_register_frame_info(function_node->children[1]->label) == FAILED)
    return FAILED;

  for (i = *tac_index + 1; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES) {
      i--;
      break;
    }
    else if (op == TAC_OP_CREATE_VARIABLE) {
      if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
        char local_label[MAX_NAME_LENGTH + 1];

        snprintf(local_label, sizeof(local_label), "_%s_%d", t->result_node->children[1]->label, g_local_variable_running_index++);
        strcpy(t->result_node->children[1]->label, local_label);
      }

      if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE &&
          ((t->result_node->children[0]->value_double == 0 && (t->result_node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
           (t->result_node->children[0]->value_double > 0 && (t->result_node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2))) {
      }
      else {
        if (*local_variables_count >= LOCAL_VAR_COUNT) {
          fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Out of space! Make variables array bigger and recompile!\n");
          return FAILED;
        }

        if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
          (*arguments_count)++;

        g_local_variables[(*local_variables_count)++] = t->result_node;
      }
    }
    else if (op == TAC_OP_DEAD) {
    }
    else {
      if (op == TAC_OP_FUNCTION_CALL || op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
        int argument_index;

        for (argument_index = 0; argument_index < t->arguments_count; argument_index++) {
          if (t->arguments[argument_index].type == TAC_ARG_TYPE_TEMP) {
            int index = (int)t->arguments[argument_index].value;
            int size = get_variable_type_size(t->arguments[argument_index].var_type);

            if (size == 0)
              fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Register r%d size is 0!\n", index);

            if (_note_register_frame_usage(function_node->children[1]->label, index, size, i, NO) == FAILED)
              return FAILED;
          }
        }
      }

      if (t->arg1_type == TAC_ARG_TYPE_TEMP) {
        int index = (int)t->arg1_d;
        int size = get_variable_type_size(t->arg1_var_type);

        if (size == 0)
          fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Register r%d size is 0!\n", index);

        if (_note_register_frame_usage(function_node->children[1]->label, index, size, i, NO) == FAILED)
          return FAILED;
      }
      if (t->arg2_type == TAC_ARG_TYPE_TEMP) {
        int index = (int)t->arg2_d;
        int size = get_variable_type_size(t->arg2_var_type);

        if (size == 0)
          fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Register r%d size is 0!\n", index);

        if (_note_register_frame_usage(function_node->children[1]->label, index, size, i, NO) == FAILED)
          return FAILED;
      }
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int index = (int)t->result_d;
        int size = get_variable_type_size(t->result_var_type);

        if (size == 0)
          fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Register r%d size is 0!\n", index);

        if (_note_register_frame_usage(function_node->children[1]->label, index, size, i, t->op == TAC_OP_ARRAY_WRITE ? NO : YES) == FAILED)
          return FAILED;
      }
    }

    t->function_node = function_node;
  }

  *used_registers = register_allocator_count_live_intervals(function_node->children[1]->label, g_register_live_intervals, REG_COUNT);
  if (*used_registers < 0)
    return FAILED;

  *tac_index = i;

  return SUCCEEDED;
}


static int _allocate_local_variable_metadata(struct tree_node *function_node, int local_variables_count, int arguments_count, int used_registers) {

  function_node->local_variables = (struct local_variables *)calloc(sizeof(struct local_variables), 1);
  if (function_node->local_variables == NULL) {
    fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Out of memory error.\n");
    return FAILED;
  }

  function_node->local_variables->arguments_count = 0;
  function_node->local_variables->local_variables_count = 0;
  function_node->local_variables->local_variables = NULL;
  function_node->local_variables->temp_registers_count = 0;
  function_node->local_variables->temp_registers = NULL;
  function_node->local_variables->offset_to_fp_total = 0;

  if (local_variables_count > 0) {
    function_node->local_variables->local_variables = (struct local_variable *)calloc(sizeof(struct local_variable) * local_variables_count, 1);
    if (function_node->local_variables->local_variables == NULL) {
      fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Out of memory error.\n");
      return FAILED;
    }
  }
  function_node->local_variables->local_variables_count = local_variables_count;
  function_node->local_variables->arguments_count = arguments_count;

  if (used_registers > 0) {
    function_node->local_variables->temp_registers = (struct temp_register *)calloc(sizeof(struct temp_register) * used_registers, 1);
    if (function_node->local_variables->temp_registers == NULL) {
      fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Out of memory error.\n");
      return FAILED;
    }
  }
  function_node->local_variables->temp_registers_count = used_registers;

  return SUCCEEDED;
}


static int assign_local_variable_offsets(struct tree_node *function_node, int local_variables_count, int *offset) {

  int j;

  for (j = 0; j < local_variables_count; j++) {
    int size = _get_variable_size(g_local_variables[j]);
    int size_bytes;

    if (size < 0)
      return FAILED;

    size_bytes = size / 8;
    function_node->local_variables->local_variables[j].node = g_local_variables[j];
    if (j < function_node->local_variables->arguments_count) {
      int argument_end_offset;

      argument_end_offset = _get_register_allocator_stack_argument_end_offset(*offset, size_bytes, j);
      if (argument_end_offset >= *offset) {
        fprintf(stderr, "assign_local_variable_offsets(): Target policy cannot place argument %d (%d bytes) for function %s! Please submit a bug report!\n", j + 1, size_bytes, function_node->children[1]->label);
        return FAILED;
      }
      function_node->local_variables->local_variables[j].offset_to_fp = argument_end_offset + 1;
      *offset = argument_end_offset;

#if defined(DEBUG_PASS_5)
      if (g_allocator_enabled == YES)
        fprintf(stderr, "register_allocator: target_argument_layout function=%s argument=%d size=%d target=%s slot_start=%d frame_end=%d\n",
            function_node->children[1]->label, j + 1, size_bytes,
            _get_register_allocator_target_policy()->name,
            function_node->local_variables->local_variables[j].offset_to_fp, *offset);
#endif
    }
    else {
      function_node->local_variables->local_variables[j].offset_to_fp = *offset - (size_bytes - 1);
      *offset -= size_bytes;
    }
    function_node->local_variables->local_variables[j].size = size;

#if defined(DEBUG_PASS_5)
    fprintf(stderr, "OFFSET %.5d SIZE %.6d VARIABLE %s\n", function_node->local_variables->local_variables[j].offset_to_fp, size_bytes, g_local_variables[j]->children[1]->label);
#endif
  }

  return SUCCEEDED;
}


static void _initialize_temp_register_metadata(struct tree_node *function_node, int used_registers) {

  int j, k;

  k = 0;
  for (j = 0; j < used_registers; j++) {
    while (k < REG_COUNT) {
      if (g_register_live_intervals[k].used == YES)
        break;
      k++;
    }

    function_node->local_variables->temp_registers[j].offset_to_fp = REGISTER_ALLOCATOR_POISON_OFFSET;
    function_node->local_variables->temp_registers[j].size = g_register_live_intervals[k].size;
    function_node->local_variables->temp_registers[j].register_index = k;
    function_node->local_variables->temp_registers[j].spill_required = YES;
    function_node->local_variables->temp_registers[j].physical_register = Z80_PHY_NONE;
    function_node->local_variables->temp_registers[j].original_register_index = _find_original_register_index_for_temp(function_node, k);
    function_node->local_variables->temp_registers[j].live_start = g_register_live_intervals[k].live_start;
    function_node->local_variables->temp_registers[j].live_end = g_register_live_intervals[k].live_end;
    function_node->local_variables->temp_registers[j].read_count = g_register_live_intervals[k].read_count;
    function_node->local_variables->temp_registers[j].write_count = g_register_live_intervals[k].write_count;
    function_node->local_variables->temp_registers[j].spill_reason = Z80_SPILL_REASON_NONE;
    function_node->local_variables->temp_registers[j].spill_boundary_tac = -1;

    k++;
  }
}


static int run_register_allocator(struct tree_node *function_node) {

  if (g_allocator_enabled == YES && g_allocator_force_all_spill == YES) {
#if defined(DEBUG_PASS_5)
    fprintf(stderr, "register_allocator: forced_all_spill function=%s temps=%d\n", function_node->children[1]->label, function_node->local_variables == NULL ? 0 : function_node->local_variables->temp_registers_count);
#endif
    return SUCCEEDED;
  }

  _force_spill_at_hard_boundaries(function_node);
  if (_scan_register_allocator_basic_blocks(function_node) == FAILED)
    return FAILED;

  return SUCCEEDED;
}


static int assign_spill_slot_offsets(struct tree_node *function_node, int *offset) {

  int j;

  for (j = 0; j < function_node->local_variables->temp_registers_count; j++) {
    struct temp_register *temp_register = &(function_node->local_variables->temp_registers[j]);
    int temp_size = temp_register->size / 8;

    if (temp_register->spill_required == YES) {
      temp_register->offset_to_fp = *offset - (temp_size - 1);

#if defined(DEBUG_PASS_5)
      fprintf(stderr, "OFFSET %.5d SIZE %.6d REGISTER r%d\n", temp_register->offset_to_fp, temp_size, temp_register->register_index);
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: spill slot function=%s r%d offset=%d size=%d\n", function_node->children[1]->label, temp_register->register_index, temp_register->offset_to_fp, temp_size);
#endif

      *offset -= temp_size;
    }
    else {
      temp_register->offset_to_fp = REGISTER_ALLOCATOR_POISON_OFFSET;

#if defined(DEBUG_PASS_5)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: no spill slot function=%s r%d phy=%s poison_offset=%d\n", function_node->children[1]->label, temp_register->register_index, _get_first_allocator_physical_register_name(temp_register->physical_register), temp_register->offset_to_fp);
#endif
    }
  }

  return SUCCEEDED;
}


int collect_and_preprocess_local_variables_inside_functions(void) {

  int i;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES && ((t->function_node->flags & TREE_NODE_FLAG_PUREASM) == 0) && strcmp("mainmain", t->function_node->children[1]->label) != 0) {
      /* function start! */
      struct tree_node *function_node = t->function_node;
      int local_variables_count = 0, used_registers = 0, arguments_count = 0;
      int offset;
      int frame_prefix_end, return_sp_offset, saved_fp_low_offset, saved_fp_high_offset;
      int return_value_type, return_value_size, return_value_end_offset;

      frame_prefix_end = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_PREFIX_END);
      return_sp_offset = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_RETURN_SP_OFFSET);
      saved_fp_low_offset = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_SAVED_FP_LOW_OFFSET);
      saved_fp_high_offset = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_SAVED_FP_HIGH_OFFSET);
      if (frame_prefix_end >= 0 || return_sp_offset != -1 || saved_fp_low_offset >= saved_fp_high_offset ||
          saved_fp_high_offset >= return_sp_offset) {
        fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Target policy has an invalid call-frame prefix for function %s! Please submit a bug report!\n", function_node->children[1]->label);
        return FAILED;
      }
      offset = frame_prefix_end;

    #if defined(DEBUG_PASS_5)
      if (g_allocator_enabled == YES)
        fprintf(stderr, "register_allocator: target_call_frame_prefix function=%s role=layout target=%s frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
        function_node->children[1]->label, _get_register_allocator_target_policy()->name,
        frame_prefix_end, return_sp_offset, saved_fp_low_offset, saved_fp_high_offset);
    #endif

      return_value_type = tree_node_get_max_var_type(function_node->children[0]);
      return_value_size = get_variable_type_size(return_value_type) / 8;
      return_value_end_offset = _get_register_allocator_stack_return_value_end_offset(return_value_size);
      if (return_value_end_offset > frame_prefix_end) {
        fprintf(stderr, "collect_and_preprocess_local_variables_inside_functions(): Target policy cannot reserve a %d-byte stack return value for function %s! Please submit a bug report!\n", return_value_size, function_node->children[1]->label);
        return FAILED;
      }
      offset = return_value_end_offset;

    #if defined(DEBUG_PASS_5)
          if (g_allocator_enabled == YES)
            fprintf(stderr, "register_allocator: target_return_transport function=%s size=%d target=%s slot_start=%d frame_end=%d\n",
                function_node->children[1]->label, return_value_size,
                _get_register_allocator_target_policy()->name,
                return_value_size > 0 ? return_value_end_offset + 1 : 0, return_value_end_offset);
    #endif

      if (collect_local_variables_and_temps(function_node, &i, &local_variables_count, &arguments_count, &used_registers) == FAILED)
        return FAILED;

      if (_allocate_local_variable_metadata(function_node, local_variables_count, arguments_count, used_registers) == FAILED)
        return FAILED;

#if defined(DEBUG_PASS_5)
      fprintf(stderr, "*** STACK FRAME of FUNCTION \"%s\"\n", function_node->children[1]->label);
      fprintf(stderr, "OFFSET %.5d SIZE %.6d \"return address\"\n", return_sp_offset, 2);
      fprintf(stderr, "OFFSET %.5d SIZE %.6d \"caller's stack frame address\"\n", saved_fp_low_offset, 2);
      if (return_value_size > 0)
        fprintf(stderr, "OFFSET %.5d SIZE %.6d \"return value\"\n", -4 - (return_value_size - 1), return_value_size);
#endif

      if (assign_local_variable_offsets(function_node, local_variables_count, &offset) == FAILED)
        return FAILED;

      _initialize_temp_register_metadata(function_node, used_registers);
      if (run_register_allocator(function_node) == FAILED)
        return FAILED;

      if (assign_spill_slot_offsets(function_node, &offset) == FAILED)
        return FAILED;

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
    if (t->op == TAC_OP_ADD && t->arg1_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg1_d) == 1) {
      tac_swap_args(t);
#if defined(DEBUG_PASS_5)
      fprintf(stderr, "register_allocator: optimize_for_inc swapped TAC %d so constant increment is arg2\n", i);
#endif
    }
  }

  return SUCCEEDED;
}


int turn_some_muls_and_divs_into_shifts(void) {

  int i, shift, multiplier;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];

    shift = 1;
    multiplier = 2;

    while (shift < 8) {
      if (t->op == TAC_OP_MUL && t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == multiplier) {
        t->op = TAC_OP_SHIFT_LEFT;
        t->arg2_d = shift;
      }
      if (t->op == TAC_OP_DIV && t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == multiplier) {
        t->op = TAC_OP_SHIFT_RIGHT;
        t->arg2_d = shift;
      }

      shift++;
      multiplier *= 2;
    }
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
      if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
        continue;

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

  /* 1. collect fully initialized non-const non-extern variables */
  index = 0;
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];

    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
        continue;

      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        if (node->value == 0) {
          /* not an array */
          if (node->added_children > 2)
            nodes[index++] = node;
        }
        else {
          /* an array */
          /* NOTE: when dealing with struct/unions, if one is initialized, it's fully initialized and
             the node actually contains the data as children (and symbols used to write that
             initialization) thus the number of data items will be at least as large as the
             number of items in the array */
          /*
          fprintf(stderr, "GLOBAL ARRAY %s\n", node->children[1]->label);
          fprintf(stderr, "  %d %d\n", tree_node_get_create_variable_data_items(node), node->value);
          fprintf(stderr, "  items in array %d\n", (int)node->value_double);
          */

          if (tree_node_get_create_variable_data_items(node) >= node->value)
            nodes[index++] = node;
        }
      }
    }
  }

  /* 2. collect partially initialized non-const non-extern variables */
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];

    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
        continue;

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

  /* 3. collect uninitialized and const non-extern variables */
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];

    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
        continue;

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
