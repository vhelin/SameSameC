
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
  if (propagate_operand_sizes() == FAILED)
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
  print_tac(&g_tacs[j]);

  j++;
  
  while (j < g_tacs_count) {
    if (_is_il_block_end(&g_tacs[j]) == YES) {
      fprintf(stderr, "%.3d: ", j);
      print_tac(&g_tacs[j]);
      return j;
    }
    j++;
  }

  *is_last_block = YES;

  fprintf(stderr, "%.3d: ", j);
  print_tac(&g_tacs[j]);
  
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

  fprintf(stderr, "FUNCTION %s %d\n%.3d: ", g_tacs[j].result_s, g_tacs[j].is_function_start, j);
  print_tac(&g_tacs[j]);

  j++;
  
  while (j < g_tacs_count) {
    if (g_tacs[j].op == TAC_OP_LABEL && g_tacs[j].is_function_start == YES) {
      fprintf(stderr, "%.3d: ", j);
      print_tac(&g_tacs[j]);
      return j;
    }
    j++;
  }

  *is_last_function = YES;

  fprintf(stderr, "%.3d: ", j-1);
  print_tac(&g_tacs[j-1]);
  
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


int optimize_il(void) {

  int i;
  
  /*
    jmp label_* <----- REMOVE
    label_*:
  */

  i = 0;
  while (i < g_tacs_count) {
    int current, next;

    current = _find_next_living_tac(i);
    next = _find_next_living_tac(current + 1);

    if (current < 0 || next < 0)
      break;
    
    if (g_tacs[current].op >= TAC_OP_JUMP && g_tacs[current].op <= TAC_OP_JUMP_GTE && g_tacs[next].op == TAC_OP_LABEL &&
        strcmp(g_tacs[current].result_s, g_tacs[next].result_s) == 0) {
      g_tacs[current].op = TAC_OP_DEAD;
    }

    i = next;
  }

  /*
    rX = ?  <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  i = 0;
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
        g_tacs[current].op = TAC_OP_DEAD;
      }
          
      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  /*
    rX = ?    <--- REMOVE rX if rX doesn't appear elsewhere
    return rX <--- REMOVE rX if rX doesn't appear elsewhere
  */

  i = 0;
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
        g_tacs[current].op = TAC_OP_DEAD;
      }
          
      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  /*
    rX = rA * rB <--- REMOVE rX if rX doesn't appear elsewhere
     ? = rX      <--- REMOVE rX if rX doesn't appear elsewhere
  */

  i = 0;
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
        g_tacs[next].op = TAC_OP_DEAD;
      }
          
      i = next;
    }

    if (is_last_function == YES)
      break;
  }

  /*
    rX = function_call() <--- REMOVE rX if rX doesn't appear elsewhere
    ?  = rX              <--- REMOVE rX if rX doesn't appear elsewhere
  */

  i = 0;
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
        g_tacs[next].op = TAC_OP_DEAD;
      }
          
      i = next;
    }

    if (is_last_function == YES)
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


static char *g_temp_register_sizes = NULL;
static int g_temp_register_sizes_count = 0;


static int _set_temp_register_size(int r, int size) {

  while (r >= g_temp_register_sizes_count) {
    int i;
    
    g_temp_register_sizes = realloc(g_temp_register_sizes, g_temp_register_sizes_count + 256);
    if (g_temp_register_sizes == NULL) {
      fprintf(stderr, "_set_temp_register_size(): Out of memory error.\n");
      return FAILED;
    }

    for (i = g_temp_register_sizes_count; i < g_temp_register_sizes_count + 256; i++)
      g_temp_register_sizes[i] = 0;
    
    g_temp_register_sizes_count += 256;
  }

  g_temp_register_sizes[r] = size;

  return SUCCEEDED;
}


static int _get_temp_register_size(int r) {

  if (r >= g_temp_register_sizes_count)
    return -1;

  return g_temp_register_sizes[r];
}


static void _clear_temp_register_sizes(void) {

  int i;

  for (i = 0; i < g_temp_register_sizes_count; i++)
    g_temp_register_sizes[i] = 0;
}


static int _find_operand_size(char *size, unsigned char type, int value, char *label, struct tree_node *node, int allow_register_errors) {

  if (type == TAC_ARG_TYPE_CONSTANT) {
    if (value < -128 || value > 127)
      *size = 16;
    else
      *size = 8;
  }
  else if (type == TAC_ARG_TYPE_TEMP) {
    *size = _get_temp_register_size(value);

    if (*size < 0) {
      if (allow_register_errors == YES)
        *size = 0;
      else {
        fprintf(stderr, "_find_operand_size(): Register %d has no size! Please submit a bug report!\n", value);
        return FAILED;
      }
    }
  }
  else if (type == TAC_ARG_TYPE_LABEL) {
    int variable_type;
    
    if (node == NULL) {
      fprintf(stderr, "_find_operand_size(): We are missing a node in a TAC for \"%s\"! Internal error. Please submit a bug report!\n", label);
      return FAILED;
    }

    variable_type = node->children[0]->value;

    /* int8 and not a pointer? */
    if (variable_type == VARIABLE_TYPE_INT8 && node->children[0]->value_double == 0.0)
      *size = 8;
    /* int8 and a pointer? */
    else if (variable_type == VARIABLE_TYPE_INT8 && node->children[0]->value_double >= 1.0)
      *size = 16;
    /* int16 and not a pointer? */
    else if (variable_type == VARIABLE_TYPE_INT16 && node->children[0]->value_double == 0.0)
      *size = 16;
    /* int16 and a pointer? */
    else if (variable_type == VARIABLE_TYPE_INT16 && node->children[0]->value_double >= 1.0)
      *size = 16;
    /* void and a pointer? */
    else if (variable_type == VARIABLE_TYPE_VOID && node->children[0]->value_double >= 1.0)
      *size = 16;
    else {
      fprintf(stderr, "_find_operand_size(): Variable \"%s\" is of type %d -> It doesn't have a size!\n", node->children[1]->label, variable_type);
      return FAILED;
    }
  }
  
  return SUCCEEDED;
}


static int _get_set_max_registers_size(struct tac *t) {

  int j, args, size_max = 8;

  /* the number of arguments */
  args = (int)t->arg2_d;

  t->registers_sizes = (int *)calloc(sizeof(int) * args, 1);
  if (t->registers_sizes == NULL) {
    fprintf(stderr, "_get_set_max_register_size(): Out of memory error.\n");
    return -1;
  }
  
  for (j = 0; j < args; j++) {
    int size = _get_temp_register_size(t->registers[j]);

    if (size <= 0) {
      fprintf(stderr, "_get_set_max_register_size(): A function argument size cannot be found!\n");
      return -1;
    }

    t->registers_sizes[j] = size;

    if (size > size_max)
      size_max = size;
  }

  return size_max;
}


int propagate_operand_sizes(void) {
  
  int i;
  
  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_DEAD) {
    }
    else if (op == TAC_OP_LABEL) {
      /* function start? */
      if (t->is_function_start == YES)
        _clear_temp_register_sizes();
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
      if (_find_operand_size(&t->arg1_size, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->arg2_size, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->result_size, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1 and ARG2? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int size_max = 8;
        
        if (t->arg1_size == 16 || t->arg2_size == 16)
          size_max = 16;

        if (t->result_size == 0) {
          /* get the size from the operands */
          t->result_size = size_max;
        }

        _set_temp_register_size((int)t->result_d, t->result_size);
      }

      if (t->arg1_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG1!\n");
      if (t->arg2_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG2!\n");
      if (t->result_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for RESULT!\n");
    }
    else if (op == TAC_OP_ASSIGNMENT) {
      if (_find_operand_size(&t->arg1_size, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->result_size, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int size_max = 8;
        
        if (t->arg1_size == 16)
          size_max = 16;

        if (t->result_size == 0) {
          /* get the size from ARG1 */
          t->result_size = size_max;
        }

        _set_temp_register_size((int)t->result_d, t->result_size);
      }

      if (t->arg1_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG1!\n");
      if (t->result_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for RESULT!\n");
    }
    else if (op == TAC_OP_ARRAY_ASSIGNMENT) {
      if (_find_operand_size(&t->arg1_size, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->arg2_size, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->result_size, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      if (t->arg1_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG1!\n");
      if (t->arg2_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG2!\n");
      if (t->result_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for RESULT!\n");
    }
    else if (op == TAC_OP_ARRAY_READ) {
      if (_find_operand_size(&t->arg1_size, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->arg2_size, t->arg2_type, (int)t->arg2_d, t->arg2_s, t->arg2_node, NO) == FAILED)
        return FAILED;
      if (_find_operand_size(&t->result_size, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      /* RESULT from ARG1 and ARG2? */
      if (t->result_type == TAC_ARG_TYPE_TEMP) {
        int size_max = 8;
        
        if (t->arg1_size == 16 || t->arg2_size == 16)
          size_max = 16;

        if (t->result_size == 0) {
          /* get the size from the operands */
          t->result_size = size_max;
        }

        _set_temp_register_size((int)t->result_d, t->result_size);
      }

      if (t->arg1_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG1!\n");
      if (t->arg2_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG2!\n");
      if (t->result_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for RESULT!\n");
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
      if (_find_operand_size(&t->arg1_size, t->arg1_type, (int)t->arg1_d, t->arg1_s, t->arg1_node, NO) == FAILED)
        return FAILED;

      if (t->arg1_size == 0)
        fprintf(stderr, "propagate_operand_sizes(): Couldn't find size for ARG1!\n");
    }
    else if (op == TAC_OP_FUNCTION_CALL) {
      int size_max = _get_set_max_registers_size(t);

      if (size_max < 0)
        return FAILED;
    }
    else if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
      int size_max = _get_set_max_registers_size(t);

      if (size_max < 0)
        return FAILED;

      if (_find_operand_size(&t->result_size, t->result_type, (int)t->result_d, t->result_s, t->result_node, YES) == FAILED)
        return FAILED;

      if (t->result_size <= 0)
        t->result_size = size_max;
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
