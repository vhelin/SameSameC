
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


int pass_5(void) {

  if (g_verbose_mode == ON)
    printf("Pass 5...\n");

  if (optimize_il() == FAILED)
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
  
  return SUCCEEDED;
}
