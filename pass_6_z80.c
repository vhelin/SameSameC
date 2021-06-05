
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


static void _load_from_iy_to_h(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  H,(IY+%d)\n", offset);
}


static void _load_from_iy_to_l(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  L,(IY+%d)\n", offset);
}


static void _load_from_iy_to_b(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  B,(IY+%d)\n", offset);
}


static void _load_from_iy_to_c(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  C,(IY+%d)\n", offset);
}


static void _load_from_ix_to_a(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  A,(IX+%d)\n", offset);
}


static void _load_from_ix_to_b(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  B,(IX+%d)\n", offset);
}


static void _load_from_ix_to_c(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  C,(IX+%d)\n", offset);
}


static void _load_from_ix_to_h(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  H,(IX+%d)\n", offset);
}


static void _load_from_ix_to_l(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  L,(IX+%d)\n", offset);
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


static void _load_h_into_ix(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  (IX+%d),H\n", offset);
}


static void _load_l_into_ix(int offset, FILE *file_out) {

  fprintf(file_out, "      LD  (IX+%d),L\n", offset);
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


static void _add_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      ADD HL,BC\n");
}


static void _add_c_to_a(FILE *file_out) {

  fprintf(file_out, "      ADD A,C\n");
}


static void _add_l_to_a(FILE *file_out) {

  fprintf(file_out, "      ADD A,L\n");
}


static void _load_value_to_bc(int value, FILE *file_out) {

  fprintf(file_out, "      LD  BC,%d\n", value);
}


static void _load_value_to_hl(int value, FILE *file_out) {

  fprintf(file_out, "      LD  HL,%d\n", value);
}


static void _load_value_to_a(int value, FILE *file_out) {

  fprintf(file_out, "      LD  A,%d\n", value);
}


static void _load_value_to_b(int value, FILE *file_out) {

  fprintf(file_out, "      LD  B,%d\n", value);
}


static void _load_value_to_c(int value, FILE *file_out) {

  fprintf(file_out, "      LD  C,%d\n", value);
}


static void _load_value_to_h(int value, FILE *file_out) {

  fprintf(file_out, "      LD  H,%d\n", value);
}


static void _load_value_to_l(int value, FILE *file_out) {

  fprintf(file_out, "      LD  L,%d\n", value);
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
  fprintf(file_out, "      LD  B,A  ; now BC is sign extended C\n");
}


static void _sign_extend_l_to_hl(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit L -> 16-bit HL\n");
  fprintf(file_out, "      LD  A,L\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  H,A  ; now HL is sign extended L\n");
}


static int _generate_asm_assignment_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, arg1_offset = -1;

  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  
  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;
  
  /* generate asm */

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
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

    _load_value_to_ix(result_offset, file_out);
    _add_de_to_ix(file_out);
  }

  /******************************************************************************************************/
  /* source address -> iy */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->arg1_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    _load_value_to_iy(arg1_offset, file_out);
    _add_de_to_iy(file_out);
  }
  
  /******************************************************************************************************/
  /* copy data -> ix */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    int value = (int)t->arg1_d;
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
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
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
        _load_from_iy_to_a(0, file_out);
        _load_a_into_ix(0, file_out);
        _load_from_iy_to_a(1, file_out);
        _load_a_into_ix(1, file_out);
      }
      else {
        /* sign extend 8-bit -> 16-bit? */
        if (t->result_var_type == VARIABLE_TYPE_INT16 && t->arg1_var_type == VARIABLE_TYPE_INT8) {
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


static int _generate_asm_add_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node) {
  
  int result_offset = -1, arg1_offset = -1, arg2_offset = -1;
  
  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  
  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;

  /* arg2 */

  if (find_stack_offset(t->arg2_type, t->arg2_s, t->arg2_d, t->arg2_node, &arg2_offset, function_node) == FAILED)
    return FAILED;
    
  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg1_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    _load_value_to_ix(arg1_offset, file_out);
    _add_de_to_ix(file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (arg1) -> l */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_l(((int)t->arg1_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_l(0, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    _load_value_to_ix(arg2_offset, file_out);
    _add_de_to_ix(file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (arg2) -> a */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_a(((int)t->arg2_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_a(0, file_out);
  }

  /******************************************************************************************************/
  /* add data l -> a */
  /******************************************************************************************************/

  _add_l_to_a(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
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

    _load_value_to_ix(result_offset, file_out);
    _add_de_to_ix(file_out);
  }

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  /* 8-bit */
  _load_a_into_ix(0, file_out);
  
  return SUCCEEDED;
}


static int _generate_asm_add_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node) {
  
  int result_offset = -1, arg1_offset = -1, arg2_offset = -1;
  
  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  
  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;

  /* arg2 */

  if (find_stack_offset(t->arg2_type, t->arg2_s, t->arg2_d, t->arg2_node, &arg2_offset, function_node) == FAILED)
    return FAILED;
    
  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg1_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    _load_value_to_ix(arg1_offset, file_out);
    _add_de_to_ix(file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (arg1) -> hl */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    _load_value_to_hl((int)t->arg1_d, file_out);
  }
  else {
    if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_l(0, file_out);
      _load_from_ix_to_h(1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                     t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_l_to_hl(file_out);
      }
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                           t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_h(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_add_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }
  
  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    _load_value_to_ix(arg2_offset, file_out);
    _add_de_to_ix(file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (arg2) -> bc */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    _load_value_to_bc((int)t->arg2_d, file_out);
  }
  else {
    if (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_c(0, file_out);
      _load_from_ix_to_b(1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->arg2_var_type == VARIABLE_TYPE_INT8 && (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                     t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_c_to_bc(file_out);
      }
      else if (t->arg2_var_type == VARIABLE_TYPE_UINT8 && (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                           t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_b(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_add_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* add data bc -> hl */
  /******************************************************************************************************/

  _add_bc_to_hl(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
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

    _load_value_to_ix(result_offset, file_out);
    _add_de_to_ix(file_out);
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_ix(0, file_out);
    _load_h_into_ix(1, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_ix(0, file_out);
  }
  
  return SUCCEEDED;
}


static int _generate_asm_add_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _generate_asm_add_z80_8bit(t, file_out, function_node);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _generate_asm_add_z80_16bit(t, file_out, function_node);

  fprintf(stderr, "_generate_asm_add_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
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
