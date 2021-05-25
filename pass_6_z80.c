
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
#include "pass_6_z80.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

static struct cpu_z80 g_cpu_z80;


static void _reset_cpu_z80(void) {

  g_cpu_z80.a_status = REGISTER_STATUS_UNKNOWN;
  g_cpu_z80.bc_status = REGISTER_STATUS_UNKNOWN;
  g_cpu_z80.de_status = REGISTER_STATUS_UNKNOWN;
  g_cpu_z80.hl_status = REGISTER_STATUS_UNKNOWN;
}


int pass_6_z80(FILE *file_out) {

  if (g_verbose_mode == ON)
    printf("Pass 6 (Z80)...\n");

  _reset_cpu_z80();
  
  if (generate_asm_z80(file_out) == FAILED)
    return FAILED;

  return SUCCEEDED;
}


int find_stack_offset(int type, char *name, int value, struct tree_node *node, int *offset, struct tree_node *function_node) {

  if (type == TAC_ARG_TYPE_LABEL) {
    struct local_variables *local_variables = function_node->local_variables;
    int i;

    for (i = 0; i < local_variables->local_variables_count; i++) {
      if (local_variables->local_variables[i].node == node) {
        *offset = local_variables->local_variables[i].offset_to_fp;
        
        return SUCCEEDED;
      }
    }

    /* it must be a global var */
    *offset = 999999;

    return SUCCEEDED;
  }
  else if (type == TAC_ARG_TYPE_TEMP) {
    struct local_variables *local_variables = function_node->local_variables;
    int i;

    for (i = 0; i < local_variables->temp_registers_count; i++) {
      if (local_variables->temp_registers[i].register_index == value) {
        *offset = local_variables->temp_registers[i].offset_to_fp;

        return SUCCEEDED;
      }
    }

    fprintf(stderr, "find_stack_offset_and_size(): Cannot find information about register %d! Please submit a bug report.\n", value);

    return FAILED;
  }
  else if (type == TAC_ARG_TYPE_CONSTANT) {
    *offset = -999999;

    return SUCCEEDED;
  }

  fprintf(stderr, "find_stack_offset_and_size(): Unknown type %d! Please submit a bug report!\n", type);

  return FAILED;
}


static void _load_value_to_hl(int value, FILE *file_out) {

  fprintf(file_out, "      ld hl, %d\n", value);
}


static void _load_label_to_hl(char *label, FILE *file_out) {

  fprintf(file_out, "      ld hl, %s\n", label);
}


static void _load_de_to_hl(FILE *file_out) {

  fprintf(file_out, "      ld h, d\n");
  fprintf(file_out, "      ld l, e\n");
}


static void _load_value_to_bc(int value, FILE *file_out) {

  fprintf(file_out, "      ld bc, %d\n", value);
}


static void _add_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      add hl, bc\n");
}


static int _generate_asm_assignment_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, result_size = 0, arg1_offset = -1, arg1_size = 0;

  fprintf(stderr, "EH %s %d\n", t->result_s, t->result_size / 8);

  /* result */

  result_size = t->result_size / 8;
  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;

  /* arg1 */

  arg1_size = t->arg1_size / 8;
  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;

  /* generate asm */

  /* target address -> hl */
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    _load_value_to_hl((int)t->result_d, file_out);
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_hl(t->result_s, file_out);
  }
  else {
    /* it's in the frame! */
    fprintf(file_out, "      // offset %d\n", result_offset);
            
    _load_de_to_hl(file_out);
    _load_value_to_bc(result_offset, file_out);
    _add_bc_to_hl(file_out);
  }

  /* data -> b/bc */


  
  return SUCCEEDED;
}


int generate_asm_z80(FILE *file_out) {

  int i;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES) {
      /* function start! */
      struct tree_node *function_node = t->function_node;

      fprintf(file_out, "  .SECTION \"%s\" FREE\n", function_node->children[1]->label);

      fprintf(file_out, "    %s:\n", function_node->children[1]->label);
      
      for (i = i + 1; i < g_tacs_count; i++) {
        t = &g_tacs[i];
        op = t->op;

        /* IL -> ASM */

        if (op == TAC_OP_LABEL && t->is_function == YES) {
          _reset_cpu_z80();
          i--;
          break;
        }

        print_tac(t, YES, file_out);
        
        if (op == TAC_OP_DEAD) {
        }
        else if (op == TAC_OP_LABEL && t->is_function == NO) {
          _reset_cpu_z80();
        }
        else if (op == TAC_OP_LABEL)
          fprintf(file_out, "    %s:\n", t->result_s);
        else if (op == TAC_OP_ASSIGNMENT) {
          if (_generate_asm_assignment_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_ADD) {

        }
      }
      
      fprintf(file_out, "  .ENDS\n");
    }
  }
  
  return SUCCEEDED;
}
