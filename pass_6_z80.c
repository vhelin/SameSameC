
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
#include "source_line_manager.h"
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


static void _load_label_to_hl(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  HL,%s\n", label);
}


static void _load_label_to_ix(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%s\n", label);
}


static void _load_label_to_iy(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  IY,%s\n", label);
}


static void _load_value_into_ix(int value, int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),%d\n", offset, value);
  else
    fprintf(file_out, "      LD  (IX%d),%d\n", offset, value);
}


static void _load_value_into_iy(int value, int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),%d\n", offset, value);
  else
    fprintf(file_out, "      LD  (IY%d),%d\n", offset, value);
}


static void _load_from_iy_to_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  A,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  A,(IY%d)\n", offset);
}


static void _load_from_iy_to_h(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  H,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  H,(IY%d)\n", offset);
}


static void _load_from_iy_to_l(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  L,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  L,(IY%d)\n", offset);
}


static void _load_from_iy_to_b(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  B,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  B,(IY%d)\n", offset);
}


static void _load_from_iy_to_c(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  C,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  C,(IY%d)\n", offset);
}


static void _load_from_ix_to_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  A,(IX%d)\n", offset);
}


static void _load_from_ix_to_b(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  B,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  B,(IX%d)\n", offset);
}


static void _load_from_ix_to_c(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  C,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  C,(IX%d)\n", offset);
}


static void _load_from_ix_to_d(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  D,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  D,(IX%d)\n", offset);
}


static void _load_from_ix_to_e(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  E,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  E,(IX%d)\n", offset);
}


static void _load_from_ix_to_h(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  H,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  H,(IX%d)\n", offset);
}


static void _load_from_ix_to_l(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  L,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  L,(IX%d)\n", offset);
}


static void _load_a_into_ix(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),A\n", offset);
}


static void _load_b_into_ix(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),B\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),B\n", offset);
}


static void _load_c_into_ix(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),C\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),C\n", offset);
}


static void _load_h_into_ix(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),H\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),H\n", offset);
}


static void _load_h_into_iy(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),H\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),H\n", offset);
}


static void _load_l_into_ix(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),L\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),L\n", offset);
}


static void _load_l_into_iy(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),L\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),L\n", offset);
}


static void _add_de_to_hl(FILE *file_out) {

  fprintf(file_out, "      ADD HL,DE\n");
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


static void _add_value_to_a(int value, FILE *file_out) {

  fprintf(file_out, "      ADD A,%d\n", value);
}


static void _add_from_ix_to_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      ADD A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      ADD A,(IX%d)\n", offset);
}


static void _sub_l_from_a(FILE *file_out) {

  fprintf(file_out, "      SUB A,L\n");
}


static void _sub_bc_from_hl(FILE *file_out) {

  fprintf(file_out, "      AND A    ; Clear carry\n");
  fprintf(file_out, "      SBC HL,BC\n");
}


static void _sub_value_from_a(int value, FILE *file_out) {

  fprintf(file_out, "      SUB A,%d\n", value);
}


static void _sub_from_ix_from_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      SUB A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      SUB A,(IX%d)\n", offset);
}


static void _or_value_from_a(int value, FILE *file_out) {

  fprintf(file_out, "      OR A,%d\n", value);
}


static void _or_from_ix_from_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      OR A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      OR A,(IX%d)\n", offset);
}


static void _or_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD A,B\n");
  fprintf(file_out, "      OR A,H\n");
  fprintf(file_out, "      LD H,A\n");
  fprintf(file_out, "      LD A,C\n");
  fprintf(file_out, "      OR A,L\n");
  fprintf(file_out, "      LD L,A\n");
}


static void _and_value_from_a(int value, FILE *file_out) {

  fprintf(file_out, "      AND A,%d\n", value);
}


static void _and_from_ix_from_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      AND A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      AND A,(IX%d)\n", offset);
}


static void _and_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD A,B\n");
  fprintf(file_out, "      AND A,H\n");
  fprintf(file_out, "      LD H,A\n");
  fprintf(file_out, "      LD A,C\n");
  fprintf(file_out, "      AND A,L\n");
  fprintf(file_out, "      LD L,A\n");
}


static void _load_value_to_bc(int value, FILE *file_out) {

  fprintf(file_out, "      LD  BC,%d\n", value);
}


static void _load_value_to_de(int value, FILE *file_out) {

  fprintf(file_out, "      LD  DE,%d\n", value);
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


static void _load_value_to_d(int value, FILE *file_out) {

  fprintf(file_out, "      LD  D,%d\n", value);
}


static void _load_value_to_e(int value, FILE *file_out) {

  fprintf(file_out, "      LD  E,%d\n", value);
}


static void _load_value_to_h(int value, FILE *file_out) {

  fprintf(file_out, "      LD  H,%d\n", value);
}


static void _load_value_to_l(int value, FILE *file_out) {

  fprintf(file_out, "      LD  L,%d\n", value);
}


static void _jump_to(char *label, FILE *file_out) {

  fprintf(file_out, "      JP  %s\n", label);
}


static void _push_de(FILE *file_out) {

  fprintf(file_out, "      PUSH DE\n");
}


static void _pop_de(FILE *file_out) {

  fprintf(file_out, "      POP DE\n");
}


static void _sign_extend_a_to_bc(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit A -> 16-bit BC\n");
  fprintf(file_out, "      LD  C,A\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  B,A  ; now BC is sign extended A\n");
}


static void _sign_extend_a_into_ix(int offset, FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit A -> (IX)\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),A\n", offset);
}


static void _sign_extend_b_into_ix(int offset, FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit B -> (IX)\n");
  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),A\n", offset);
}


static void _sign_extend_c_to_bc(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit C -> 16-bit BC\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  B,A  ; now BC is sign extended C\n");
}


static void _sign_extend_e_to_de(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit E -> 16-bit DE\n");
  fprintf(file_out, "      LD  A,E\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  D,A  ; now DE is sign extended E\n");
}


static void _sign_extend_l_to_hl(FILE *file_out) {

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit L -> 16-bit HL\n");
  fprintf(file_out, "      LD  A,L\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  fprintf(file_out, "      LD  H,A  ; now HL is sign extended L\n");
}


static void _add_label(char *label, FILE *file_out, char is_local) {

  if (is_local == YES)
    fprintf(file_out, "    %s\n", label);
  else
    fprintf(file_out, "    %s:\n", label);
}


static void _shift_left_hl_by_bc(FILE *file_out) {

  fprintf(file_out, "      ; shift left HL by BC\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      OR  A,C\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      ADD HL,HL\n");
  fprintf(file_out, "      DEC BC\n");
  fprintf(file_out, "      JR  -\n");

  _add_label("+", file_out, YES);
}


static void _shift_right_hl_by_bc(FILE *file_out) {

  fprintf(file_out, "      ; shift right HL by BC\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      OR  A,C\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      SRL H\n");
  fprintf(file_out, "      RR  L\n");
  fprintf(file_out, "      DEC BC\n");
  fprintf(file_out, "      JR  -\n");

  _add_label("+", file_out, YES);
}


static void _shift_left_b_by_a(FILE *file_out) {

  fprintf(file_out, "      ; shift left B by A\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      OR  A,A\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      SLA B\n");
  fprintf(file_out, "      DEC A\n");
  fprintf(file_out, "      JR  -\n");

  _add_label("+", file_out, YES);
}


static void _shift_right_b_by_a(FILE *file_out) {

  fprintf(file_out, "      ; shift right B by A\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      OR  A,A\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      SRL B\n");
  fprintf(file_out, "      DEC A\n");
  fprintf(file_out, "      JR  -\n");

  _add_label("+", file_out, YES);
}


static void _multiply_h_and_e_to_hl(FILE *file_out) {

  /* from http://map.grauw.nl/articles/mult_div_shifts.php */
  fprintf(file_out, "      ; multiply H * E -> HL\n");
  fprintf(file_out, "      LD  D,0\n");
  fprintf(file_out, "      LD  L,D\n");
  fprintf(file_out, "      LD  B,8  ; number of bits to process\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      ADD HL,HL\n");
  fprintf(file_out, "      JR  NC,+\n");
  fprintf(file_out, "      ADD HL,DE\n");

  _add_label("+", file_out, YES);

  fprintf(file_out, "      DJNZ -\n");
}


static void _multiply_bc_and_de_to_hl(FILE *file_out) {

  /* from http://cpctech.cpc-live.com/docs/mult.html */
  fprintf(file_out, "      ; multiply BC * DE -> HL\n");
  fprintf(file_out, "      LD  A,16 ; number of bits to process\n");
  fprintf(file_out, "      LD  HL,0\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      SRL B\n");
  fprintf(file_out, "      RR  C\n");
  fprintf(file_out, "      JR  NC, +\n");
  fprintf(file_out, "      ADD HL,DE\n");

  _add_label("+", file_out, YES);

  fprintf(file_out, "      EX  DE,HL\n");
  fprintf(file_out, "      ADD HL,HL\n");
  fprintf(file_out, "      EX  DE,HL\n");
  fprintf(file_out, "      DEC A\n");
  fprintf(file_out, "      JR  NZ,-\n");
}


static void _divide_bc_by_de_to_ca_hl(FILE *file_out) {

  /* from http://map.grauw.nl/articles/mult_div_shifts.php */
  fprintf(file_out, "      ; divide BC / DE -> CA (result) & HL (remainder)\n");
  fprintf(file_out, "      LD  HL,0\n");
  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      LD  B,8\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      ADC HL,HL\n");
  fprintf(file_out, "      SBC HL,DE\n");
  fprintf(file_out, "      JR  NC,+\n");
  fprintf(file_out, "      ADD HL,DE\n");

  _add_label("+", file_out, YES);
  
  fprintf(file_out, "      DJNZ -\n");
  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      CPL\n");
  fprintf(file_out, "      LD  B,A\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      LD  C,B\n");
  fprintf(file_out, "      LD  B,8\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      ADC HL,HL\n");
  fprintf(file_out, "      SBC HL,DE\n");
  fprintf(file_out, "      JR  NC,+\n");
  fprintf(file_out, "      ADD HL,DE\n");

  _add_label("+", file_out, YES);
  
  fprintf(file_out, "      DJNZ -\n");
  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      CPL\n");
}


static void _divide_h_by_e_to_a_b(FILE *file_out) {

  /* from http://map.grauw.nl/articles/mult_div_shifts.php */
  fprintf(file_out, "      ; divide H / E -> A (result) & B (remainder)\n");  
  fprintf(file_out, "      XOR A\n");
  fprintf(file_out, "      LD  B,8\n");

  _add_label("-", file_out, YES);
    
  fprintf(file_out, "      RL  H\n");
  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      SUB E\n");
  fprintf(file_out, "      JR  NC,+\n");
  fprintf(file_out, "      ADD A,E\n");

  _add_label("+", file_out, YES);
    
  fprintf(file_out, "      DJNZ -\n");
  fprintf(file_out, "      LD  B,A\n");
  fprintf(file_out, "      LD  A,H\n");
  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      CPL\n");
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


static int _generate_asm_add_sub_or_and_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {
  
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
  /* copy data (arg1) -> a */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_a(((int)t->arg1_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_a(0, file_out);
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
  /* add/sub/or/and data (arg2) -> a */
  /******************************************************************************************************/

  if (op == TAC_OP_ADD) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      _add_value_to_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _add_from_ix_to_a(0, file_out);
    }
  }
  else if (op == TAC_OP_SUB) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      _sub_value_from_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _sub_from_ix_from_a(0, file_out);
    }
  }
  else if (op == TAC_OP_OR) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      _or_value_from_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _or_from_ix_from_a(0, file_out);
    }
  }
  else if (op == TAC_OP_AND) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      _and_value_from_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _and_from_ix_from_a(0, file_out);
    }
  }
  else {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Unsupported TAC op %d! Please submit a bug report!\n", op);
    return FAILED;
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
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

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */

    /* lower byte */
    _load_a_into_ix(0, file_out);

    /* sign extend 8-bit -> 16-bit? */
    if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                      t->arg2_var_type == VARIABLE_TYPE_INT8)) {
      /* yes */
      _sign_extend_a_into_ix(1, file_out);
    }
    else {
      /* upper byte = 0 */
      _load_value_into_ix(0, 1, file_out);
    }
  }
  else {
    /* 8-bit */
    _load_a_into_ix(0, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_add_sub_or_and_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {
  
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
        fprintf(stderr, "_generate_asm_add_sub_or_and_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
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
        fprintf(stderr, "_generate_asm_add_sub_or_and_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* add/sub data bc -> hl */
  /******************************************************************************************************/

  if (op == TAC_OP_ADD)
    _add_bc_to_hl(file_out);
  else if (op == TAC_OP_SUB)
    _sub_bc_from_hl(file_out);
  else if (op == TAC_OP_OR)
    _or_bc_to_hl(file_out);
  else if (op == TAC_OP_AND)
    _and_bc_to_hl(file_out);
  else {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_16bit(): Unsupported TAC op %d! Please submit a bug report!\n", op);
    return FAILED;
  }
  
  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_16bit(): Target cannot be a value!\n");
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


static int _generate_asm_add_sub_or_and_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _generate_asm_add_sub_or_and_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _generate_asm_add_sub_or_and_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_add_sub_or_and_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _generate_asm_get_address_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1;
  
  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  
  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;

  /* arg2 */

  if (op == TAC_OP_GET_ADDRESS_ARRAY) {
    if (find_stack_offset(t->arg2_type, t->arg2_s, t->arg2_d, t->arg2_node, &arg2_offset, function_node) == FAILED)
      return FAILED;
  }
  
  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> hl */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_get_address_z80(): Source cannot be a value!\n");
    return FAILED;
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_hl(t->arg1_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    _load_value_to_hl(arg1_offset, file_out);
    _add_de_to_hl(file_out);
  }

  if (op == TAC_OP_GET_ADDRESS_ARRAY) {
    int var_type;
    
    /******************************************************************************************************/
    /* index address (arg2) -> ix */
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
    /* copy data (ix) -> bc */
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
        if (t->arg2_var_type == VARIABLE_TYPE_INT8) {
          /* yes */
          _sign_extend_c_to_bc(file_out);
        }
        else if (t->arg2_var_type == VARIABLE_TYPE_UINT8) {
          /* upper byte -> 0 */
          _load_value_to_b(0, file_out);
        }
        else {
          fprintf(stderr, "_generate_asm_get_address_z80(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
          return FAILED;
        }
      }
    }

    /******************************************************************************************************/
    /* add bc -> hl */
    /******************************************************************************************************/

    _add_bc_to_hl(file_out);

    var_type = tree_node_get_max_var_type(t->arg1_node->children[0]);
    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* the array has 16-bit items -> add bc -> hl twice! */
      _add_bc_to_hl(file_out);
    }
  }
  
  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_get_address_z80(): Target cannot be a value!\n");
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
    fprintf(stderr, "_generate_asm_get_address_z80(): The target cannot be an 8-bit value!\n");
    return FAILED;
  }
  
  return SUCCEEDED;
}


static int _generate_asm_array_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, var_type;

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
  /* source address (arg1) -> iy */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Source cannot be a value!\n");
    return FAILED;
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
  /* index address (arg2) -> ix */
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
  /* copy data (ix) -> bc */
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
      if (t->arg2_var_type == VARIABLE_TYPE_INT8) {
        /* yes */
        _sign_extend_c_to_bc(file_out);
      }
      else if (t->arg2_var_type == VARIABLE_TYPE_UINT8) {
        /* upper byte -> 0 */
        _load_value_to_b(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_array_read_z80(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* add bc -> iy */
  /******************************************************************************************************/

  _add_bc_to_iy(file_out);

  var_type = tree_node_get_max_var_type(t->arg1_node->children[0]);
  if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
    /* the array has 16-bit items -> add bc -> iy twice! */
    _add_bc_to_iy(file_out);
  }

  /******************************************************************************************************/
  /* copy data (iy) -> hl */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Source cannot be a value!\n");
    return FAILED;
  }
  else {
    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_iy_to_l(0, file_out);
      _load_from_iy_to_h(1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_iy_to_l(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (var_type == VARIABLE_TYPE_INT8 && (t->result_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                             t->result_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_l_to_hl(file_out);
      }
      else if (var_type == VARIABLE_TYPE_UINT8 && (t->result_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                   t->result_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_h(0, file_out);
      }
      else if ((var_type == VARIABLE_TYPE_UINT8 || var_type == VARIABLE_TYPE_INT8) &&
               (t->result_var_type_promoted == VARIABLE_TYPE_INT8 || t->result_var_type_promoted == VARIABLE_TYPE_UINT8)) {
      }
      else {
        fprintf(stderr, "_generate_asm_array_read_z80: Unhandled 8-bit ARG1! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }
  
  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Target cannot be a value!\n");
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


static int _generate_asm_array_assignment_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

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
  /* index address (arg2) -> iy */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->arg2_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    _load_value_to_iy(arg2_offset, file_out);
    _add_de_to_iy(file_out);
  }

  /******************************************************************************************************/
  /* copy data (iy) -> bc */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    _load_value_to_bc((int)t->arg2_d, file_out);
  }
  else {
    if (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_iy_to_c(0, file_out);
      _load_from_iy_to_b(1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_iy_to_c(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->arg2_var_type == VARIABLE_TYPE_INT8) {
        /* yes */
        _sign_extend_c_to_bc(file_out);
      }
      else if (t->arg2_var_type == VARIABLE_TYPE_UINT8) {
        /* upper byte -> 0 */
        _load_value_to_b(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_array_assignment_z80(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* result address (result) -> iy */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->result_s, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    _load_value_to_iy(result_offset, file_out);
    _add_de_to_iy(file_out);
  }
  
  /******************************************************************************************************/
  /* add index bc -> iy */
  /******************************************************************************************************/

  _add_bc_to_iy(file_out);

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* the array has 16-bit items -> add bc -> ix twice! */
    _add_bc_to_iy(file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (ix) -> hl */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    int value = (int)t->arg1_d;
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_value_into_iy(value & 0xff, 0, file_out);
      _load_value_into_iy((value >> 8) & 0xff, 1, file_out);
      return SUCCEEDED;
    }
    else {
      /* 8-bit */
      _load_value_into_iy(value & 0xff, 0, file_out);
      return SUCCEEDED;
    }
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
      if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->result_var_type == VARIABLE_TYPE_INT16 ||
                                                     t->result_var_type == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_l_to_hl(file_out);
      }
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->result_var_type == VARIABLE_TYPE_INT16 ||
                                                           t->result_var_type == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_h(0, file_out);
      }
      else if ((t->arg1_var_type == VARIABLE_TYPE_UINT8 || t->arg1_var_type == VARIABLE_TYPE_INT8) &&
               (t->result_var_type == VARIABLE_TYPE_INT8 || t->result_var_type == VARIABLE_TYPE_UINT8)) {
      }
      else {
        fprintf(stderr, "_generate_asm_array_assignment_z80: Unhandled 8-bit ARG1! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }
  
  /******************************************************************************************************/
  /* copy data hl -> (iy) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_iy(0, file_out);
    _load_h_into_iy(1, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_iy(0, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_shift_left_right_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

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
        fprintf(stderr, "_generate_asm_shift_left_right_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
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
        fprintf(stderr, "_generate_asm_shift_left_right_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* hl <</>> bc */
  /******************************************************************************************************/

  if (op == TAC_OP_SHIFT_LEFT) {
    /*
      -
      ld a, b
      or a, c
      jr z, +

      add hl,hl
      dec bc
      jr -
      +
    */

    _shift_left_hl_by_bc(file_out);
  }
  else if (op == TAC_OP_SHIFT_RIGHT) {
    /*
      -
      ld a, b
      or a, c
      jr z, +

      srl h
      rr l
      dec bc
      jr -
      +
    */
    
    _shift_right_hl_by_bc(file_out);
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_shift_left_right_z80_16bit(): Target cannot be a value!\n");
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


static int _generate_asm_shift_left_right_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

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
  /* copy data (arg1) -> b */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_b(((int)t->arg1_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_b(0, file_out);
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
  /* b <</>> a */
  /******************************************************************************************************/

  if (op == TAC_OP_SHIFT_LEFT) {
    /*
      -
      or a, a
      jr z, +

      sla b
      dec a
      jr -
      +
    */

    _shift_left_b_by_a(file_out);
  }
  else if (op == TAC_OP_SHIFT_RIGHT) {
    /*
      -
      or a, a
      jr z, +

      srl b
      dec a
      jr -
      +
    */
    
    _shift_right_b_by_a(file_out);
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
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
  /* copy data b -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */

    /* lower byte */
    _load_b_into_ix(0, file_out);

    /* sign extend 8-bit -> 16-bit? */
    if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                      t->arg2_var_type == VARIABLE_TYPE_INT8)) {
      /* yes */
      _sign_extend_b_into_ix(1, file_out);
    }
    else {
      /* upper byte = 0 */
      _load_value_into_ix(0, 1, file_out);
    }
  }
  else {
    /* 8-bit */
    _load_b_into_ix(0, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_shift_left_right_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _generate_asm_shift_left_right_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _generate_asm_shift_left_right_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_shift_left_right_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _generate_asm_mul_div_mod_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

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
  /* copy data (arg1) -> bc */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    _load_value_to_bc((int)t->arg1_d, file_out);
  }
  else {
    if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_c(0, file_out);
      _load_from_ix_to_b(1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                     t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_c_to_bc(file_out);
      }
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                           t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_b(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
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
  /* copy data (arg2) -> de */
  /******************************************************************************************************/
  
  /* NOTE!!! */
  _push_de(file_out);

  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    _load_value_to_de((int)t->arg2_d, file_out);
  }
  else {
    if (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_e(0, file_out);
      _load_from_ix_to_d(1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_e(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->arg2_var_type == VARIABLE_TYPE_INT8 && (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                     t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_e_to_de(file_out);
      }
      else if (t->arg2_var_type == VARIABLE_TYPE_UINT8 && (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                           t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_d(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* bc * de -> hl */
  /******************************************************************************************************/

  if (op == TAC_OP_MUL) {
    /*
      ld a,16     ; this is the number of bits of the number to process
      ld hl,0     ; HL is updated with the partial result, and at the end it will hold 
                  ; the final result.
    .mul_loop
      srl b
      rr c        ;; divide BC by 2 and shifting the state of bit 0 into the carry
                  ;; if carry = 0, then state of bit 0 was 0, (the rightmost digit was 0) 
                  ;; if carry = 1, then state of bit 1 was 1. (the rightmost digit was 1)
                  ;; if rightmost digit was 0, then the result would be 0, and we do the add.
                  ;; if rightmost digit was 1, then the result is DE and we do the add.
      jr nc,no_add	

      ;; will get to here if carry = 1        
      add hl,de   

    .no_add
      ;; at this point BC has already been divided by 2

      ex de,hl    ;; swap DE and HL
      add hl,hl   ;; multiply DE by 2
      ex de,hl    ;; swap DE and HL

      ;; at this point DE has been multiplied by 2
    
      dec a
      jr nz,mul_loop  ;; process more bits
    */

    _multiply_bc_and_de_to_hl(file_out);
  }

  /******************************************************************************************************/
  /* bc / de -> ca (result), hl (remainder) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV || op == TAC_OP_MOD) {
    /*
      ;
      ; Divide 16-bit values (with 16-bit result)
      ; In: Divide BC by divider DE
      ; Out: BC = result, HL = rest
      ;
      Div16:
      ld hl,0
      ld a,b
      ld b,8
    Div16_Loop1:
      rla
      adc hl,hl
      sbc hl,de
      jr nc,Div16_NoAdd1
      add hl,de
    Div16_NoAdd1:
      djnz Div16_Loop1
      rla
      cpl
      ld b,a
      ld a,c
      ld c,b
      ld b,8
    Div16_Loop2:
      rla
      adc hl,hl
      sbc hl,de
      jr nc,Div16_NoAdd2
      add hl,de
    Div16_NoAdd2:
      djnz Div16_Loop2
      rla
      cpl
      ;ld b,c
      ;ld c,a
    */

    _divide_bc_by_de_to_ca_hl(file_out);
  }
    
  /* NOTE!!! */
  _pop_de(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Target cannot be a value!\n");
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
  /* copy data ca -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_a_into_ix(0, file_out);
      _load_c_into_ix(1, file_out);
    }
    else {
      /* 8-bit */
      _load_a_into_ix(0, file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_MUL || op == TAC_OP_MOD) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_l_into_ix(0, file_out);
      _load_h_into_ix(1, file_out);
    }
    else {
      /* 8-bit */
      _load_l_into_ix(0, file_out);
    }
  }

  return SUCCEEDED;
}


static int _generate_asm_mul_div_mod_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

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
  /* copy data (arg1) -> h */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_h(((int)t->arg1_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_h(0, file_out);
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
  /* copy data (arg2) -> e */
  /******************************************************************************************************/

  /* NOTE!!! */
  _push_de(file_out);
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_e(((int)t->arg2_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_e(0, file_out);
  }

  /******************************************************************************************************/
  /* h * e -> hl */
  /******************************************************************************************************/

  if (op == TAC_OP_MUL) {
    /*
      ;
      ; Multiply 8-bit values
      ; In:  Multiply H with E
      ; Out: HL = result
      ;
    Mult8:
      ld d,0
      ld l,d
      ld b,8
    Mult8_Loop:
      add hl,hl
      jr nc,Mult8_NoAdd
      add hl,de
    Mult8_NoAdd:
      djnz Mult8_Loop
    */

    _multiply_h_and_e_to_hl(file_out);
  }

  /******************************************************************************************************/
  /* h / e -> a (result) and b (reminder) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV || op == TAC_OP_MOD) {
    /*
    ;
    ; Divide 8-bit values
    ; In: Divide H by divider E
    ; Out: A = result, B = rest
    ;
  Div8:
    xor a
    ld b,8
  Div8_Loop:
    rl h
    rla
    sub e
    jr nc,Div8_NoAdd
    add a,e
  Div8_NoAdd:
    djnz Div8_Loop
    ld b,a
    ld a,h
    rla
    cpl
    */

    _divide_h_by_e_to_a_b(file_out);
  }
  
  /* NOTE!!! */
  _pop_de(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
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

  if (op == TAC_OP_MUL) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_l_into_ix(0, file_out);
      _load_h_into_ix(1, file_out);
    }
    else {
      /* 8-bit */
      _load_l_into_ix(0, file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */

      /* lower byte */
      _load_a_into_ix(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8)) {
        /* yes */
        _sign_extend_a_into_ix(1, file_out);
      }
      else {
        /* upper byte = 0 */
        _load_value_into_ix(0, 1, file_out);
      }
    }
    else {
      /* 8-bit */
      _load_a_into_ix(0, file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data b -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_MOD) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */

      /* lower byte */
      _load_b_into_ix(0, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8)) {
        /* yes */
        _sign_extend_b_into_ix(1, file_out);
      }
      else {
        /* upper byte = 0 */
        _load_value_into_ix(0, 1, file_out);
      }
    }
    else {
      /* 8-bit */
      _load_b_into_ix(0, file_out);
    }
  }
  
  return SUCCEEDED;
}


static int _generate_asm_mul_div_mod_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _generate_asm_mul_div_mod_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _generate_asm_mul_div_mod_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_shift_left_right_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


int generate_asm_z80(FILE *file_out) {

  int i, file_id = -1, line_number = -1;

  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES) {
      /* function start! */
      struct tree_node *function_node = t->function_node;

      fprintf(file_out, "  ; =================================================================\n");
      fprintf(file_out, "  ; %s:%d: ", get_file_name(t->file_id), t->line_number);
      get_source_line(t->file_id, t->line_number, file_out);
      fprintf(file_out, "\n");
      fprintf(file_out, "  ; =================================================================\n");
      
      fprintf(file_out, "  .SECTION \"%s\" FREE\n", function_node->children[1]->label);

      _add_label(function_node->children[1]->label, file_out, NO);

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

        if (t->file_id != file_id || t->line_number != line_number) {
          /* the source code line has changed, print info about the new line */
          file_id = t->file_id;
          line_number = t->line_number;
          fprintf(file_out, "      ; =================================================================\n");
          fprintf(file_out, "      ; %s:%d: ", get_file_name(t->file_id), t->line_number);
          get_source_line(t->file_id, t->line_number, file_out);
          fprintf(file_out, "\n");
          fprintf(file_out, "      ; =================================================================\n");
        }
        
        if (op == TAC_OP_LABEL && t->is_function == YES) {
          _reset_cpu_z80();
          i--;
          break;
        }

        fprintf(file_out, "      ; -----------------------------------------------------------------\n");
        print_tac(t, YES, file_out);
        fprintf(file_out, "      ; -----------------------------------------------------------------\n");
        
        if (op == TAC_OP_LABEL && t->is_function == NO) {
          fprintf(file_out, "    %s:\n", t->result_s);
          _reset_cpu_z80();
        }
        else if (op == TAC_OP_LABEL)
          fprintf(file_out, "    %s:\n", t->result_s);
        else if (op == TAC_OP_ASSIGNMENT) {
          if (_generate_asm_assignment_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_ADD || op == TAC_OP_SUB || op == TAC_OP_OR || op == TAC_OP_AND) {
          if (_generate_asm_add_sub_or_and_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_GET_ADDRESS || op == TAC_OP_GET_ADDRESS_ARRAY) {
          if (_generate_asm_get_address_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_ARRAY_READ) {
          if (_generate_asm_array_read_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_ARRAY_ASSIGNMENT) {
          if (_generate_asm_array_assignment_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_SHIFT_LEFT || op == TAC_OP_SHIFT_RIGHT) {
          if (_generate_asm_shift_left_right_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_MUL || op == TAC_OP_DIV || op == TAC_OP_MOD) {
          if (_generate_asm_mul_div_mod_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_JUMP)
          _jump_to(t->result_s, file_out);
        else if (op == TAC_OP_CREATE_VARIABLE) {
        }
        else
          fprintf(stderr, "generate_asm_z80(): Unimplemented IL -> Z80 ASM op %d! Please submit a bug report!\n", op);
      }
      
      fprintf(file_out, "  .ENDS\n");
    }
  }
  
  return SUCCEEDED;
}
