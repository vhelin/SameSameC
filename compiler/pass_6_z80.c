
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
#include "source_line_manager.h"
#include "pass_6_z80.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"
#include "inline_asm.h"


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max, g_bank, g_slot, g_ram_bank, g_ram_slot;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

static int g_return_id = 1, g_is_ix_de = NO;


int pass_6_z80(char *file_name, FILE *file_out) {

  if (g_verbose_mode == ON)
    printf("Pass 6 (Z80)...\n");

  if (generate_global_variables_z80(file_name, file_out) == FAILED)
    return FAILED;
  
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


static void _load_de_with_offset_to_ix(int offset, FILE *file_out) {

  if (g_is_ix_de == YES && offset == 0) {
    /* IX is already DE! */
  }
  else {
    fprintf(file_out, "      LD  IX,%d\n", offset);
    fprintf(file_out, "      ADD IX,DE\n");

    if (offset == 0)
      g_is_ix_de = YES;
    else
      g_is_ix_de = NO;
  }
}


static void _load_value_to_ix(int value, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%d\n", value);

  /* NOTE! for _load_de_with_offset_to_ix() optimization every time we touch IX (or create a label)
     we need to set the variable to say that IX is no longer DE */
  g_is_ix_de = NO;
}


static void _load_value_to_iy(int value, FILE *file_out) {

  fprintf(file_out, "      LD  IY,%d\n", value);
}


static void _load_label_to_bc(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  BC,%s\n", label);
}


static void _load_label_to_de(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  DE,%s\n", label);
}


static void _load_label_to_hl(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  HL,%s\n", label);
}


static void _load_label_to_ix(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%s\n", label);

  /* NOTE! for _load_de_with_offset_to_ix() optimization every time we touch IX (or create a label)
     we need to set the variable to say that IX is no longer DE */
  g_is_ix_de = NO;
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


static void _load_from_ix_to_register(int offset, char reg, FILE *file_out) {

  char name[2];

  name[0] = toupper(reg);
  name[1] = 0;
  
  if (offset >= 0)
    fprintf(file_out, "      LD  %s,(IX+%d)\n", name, offset);
  else
    fprintf(file_out, "      LD  %s,(IX%d)\n", name, offset);
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


static void _load_register_into_ix(int offset, char reg, FILE *file_out) {

  char name[2];

  name[0] = toupper(reg);
  name[1] = 0;

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),%s\n", offset, name);
  else
    fprintf(file_out, "      LD  (IX%d),%s\n", offset, name);
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


static void _add_bc_to_ix(FILE *file_out) {

  fprintf(file_out, "      ADD IX,BC\n");

  /* NOTE! for _load_de_with_offset_to_ix() optimization every time we touch IX (or create a label)
     we need to set the variable to say that IX is no longer DE */
  g_is_ix_de = NO;
}


static void _add_de_to_ix(FILE *file_out) {

  fprintf(file_out, "      ADD IX,DE\n");

  /* NOTE! for _load_de_with_offset_to_ix() optimization every time we touch IX (or create a label)
     we need to set the variable to say that IX is no longer DE */
  g_is_ix_de = NO;
}


static void _add_bc_to_iy(FILE *file_out) {

  fprintf(file_out, "      ADD IY,BC\n");
}


static void _add_de_to_iy(FILE *file_out) {

  fprintf(file_out, "      ADD IY,DE\n");
}


static void _add_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      ADD HL,BC\n");
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


static void _sub_b_from_a(FILE *file_out) {

  fprintf(file_out, "      SUB A,B\n");
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


static void _or_value_to_a(int value, FILE *file_out) {

  fprintf(file_out, "      OR  A,%d\n", value);
}


static void _or_from_ix_to_a(int offset, FILE *file_out) {

  if (offset >= 0)
    fprintf(file_out, "      OR  A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      OR  A,(IX%d)\n", offset);
}


static void _or_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      OR  A,H\n");
  fprintf(file_out, "      LD  H,A\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      OR  A,L\n");
  fprintf(file_out, "      LD  L,A\n");
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

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      AND A,H\n");
  fprintf(file_out, "      LD  H,A\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      AND A,L\n");
  fprintf(file_out, "      LD  L,A\n");
}


static void _load_hl_to_bc(FILE *file_out) {

  fprintf(file_out, "      LD  B,H\n");
  fprintf(file_out, "      LD  C,L\n");
}


static void _load_hl_to_de(FILE *file_out) {

  fprintf(file_out, "      LD  D,H\n");
  fprintf(file_out, "      LD  E,L\n");
}


static void _load_hl_to_sp(FILE *file_out) {

  fprintf(file_out, "      LD  SP,HL\n");
}


static void _load_sp_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD  HL,0\n");
  fprintf(file_out, "      ADD HL,SP\n");
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


static void _in_a_from_value(int value, FILE *file_out) {

  fprintf(file_out, "      IN  A,($%.2X)\n", value);
}


static void _in_a_from_c(FILE *file_out) {

  fprintf(file_out, "      IN  A,(C)\n");
}


static void _out_a_into_value(int value, FILE *file_out) {

  fprintf(file_out, "      OUT ($%.2X),A\n", value);
}


static void _out_a_into_c(FILE *file_out) {

  fprintf(file_out, "      OUT (C),A\n");
}


static void _jump_to(char *label, FILE *file_out) {

  fprintf(file_out, "      JP  %s\n", label);
}


static void _jump_c_to(char *label, FILE *file_out) {

  fprintf(file_out, "      JP  C,%s\n", label);
}


static void _jump_nc_to(char *label, FILE *file_out) {

  fprintf(file_out, "      JP  NC,%s\n", label);
}


static void _jump_z_to(char *label, FILE *file_out) {

  fprintf(file_out, "      JP  Z,%s\n", label);
}


static void _jump_nz_to(char *label, FILE *file_out) {

  fprintf(file_out, "      JP  NZ,%s\n", label);
}


static void _call_to(char *label, FILE *file_out) {

  fprintf(file_out, "      CALL %s\n", label);
}


static void _ret(FILE *file_out) {

  fprintf(file_out, "      RET\n");
}


static void _push_bc(FILE *file_out) {

  fprintf(file_out, "      PUSH BC\n");
}


static void _push_de(FILE *file_out) {

  fprintf(file_out, "      PUSH DE\n");
}


static void _pop_bc(FILE *file_out) {

  fprintf(file_out, "      POP BC\n");
}


static void _pop_de(FILE *file_out) {

  fprintf(file_out, "      POP DE\n");
}


static void _inc_a(FILE *file_out) {

  fprintf(file_out, "      INC A\n");
}


static void _inc_hl(FILE *file_out) {

  fprintf(file_out, "      INC HL\n");
}


static void _dec_a(FILE *file_out) {

  fprintf(file_out, "      DEC A\n");
}


static void _dec_hl(FILE *file_out) {

  fprintf(file_out, "      DEC HL\n");
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

  if (is_local == NO) {
    /* NOTE! for _load_de_with_offset_to_ix() optimization every time we touch IX (or create a label)
       we need to set the variable to say that IX is no longer DE */
    g_is_ix_de = NO;
  }
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

  int result_offset = -1, arg1_offset = -1, ix_offset = 0;

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
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address -> iy */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->arg1_node->children[1]->label, file_out);
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
      _load_value_into_ix(value & 0xff, ix_offset, file_out);
      _load_value_into_ix((value >> 8) & 0xff, ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_value_into_ix(value & 0xff, ix_offset, file_out);
    }
  }
  else {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
        _load_from_iy_to_a(0, file_out);
        _load_a_into_ix(ix_offset, file_out);

        _load_from_iy_to_a(1, file_out);
        _load_a_into_ix(ix_offset + 1, file_out);
      }
      else {
        /* sign extend 8-bit -> 16-bit? */
        if (t->result_var_type == VARIABLE_TYPE_INT16 && t->arg1_var_type == VARIABLE_TYPE_INT8) {
          /* lower byte must be sign extended */
          _load_from_iy_to_a(0, file_out);
          _sign_extend_a_to_bc(file_out);
          _load_c_into_ix(ix_offset, file_out);
          _load_b_into_ix(ix_offset + 1, file_out);
        }
        else {
          /* lower byte */
          _load_from_iy_to_a(0, file_out);
          _load_a_into_ix(ix_offset, file_out);
          /* upper byte is 0 */
          _load_value_into_ix(0, ix_offset + 1, file_out);
        }
      }
    }
    else {
      /* 8-bit */
      _load_from_iy_to_a(0, file_out);
      _load_a_into_ix(ix_offset, file_out);
    }
  }
  
  return SUCCEEDED;
}


static int _generate_asm_add_sub_or_and_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {
  
  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }
    
    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
    _load_from_ix_to_a(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }
  
  /******************************************************************************************************/
  /* add/sub/or/and data (arg2) -> a */
  /******************************************************************************************************/

  if (op == TAC_OP_ADD) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      if (((int)t->arg2_d) == 1)
        _inc_a(file_out);
      else
        _add_value_to_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _add_from_ix_to_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_SUB) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      if (((int)t->arg2_d) == 1)
        _dec_a(file_out);
      else
        _sub_value_from_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _sub_from_ix_from_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_OR) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      _or_value_to_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _or_from_ix_to_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_AND) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
      /* 8-bit */
      _and_value_from_a(((int)t->arg2_d) & 0xff, file_out);
    }
    else {
      /* 8-bit */
      _and_from_ix_from_a(ix_offset, file_out);
    }
  }
  else {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Unsupported TAC op %d! Please submit a bug report!\n", op);
    return FAILED;
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }
    
    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */

    /* lower byte */
    _load_a_into_ix(ix_offset, file_out);

    /* sign extend 8-bit -> 16-bit? */
    if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                      t->arg2_var_type == VARIABLE_TYPE_INT8)) {
      /* yes */
      _sign_extend_a_into_ix(ix_offset + 1, file_out);
    }
    else {
      /* upper byte = 0 */
      _load_value_into_ix(0, ix_offset + 1, file_out);
    }
  }
  else {
    /* 8-bit */
    _load_a_into_ix(ix_offset, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_add_sub_or_and_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {
  
  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }
    
    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
      _load_from_ix_to_l(ix_offset, file_out);
      _load_from_ix_to_h(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(ix_offset, file_out);

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

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (arg2) -> bc */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    if ((op == TAC_OP_ADD || op == TAC_OP_SUB) && ((int)t->arg2_d) == 1) {
    }
    else
      _load_value_to_bc((int)t->arg2_d, file_out);
  }
  else {
    if (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(ix_offset, file_out);

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

  if (op == TAC_OP_ADD) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 1)
      _inc_hl(file_out);
    else
      _add_bc_to_hl(file_out);
  }
  else if (op == TAC_OP_SUB) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 1)
      _dec_hl(file_out);
    else
      _sub_bc_from_hl(file_out);
  }
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

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_16bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_ix(ix_offset, file_out);
    _load_h_into_ix(ix_offset + 1, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
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

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_hl(t->arg1_node->children[1]->label, file_out);
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
      _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
    }
    else {
      /* it's a variable in the frame! */
      fprintf(file_out, "      ; offset %d\n", arg2_offset);

      if (arg2_offset >= -128 && arg2_offset <= 126) {
        ix_offset = arg2_offset;
        arg2_offset = 0;
      }

      _load_de_with_offset_to_ix(arg2_offset, file_out);
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
        _load_from_ix_to_c(ix_offset, file_out);
        _load_from_ix_to_b(ix_offset + 1, file_out);
      }
      else {
        /* 8-bit */
        _load_from_ix_to_c(ix_offset, file_out);

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

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_get_address_z80(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_ix(ix_offset, file_out);
    _load_h_into_ix(ix_offset + 1, file_out);
  }
  else {
    fprintf(stderr, "_generate_asm_get_address_z80(): The target cannot be an 8-bit value!\n");
    return FAILED;
  }
  
  return SUCCEEDED;
}


static int _generate_asm_z80_in_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, arg2_offset = -1, ix_offset = 0;

  /* result */

  if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
    return FAILED;
  
  /* arg2 */

  if (find_stack_offset(t->arg2_type, t->arg2_s, t->arg2_d, t->arg2_node, &arg2_offset, function_node) == FAILED)
    return FAILED;
  
  /* generate asm */

  /******************************************************************************************************/
  /* index address (arg2) -> ix */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> c */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else {
    _load_from_ix_to_c(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* in a, (?) / in a, (c) */
  /******************************************************************************************************/

  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT)
    _in_a_from_value(((int)t->arg2_d) & 0xff, file_out);
  else
    _in_a_from_c(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/
  
  ix_offset = 0;

  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_z80_in_read_z80(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_a_into_ix(ix_offset, file_out);
    _load_value_into_ix(0, ix_offset + 1, file_out);
  }
  else {
    /* 8-bit */
    _load_a_into_ix(ix_offset, file_out);
  }
  
  return SUCCEEDED;
}


static int _generate_asm_array_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, var_type, ix_offset = 0, iy_offset = 0;

  /* OVERRIDE for __z80_in */
  if (t->arg1_type == TAC_ARG_TYPE_LABEL && strcmp(t->arg1_s, "__z80_in") == 0)
    return _generate_asm_z80_in_read_z80(t, file_out, function_node);

  /* error for a read from __z80_out */
  if (t->arg1_type == TAC_ARG_TYPE_LABEL && strcmp(t->arg1_s, "__z80_out") == 0) {
    g_current_filename_id = t->file_id;
    g_current_line_number = t->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_arm_array_read_z80(): You cannot read from \"__z80_out\"!\n");
    print_error(g_error_message, ERROR_ERR);

    return FAILED;
  }
  
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
  /* index address (arg2) -> ix */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> bc */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    if (((int)t->arg2_d) != 0)
      _load_value_to_bc((int)t->arg2_d, file_out);
  }
  else {
    if (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(ix_offset, file_out);

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
  /* source address (arg1) -> iy */
  /******************************************************************************************************/

  iy_offset = 0;
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Source cannot be a value!\n");
    return FAILED;
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (t->arg1_node->children[0]->value_double > 0.0) {
      if (arg1_offset >= -128 && arg1_offset <= 126) {
        iy_offset = arg1_offset;
        arg1_offset = 0;
      }
    }

    _load_value_to_iy(arg1_offset, file_out);
    _add_de_to_iy(file_out);
  }

  /******************************************************************************************************/
  /* is the source address actually an address of a pointer? */
  /******************************************************************************************************/
  
  if (t->arg1_node->children[0]->value_double > 0.0) {
    /* yes -> get the real address */

    /******************************************************************************************************/
    /* copy data (iy) -> bc -> iy */
    /******************************************************************************************************/

    if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0))
      _push_bc(file_out);
    
    /* 16-bit */
    _load_from_iy_to_c(iy_offset, file_out);
    _load_from_iy_to_b(iy_offset + 1, file_out);

    _load_value_to_iy(0, file_out);
    _add_bc_to_iy(file_out);

    if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0))
      _pop_bc(file_out);
  }
  
  /******************************************************************************************************/
  /* add bc -> iy */
  /******************************************************************************************************/

  /* NOTE! var_type is actually the type of the item we have inside arg1 array */
  var_type = get_array_item_variable_type(t->arg1_node->children[0], t->arg1_node->children[0]->label);
  
  if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0)) {
    _add_bc_to_iy(file_out);

    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* the array has 16-bit items -> add bc -> iy twice! */
      _add_bc_to_iy(file_out);
    }
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

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_ix(ix_offset, file_out);
    _load_h_into_ix(ix_offset + 1, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_z80_out_write_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int arg1_offset = -1, arg2_offset = -1, ix_offset = 0;

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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> a */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    _load_value_to_a(((int)t->arg1_d) & 0xff, file_out);
  }
  else {
    _load_from_ix_to_a(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* index address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> c */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else {
    _load_from_ix_to_c(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* out (?), a / out (c), a */
  /******************************************************************************************************/

  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT)
    _out_a_into_value(((int)t->arg2_d) & 0xff, file_out);
  else
    _out_a_into_c(file_out);  

  return SUCCEEDED;
}


static int _generate_asm_array_assignment_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, var_type, ix_offset = 0, iy_offset = 0;

  /* OVERRIDE for __z80_out */
  if (t->result_type == TAC_ARG_TYPE_LABEL && strcmp(t->result_s, "__z80_out") == 0)
    return _generate_asm_z80_out_write_z80(t, file_out, function_node);

  /* error for a write to __z80_in */
  if (t->result_type == TAC_ARG_TYPE_LABEL && strcmp(t->result_s, "__z80_in") == 0) {
    g_current_filename_id = t->file_id;
    g_current_line_number = t->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "_generate_arm_array_assignment_z80(): You cannot write to \"__z80_in\"!\n");
    print_error(g_error_message, ERROR_ERR);

    return FAILED;
  }
  
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
  /* index address (arg2) -> ix */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> bc */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 16-bit */
    if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0))
      _load_value_to_bc((int)t->arg2_d, file_out);
  }
  else {
    if (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(ix_offset, file_out);

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

  iy_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_iy(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (t->result_node->children[0]->value_double > 0.0) {
      if (result_offset >= -128 && result_offset <= 126) {
        iy_offset = result_offset;
        result_offset = 0;
      }
    }

    _load_value_to_iy(result_offset, file_out);
    _add_de_to_iy(file_out);
  }

  /******************************************************************************************************/
  /* is the result address actually an address of a pointer? */
  /******************************************************************************************************/
  
  if (t->result_node->children[0]->value_double > 0.0) {
    /* yes -> get the real address */

    /******************************************************************************************************/
    /* copy data (iy) -> bc -> iy */
    /******************************************************************************************************/

    if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0))
      _push_bc(file_out);
    
    /* 16-bit */
    _load_from_iy_to_c(iy_offset, file_out);
    _load_from_iy_to_b(iy_offset + 1, file_out);

    _load_value_to_iy(0, file_out);
    _add_bc_to_iy(file_out);

    if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0))
      _pop_bc(file_out);
  }
  
  /******************************************************************************************************/
  /* add index bc -> iy */
  /******************************************************************************************************/

  /* NOTE! var_type is actually the type of the item we have inside result array */
  var_type = get_array_item_variable_type(t->result_node->children[0], t->result_node->children[0]->label);

  if (!(t->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)t->arg2_d) == 0)) {
    _add_bc_to_iy(file_out);

    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* the array has 16-bit items -> add bc -> ix twice! */
      _add_bc_to_iy(file_out);
    }
  }
  
  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }
    
    _load_de_with_offset_to_ix(arg1_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> hl */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    int value = (int)t->arg1_d;
    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
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
      _load_from_ix_to_l(ix_offset, file_out);
      _load_from_ix_to_h(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(ix_offset, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (var_type == VARIABLE_TYPE_INT16 ||
                                                     var_type == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_l_to_hl(file_out);
      }
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (var_type == VARIABLE_TYPE_INT16 ||
                                                           var_type == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_h(0, file_out);
      }
      else if ((t->arg1_var_type == VARIABLE_TYPE_UINT8 || t->arg1_var_type == VARIABLE_TYPE_INT8) &&
               (var_type == VARIABLE_TYPE_INT8 || var_type == VARIABLE_TYPE_UINT8)) {
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

  if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
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

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
      _load_from_ix_to_l(ix_offset, file_out);
      _load_from_ix_to_h(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(ix_offset, file_out);

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

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
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
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(ix_offset, file_out);

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

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_shift_left_right_z80_16bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_ix(ix_offset, file_out);
    _load_h_into_ix(ix_offset + 1, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
  }
  
  return SUCCEEDED;
}


static int _generate_asm_shift_left_right_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
    _load_from_ix_to_b(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
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
    _load_from_ix_to_a(ix_offset, file_out);
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

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data b -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */

    /* lower byte */
    _load_b_into_ix(ix_offset, file_out);

    /* sign extend 8-bit -> 16-bit? */
    if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                      t->arg2_var_type == VARIABLE_TYPE_INT8)) {
      /* yes */
      _sign_extend_b_into_ix(ix_offset + 1, file_out);
    }
    else {
      /* upper byte = 0 */
      _load_value_into_ix(0, ix_offset + 1, file_out);
    }
  }
  else {
    /* 8-bit */
    _load_b_into_ix(ix_offset, file_out);
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

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(ix_offset, file_out);

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

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
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
      _load_from_ix_to_e(ix_offset, file_out);
      _load_from_ix_to_d(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_e(ix_offset, file_out);

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
  
  ix_offset = 0;

  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data ca -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_a_into_ix(ix_offset, file_out);
      _load_c_into_ix(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_a_into_ix(ix_offset, file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_MUL || op == TAC_OP_MOD) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_l_into_ix(ix_offset, file_out);
      _load_h_into_ix(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_l_into_ix(ix_offset, file_out);
    }
  }

  return SUCCEEDED;
}


static int _generate_asm_mul_div_mod_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  int result_offset = -1, arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
    _load_from_ix_to_h(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/
  
  ix_offset = 0;

  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

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
    _load_from_ix_to_e(ix_offset, file_out);
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

  ix_offset = 0;
  
  if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->result_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", result_offset);

    if (result_offset >= -128 && result_offset <= 126) {
      ix_offset = result_offset;
      result_offset = 0;
    }

    _load_de_with_offset_to_ix(result_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_MUL) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_l_into_ix(ix_offset, file_out);
      _load_h_into_ix(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_l_into_ix(ix_offset, file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */

      /* lower byte */
      _load_a_into_ix(ix_offset, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8)) {
        /* yes */
        _sign_extend_a_into_ix(ix_offset + 1, file_out);
      }
      else {
        /* upper byte = 0 */
        _load_value_into_ix(0, ix_offset + 1, file_out);
      }
    }
    else {
      /* 8-bit */
      _load_a_into_ix(ix_offset, file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data b -> (ix) */
  /******************************************************************************************************/

  if (op == TAC_OP_MOD) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */

      /* lower byte */
      _load_b_into_ix(ix_offset, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8)) {
        /* yes */
        _sign_extend_b_into_ix(ix_offset + 1, file_out);
      }
      else {
        /* upper byte = 0 */
        _load_value_into_ix(0, ix_offset + 1, file_out);
      }
    }
    else {
      /* 8-bit */
      _load_b_into_ix(ix_offset, file_out);
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

  fprintf(stderr, "_generate_asm_mul_div_mod_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  int arg1_offset = -1, arg2_offset = -1, ix_offset = 0;

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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
      _load_from_ix_to_l(ix_offset, file_out);
      _load_from_ix_to_h(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(ix_offset, file_out);

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
        fprintf(stderr, "_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/
  
  ix_offset = 0;

  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
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
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_c(ix_offset, file_out);

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
        fprintf(stderr, "_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* hl = hl - bc */
  /******************************************************************************************************/

  _sub_bc_from_hl(file_out);

  /******************************************************************************************************/
  /* jumps */
  /******************************************************************************************************/

  if (op == TAC_OP_JUMP_EQ)
    _jump_z_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_NEQ)
    _jump_nz_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_LT)
    _jump_c_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_GT)
    _jump_nc_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_LTE) {
    _jump_z_to(t->result_s, file_out);
    _jump_c_to(t->result_s, file_out);
  }
  else if (op == TAC_OP_JUMP_GTE) {
    _jump_z_to(t->result_s, file_out);
    _jump_nc_to(t->result_s, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  int arg1_offset = -1, arg2_offset = -1, ix_offset = 0;
  
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
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
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
    _load_from_ix_to_a(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg2_type == TAC_ARG_TYPE_LABEL && arg2_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg2_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg2_offset);

    if (arg2_offset >= -128 && arg2_offset <= 126) {
      ix_offset = arg2_offset;
      arg2_offset = 0;
    }

    _load_de_with_offset_to_ix(arg2_offset, file_out);
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> b */
  /******************************************************************************************************/
  
  if (t->arg2_type == TAC_ARG_TYPE_CONSTANT) {
    /* 8-bit */
    _load_value_to_b(((int)t->arg2_d) & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_b(ix_offset, file_out);
  }
  
  /******************************************************************************************************/
  /* a = a - b */
  /******************************************************************************************************/

  _sub_b_from_a(file_out);

  /******************************************************************************************************/
  /* jumps */
  /******************************************************************************************************/

  if (op == TAC_OP_JUMP_EQ)
    _jump_z_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_NEQ)
    _jump_nz_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_LT)
    _jump_c_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_GT)
    _jump_nc_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_LTE) {
    _jump_z_to(t->result_s, file_out);
    _jump_c_to(t->result_s, file_out);
  }
  else if (op == TAC_OP_JUMP_GTE) {
    _jump_z_to(t->result_s, file_out);
    _jump_nc_to(t->result_s, file_out);
  }
  
  return SUCCEEDED;
}


static int _generate_asm_jump_eq_lt_gt_neq_lte_gte_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _generate_asm_return_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  /* ret */

  /* bytes -1 and 0 of stack frame contain the return address */
  fprintf(file_out, "      ; bytes -1 and 0 of stack frame contain the return address\n");
  
  _load_value_to_hl(-1, file_out);
  _add_de_to_hl(file_out);  
  _load_hl_to_sp(file_out);

  _ret(file_out);
  
  return SUCCEEDED;
}


static int _generate_asm_return_value_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int arg1_offset = -1, return_var_type, ix_offset = 0;

  /* arg1 */

  if (find_stack_offset(t->arg1_type, t->arg1_s, t->arg1_d, t->arg1_node, &arg1_offset, function_node) == FAILED)
    return FAILED;

  /* return */

  return_var_type = tree_node_get_max_var_type(function_node->children[0]);

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
  }
  else if (t->arg1_type == TAC_ARG_TYPE_LABEL && arg1_offset == 999999) {
    /* global var */
    _load_label_to_ix(t->arg1_node->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", arg1_offset);

    if (arg1_offset >= -128 && arg1_offset <= 126) {
      ix_offset = arg1_offset;
      arg1_offset = 0;
    }

    _load_de_with_offset_to_ix(arg1_offset, file_out);
  }
  
  /******************************************************************************************************/
  /* copy data (arg1) -> hl/l */
  /******************************************************************************************************/
  
  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_value_to_hl((int)t->arg1_d, file_out);
    }
    else {
      /* 8-bit */
      _load_value_to_l(((int)t->arg1_d) & 0xff, file_out);
    }
  }
  else {
    if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
      if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
        /* 16-bit */
        _load_from_ix_to_l(ix_offset, file_out);
        _load_from_ix_to_h(ix_offset + 1, file_out);
      }
      else {
        /* 8-bit */
        _load_from_ix_to_l(ix_offset, file_out);
      }
    }
    else {
      if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
        /* 8-bit -> 16bit */
        _load_from_ix_to_l(ix_offset, file_out);

        /* sign extend 8-bit -> 16-bit? */
        if (t->arg1_var_type == VARIABLE_TYPE_INT8) {
          /* yes */
          _sign_extend_l_to_hl(file_out);
        }
        else if (t->arg1_var_type == VARIABLE_TYPE_UINT8) {
          /* upper byte -> 0 */
          _load_value_to_h(0, file_out);
        }
        else {
          fprintf(stderr, "_generate_asm_return_value_z80(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
          return FAILED;
        }
      }
      else {
        /* 8-bit */
        _load_from_ix_to_l(ix_offset, file_out);
      }
    }
  }

  /******************************************************************************************************/
  /* copy return data hl/l -> stack frame */
  /******************************************************************************************************/

  _load_de_with_offset_to_ix(0, file_out);
  
  if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    _load_l_into_ix(-5, file_out);
    _load_h_into_ix(-4, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_ix(-4, file_out);
  }
  
  /******************************************************************************************************/
  /* return */
  /******************************************************************************************************/

  if (_generate_asm_return_z80(t, file_out, function_node) == FAILED)
    return FAILED;
  
  return SUCCEEDED;
}


static int _generate_asm_function_call_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  int j, argument, reset_iy = YES, init_ix = NO;
  char return_label[32];
  
  /* __pureasm function calls are very simple */
  if ((t->arg1_node->flags & TREE_NODE_FLAG_PUREASM) == TREE_NODE_FLAG_PUREASM) {
    _call_to(t->arg1_node->children[1]->label, file_out);

    return SUCCEEDED;
  }

  /* sp -> hl (the new stack frame address) */
  _load_sp_to_hl(file_out);
  _dec_hl(file_out);

  /* address of return address in stack frame -> bc */
  snprintf(return_label, sizeof(return_label), "_return_%d", g_return_id++);
  _load_label_to_bc(return_label, file_out);

  /* address of return address in stack frame -> (stack) */
  _push_bc(file_out);

  /* old stack frame address -> (stack) */
  _push_de(file_out);

  /* copy arguments to stack frame */
  argument = 0;
  for (j = 2; j < t->arg1_node->added_children; j += 2) {
    struct tree_node *type_node = t->arg1_node->children[j];

    if (type_node->type == TREE_NODE_TYPE_BLOCK) {
      /* the end of arguments */
      if (argument != t->arg1_node->local_variables->arguments_count) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Function \"%s\" was not called with enough arguments! Please submit a bug report!\n", t->arg1_node->children[1]->label);
        return FAILED;
      }

      /* all done */
      break;
    }
    else if (type_node->type == TREE_NODE_TYPE_VARIABLE_TYPE) {
      int source_var_type, source_offset, target_var_type, target_offset;
      struct tree_node *name_node = t->arg1_node->children[j+1];
      struct local_variable *local_variable;
      struct function_argument *arg;
      
      if (name_node->type != TREE_NODE_TYPE_VALUE_STRING) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Corrupted function \"%s\" definition (A)! Please submit a bug report!\n", t->arg1_node->children[1]->label);
        return FAILED;
      }
      if (argument >= t->arg1_node->local_variables->arguments_count) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Trying to access argument number %d, but the function \"%s\" is defined to have only %d arguments! Please submit a bug report!\n", argument + 1, t->arg1_node->children[1]->label, t->arg1_node->local_variables->arguments_count);
        return FAILED;
      }

      local_variable = &(t->arg1_node->local_variables->local_variables[argument]);

      if (strcmp(name_node->label, local_variable->node->children[1]->label) != 0) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Corrupted function \"%s\" definition (C)! Was expecting that argument %d was \"%s\", but it was \"%s\" instead. Please submit a bug report!\n", t->arg1_node->children[1]->label, argument + 1, name_node->label, local_variable->node->children[1]->label);
        return FAILED;
      }

      /* start copying the argument to new stack frame */
      arg = &(t->arguments[argument]);
      
      /* find the source offset in the old stack frame */
      if (find_stack_offset(arg->type, arg->label, arg->value, arg->node, &source_offset, function_node) == FAILED)
        return FAILED;

      source_var_type = arg->var_type;
      target_var_type = tree_node_get_max_var_type(local_variable->node->children[0]);
      target_offset = local_variable->offset_to_fp;

      if (init_ix == NO) {
        /* load the new stack frame address -> ix */
        init_ix = YES;

        fprintf(file_out, "      ; new stack frame -> IX\n");

        _load_value_to_ix(0, file_out);
        _load_hl_to_bc(file_out);
        _add_bc_to_ix(file_out);
      }

      if (arg->type == TAC_ARG_TYPE_CONSTANT) {
      }
      else if (arg->type == TAC_ARG_TYPE_LABEL && source_offset == 999999) {
      }
      else {
        /* it's a variable in the frame! */      
        if (reset_iy == YES && source_offset > -127) {
          /* load the old stack frame address -> iy */
          reset_iy = NO;

          fprintf(file_out, "      ; old stack frame -> IY\n");

          _load_value_to_iy(0, file_out);
          _add_de_to_iy(file_out);
        }

        /* NOTE! if the offset is <= -127 then we need extra instructions */
        if (source_offset <= -127) {
          /* load the old stack frame address -> iy */
          reset_iy = YES;

          fprintf(file_out, "      ; old stack frame with offset -> IY\n");

          _load_value_to_iy(source_offset, file_out);
          _add_de_to_iy(file_out);

          source_offset = 0;
        }
      }
      
      /******************************************************************************************************/
      /* copy data (reg) -> bc */
      /******************************************************************************************************/

      fprintf(file_out, "      ; copy argument %d\n", argument + 1);

      if (arg->type == TAC_ARG_TYPE_LABEL && source_offset == 999999) {
        /* global var */
        _load_label_to_iy(arg->node->children[1]->label, file_out);
        reset_iy = YES;
        source_offset = 0;
      }
      
      if (arg->type == TAC_ARG_TYPE_CONSTANT) {
      }
      else {
        if (source_var_type == VARIABLE_TYPE_INT16 || source_var_type == VARIABLE_TYPE_UINT16) {
          /* 16-bit */        
          _load_from_iy_to_c(source_offset, file_out);
          _load_from_iy_to_b(source_offset + 1, file_out);
        }
        else {
          /* 8-bit */
          _load_from_iy_to_c(source_offset, file_out);

          /* sign extend 8-bit -> 16-bit? */
          if (source_var_type == VARIABLE_TYPE_INT8 && (target_var_type == VARIABLE_TYPE_INT16 ||
                                                        target_var_type == VARIABLE_TYPE_UINT16)) {
            /* yes */
            _sign_extend_c_to_bc(file_out);
          }
          else if (source_var_type == VARIABLE_TYPE_UINT8 && (target_var_type == VARIABLE_TYPE_INT16 ||
                                                              target_var_type == VARIABLE_TYPE_UINT16)) {
            /* upper byte -> 0 */
            _load_value_to_b(0, file_out);
          }
        }
      }

      /******************************************************************************************************/
      /* copy data bc/c -> (ix) */
      /******************************************************************************************************/

      if (arg->type == TAC_ARG_TYPE_CONSTANT) {
        if (target_var_type == VARIABLE_TYPE_INT16 || target_var_type == VARIABLE_TYPE_UINT16) {
          /* 16-bit */
          _load_value_into_ix(((int)arg->value) & 0xff, target_offset, file_out);
          _load_value_into_ix((((int)arg->value) >> 8) & 0xff, target_offset + 1, file_out);
        }
        else {
          /* 8-bit */
          _load_value_into_ix(((int)arg->value) & 0xff, target_offset, file_out);
        }
      }
      else {
        if (target_var_type == VARIABLE_TYPE_INT16 || target_var_type == VARIABLE_TYPE_UINT16) {
          /* 16-bit */
          _load_c_into_ix(target_offset, file_out);
          _load_b_into_ix(target_offset + 1, file_out);
        }
        else {
          /* 8-bit */
          _load_c_into_ix(target_offset, file_out);
        }
      }

      argument++;
    }
    else {
      fprintf(stderr, "_generate_asm_function_call_z80(): Corrupted function \"%s\" definition (B)! Unknown node type %d! Please submit a bug report!\n", t->arg1_node->children[1]->label, type_node->type);
      return FAILED;
    }
  }

  /*
  fprintf(stderr, "FUNCTION CALL %s TOTAL OFFSET %d\n", t->arg1_node->children[1]->label, t->arg1_node->local_variables->offset_to_fp_total);
  */
  
  fprintf(file_out, "      ; new stack frame -> DE\n");

  /* hl (new stack frame) -> de */
  _load_hl_to_de(file_out);

  /* jump! */
  _jump_to(t->arg1_node->children[1]->label, file_out);

  /* return label (used to get the return address) */
  _add_label(return_label, file_out, NO);
  
  /* go back to the old stack frame */

  fprintf(file_out, "      ; new stack frame -> IX\n");
  
  /* stack frame -> ix */
  _load_de_with_offset_to_ix(0, file_out);

  if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    int return_var_type = tree_node_get_max_var_type(t->arg1_node->children[0]);

    if (return_var_type == VARIABLE_TYPE_NONE || return_var_type == VARIABLE_TYPE_VOID) {
      fprintf(stderr, "_generate_asm_function_call_z80(): Function \"%s\" doesn't return a value yet we are trying to use the return value!\n", t->arg1_node->children[1]->label);
      return FAILED;
    }
    
    /* copy back the return value */

    /******************************************************************************************************/
    /* return value -> hl/l */
    /******************************************************************************************************/

    if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16)
      fprintf(file_out, "      ; return value -> HL\n");
    else
      fprintf(file_out, "      ; return value -> L\n");
    
    if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_l(-5, file_out);
      _load_from_ix_to_h(-4, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(-4, file_out);

      /* sign extend 8-bit -> 16-bit? */
      if (return_var_type == VARIABLE_TYPE_INT8 && (t->result_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                    t->result_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* yes */
        _sign_extend_l_to_hl(file_out);
      }
      else if (return_var_type == VARIABLE_TYPE_UINT8 && (t->result_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                          t->result_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        /* upper byte -> 0 */
        _load_value_to_h(0, file_out);
      }
    }
  }
  
  fprintf(file_out, "      ; old stack frame address -> DE\n");

  /* old stack frame address -> de */
  _load_from_ix_to_e(-3, file_out);
  _load_from_ix_to_d(-2, file_out);

  if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    int result_offset = -1;

    if (find_stack_offset(t->result_type, t->result_s, t->result_d, t->result_node, &result_offset, function_node) == FAILED)
      return FAILED;

    fprintf(file_out, "      ; copy return value to its destination\n");

    /******************************************************************************************************/
    /* target address -> iy */
    /******************************************************************************************************/
  
    if (t->result_type == TAC_ARG_TYPE_CONSTANT) {
    }
    else if (t->result_type == TAC_ARG_TYPE_LABEL && result_offset == 999999) {
      /* global var */
      _load_label_to_iy(t->result_node->children[1]->label, file_out);
    }
    else {
      /* it's a variable in the frame! */
      fprintf(file_out, "      ; offset %d\n", result_offset);

      _load_value_to_iy(result_offset, file_out);
      _add_de_to_iy(file_out);
    }

    /******************************************************************************************************/
    /* copy data hl/l -> (iy) */
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
  }
  
  return SUCCEEDED;
}


static int _generate_asm_inline_asm_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, struct asm_line *al, int file_id) {

  int offset = -1, ix_offset = 0;

  if (find_stack_offset(TAC_ARG_TYPE_LABEL, al->variable->children[1]->label, 0, al->variable, &offset, function_node) == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* source address -> ix */
  /******************************************************************************************************/

  if (offset == 999999) {
    /* global var */
    _load_label_to_ix(al->variable->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", offset);

    if (offset >= -128 && offset <= 126) {
      ix_offset = offset;
      offset = 0;
    }

    _load_de_with_offset_to_ix(offset, file_out);
  }
  
  /******************************************************************************************************/
  /* copy data -> target register */
  /******************************************************************************************************/

  if (al->cpu_register[1] == 0) {
    /* target is A/B/C/D/E/H/L */
    _load_from_ix_to_register(ix_offset, al->cpu_register[0], file_out);
  }
  else {
    /* target is BC/DE/HL */
    int var_type;

    var_type = tree_node_get_max_var_type(al->variable->children[0]);

    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_from_ix_to_register(ix_offset, al->cpu_register[1], file_out);
      _load_from_ix_to_register(ix_offset + 1, al->cpu_register[0], file_out);
    }
    else {
      fprintf(stderr, "%s:%d: _generate_asm_inline_asm_read_z80(): Reading an 8-bit variable into 16-bit register pair! We only read the lower 8 bits, remember to take care of the upper 8 bits!\n", get_file_name(file_id), al->line_number);
      _load_from_ix_to_register(ix_offset, al->cpu_register[1], file_out);
    }
  }

  return SUCCEEDED;
}


static int _generate_asm_inline_asm_write_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, struct asm_line *al, int file_id) {

  int offset = -1, ix_offset = 0;

  if (find_stack_offset(TAC_ARG_TYPE_LABEL, al->variable->children[1]->label, 0, al->variable, &offset, function_node) == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (offset == 999999) {
    /* global var */
    _load_label_to_ix(al->variable->children[1]->label, file_out);
  }
  else {
    /* it's a variable in the frame! */
    fprintf(file_out, "      ; offset %d\n", offset);

    if (offset >= -128 && offset <= 126) {
      ix_offset = offset;
      offset = 0;
    }

    _load_de_with_offset_to_ix(offset, file_out);
  }
  
  /******************************************************************************************************/
  /* copy data -> target address */
  /******************************************************************************************************/

  if (al->cpu_register[1] == 0) {
    /* source is A/B/C/D/E/H/L */
    _load_register_into_ix(ix_offset, al->cpu_register[0], file_out);
  }
  else {
    /* source is BC/DE/HL */
    int var_type;

    var_type = tree_node_get_max_var_type(al->variable->children[0]);

    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_register_into_ix(ix_offset, al->cpu_register[1], file_out);
      _load_register_into_ix(ix_offset + 1, al->cpu_register[0], file_out);
    }
    else {
      fprintf(stderr, "%s:%d: _generate_asm_inline_asm_write_z80(): Writing a 16-bit value into an 8-bit variable! We only write the lower 8 bits...\n", get_file_name(file_id), al->line_number);
      _load_register_into_ix(ix_offset, al->cpu_register[1], file_out);
    }
  }

  return SUCCEEDED;
}


static int _generate_asm_inline_asm_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct inline_asm *ia;
  struct asm_line *al;

  ia = inline_asm_find(t->result_d);
  if (ia == NULL)
    return FAILED;

  al = ia->asm_line_first;
  while (al != NULL) {
    if (al->flags == 0) {
      /* no outside access -> output as it is */
      int i, length;

      /* skip white space */
      length = strlen(al->line);
      for (i = 0; i < length && (al->line[i] == ' ' || al->line[i] == '\t'); i++)
        ;

      fprintf(file_out, "      %s\n", &al->line[i]);
    }
    else if ((al->flags & ASM_LINE_FLAG_READ) == ASM_LINE_FLAG_READ) {
      /* read! */
      if (_generate_asm_inline_asm_read_z80(t, file_out, function_node, al, ia->file_id) == FAILED)
        return FAILED;
    }
    else if ((al->flags & ASM_LINE_FLAG_WRITE) == ASM_LINE_FLAG_WRITE) {
      /* write! */
      if (_generate_asm_inline_asm_write_z80(t, file_out, function_node, al, ia->file_id) == FAILED)
        return FAILED;
    }
    else {
      fprintf(stderr, "%s:%d: _generate_asm_inline_asm_z80(): Unhandled internal flags! Please submit a bug report!\n", get_file_name(ia->file_id), al->line_number);
      return FAILED;
    }
    
    al = al->next;
  }
  
  return SUCCEEDED;
}


static int _add_const_variables(struct tree_node *const_variables[256], int const_variables_count, FILE *file_out) {

  int i, j, k, last, element_size, element_type;

  for (i = 0; i < const_variables_count; i++) {
    struct tree_node *node = const_variables[i];

    /* turn the label into ASM's local label so that it cannot be accessed from outside. PS. the label's name has already
       been turned into a local label in pass_5/collect_and_preprocess_local_variables_inside_functions() */
    _add_label(node->children[1]->label, file_out, NO);

    element_type = tree_node_get_max_var_type(node->children[0]);
    element_size = get_variable_type_size(element_type);

    if (element_size != 8 && element_size != 16) {
      fprintf(stderr, "_add_const_variables(): Unsupported global variable \"%s\" size %d! Please submit a bug report!\n", node->children[1]->label, element_size);
      return FAILED;
    }

    /* check element types */
    if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == TREE_NODE_FLAG_DATA_IS_CONST) {
      for (j = 2; j < node->added_children; j++) {
        if (node->children[j]->type != TREE_NODE_TYPE_VALUE_INT &&
            node->children[j]->type != TREE_NODE_TYPE_VALUE_DOUBLE &&
            node->children[j]->type != TREE_NODE_TYPE_BYTES) {
          g_current_filename_id = node->file_id;
          g_current_line_number = node->line_number;
          snprintf(g_error_message, sizeof(g_error_message), "_add_const_variables(): Const variable (\"%s\") can only be initialized with an immediate number!\n", node->children[1]->label);
          print_error(g_error_message, ERROR_ERR);

          return FAILED;
        }
      }
    }

    if (element_size == 8)
      fprintf(file_out, "      .DB ");
    else if (element_size == 16)
      fprintf(file_out, "      .DW ");

    /* calculate how many constants there are in the array */
    last = -1;
    for (k = 0; k < node->added_children - 2; k++) {
      if (tree_node_is_expression_just_a_constant(node->children[2 + k]) == YES)
        last = k;
    }

    for (j = 2; j - 2 <= last && j < node->added_children; j++) {
      if (j > 2)
        fprintf(file_out, ", ");
        
      if (node->children[j]->type == TREE_NODE_TYPE_VALUE_INT)
        fprintf(file_out, "%d", node->children[j]->value);
      else if (node->children[j]->type == TREE_NODE_TYPE_VALUE_DOUBLE)
        fprintf(file_out, "%d", (int)(node->children[j]->value));
      else if (node->children[j]->type == TREE_NODE_TYPE_BYTES) {
        for (k = 0; k < node->children[j]->value; k++) {
          if (k > 0)
            fprintf(file_out, ", ");
          fprintf(file_out, "%d", node->children[j]->label[k]);
        }
      }
      else {
        /* non-constants are 0 here, but they are overwritten later */
        fprintf(file_out, "%d", 0);
      }
    }

    fprintf(file_out, "\n");
  }
  
  return SUCCEEDED;
}


static int _copy_non_const_array_constants(struct tac *t, struct tree_node *node, int items, struct tree_node *function_node, FILE *file_out) {

  char copy_function_name[MAX_NAME_LENGTH+1];
  int offset, size, type;

  if (find_stack_offset(TAC_ARG_TYPE_LABEL, node->children[1]->label, 0, node, &offset, function_node) == FAILED)
    return FAILED;

  type = get_array_item_variable_type(node->children[0], node->children[1]->label);
  size = get_variable_type_size(type) / 8;
  
  _push_de(file_out);
  
  /* target address -> hl */
  _load_value_to_hl(offset, file_out);
  _add_de_to_hl(file_out);
  
  /* source address -> de */
  _load_label_to_de(node->children[1]->label, file_out);

  /* counter -> bc */
  _load_value_to_bc(size * items, file_out);

  /* call */
  snprintf(copy_function_name, sizeof(copy_function_name), "copy_bytes_bank_%.3d", g_bank);
  _call_to(copy_function_name, file_out);

  _pop_de(file_out);
  
  return SUCCEEDED;
}


int generate_asm_z80(FILE *file_out) {

  int i, file_id = -1, line_number = -1, const_variables_count;
  struct tree_node *const_variables[256];
  
  fprintf(file_out, "  .BANK %d SLOT %d\n", g_bank, g_slot);
  fprintf(file_out, "  .ORG $0000\n");
  fprintf(file_out, "\n");
  
  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    int op = t->op;

    if (op == TAC_OP_LABEL && t->is_function == YES) {
      /* function start! */
      struct tree_node *function_node = t->function_node;

      fprintf(file_out, "  .SECTION \"%s\" FREE\n", function_node->children[1]->label);

      file_id = t->file_id;
      line_number = t->line_number;

      fprintf(file_out, "    ; =================================================================\n");
      if (file_id == -1 && line_number == -1) {
        /* these TACs were generated from symbols that were generated by the compiler thus no line or file */
        fprintf(file_out, "    ; INTERNAL");
      }
      else {
        fprintf(file_out, "    ; %s:%d: ", get_file_name(file_id), line_number);
        get_source_line(file_id, line_number, file_out);
      }
      fprintf(file_out, "\n");
      fprintf(file_out, "    ; =================================================================\n");

      _add_label(function_node->children[1]->label, file_out, NO);

      if ((function_node->flags & TREE_NODE_FLAG_PUREASM) == TREE_NODE_FLAG_PUREASM) {
      }
      else {
        fprintf(file_out, "      ; A  - tmp\n");
        fprintf(file_out, "      ; BC - tmp\n");
        fprintf(file_out, "      ; DE - frame pointer\n");
        fprintf(file_out, "      ; HL - tmp\n");
        fprintf(file_out, "      ; SP - stack pointer\n");
        fprintf(file_out, "      ; IX - tmp\n");
        fprintf(file_out, "      ; IY - tmp\n");

        if (strcmp(function_node->children[1]->label, "mainmain") != 0) {
          /* allocate stack space for the stack frame */
          fprintf(file_out, "      ; allocate space for the stack frame\n");
          _load_value_to_hl(function_node->local_variables->offset_to_fp_total + 1, file_out);
          _add_de_to_hl(file_out);  
          _load_hl_to_sp(file_out);
        }
      }

      const_variables_count = 0;
      
      for (i = i + 1; i < g_tacs_count; i++) {
        t = &g_tacs[i];
        op = t->op;

        if (op == TAC_OP_DEAD)
          continue;

        /* IL -> ASM */

        if (op == TAC_OP_LABEL) {
          /* NOTE! for _load_de_with_offset_to_ix() optimization every time we touch IX (or create a label)
             we need to set the variable to say that IX is no longer DE */
          g_is_ix_de = NO;
        }

        if (op == TAC_OP_LABEL && t->is_function == YES) {
          i--;
          break;
        }

        if (t->file_id != file_id || t->line_number != line_number) {
          /* the source code line has changed, print info about the new line */
          if (t->file_id == file_id && t->line_number < line_number) {
            /* we don't go back in the same file */
          }
          else {
            file_id = t->file_id;
            line_number = t->line_number;
            fprintf(file_out, "      ; =================================================================\n");
            fprintf(file_out, "      ; %s:%d: ", get_file_name(t->file_id), t->line_number);
            get_source_line(t->file_id, t->line_number, file_out);
            fprintf(file_out, "\n");
            fprintf(file_out, "      ; =================================================================\n");
          }
        }
        
        fprintf(file_out, "      ; -----------------------------------------------------------------\n");
        print_tac(t, YES, file_out);
        fprintf(file_out, "      ; -----------------------------------------------------------------\n");
        
        if (op == TAC_OP_LABEL && t->is_function == NO)
          fprintf(file_out, "    %s:\n", t->result_s);
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
        else if (op == TAC_OP_JUMP_EQ || op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_GT || op == TAC_OP_JUMP_NEQ ||
                 op == TAC_OP_JUMP_LTE || op == TAC_OP_JUMP_GTE) {
          if (_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_RETURN) {
          if (_generate_asm_return_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_RETURN_VALUE) {
          if (_generate_asm_return_value_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_FUNCTION_CALL || op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
          if (_generate_asm_function_call_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_CREATE_VARIABLE) {
          if (t->result_node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
            struct tree_node *node = t->result_node;
            
            if ((node->children[0]->value_double == 0 && (node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1) ||
                (node->children[0]->value_double > 0 && (node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)) {
              node->flags |= TREE_NODE_FLAG_DATA_IS_CONST;
              
              /* collect all const local variables to be placed later at the end of the function */
              if (const_variables_count == 256) {
                fprintf(stderr, "generate_asm_z80(): The function \"%s\" has more than 256 const variables. Please submit a bug report!\n", function_node->children[1]->label);
                return FAILED;
              }
              
              const_variables[const_variables_count++] = node;
            }
            else {
              /* if the variable is in fact an array with more than 3 constants, then we need to bulk copy those constants */
              int constants = 0, size = -1, j, items = 0;

              /* calculate how many constants there are in the array */
              for (j = 2; j < node->added_children; j++) {
                if (tree_node_is_expression_just_a_constant(node->children[j]) == YES) {
                  if (node->children[j]->type == TREE_NODE_TYPE_BYTES) {
                    constants += node->children[j]->value;
                    items += node->children[j]->value;
                  }
                  else {
                    constants++;
                    items++;
                  }
                  
                  size = items;
                }
                else
                  items++;
              }

              if (constants > 3) {
                _copy_non_const_array_constants(t, node, size, function_node, file_out);

                /* the constants in this array need to be available for a bulk copy at the end of the function, just like
                   constant variables... */
                const_variables[const_variables_count++] = node;
              }
            }
          }
        }
        else if (op == TAC_OP_ASM) {
          if (_generate_asm_inline_asm_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else
          fprintf(stderr, "generate_asm_z80(): Unimplemented IL -> Z80 ASM op %d! Please submit a bug report!\n", op);
      }

      /* add the const variables to the end of the function */
      if (_add_const_variables(const_variables, const_variables_count, file_out) == FAILED)
        return FAILED;
      
      fprintf(file_out, "  .ENDS\n\n");
    }
  }
  
  return SUCCEEDED;
}


static void _kill_z80_in_out(void) {

  int i;

  /* we don't want to output __z80_in and __z80_out as normal arrays so we kill them here */
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && (node->type == TREE_NODE_TYPE_CREATE_VARIABLE)) {
      if (strcmp(node->children[1]->label, "__z80_in") == 0 ||
          strcmp(node->children[1]->label, "__z80_out") == 0)
        node->type = TREE_NODE_TYPE_DEAD;
    }
  }
}


static void _get_variable_initialized_size(struct tree_node *node, int *bytes, int *is_fully_initialized) {

  int elements, max_elements, element_type, element_size;

  if (node->value == 0)
    max_elements = 1;
  else
    max_elements = node->value;

  elements = tree_node_get_create_variable_data_items(node);
  
  element_type = tree_node_get_max_var_type(node->children[0]);
  element_size = get_variable_type_size(element_type) / 8;

  if (elements == max_elements)
    *is_fully_initialized = YES;
  else
    *is_fully_initialized = NO;

  *bytes = element_size * elements;
}


int generate_global_variables_z80(char *file_name, FILE *file_out) {

  int i, j, global_variables_found = 0, length, max_length = 0, elements, element_size, element_type, bytes, total_bytes, is_fully_initialized;
  char label_tmp[MAX_NAME_LENGTH + 1];
  struct tree_node *node_1st = NULL;
  
  /* kill __z80_out and __z80_in as they are not normal arrays */
  _kill_z80_in_out();

  fprintf(file_out, "\n");
  
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        length = strlen(node->children[1]->label);
        if (length > max_length)
          max_length = length;
        global_variables_found++;
      }
    }
  }

  /* no global variables? */
  if (global_variables_found == 0)
    return SUCCEEDED;

  /* create .RAMSECTION for global variables */
  fprintf(file_out, "  .RAMSECTION \"global_variables_%s_ram\" BANK %d SLOT %d FREE\n", file_name, g_ram_bank, g_ram_slot);

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        length = strlen(node->children[1]->label);

        fprintf(file_out, "    %s", node->children[1]->label);

        for (j = length; j < max_length + 1; j++)
          fprintf(file_out, " ");

        elements = node->value;
        if (elements == 0)
          elements = 1;
        element_type = tree_node_get_max_var_type(node->children[0]);
        element_size = get_variable_type_size(element_type);

        if (element_size != 8 && element_size != 16) {
          fprintf(stderr, "generate_global_variables_z80(): Unsupported global variable \"%s\" size %d! Please submit a bug report!\n", node->children[1]->label, element_size);
          return FAILED;
        }

        /* check element types */
        for (j = 2; j < node->added_children; j++) {
          if (node->children[j]->type != TREE_NODE_TYPE_VALUE_INT &&
              node->children[j]->type != TREE_NODE_TYPE_VALUE_DOUBLE &&
              node->children[j]->type != TREE_NODE_TYPE_BYTES) {
            g_current_filename_id = node->file_id;
            g_current_line_number = node->line_number;
            snprintf(g_error_message, sizeof(g_error_message), "generate_global_variables_z80(): Global variable (\"%s\") can only be initialized with an immediate number!\n", node->children[1]->label);
            print_error(g_error_message, ERROR_ERR);

            return FAILED;
          }
        }

        if (elements == 1) {
          /* not an array */
          if (element_size == 8)
            fprintf(file_out, "DB");
          else if (element_size == 16)
            fprintf(file_out, "DW");
        }
        else {
          /* an array */
          if (element_size == 8)
            fprintf(file_out, "DSB %d", elements);
          else if (element_size == 16)
            fprintf(file_out, "DSW %d", elements);
        }
        
        fprintf(file_out, "\n");
      }
    }
  }
  
  fprintf(file_out, "  .ENDS\n");

  fprintf(file_out, "\n");
  fprintf(file_out, "  .BANK %d SLOT %d\n", g_bank, g_slot);
  fprintf(file_out, "  .ORG $0000\n");
  fprintf(file_out, "\n");

  /* create .SECTION for global variables */
  fprintf(file_out, "  .SECTION \"global_variables_%s_rom\" FREE\n", file_name);  

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0)
        snprintf(label_tmp, sizeof(label_tmp), "global_variable_rom_%s", node->children[1]->label);
      else
        snprintf(label_tmp, sizeof(label_tmp), "%s", node->children[1]->label);
      _add_label(label_tmp, file_out, NO);

      elements = node->added_children - 2;
      if (elements > 0) {
        element_type = tree_node_get_max_var_type(node->children[0]);
        element_size = get_variable_type_size(element_type);

        if (element_size == 8)
          fprintf(file_out, "      .DB ");
        else if (element_size == 16)
          fprintf(file_out, "      .DW ");

        for (j = 2; j < node->added_children; j++) {
          if (j > 2)
            fprintf(file_out, ", ");
        
          if (node->children[j]->type == TREE_NODE_TYPE_VALUE_INT)
            fprintf(file_out, "%d", node->children[j]->value);
          else if (node->children[j]->type == TREE_NODE_TYPE_VALUE_DOUBLE)
            fprintf(file_out, "%d", (int)(node->children[j]->value));
          else if (node->children[j]->type == TREE_NODE_TYPE_BYTES) {
            int k;

            for (k = 0; k < node->children[j]->value; k++) {
              if (k > 0)
                fprintf(file_out, ", ");
              fprintf(file_out, "%d", node->children[j]->label[k]);
            }
          }
        }

        fprintf(file_out, "\n");
      }
    }
  }
  
  fprintf(file_out, "  .ENDS\n\n");  
  
  /* create .SECTION that copies data from .SECTION to .RAMSECTION */
  fprintf(file_out, "  .SECTION \"global_variables_%s_init\" FREE\n", file_name);

  snprintf(label_tmp, sizeof(label_tmp), "global_variables_%s_init", file_name);
  _add_label(label_tmp, file_out, NO);  

  /* 1. copy the data of all fully initialized non-const global variables in one call */
  total_bytes = 0;
  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        if (node_1st == NULL)
          node_1st = node;
      
        _get_variable_initialized_size(node, &bytes, &is_fully_initialized);
        if (is_fully_initialized == NO)
          break;

        total_bytes += bytes;
      }
    }
  }

  if (total_bytes > 0) {
    char copy_function_name[MAX_NAME_LENGTH+1];
    
    fprintf(file_out, "      ; copy all fully initialized global variables in a single call\n");
    
    /* target address -> hl */
    _load_label_to_hl(node_1st->children[1]->label, file_out);

    /* source address -> de */
    snprintf(label_tmp, sizeof(label_tmp), "global_variable_rom_%s", node_1st->children[1]->label);
    _load_label_to_de(label_tmp, file_out);

    /* counter -> bc */
    _load_value_to_bc(total_bytes, file_out);

    /* call */
    snprintf(copy_function_name, sizeof(copy_function_name), "copy_bytes_bank_%.3d", g_bank);
    _call_to(copy_function_name, file_out);
  }

  /* 2. copy the data of all partially initialized non_const global variables in multiple calls */
  for (; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        if (node_1st == NULL)
          node_1st = node;
      
        _get_variable_initialized_size(node, &bytes, &is_fully_initialized);
        if (bytes > 0) {
          char copy_function_name[MAX_NAME_LENGTH+1];

          fprintf(file_out, "      ; copy partially initialized global variable \"%s\"\n", node->children[1]->label);
    
          /* target address -> hl */
          _load_label_to_hl(node->children[1]->label, file_out);

          /* source address -> de */
          snprintf(label_tmp, sizeof(label_tmp), "global_variable_rom_%s", node->children[1]->label);
          _load_label_to_de(label_tmp, file_out);

          /* counter -> bc */
          _load_value_to_bc(bytes, file_out);

          /* call */
          snprintf(copy_function_name, sizeof(copy_function_name), "copy_bytes_bank_%.3d", g_bank);
          _call_to(copy_function_name, file_out);
        }
      }
    }
  }
  
  _ret(file_out);

  fprintf(file_out, "  .ENDS\n\n");
  
  return SUCCEEDED;
}
