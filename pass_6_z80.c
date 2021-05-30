
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


static void _load_value_to_ix(int value, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%d\n", value);
}


static void _load_value_to_iy(int value, FILE *file_out) {

  fprintf(file_out, "      LD  IY,%d\n", value);
}


static void _load_label_to_ix(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%s\n", label);
}


static void _load_label_to_iy(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  IY,%s\n", label);
}


static void _load_value_into_ix(int value, int offset, FILE *file_out) {

  fprintf(file_out, "      LD  (IX+%d),%d\n", offset, value);
}


static void _load_from_iy_to_a(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  A,(IY+%d)\n", offset);
}


static void _load_from_iy_to_b(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  B,(IY+%d)\n", offset);
}


static void _load_from_iy_to_c(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  C,(IY+%d)\n", offset);
}


static void _load_a_into_ix(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  (IX+%d),A\n", offset);
}


static void _load_b_into_ix(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  (IX+%d),B\n", offset);
}


static void _load_c_into_ix(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  (IX+%d),C\n", offset);
}


static void _add_de_to_ix(FILE *file_out) {

  fprintf(file_out, "      ADD IX,DE\n");
}


static void _add_de_to_iy(FILE *file_out) {

  fprintf(file_out, "      ADD IY,DE\n");
}


static void _add_bc_to_ix(FILE *file_out) {

  fprintf(file_out, "      ADD IX,BC\n");
}


static void _add_bc_to_iy(FILE *file_out) {

  fprintf(file_out, "      ADD IY,BC\n");
}


static void _load_value_to_bc(int value, FILE *file_out) {

  fprintf(file_out, "      LD  BC,%d\n", value);
}


static void _load_value_to_b(int value, FILE *file_out) {

  fprintf(file_out, "      LD  B,%d\n", value);
}


static void _load_value_to_c(int value, FILE *file_out) {

  fprintf(file_out, "      LD  C,%d\n", value);
}


static void _sign_extend_a_to_bc(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit A -> 16-bit BC\n");
  fprintf(file_out, "      LD  C,A\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  B,A  ; now BC is sign extended A\n");
}


static void _sign_extend_c_to_bc(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit C -> 16-bit BC\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  B,A  ; now BC is sign extended A\n");
}


static int _generate_asm_assignment_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, result_size = 0, arg1_offset = -1, arg1_size = 0;
  int result_var_type, arg1_var_type;

  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  result_size = t->result_size;
  if (t->result_node != NULL)
    result_var_type = t->result_node->children[0]->value;
  else
    result_var_type = VARIABLE_TYPE_NONE;
  
  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;
  arg1_size = t->arg1_size;
  if (t->arg1_node != NULL)
    arg1_var_type = t->arg1_node->children[0]->value;
  else
    arg1_var_type = VARIABLE_TYPE_NONE;
  
  /* generate asm */

  /* target address -> ix */
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_assignment_z80(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    _load_value_to_ix(0, file_out);
    _add_de_to_ix(file_out);
    _load_value_to_bc(result_offset, file_out);
    _add_bc_to_ix(file_out);
  }

  /* source address -> iy */

  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->arg1_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    _load_value_to_iy(0, file_out);
    _add_de_to_iy(file_out);
    _load_value_to_bc(arg1_offset, file_out);
    _add_bc_to_iy(file_out);
  }
  
  /* copy data */

  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    int value = (int)t->arg1_d;
    if (result_size == 16) {
      /* 16-bit */
      _load_value_into_ix(value & 0xff, 0, file_out);
      _load_value_into_ix((value >> 8) & 0xff, 1, file_out);
    }
    else {
      /* 8-bit */
      _load_value_into_ix(value & 0xff, 0, file_out);
    }
  }
  else {
    if (result_size == 16) {
      /* 16-bit */
      if (arg1_size == 16) {
        _load_from_iy_to_a(0, file_out);
        _load_a_into_ix(0, file_out);
        _load_from_iy_to_a(1, file_out);
        _load_a_into_ix(1, file_out);
      }
      else {
        /* sign extend 8-bit -> 16-bit? */
        if (result_var_type == VARIABLE_TYPE_INT16 && arg1_var_type == VARIABLE_TYPE_INT8) {
          /* lower byte must be sign extended */
          _load_from_iy_to_a(0, file_out);
          _sign_extend_a_to_bc(file_out);
          _load_c_into_ix(0, file_out);
          _load_b_into_ix(1, file_out);
        }
        else {
          /* lower byte */
          _load_from_iy_to_a(0, file_out);
          _load_a_into_ix(0, file_out);
          /* upper byte is 0 */
          _load_value_into_ix(0, 1, file_out);
        }
      }
    }
    else {
      /* 8-bit */
      _load_from_iy_to_a(0, file_out);
      _load_a_into_ix(0, file_out);
    }
  }
  
  return SUCCEEDED;
}


static int _get_var_type(struct tree_node *node) {

  if (node != NULL)
    return (int)node->children[0]->value;
  else
    return VARIABLE_TYPE_NONE;
}


static int _generate_asm_add_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, result_size = 0, arg1_offset = -1, arg1_size = 0, arg2_offset = -1, arg2_size = 0;
  int result_var_type, arg1_var_type, arg2_var_type;
  
  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  result_size = t->result_size;
  result_var_type = _get_var_type(t->result_node);
  
  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;
  arg1_size = t->arg1_size;
  arg1_var_type = _get_var_type(t->arg1_node);

  /* arg2 */

  if (find_stack_offset(t->arg2_type, t->arg2_s, t->arg2_d, t->arg2_node, &arg2_offset, function_node) == FAILED)
    return FAILED;
  arg2_size = t->arg2_size;
  arg2_var_type = _get_var_type(t->arg2_node);
    
  /* generate asm */

  /* target address -> ix */

  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_z80(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    _load_value_to_ix(0, file_out);
    _add_de_to_ix(file_out);
    _load_value_to_bc(result_offset, file_out);
    _add_bc_to_ix(file_out);
  }

  /* source address (arg1) -> iy */

  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->arg1_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    _load_value_to_iy(0, file_out);
    _add_de_to_iy(file_out);
    _load_value_to_bc(arg1_offset, file_out);
    _add_bc_to_iy(file_out);
  }
  
  /* copy data (arg1) -> b/bc */

  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    int value = (int)t->arg1_d;
    if (arg1_size == 16 || arg2_size == 16) {
      /* 16-bit */
      _load_value_to_bc(value, file_out);
      arg1_size = 16;
    }
    else {
      /* 8-bit */
      _load_value_to_c(value & 0xff, file_out);
      arg1_size = 8;
    }
  }
  else {
    if (arg1_size == 16) {
      /* 16-bit */
      _load_from_iy_to_c(0, file_out);
      _load_from_iy_to_b(1, file_out);
      arg1_size = 16;
    }
    else {
      /* 8-bit */
      _load_from_iy_to_c(0, file_out);
      arg1_size = 8;
    }
  }

  /* sign extend 8-bit -> 16-bit? */
  if (arg1_var_type == VARIABLE_TYPE_INT8 && (result_var_type == VARIABLE_TYPE_INT16 || arg2_var_type == VARIABLE_TYPE_INT16)) {
    /* yes */
    _sign_extend_c_to_bc(file_out);
    arg1_size = 16;
  }
  else if (arg1_size == 8 && (result_size == 16 || arg2_size == 16)) {
    /* upper byte -> 0 */
    _load_value_to_b(0, file_out);
    arg1_size = 16;
  }



  
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

      fprintf(file_out, "      ; A  - tmp\n");
      fprintf(file_out, "      ; BC - tmp\n");
      fprintf(file_out, "      ; DE - frame pointer\n");
      fprintf(file_out, "      ; HL - tmp\n");
      fprintf(file_out, "      ; SP - stack pointer\n");
      fprintf(file_out, "      ; IX - tmp\n");
      fprintf(file_out, "      ; IY - tmp\n");
      
      for (i = i + 1; i < g_tacs_count; i++) {
        t = &g_tacs[i];
        op = t->op;

        if (op == TAC_OP_DEAD)
          continue;

        /* IL -> ASM */

        if (op == TAC_OP_LABEL && t->is_function == YES) {
          _reset_cpu_z80();
          i--;
          break;
        }

        fprintf(file_out, "      ; -------------------------------------------------------\n");
        print_tac(t, YES, file_out);
        
        if (op == TAC_OP_LABEL && t->is_function == NO) {
          _reset_cpu_z80();
        }
        else if (op == TAC_OP_LABEL)
          fprintf(file_out, "    %s:\n", t->result_s);
        else if (op == TAC_OP_ASSIGNMENT) {
          if (_generate_asm_assignment_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_ADD) {
          if (_generate_asm_add_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
      }
      
      fprintf(file_out, "  .ENDS\n");
    }
  }
  
  return SUCCEEDED;
}
