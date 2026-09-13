
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
#include "pass_5.h"
#include "pass_6_z80.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "il.h"
#include "tac.h"
#include "inline_asm.h"
#include "struct_item.h"
#include "register_allocator.h"
#include "z80_register_spill.h"


extern struct tree_node *g_global_nodes;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern struct tac *g_tacs;
extern int g_tacs_count, g_tacs_max, g_bank, g_slot, g_ram_bank, g_ram_slot;
extern int g_allocator_enabled;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

static int g_return_id = 1, g_is_ix_de = NO;

#define DEBUG_PASS_6_REGISTER_ALLOCATOR 1

#define Z80_GLOBAL_OFFSET 999999

#define LOC_CONST       RA_LOCATION_CONST
#define LOC_GLOBAL      RA_LOCATION_GLOBAL
#define LOC_STACK_LOCAL RA_LOCATION_STACK_LOCAL
#define LOC_STACK_SPILL RA_LOCATION_STACK_SPILL
#define LOC_PHY_A       RA_LOCATION_PHY_A
#define LOC_PHY_HL      RA_LOCATION_PHY_HL
#define LOC_PHY_BC      RA_LOCATION_PHY_BC
#define LOC_PHY_C       RA_LOCATION_PHY_C
#define LOC_PHY_B       RA_LOCATION_PHY_B

struct z80_location {
  int kind;
  int offset;
  int phy;
  int size;
  int value;
  char *label;
  struct tree_node *node;
};


static int _get_z80_call_frame_prefix(int *frame_end, int *return_sp_offset, int *saved_fp_low_offset, int *saved_fp_high_offset) {

  *frame_end = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_PREFIX_END);
  *return_sp_offset = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_RETURN_SP_OFFSET);
  *saved_fp_low_offset = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_SAVED_FP_LOW_OFFSET);
  *saved_fp_high_offset = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_SAVED_FP_HIGH_OFFSET);

  if (*frame_end >= 0 || *return_sp_offset != -1 || *saved_fp_low_offset >= *saved_fp_high_offset ||
      *saved_fp_high_offset >= *return_sp_offset)
    return FAILED;

  return SUCCEEDED;
}


static int _get_z80_return_value_slot_offsets(int return_value_size, int *low_offset, int *high_offset) {

  *low_offset = get_register_allocator_return_value_byte_offset(return_value_size, 0);
  *high_offset = 0;

  if (*low_offset == 0)
    return FAILED;

  if (return_value_size == 2) {
    *high_offset = get_register_allocator_return_value_byte_offset(return_value_size, 1);
    if (*high_offset == 0)
      return FAILED;
  }
  else if (return_value_size != 1)
    return FAILED;

  return SUCCEEDED;
}


static char *_get_register_allocator_argument_transport_name(int transport) {

  if (transport == RA_ARGUMENT_TRANSPORT_BYTE)
    return "byte";
  if (transport == RA_ARGUMENT_TRANSPORT_WORD)
    return "word";
  if (transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND)
    return "sign_extend";
  if (transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND)
    return "zero_extend";
  if (transport == RA_ARGUMENT_TRANSPORT_TRUNCATE)
    return "truncate";

  return "none";
}


static int _get_z80_argument_byte_offsets(int transport, int *source_low, int *source_high, int *target_low, int *target_high) {

  *source_low = get_register_allocator_argument_byte_offset(transport, RA_ARGUMENT_BYTE_ACCESS_SOURCE, 0);
  *source_high = get_register_allocator_argument_byte_offset(transport, RA_ARGUMENT_BYTE_ACCESS_SOURCE, 1);
  *target_low = get_register_allocator_argument_byte_offset(transport, RA_ARGUMENT_BYTE_ACCESS_TARGET, 0);
  *target_high = get_register_allocator_argument_byte_offset(transport, RA_ARGUMENT_BYTE_ACCESS_TARGET, 1);

  if (*source_low < 0 || *source_low > 1 || *target_low < 0 || *target_low > 1)
    return FAILED;
  if (*source_high > 1 || *target_high > 1)
    return FAILED;
  if (*source_high >= 0 && *source_high == *source_low)
    return FAILED;
  if (*target_high >= 0 && *target_high == *target_low)
    return FAILED;

  if (transport == RA_ARGUMENT_TRANSPORT_BYTE)
    return *source_high < 0 && *target_high < 0 ? SUCCEEDED : FAILED;
  if (transport == RA_ARGUMENT_TRANSPORT_WORD)
    return *source_high >= 0 && *target_high >= 0 ? SUCCEEDED : FAILED;
  if (transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND || transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND)
    return *source_high < 0 && *target_high >= 0 ? SUCCEEDED : FAILED;
  if (transport == RA_ARGUMENT_TRANSPORT_TRUNCATE)
    return *source_high >= 0 && *target_high < 0 ? SUCCEEDED : FAILED;

  return FAILED;
}


static char *_get_z80_physical_register_name(int physical_register) {

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


static char *_get_z80_address_target_name(int address_target) {

  if (address_target == RA_ADDRESS_TARGET_IX)
    return "IX";
  if (address_target == RA_ADDRESS_TARGET_IY)
    return "IY";
  if (address_target == RA_ADDRESS_TARGET_HL)
    return "HL";
  if (address_target == RA_ADDRESS_TARGET_IX_UNCACHED)
    return "IX_UNCACHED";
  if (address_target == RA_ADDRESS_TARGET_IY_OLD_FRAME)
    return "IY_OLD_FRAME";

  return "INVALID";
}


static char *_get_z80_address_mode_name(int address_mode) {

  if (address_mode == RA_ADDRESS_MODE_NONE)
    return "none";
  if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL)
    return "global_label";
  if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS)
    return "frame_address";
  if (address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT)
    return "frame_displacement";

  return "invalid";
}


static char *_get_z80_location_kind_name(int kind);


#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
static void _trace_z80_specialized_address_materialization(struct tree_node *function_node, char *emitter,
                                                            char *operand, struct z80_location *location,
                                                            int address_target, int folded_offset) {

  char *function_name;
  int address_mode;

  if (g_allocator_enabled == NO)
    return;

  function_name = "<unknown>";
  if (function_node != NULL && function_node->children[1] != NULL)
    function_name = function_node->children[1]->label;
  address_mode = get_register_allocator_address_materialization_mode(location->kind, address_target, location->offset);

    fprintf(stderr, "register_allocator: target_specialized_address_materialization function=%s emitter=%s operand=%s kind=%s offset=%d mode=%s folded_offset=%d address_target=%s target=z80\n",
      function_name, emitter, operand, _get_z80_location_kind_name(location->kind), location->offset,
      _get_z80_address_mode_name(address_mode), folded_offset, _get_z80_address_target_name(address_target));
}
#endif


static char *_get_z80_spill_reason_name(int spill_reason) {

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


static struct temp_register *_find_z80_temp_register(struct tree_node *function_node, int register_index) {

  int i;

  if (function_node == NULL || function_node->local_variables == NULL)
    return NULL;

  for (i = 0; i < function_node->local_variables->temp_registers_count; i++) {
    if (function_node->local_variables->temp_registers[i].register_index == register_index)
      return &(function_node->local_variables->temp_registers[i]);
  }

  return NULL;
}


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
    *offset = Z80_GLOBAL_OFFSET;

    return SUCCEEDED;
  }
  else if (type == TAC_ARG_TYPE_TEMP) {
    struct local_variables *local_variables = function_node->local_variables;
    struct temp_register *temp_register = NULL;
    int i;

    for (i = 0; i < local_variables->temp_registers_count; i++) {
      if (local_variables->temp_registers[i].register_index == value) {
        temp_register = &(local_variables->temp_registers[i]);
        break;
      }
    }

    if (temp_register != NULL) {
      if (g_allocator_enabled == YES && temp_register->physical_register != Z80_PHY_NONE) {
        fprintf(stderr, "register_allocator: stack offset requested for retained r%d in %s; emitter must use %s directly.\n", value, function_node->children[1]->label, _get_z80_physical_register_name(temp_register->physical_register));
        return FAILED;
      }

      *offset = temp_register->offset_to_fp;

      return SUCCEEDED;
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


static void _load_hl_to_iy(FILE *file_out) {

  fprintf(file_out, "      PUSH HL\n");
  fprintf(file_out, "      POP IY\n");
}


static void _load_bc_to_iy(FILE *file_out) {

  fprintf(file_out, "      PUSH BC\n");
  fprintf(file_out, "      POP IY\n");
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

  value &= 0xff;
  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_value_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),%d\n", offset, value);
  else
    fprintf(file_out, "      LD  (IX%d),%d\n", offset, value);
}


static void _load_value_into_iy(int value, int offset, FILE *file_out) {

  value &= 0xff;
  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_value_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),%d\n", offset, value);
  else
    fprintf(file_out, "      LD  (IY%d),%d\n", offset, value);
}


static void _load_from_iy_to_a(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_iy_to_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  A,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  A,(IY%d)\n", offset);
}


static void _load_from_iy_to_b(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_iy_to_b(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  B,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  B,(IY%d)\n", offset);
}


static void _load_from_iy_to_c(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_iy_to_c(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  C,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  C,(IY%d)\n", offset);
}


static void _load_from_iy_to_h(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_iy_to_h(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  H,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  H,(IY%d)\n", offset);
}


static void _load_from_iy_to_l(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_iy_to_l(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  L,(IY+%d)\n", offset);
  else
    fprintf(file_out, "      LD  L,(IY%d)\n", offset);
}


static void _load_from_ix_to_a(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  A,(IX%d)\n", offset);
}


static void _load_from_ix_to_b(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_b(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  B,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  B,(IX%d)\n", offset);
}


static void _load_from_ix_to_c(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_c(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  C,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  C,(IX%d)\n", offset);
}


static void _load_from_ix_to_d(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_d(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  D,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  D,(IX%d)\n", offset);

  g_is_ix_de = NO;
}


static void _load_from_ix_to_e(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_e(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  E,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  E,(IX%d)\n", offset);

  g_is_ix_de = NO;
}


static void _load_from_ix_to_h(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_h(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  H,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  H,(IX%d)\n", offset);
}


static void _load_from_ix_to_l(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_l(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  L,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      LD  L,(IX%d)\n", offset);
}


static void _load_from_ix_to_register(int offset, char reg, FILE *file_out) {

  char name[2];

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_from_ix_to_register(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  name[0] = toupper(reg);
  name[1] = 0;

  if (offset >= 0)
    fprintf(file_out, "      LD  %s,(IX+%d)\n", name, offset);
  else
    fprintf(file_out, "      LD  %s,(IX%d)\n", name, offset);
}


static void _load_a_into_ix(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_a_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),A\n", offset);
}


static void _load_b_into_ix(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_b_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),B\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),B\n", offset);
}


static void _load_c_into_ix(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_c_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),C\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),C\n", offset);
}


static void _load_h_into_ix(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_h_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),H\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),H\n", offset);
}


static void _load_h_into_iy(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_h_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),H\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),H\n", offset);
}


static void _load_b_into_iy(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_b_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),B\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),B\n", offset);
}


static void _load_c_into_iy(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_c_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),C\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),C\n", offset);
}


static void _load_a_into_iy(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_a_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),A\n", offset);
}


static void _load_l_into_ix(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_l_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),L\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),L\n", offset);
}


static void _load_register_into_ix(int offset, char reg, FILE *file_out) {

  char name[2];

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_register_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  name[0] = toupper(reg);
  name[1] = 0;

  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),%s\n", offset, name);
  else
    fprintf(file_out, "      LD  (IX%d),%s\n", offset, name);
}


static void _load_l_into_iy(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_load_l_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

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


static void _add_hl_to_hl(FILE *file_out) {

  fprintf(file_out, "      ADD HL,HL\n");
}


static void _add_value_to_a(int value, FILE *file_out) {

  fprintf(file_out, "      ADD A,%d\n", value);
}


static void _add_from_ix_to_a(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_add_from_ix_to_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

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

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_add_from_ix_from_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      SUB A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      SUB A,(IX%d)\n", offset);
}


static void _or_value_to_a(int value, FILE *file_out) {

  fprintf(file_out, "      OR  A,%d\n", value);
}


static void _xor_value_to_a(int value, FILE *file_out) {

  fprintf(file_out, "      XOR A,%d\n", value);
}


static void _or_from_ix_to_a(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_or_from_ix_to_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      OR  A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      OR  A,(IX%d)\n", offset);
}


static void _xor_from_ix_to_a(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_xor_from_ix_to_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  if (offset >= 0)
    fprintf(file_out, "      XOR A,(IX+%d)\n", offset);
  else
    fprintf(file_out, "      XOR A,(IX%d)\n", offset);
}


static void _or_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      OR  A,H\n");
  fprintf(file_out, "      LD  H,A\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      OR  A,L\n");
  fprintf(file_out, "      LD  L,A\n");
}


static void _xor_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      XOR A,H\n");
  fprintf(file_out, "      LD  H,A\n");
  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      XOR A,L\n");
  fprintf(file_out, "      LD  L,A\n");
}


static void _complement_hl(FILE *file_out) {

  fprintf(file_out, "      LD  A,L\n");
  fprintf(file_out, "      XOR A,255\n");
  fprintf(file_out, "      LD  L,A\n");
  fprintf(file_out, "      LD  A,H\n");
  fprintf(file_out, "      XOR A,255\n");
  fprintf(file_out, "      LD  H,A\n");
}


static void _complement_bc(FILE *file_out) {

  fprintf(file_out, "      LD  A,C\n");
  fprintf(file_out, "      XOR A,255\n");
  fprintf(file_out, "      LD  C,A\n");
  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      XOR A,255\n");
  fprintf(file_out, "      LD  B,A\n");
}


static void _and_value_from_a(int value, FILE *file_out) {

  fprintf(file_out, "      AND A,%d\n", value);
}


static void _and_from_ix_from_a(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_and_from_ix_from_a(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

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


static void _load_bc_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD  H,B\n");
  fprintf(file_out, "      LD  L,C\n");
}


static void _load_a_to_b(FILE *file_out) {

  fprintf(file_out, "      LD  B,A\n");
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


static void _load_value_to_bc_array(int *sizes, int items, FILE *file_out) {

  int i;

  fprintf(file_out, "      LD  BC,");
  for (i = 0; i < items; i++) {
    fprintf(file_out, "%d", sizes[i]);
    if (i < items-1)
      fprintf(file_out, "+");
  }
  fprintf(file_out, "\n");
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

  g_is_ix_de = NO;
}


static void _load_value_to_e(int value, FILE *file_out) {

  fprintf(file_out, "      LD  E,%d\n", value);

  g_is_ix_de = NO;
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

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_sign_extend_a_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  /* from https://stackoverflow.com/questions/49070981/z80-assembly-how-to-add-signed-8-bit-value-to-16-bit-register */
  fprintf(file_out, "      ; sign extend 8-bit A -> (IX)\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  if (offset >= 0)
    fprintf(file_out, "      LD  (IX+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IX%d),A\n", offset);
}


static void _sign_extend_a_into_iy(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_sign_extend_a_into_iy(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

  fprintf(file_out, "      ; sign extend 8-bit A -> (IY)\n");
  fprintf(file_out, "      ADD A,A  ; sign bit of A into carry\n");
  fprintf(file_out, "      SBC A,A  ; A = 0 if carry == 0, $FF otherwise\n");
  if (offset >= 0)
    fprintf(file_out, "      LD  (IY+%d),A\n", offset);
  else
    fprintf(file_out, "      LD  (IY%d),A\n", offset);
}


static void _sign_extend_b_into_ix(int offset, FILE *file_out) {

  if (offset < -128 || offset > 127) {
    fprintf(stderr, "_sign_extend_b_into_ix(): Offset %d is out of range [-128, 127]. Cannot continue! Please submit a bug report!\n", offset);
    exit(1);
  }

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


static void _sign_extend_c_to_bc_preserving_a(FILE *file_out) {

  fprintf(file_out, "      ; sign extend 8-bit C -> 16-bit BC preserving A\n");
  fprintf(file_out, "      LD  B,0\n");
  fprintf(file_out, "      BIT 7,C\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      DEC B\n");

  _add_label("+", file_out, YES);
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


static char *_get_z80_location_kind_name(int kind) {

  if (kind == LOC_CONST)
    return "CONST";
  if (kind == LOC_GLOBAL)
    return "GLOBAL";
  if (kind == LOC_STACK_LOCAL)
    return "STACK_LOCAL";
  if (kind == LOC_STACK_SPILL)
    return "STACK_SPILL";
  if (kind == LOC_PHY_A)
    return "PHY_A";
  if (kind == LOC_PHY_HL)
    return "PHY_HL";
  if (kind == LOC_PHY_BC)
    return "PHY_BC";
  if (kind == LOC_PHY_B)
    return "PHY_B";
  if (kind == LOC_PHY_C)
    return "PHY_C";

  return "UNKNOWN";
}


static char *_get_register_allocator_location_source_name(int source_kind) {

  if (source_kind == RA_LOCATION_SOURCE_CONSTANT)
    return "constant";
  if (source_kind == RA_LOCATION_SOURCE_GLOBAL)
    return "global";
  if (source_kind == RA_LOCATION_SOURCE_STACK_LOCAL)
    return "stack_local";
  if (source_kind == RA_LOCATION_SOURCE_STACK_SPILL)
    return "stack_spill";
  if (source_kind == RA_LOCATION_SOURCE_PHYSICAL)
    return "physical";

  return "unknown";
}


static char *_get_z80_tac_operand_name(int which_operand) {

  if (which_operand == TAC_USE_RESULT)
    return "result";
  if (which_operand == TAC_USE_ARG1)
    return "arg1";
  if (which_operand == TAC_USE_ARG2)
    return "arg2";

  return "unknown";
}


static int _get_z80_location_size(int var_type) {

  int bits;

  bits = get_variable_type_size(var_type);
  if (bits <= 0)
    return 0;

  return bits / 8;
}


static int _set_z80_location_to_physical_register(struct z80_location *location, int physical_register, int register_index) {

  location->phy = physical_register;
  location->kind = get_register_allocator_location_kind(RA_LOCATION_SOURCE_PHYSICAL, physical_register);

  if (location->kind == RA_LOCATION_NONE) {
    fprintf(stderr, "_set_z80_location_to_physical_register(): Target policy cannot materialize retained r%d in physical register %d! Please submit a bug report!\n", register_index, physical_register);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _resolve_z80_location(int type, char *name, int value, struct tree_node *node, int var_type, struct tree_node *function_node, struct z80_location *location, char *operand_name, int retained_physical_register) {

  int offset, source_kind;
  struct temp_register *temp_register;
  char *function_name;

  source_kind = RA_LOCATION_SOURCE_CONSTANT;
  location->kind = RA_LOCATION_NONE;
  location->offset = 0;
  location->phy = Z80_PHY_NONE;
  location->size = _get_z80_location_size(var_type);
  location->value = value;
  location->label = name;
  location->node = node;

  if (type == TAC_ARG_TYPE_CONSTANT) {
    source_kind = RA_LOCATION_SOURCE_CONSTANT;
    location->kind = get_register_allocator_location_kind(source_kind, Z80_PHY_NONE);
  }
  else if (type == TAC_ARG_TYPE_TEMP && g_allocator_enabled == YES && retained_physical_register != Z80_PHY_NONE) {
    source_kind = RA_LOCATION_SOURCE_PHYSICAL;
    temp_register = _find_z80_temp_register(function_node, value);
    if (temp_register != NULL)
      location->size = temp_register->size / 8;

    if (_set_z80_location_to_physical_register(location, retained_physical_register, value) == FAILED)
      return FAILED;
  }
  else if (type == TAC_ARG_TYPE_TEMP && g_allocator_enabled == YES) {
    temp_register = _find_z80_temp_register(function_node, value);

    if (temp_register != NULL && temp_register->spill_required == NO) {
      source_kind = RA_LOCATION_SOURCE_PHYSICAL;
      location->size = temp_register->size / 8;

      if (_set_z80_location_to_physical_register(location, temp_register->physical_register, value) == FAILED)
        return FAILED;
    }
    else {
      source_kind = RA_LOCATION_SOURCE_STACK_SPILL;
      if (find_stack_offset(type, name, value, node, &offset, function_node) == FAILED)
        return FAILED;
      location->kind = get_register_allocator_location_kind(source_kind, Z80_PHY_NONE);
      location->offset = offset;
    }
  }
  else if (type == TAC_ARG_TYPE_TEMP) {
    source_kind = RA_LOCATION_SOURCE_STACK_SPILL;
    if (find_stack_offset(type, name, value, node, &offset, function_node) == FAILED)
      return FAILED;
    location->kind = get_register_allocator_location_kind(source_kind, Z80_PHY_NONE);
    location->offset = offset;
  }
  else if (type == TAC_ARG_TYPE_LABEL) {
    if (find_stack_offset(type, name, value, node, &offset, function_node) == FAILED)
      return FAILED;

    location->offset = offset;
    if (offset == Z80_GLOBAL_OFFSET) {
      source_kind = RA_LOCATION_SOURCE_GLOBAL;
      location->kind = get_register_allocator_location_kind(source_kind, Z80_PHY_NONE);
      if (node != NULL)
        location->label = node->children[1]->label;
    }
    else {
      source_kind = RA_LOCATION_SOURCE_STACK_LOCAL;
      location->kind = get_register_allocator_location_kind(source_kind, Z80_PHY_NONE);
    }
  }
  else {
    fprintf(stderr, "_resolve_z80_location(): Unknown TAC argument type %d! Please submit a bug report!\n", type);
    return FAILED;
  }

  if (location->kind == RA_LOCATION_NONE) {
    fprintf(stderr, "_resolve_z80_location(): Target policy cannot materialize source kind %d with physical register %d! Please submit a bug report!\n", source_kind, location->phy);
    return FAILED;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    function_name = "<global>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: resolve function=%s operand=%s kind=%s offset=%d size=%d phy=%s\n",
            function_name, operand_name, _get_z80_location_kind_name(location->kind),
            location->offset, location->size, _get_z80_physical_register_name(location->phy));
        fprintf(stderr, "register_allocator: target_location_materialization function=%s operand=%s source=%s kind=%s offset=%d size=%d phy=%s target=z80\n",
          function_name, operand_name, _get_register_allocator_location_source_name(source_kind),
          _get_z80_location_kind_name(location->kind), location->offset, location->size,
          _get_z80_physical_register_name(location->phy));
  }
#endif

  return SUCCEEDED;
}


static int _resolve_tac_location(struct tac *t, int which_operand, struct tree_node *function_node, struct z80_location *location) {

  int type;
  int value;
  int var_type;
  int retained_physical_register;
  char *name;
  struct tree_node *node;

  type = TAC_ARG_TYPE_NONE;
  value = 0;
  var_type = VARIABLE_TYPE_NONE;
  retained_physical_register = Z80_PHY_NONE;
  name = NULL;
  node = NULL;

  if (which_operand == TAC_USE_RESULT) {
    type = t->result_type;
    value = (int)t->result_d;
    name = t->result_s;
    node = t->result_node;
    var_type = t->result_var_type;
    retained_physical_register = t->result_physical_register;
  }
  else if (which_operand == TAC_USE_ARG1) {
    type = t->arg1_type;
    value = (int)t->arg1_d;
    name = t->arg1_s;
    node = t->arg1_node;
    var_type = t->arg1_var_type;
    retained_physical_register = t->arg1_physical_register;
  }
  else if (which_operand == TAC_USE_ARG2) {
    type = t->arg2_type;
    value = (int)t->arg2_d;
    name = t->arg2_s;
    node = t->arg2_node;
    var_type = t->arg2_var_type;
    retained_physical_register = t->arg2_physical_register;
  }
  else {
    fprintf(stderr, "_resolve_tac_location(): Unknown operand %d! Please submit a bug report!\n", which_operand);
    return FAILED;
  }

  if (type != TAC_ARG_TYPE_TEMP)
    retained_physical_register = Z80_PHY_NONE;

  return _resolve_z80_location(type, name, value, node, var_type, function_node, location, _get_z80_tac_operand_name(which_operand), retained_physical_register);
}


static int _load_location_address_to_ix(struct z80_location *location, int *ix_offset, FILE *file_out) {

  int address_mode;
  int offset;

  *ix_offset = 0;
  address_mode = get_register_allocator_address_materialization_mode(location->kind, RA_ADDRESS_TARGET_IX, location->offset);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=%d target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_IX), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode), address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT ? location->offset : 0);
#endif

  if (address_mode == RA_ADDRESS_MODE_NONE) {
  }
  else if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL) {
    _load_label_to_ix(location->label, file_out);
  }
  else if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS || address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    offset = location->offset;
    fprintf(file_out, "      ; offset %d\n", offset);

    if (address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
      *ix_offset = offset;
      offset = 0;
    }

    _load_de_with_offset_to_ix(offset, file_out);
  }
  else {
    fprintf(stderr, "_load_location_address_to_ix(): Location kind %d has no IX address materialization! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _load_location_address_to_ix_uncached(struct z80_location *location, int *ix_offset, FILE *file_out) {

  int address_mode;
  int offset;

  *ix_offset = 0;
  address_mode = get_register_allocator_address_materialization_mode(location->kind, RA_ADDRESS_TARGET_IX_UNCACHED, location->offset);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=%d target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_IX_UNCACHED), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode), address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT ? location->offset : 0);
#endif

  if (address_mode == RA_ADDRESS_MODE_NONE) {
  }
  else if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL) {
    _load_label_to_ix(location->label, file_out);
  }
  else if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS || address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    offset = location->offset;
    fprintf(file_out, "      ; offset %d\n", offset);

    if (address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
      *ix_offset = offset;
      offset = 0;
    }

    _load_value_to_ix(offset, file_out);
    _add_de_to_ix(file_out);
  }
  else {
    fprintf(stderr, "_load_location_address_to_ix_uncached(): Location kind %d has no IX address materialization! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _load_call_argument_address_to_iy(struct z80_location *location, int *source_offset,
                                             int *reset_iy, struct tree_node *function_node,
                                             char *callee_name, int argument, FILE *file_out) {

  int address_mode;
  int cache_was_reset;
  char *cache_action;
  char *function_name;

  cache_was_reset = *reset_iy;
  address_mode = get_register_allocator_address_materialization_mode(
      location->kind, RA_ADDRESS_TARGET_IY_OLD_FRAME, location->offset);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: target_address_materialization target=IY_OLD_FRAME kind=%s offset=%d mode=%s folded_offset=%d target_policy=z80\n",
            _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode),
            address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT ? location->offset : 0);
#endif

  if (address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    if (*reset_iy == YES) {
      *reset_iy = NO;
      cache_action = "initialize";

      fprintf(file_out, "      ; old stack frame -> IY\n");

      _load_value_to_iy(0, file_out);
      _add_de_to_iy(file_out);
    }
    else
      cache_action = "reuse";

  }
  else if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS) {
    *reset_iy = YES;
    cache_action = "rebase";

    fprintf(file_out, "      ; old stack frame with offset -> IY\n");

    _load_value_to_iy(*source_offset, file_out);
    _add_de_to_iy(file_out);

    *source_offset = 0;
  }
  else {
    fprintf(stderr, "_load_call_argument_address_to_iy(): Location kind %d has no old-frame IY address materialization! Please submit a bug report!\n",
            location->kind);
    return FAILED;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: target_call_old_frame_address function=%s callee=%s argument=%d kind=%s offset=%d mode=%s folded_offset=%d cache_before=%s cache_action=%s cache_after=%s address_target=IY_OLD_FRAME target=z80\n",
            function_name, callee_name, argument, _get_z80_location_kind_name(location->kind),
            location->offset, _get_z80_address_mode_name(address_mode), *source_offset,
            cache_was_reset == YES ? "reset" : "ready", cache_action,
            *reset_iy == YES ? "reset" : "ready");
  }
#endif

  return SUCCEEDED;
}


static int _load_location_address_to_iy(struct z80_location *location, FILE *file_out) {

  int address_mode;

  address_mode = get_register_allocator_address_materialization_mode(location->kind, RA_ADDRESS_TARGET_IY, location->offset);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=0 target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_IY), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode));
#endif

  if (address_mode == RA_ADDRESS_MODE_NONE) {
  }
  else if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL) {
    _load_label_to_iy(location->label, file_out);
  }
  else if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS) {
    fprintf(file_out, "      ; offset %d\n", location->offset);
    _load_value_to_iy(location->offset, file_out);
    _add_de_to_iy(file_out);
  }
  else {
    fprintf(stderr, "_load_location_address_to_iy(): Location kind %d has no IY address materialization! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _load_location_address_to_hl(struct z80_location *location, FILE *file_out) {

  int address_mode;

  address_mode = get_register_allocator_address_materialization_mode(location->kind, RA_ADDRESS_TARGET_HL, location->offset);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=0 target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_HL), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode));
#endif

  if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL) {
    _load_label_to_hl(location->label, file_out);
  }
  else if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS) {
    fprintf(file_out, "      ; offset %d\n", location->offset);
    _load_value_to_hl(location->offset, file_out);
    _add_de_to_hl(file_out);
  }
  else {
    fprintf(stderr, "_load_location_address_to_hl(): Location kind %d has no frame/global address! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _materialize_to_a(struct z80_location *location, int byte_offset, FILE *file_out) {

  if (location->kind == LOC_CONST) {
    _load_value_to_a((location->value >> (byte_offset * 8)) & 0xff, file_out);
  }
  else if (location->kind == LOC_PHY_A) {
    if (byte_offset != 0) {
      fprintf(stderr, "_materialize_to_a(): Cannot read byte %d from A! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_HL) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  A,L\n");
    else if (byte_offset == 1)
      fprintf(file_out, "      LD  A,H\n");
    else {
      fprintf(stderr, "_materialize_to_a(): Cannot read byte %d from HL! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_BC) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  A,C\n");
    else if (byte_offset == 1)
      fprintf(file_out, "      LD  A,B\n");
    else {
      fprintf(stderr, "_materialize_to_a(): Cannot read byte %d from BC! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_B) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  A,B\n");
    else {
      fprintf(stderr, "_materialize_to_a(): Cannot read byte %d from B! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_C) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  A,C\n");
    else {
      fprintf(stderr, "_materialize_to_a(): Cannot read byte %d from C! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    _load_from_iy_to_a(byte_offset, file_out);
  }
  else {
    fprintf(stderr, "_materialize_to_a(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _materialize_to_hl(struct z80_location *location, FILE *file_out) {

  if (location->kind == LOC_CONST) {
    _load_value_to_hl(location->value, file_out);
  }
  else if (location->kind == LOC_PHY_HL) {
  }
  else if (location->kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  L,A\n");
    _load_value_to_h(0, file_out);
  }
  else if (location->kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  L,C\n");
    fprintf(file_out, "      LD  H,B\n");
  }
  else if (location->kind == LOC_PHY_B) {
    fprintf(file_out, "      LD  L,B\n");
    _load_value_to_h(0, file_out);
  }
  else if (location->kind == LOC_PHY_C) {
    fprintf(file_out, "      LD  L,C\n");
    _load_value_to_h(0, file_out);
  }
  else if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    _load_from_iy_to_l(0, file_out);
    _load_from_iy_to_h(1, file_out);
  }
  else {
    fprintf(stderr, "_materialize_to_hl(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _materialize_array_index_to_bc(struct z80_location *location, int var_type, int ix_offset, int preserve_a, FILE *file_out) {

  if (location->kind == LOC_CONST) {
    if (location->value != 0)
      _load_value_to_bc(location->value, file_out);
  }
  else if (location->kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  C,A\n");
    if (var_type == VARIABLE_TYPE_INT8)
      _sign_extend_c_to_bc_preserving_a(file_out);
    else if (var_type == VARIABLE_TYPE_UINT8)
      _load_value_to_b(0, file_out);
    else {
      fprintf(stderr, "_materialize_array_index_to_bc(): Cannot read a 16-bit index from A! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else if (location->kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  C,L\n");
    if (var_type == VARIABLE_TYPE_INT8)
      _sign_extend_c_to_bc_preserving_a(file_out);
    else if (var_type == VARIABLE_TYPE_UINT8)
      _load_value_to_b(0, file_out);
    else if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16)
      fprintf(file_out, "      LD  B,H\n");
    else {
      fprintf(stderr, "_materialize_array_index_to_bc(): Unknown retained HL index type %d! Please submit a bug report!\n", var_type);
      return FAILED;
    }
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else if (location->kind == LOC_PHY_BC) {
    fprintf(file_out, "      ; retained arg2 in BC\n");
  }
  else if (location->kind == LOC_PHY_B) {
    fprintf(file_out, "      LD  C,B\n");
    if (var_type == VARIABLE_TYPE_INT8) {
      _sign_extend_c_to_bc_preserving_a(file_out);
    }
    else if (var_type == VARIABLE_TYPE_UINT8) {
      _load_value_to_b(0, file_out);
    }
    else {
      fprintf(stderr, "_materialize_array_index_to_bc(): Cannot read a 16-bit index from B! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      ; retained arg2 in B\n");
  }
  else if (location->kind == LOC_PHY_C) {
    if (var_type == VARIABLE_TYPE_INT8) {
      _sign_extend_c_to_bc_preserving_a(file_out);
    }
    else if (var_type == VARIABLE_TYPE_UINT8) {
      _load_value_to_b(0, file_out);
    }
    else {
      fprintf(stderr, "_materialize_array_index_to_bc(): Cannot read a 16-bit index from C! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      ; retained arg2 in C\n");
  }
  else if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      _load_from_ix_to_c(ix_offset, file_out);
      _load_from_ix_to_b(ix_offset + 1, file_out);
    }
    else {
      _load_from_ix_to_c(ix_offset, file_out);

      if (var_type == VARIABLE_TYPE_INT8) {
        if (preserve_a == YES)
          _sign_extend_c_to_bc_preserving_a(file_out);
        else
          _sign_extend_c_to_bc(file_out);
      }
      else if (var_type == VARIABLE_TYPE_UINT8) {
        _load_value_to_b(0, file_out);
      }
      else {
        fprintf(stderr, "_materialize_array_index_to_bc(): Unhandled 8-bit index! Please submit a bug report!\n");
        return FAILED;
      }
    }
  }
  else {
    fprintf(stderr, "_materialize_array_index_to_bc(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _materialize_z80_port_to_c(struct z80_location *location, int ix_offset, FILE *file_out) {

  if (location->kind == LOC_CONST) {
  }
  else if (location->kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  C,A\n");
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else if (location->kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  C,L\n");
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else if (location->kind == LOC_PHY_BC) {
    fprintf(file_out, "      ; retained arg2 in BC\n");
  }
  else if (location->kind == LOC_PHY_B) {
    fprintf(file_out, "      LD  C,B\n");
    fprintf(file_out, "      ; retained arg2 in B\n");
  }
  else if (location->kind == LOC_PHY_C) {
    fprintf(file_out, "      ; retained arg2 in C\n");
  }
  else if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    _load_from_ix_to_c(ix_offset, file_out);
  }
  else {
    fprintf(stderr, "_materialize_z80_port_to_c(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _store_from_a(struct z80_location *location, int byte_offset, int ix_offset, FILE *file_out) {

  if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    _load_a_into_ix(ix_offset + byte_offset, file_out);
  }
  else if (location->kind == LOC_PHY_A) {
    if (byte_offset != 0) {
      fprintf(stderr, "_store_from_a(): Cannot write byte %d to A! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_HL) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  L,A\n");
    else if (byte_offset == 1)
      fprintf(file_out, "      LD  H,A\n");
    else {
      fprintf(stderr, "_store_from_a(): Cannot write byte %d to HL! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_BC) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  C,A\n");
    else if (byte_offset == 1)
      fprintf(file_out, "      LD  B,A\n");
    else {
      fprintf(stderr, "_store_from_a(): Cannot write byte %d to BC! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_B) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  B,A\n");
    else {
      fprintf(stderr, "_store_from_a(): Cannot write byte %d to B! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else if (location->kind == LOC_PHY_C) {
    if (byte_offset == 0)
      fprintf(file_out, "      LD  C,A\n");
    else {
      fprintf(stderr, "_store_from_a(): Cannot write byte %d to C! Please submit a bug report!\n", byte_offset);
      return FAILED;
    }
  }
  else {
    fprintf(stderr, "_store_from_a(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _store_from_hl(struct z80_location *location, int ix_offset, FILE *file_out) {

  if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    _load_l_into_ix(ix_offset, file_out);
    _load_h_into_ix(ix_offset + 1, file_out);
  }
  else if (location->kind == LOC_PHY_HL) {
  }
  else if (location->kind == LOC_PHY_A) {
    fprintf(stderr, "_store_from_hl(): Cannot write HL into A! Please submit a bug report!\n");
    return FAILED;
  }
  else if (location->kind == LOC_PHY_BC) {
    _load_hl_to_bc(file_out);
  }
  else {
    fprintf(stderr, "_store_from_hl(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _store_constant_to_location(struct z80_location *location, int value, int ix_offset, FILE *file_out) {

  if (location->kind == LOC_GLOBAL || location->kind == LOC_STACK_LOCAL || location->kind == LOC_STACK_SPILL) {
    if (location->size == 2) {
      _load_value_into_ix(value & 0xff, ix_offset, file_out);
      _load_value_into_ix((value >> 8) & 0xff, ix_offset + 1, file_out);
    }
    else {
      _load_value_into_ix(value & 0xff, ix_offset, file_out);
    }
  }
  else if (location->kind == LOC_PHY_A) {
    _load_value_to_a(value & 0xff, file_out);
  }
  else if (location->kind == LOC_PHY_HL) {
    _load_value_to_hl(value, file_out);
  }
  else if (location->kind == LOC_PHY_BC) {
    _load_value_to_bc(value, file_out);
  }
  else if (location->kind == LOC_PHY_B) {
    _load_value_to_b(value & 0xff, file_out);
  }
  else if (location->kind == LOC_PHY_C) {
    _load_value_to_c(value & 0xff, file_out);
  }
  else {
    fprintf(stderr, "_store_constant_to_location(): Unknown location kind %d! Please submit a bug report!\n", location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _generate_asm_assignment_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  int ix_offset = 0;
  int value;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: assignment function=%s arg1_kind=%s arg1_phy=%s arg1_size=%d result_kind=%s result_phy=%s result_size=%d\n",
            function_node->children[1]->label,
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy), arg1_location.size,
            _get_z80_location_kind_name(result_location.kind), _get_z80_physical_register_name(result_location.phy), result_location.size);
  }
#endif

  if (arg1_location.kind == LOC_PHY_A)
    fprintf(file_out, "      ; retained arg1 in A\n");
  else if (arg1_location.kind == LOC_PHY_HL)
    fprintf(file_out, "      ; retained arg1 in HL\n");
  else if (arg1_location.kind == LOC_PHY_BC)
    fprintf(file_out, "      ; retained arg1 in BC\n");

  /* generate asm */

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (result_location.kind == LOC_GLOBAL || result_location.kind == LOC_STACK_LOCAL || result_location.kind == LOC_STACK_SPILL)
    _trace_z80_specialized_address_materialization(function_node, "assignment_result_address", "result",
                                                    &result_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif

  /******************************************************************************************************/
  /* source address -> iy */
  /******************************************************************************************************/

  if (_load_location_address_to_iy(&arg1_location, file_out) == FAILED)
    return FAILED;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL)
    _trace_z80_specialized_address_materialization(function_node, "assignment_arg1_address", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_IY, 0);
#endif

  /******************************************************************************************************/
  /* copy data -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST) {
    value = arg1_location.value;
    if (_store_constant_to_location(&result_location, value, ix_offset, file_out) == FAILED)
      return FAILED;
  }
  else {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
        if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
          return FAILED;
        if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
          return FAILED;

        if (_materialize_to_a(&arg1_location, 1, file_out) == FAILED)
          return FAILED;
        if (_store_from_a(&result_location, 1, ix_offset, file_out) == FAILED)
          return FAILED;
      }
      else {
        /* sign extend 8-bit -> 16-bit? */
        if (t->result_var_type == VARIABLE_TYPE_INT16 && t->arg1_var_type == VARIABLE_TYPE_INT8) {
          /* lower byte must be sign extended */
          if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
            return FAILED;
          _sign_extend_a_to_bc(file_out);
          _load_c_into_ix(ix_offset, file_out);
          _load_b_into_ix(ix_offset + 1, file_out);
        }
        else {
          /* lower byte */
          if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
            return FAILED;
          if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
            return FAILED;
          /* upper byte is 0 */
          _load_value_into_ix(0, ix_offset + 1, file_out);
        }
      }
    }
    else {
      /* 8-bit */
      if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
        return FAILED;
      if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
        return FAILED;
    }
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES && result_location.kind == LOC_PHY_B)
    fprintf(stderr, "register_allocator: assignment_physical_result function=%s source_kind=%s source_size=%d result_phy=B result_size=%d via=A store=B status=complete\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, result_location.size);
#endif

  return SUCCEEDED;
}


/* NOTE! because SAS/C on Amiga thinks that this and the next function were called the same
   prefixes "aaa" and "bbb" were added to separate them */


static int _bbb_generate_asm_add_sub_or_xor_and_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_a = NO, arg2_in_a = NO, result_in_a = NO;
  int arg1_in_hl = NO, arg1_in_bc = NO, arg2_in_hl = NO, arg2_in_bc = NO;
  int arg1_in_b = NO, arg2_in_b = NO, result_in_b = NO;
  int arg1_in_c = NO, arg2_in_c = NO, result_in_c = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_a = arg1_location.kind == LOC_PHY_A ? YES : NO;
  arg2_in_a = arg2_location.kind == LOC_PHY_A ? YES : NO;
  result_in_a = result_location.kind == LOC_PHY_A ? YES : NO;
  arg1_in_hl = arg1_location.kind == LOC_PHY_HL ? YES : NO;
  arg1_in_bc = arg1_location.kind == LOC_PHY_BC ? YES : NO;
  arg2_in_hl = arg2_location.kind == LOC_PHY_HL ? YES : NO;
  arg2_in_bc = arg2_location.kind == LOC_PHY_BC ? YES : NO;
  arg1_in_b = arg1_location.kind == LOC_PHY_B ? YES : NO;
  arg2_in_b = arg2_location.kind == LOC_PHY_B ? YES : NO;
  result_in_b = result_location.kind == LOC_PHY_B ? YES : NO;
  arg1_in_c = arg1_location.kind == LOC_PHY_C ? YES : NO;
  arg2_in_c = arg2_location.kind == LOC_PHY_C ? YES : NO;
  result_in_c = result_location.kind == LOC_PHY_C ? YES : NO;

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_a == YES) {
    fprintf(file_out, "      ; retained arg1 in A\n");
  }
  else if (arg1_in_hl == YES) {
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_in_bc == YES) {
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else if (arg1_in_b == YES) {
    fprintf(file_out, "      ; retained arg1 in B\n");
  }
  else if (arg1_in_c == YES) {
    fprintf(file_out, "      ; retained arg1 in C\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> a */
  /******************************************************************************************************/

  if (arg1_in_a == YES) {
  }
  else if (arg1_in_hl == YES) {
    fprintf(file_out, "      LD  A,L\n");
  }
  else if (arg1_in_bc == YES) {
    fprintf(file_out, "      LD  A,C\n");
  }
  else if (arg1_in_b == YES) {
    if (arg2_in_a == NO)
      fprintf(file_out, "      LD  A,B\n");
  }
  else if (arg1_in_c == YES) {
    if (arg2_in_a == NO)
      fprintf(file_out, "      LD  A,C\n");
  }
  else if (arg2_in_a == YES) {
  }
  else if (arg1_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_a(arg1_location.value & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_a(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_in_a == YES) {
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else if (arg2_in_hl == YES) {
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else if (arg2_in_bc == YES) {
    fprintf(file_out, "      ; retained arg2 in BC\n");
  }
  else if (arg2_in_b == YES) {
    fprintf(file_out, "      ; retained arg2 in B\n");
  }
  else if (arg2_in_c == YES) {
    fprintf(file_out, "      ; retained arg2 in C\n");
  }
  else {
    ix_offset = 0;

    if (arg2_location.kind == LOC_CONST) {
    }
    else {
      if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
        return FAILED;
    }
  }

  /******************************************************************************************************/
  /* add/sub/or/xor/and data (arg2) -> a */
  /******************************************************************************************************/

  if (arg2_in_a == YES && op == TAC_OP_SUB) {
    fprintf(stderr, "_bbb_generate_asm_add_sub_or_xor_and_z80_8bit(): Retained ARG2 is not supported for SUB! Please submit a bug report!\n");
    return FAILED;
  }
  else if (op == TAC_OP_ADD) {
    if (arg2_in_a == YES) {
      if (arg1_location.kind == LOC_CONST) {
        /* 8-bit */
        if (arg1_location.value == 1)
          _inc_a(file_out);
        else
          _add_value_to_a(arg1_location.value & 0xff, file_out);
      }
      else if (arg1_in_hl == YES)
        fprintf(file_out, "      ADD A,L\n");
      else if (arg1_in_bc == YES)
        fprintf(file_out, "      ADD A,C\n");
      else if (arg1_in_b == YES)
        fprintf(file_out, "      ADD A,B\n");
      else if (arg1_in_c == YES)
        fprintf(file_out, "      ADD A,C\n");
      else {
        /* 8-bit */
        _add_from_ix_to_a(ix_offset, file_out);
      }
    }
    else
    if (arg2_location.kind == LOC_CONST) {
      /* 8-bit */
      if (arg2_location.value == 1)
        _inc_a(file_out);
      else
        _add_value_to_a(arg2_location.value & 0xff, file_out);
    }
    else if (arg2_in_hl == YES)
      fprintf(file_out, "      ADD A,L\n");
    else if (arg2_in_bc == YES)
      fprintf(file_out, "      ADD A,C\n");
    else if (arg2_in_b == YES)
      fprintf(file_out, "      ADD A,B\n");
    else if (arg2_in_c == YES)
      fprintf(file_out, "      ADD A,C\n");
    else {
      /* 8-bit */
      _add_from_ix_to_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_SUB) {
    if (arg2_location.kind == LOC_CONST) {
      /* 8-bit */
      if (arg2_location.value == 1)
        _dec_a(file_out);
      else
        _sub_value_from_a(arg2_location.value & 0xff, file_out);
    }
    else if (arg2_in_hl == YES)
      fprintf(file_out, "      SUB A,L\n");
    else if (arg2_in_bc == YES)
      fprintf(file_out, "      SUB A,C\n");
    else if (arg2_in_b == YES)
      fprintf(file_out, "      SUB A,B\n");
    else if (arg2_in_c == YES)
      fprintf(file_out, "      SUB A,C\n");
    else {
      /* 8-bit */
      _sub_from_ix_from_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_OR) {
    if (arg2_in_a == YES) {
      if (arg1_location.kind == LOC_CONST) {
        /* 8-bit */
        _or_value_to_a(arg1_location.value & 0xff, file_out);
      }
      else if (arg1_in_hl == YES)
        fprintf(file_out, "      OR  A,L\n");
      else if (arg1_in_bc == YES)
        fprintf(file_out, "      OR  A,C\n");
      else if (arg1_in_b == YES)
        fprintf(file_out, "      OR  A,B\n");
      else if (arg1_in_c == YES)
        fprintf(file_out, "      OR  A,C\n");
      else {
        /* 8-bit */
        _or_from_ix_to_a(ix_offset, file_out);
      }
    }
    else if (arg2_location.kind == LOC_CONST) {
      /* 8-bit */
      _or_value_to_a(arg2_location.value & 0xff, file_out);
    }
    else if (arg2_in_hl == YES)
      fprintf(file_out, "      OR  A,L\n");
    else if (arg2_in_bc == YES)
      fprintf(file_out, "      OR  A,C\n");
    else if (arg2_in_b == YES)
      fprintf(file_out, "      OR  A,B\n");
    else if (arg2_in_c == YES)
      fprintf(file_out, "      OR  A,C\n");
    else {
      /* 8-bit */
      _or_from_ix_to_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_XOR) {
    if (arg2_in_a == YES) {
      if (arg1_location.kind == LOC_CONST) {
        /* 8-bit */
        _xor_value_to_a(arg1_location.value & 0xff, file_out);
      }
      else if (arg1_in_hl == YES)
        fprintf(file_out, "      XOR A,L\n");
      else if (arg1_in_bc == YES)
        fprintf(file_out, "      XOR A,C\n");
      else if (arg1_in_b == YES)
        fprintf(file_out, "      XOR A,B\n");
      else if (arg1_in_c == YES)
        fprintf(file_out, "      XOR A,C\n");
      else {
        /* 8-bit */
        _xor_from_ix_to_a(ix_offset, file_out);
      }
    }
    else if (arg2_location.kind == LOC_CONST) {
      /* 8-bit */
      _xor_value_to_a(arg2_location.value & 0xff, file_out);
    }
    else if (arg2_in_hl == YES)
      fprintf(file_out, "      XOR A,L\n");
    else if (arg2_in_bc == YES)
      fprintf(file_out, "      XOR A,C\n");
    else if (arg2_in_b == YES)
      fprintf(file_out, "      XOR A,B\n");
    else if (arg2_in_c == YES)
      fprintf(file_out, "      XOR A,C\n");
    else {
      /* 8-bit */
      _xor_from_ix_to_a(ix_offset, file_out);
    }
  }
  else if (op == TAC_OP_AND) {
    if (arg2_in_a == YES) {
      if (arg1_location.kind == LOC_CONST) {
        /* 8-bit */
        _and_value_from_a(arg1_location.value & 0xff, file_out);
      }
      else if (arg1_in_hl == YES)
        fprintf(file_out, "      AND A,L\n");
      else if (arg1_in_bc == YES)
        fprintf(file_out, "      AND A,C\n");
      else if (arg1_in_b == YES)
        fprintf(file_out, "      AND A,B\n");
      else if (arg1_in_c == YES)
        fprintf(file_out, "      AND A,C\n");
      else {
        /* 8-bit */
        _and_from_ix_from_a(ix_offset, file_out);
      }
    }
    else if (arg2_location.kind == LOC_CONST) {
      /* 8-bit */
      _and_value_from_a(arg2_location.value & 0xff, file_out);
    }
    else if (arg2_in_hl == YES)
      fprintf(file_out, "      AND A,L\n");
    else if (arg2_in_bc == YES)
      fprintf(file_out, "      AND A,C\n");
    else if (arg2_in_b == YES)
      fprintf(file_out, "      AND A,B\n");
    else if (arg2_in_c == YES)
      fprintf(file_out, "      AND A,C\n");
    else {
      /* 8-bit */
      _and_from_ix_from_a(ix_offset, file_out);
    }
  }
  else {
    fprintf(stderr, "_bbb_generate_asm_add_sub_or_xor_and_z80_8bit(): Unsupported TAC op %d! Please submit a bug report!\n", op);
    return FAILED;
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_in_a == YES) {
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_in_b == YES) {
    fprintf(file_out, "      LD  B,A\n");
    fprintf(file_out, "      ; retained result in B\n");
    return SUCCEEDED;
  }
  if (result_in_c == YES) {
    fprintf(file_out, "      LD  C,A\n");
    fprintf(file_out, "      ; retained result in C\n");
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */

    /* lower byte */
    if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
      return FAILED;

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
    if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
      return FAILED;
  }

  return SUCCEEDED;
}


static int _aaa_generate_asm_add_sub_or_xor_and_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_hl = NO, arg2_in_hl = NO, result_in_hl = NO, operand_is_constant_one = NO;
  int arg1_in_bc = NO, arg2_in_bc = NO, result_in_bc = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_hl = arg1_location.kind == LOC_PHY_HL ? YES : NO;
  arg2_in_hl = arg2_location.kind == LOC_PHY_HL ? YES : NO;
  result_in_hl = result_location.kind == LOC_PHY_HL ? YES : NO;
  arg1_in_bc = arg1_location.kind == LOC_PHY_BC ? YES : NO;
  arg2_in_bc = arg2_location.kind == LOC_PHY_BC ? YES : NO;
  result_in_bc = result_location.kind == LOC_PHY_BC ? YES : NO;

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_in_bc == YES) {
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> hl */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
  }
  else if (arg1_in_bc == YES) {
    if (arg2_in_hl == NO) {
      if (_materialize_to_hl(&arg1_location, file_out) == FAILED)
        return FAILED;
    }
  }
  else if (arg2_in_hl == YES) {
  }
  else if (arg1_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_hl(arg1_location.value, file_out);
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
        fprintf(stderr, "_aaa_generate_asm_add_sub_or_xor_and_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_in_hl == YES) {
    fprintf(file_out, "      ; retained arg2 in HL\n");

    if (op == TAC_OP_SUB) {
      fprintf(stderr, "_aaa_generate_asm_add_sub_or_xor_and_z80_16bit(): Retained ARG2 is not supported for SUB! Please submit a bug report!\n");
      return FAILED;
    }
  }
  else if (arg2_in_bc == YES) {
    fprintf(file_out, "      ; retained arg2 in BC\n");
  }
  else {
    ix_offset = 0;

    if (arg2_location.kind == LOC_CONST) {
    }
    else if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> bc */
  /******************************************************************************************************/

  if (arg2_in_bc == YES) {
  }
  else if (arg2_in_hl == YES) {
    if (arg1_location.kind == LOC_CONST) {
      /* 16-bit */
      if (op == TAC_OP_ADD && arg1_location.value == 1)
        operand_is_constant_one = YES;
      else
        _load_value_to_bc(arg1_location.value, file_out);
    }
    else if (arg1_in_bc == YES) {
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
          fprintf(stderr, "_aaa_generate_asm_add_sub_or_xor_and_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
          print_tac(t, NO, stderr);
          return FAILED;
        }
      }
    }
  }
  else if (arg2_location.kind == LOC_CONST) {
    /* 16-bit */
    if ((op == TAC_OP_ADD || op == TAC_OP_SUB) && arg2_location.value == 1) {
      operand_is_constant_one = YES;
    }
    else
      _load_value_to_bc(arg2_location.value, file_out);
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
        fprintf(stderr, "_aaa_generate_asm_add_sub_or_xor_and_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* add/sub/or/xor/and data bc -> hl */
  /******************************************************************************************************/

  if (op == TAC_OP_ADD) {
    if (operand_is_constant_one == YES)
      _inc_hl(file_out);
    else
      _add_bc_to_hl(file_out);
  }
  else if (op == TAC_OP_SUB) {
    if (operand_is_constant_one == YES)
      _dec_hl(file_out);
    else
      _sub_bc_from_hl(file_out);
  }
  else if (op == TAC_OP_OR)
    _or_bc_to_hl(file_out);
  else if (op == TAC_OP_XOR)
    _xor_bc_to_hl(file_out);
  else if (op == TAC_OP_AND)
    _and_bc_to_hl(file_out);
  else {
    fprintf(stderr, "_aaa_generate_asm_add_sub_or_xor_and_z80_16bit(): Unsupported TAC op %d! Please submit a bug report!\n", op);
    return FAILED;
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_in_hl == YES) {
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_in_bc == YES) {
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_add_sub_or_xor_and_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _bbb_generate_asm_add_sub_or_xor_and_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
      return _aaa_generate_asm_add_sub_or_xor_and_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_add_sub_or_xor_and_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _generate_asm_complement_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  int ix_offset = 0;
  int result_in_a = NO, result_in_b = NO, result_in_c = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  result_in_a = result_location.kind == LOC_PHY_A ? YES : NO;
  result_in_b = result_location.kind == LOC_PHY_B ? YES : NO;
  result_in_c = result_location.kind == LOC_PHY_C ? YES : NO;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: complement_8bit function=%s arg1_kind=%s arg1_phy=%s result_kind=%s result_phy=%s\n",
            function_node->children[1]->label,
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy),
            _get_z80_location_kind_name(result_location.kind), _get_z80_physical_register_name(result_location.phy));
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_A) {
  }
  else {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "complement_8bit_arg1_address", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_IX, ix_offset);
  }

  /******************************************************************************************************/
  /* copy data (arg1) -> a */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_a(arg1_location.value & 0xff, file_out);
  }
  else if (arg1_location.kind == LOC_PHY_A) {
  }
  else {
    /* 8-bit */
    _load_from_ix_to_a(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* complement a */
  /******************************************************************************************************/

  /* 8-bit */
  _xor_value_to_a(0xff, file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_in_a == YES || result_in_b == YES || result_in_c == YES) {
    if (_store_from_a(&result_location, 0, 0, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      ; retained result in %s\n", _get_z80_physical_register_name(result_location.phy));
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  _trace_z80_specialized_address_materialization(function_node, "complement_8bit_result_address", "result",
                                                  &result_location, RA_ADDRESS_TARGET_IX, ix_offset);

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */

    /* lower byte */
    if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
      return FAILED;

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
    if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
      return FAILED;
  }

  return SUCCEEDED;
}


static int _generate_asm_complement_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  int ix_offset = 0;
  int result_in_hl = NO;
  int result_in_bc = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  result_in_hl = result_location.kind == LOC_PHY_HL ? YES : NO;
  result_in_bc = result_location.kind == LOC_PHY_BC ? YES : NO;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: complement_16bit function=%s arg1_kind=%s arg1_phy=%s result_kind=%s result_phy=%s\n",
            function_node->children[1]->label,
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy),
            _get_z80_location_kind_name(result_location.kind), _get_z80_physical_register_name(result_location.phy));
  }
#endif

  /* generate asm */

  if (result_in_bc == YES) {
    if (arg1_location.kind == LOC_CONST) {
      _load_value_to_bc(arg1_location.value, file_out);
    }
    else if (arg1_location.kind == LOC_PHY_BC) {
    }
    else if (arg1_location.kind == LOC_PHY_HL) {
      _load_hl_to_bc(file_out);
    }
    else if (arg1_location.kind == LOC_PHY_A) {
      fprintf(file_out, "      LD  C,A\n");

      if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                     t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        _sign_extend_c_to_bc(file_out);
      }
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                           t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
        _load_value_to_b(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_complement_z80_16bit(): Unhandled A ARG1 for BC result! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
    else {
      if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
        return FAILED;
      _trace_z80_specialized_address_materialization(function_node, "complement_16bit_arg1_address", "arg1",
                                                      &arg1_location, RA_ADDRESS_TARGET_IX, ix_offset);

      if (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) {
        _load_from_ix_to_c(ix_offset, file_out);
        _load_from_ix_to_b(ix_offset + 1, file_out);
      }
      else {
        _load_from_ix_to_c(ix_offset, file_out);

        if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                       t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
          _sign_extend_c_to_bc(file_out);
        }
        else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                             t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
          _load_value_to_b(0, file_out);
        }
        else {
          fprintf(stderr, "_generate_asm_complement_z80_16bit(): Unhandled 8-bit ARG1 for BC result! Please submit a bug report!\n");
          print_tac(t, NO, stderr);
          return FAILED;
        }
      }
    }

    _complement_bc(file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_HL || arg1_location.kind == LOC_PHY_A) {
  }
  else {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "complement_16bit_arg1_address", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_IX, ix_offset);
  }

  /******************************************************************************************************/
  /* copy data (arg1) -> hl */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_hl(arg1_location.value, file_out);
  }
  else if (arg1_location.kind == LOC_PHY_HL) {
  }
  else if (arg1_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  L,A\n");

    if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                   t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _sign_extend_l_to_hl(file_out);
    }
    else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                         t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _load_value_to_h(0, file_out);
    }
    else {
      fprintf(stderr, "_generate_asm_complement_z80_16bit(): Unhandled A ARG1! Please submit a bug report!\n");
      print_tac(t, NO, stderr);
      return FAILED;
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
        fprintf(stderr, "_generate_asm_complement_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* complement hl */
  /******************************************************************************************************/

  _complement_hl(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_in_hl == YES) {
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  _trace_z80_specialized_address_materialization(function_node, "complement_16bit_result_address", "result",
                                                  &result_location, RA_ADDRESS_TARGET_IX, ix_offset);

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
    /* 16-bit */
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_complement_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  /* 8-bit or 16-bit? */
  if (t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8)
    return _generate_asm_complement_z80_8bit(t, file_out, function_node);
  else if (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)
    return _generate_asm_complement_z80_16bit(t, file_out, function_node);

  fprintf(stderr, "_generate_asm_complement_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _get_address_arg1_needs_memory_pointer_load(struct tac *tac, int op, struct z80_location *arg1_location) {

  if (op != TAC_OP_GET_ADDRESS_ARRAY)
    return NO;

  if (arg1_location->kind == LOC_PHY_A || arg1_location->kind == LOC_PHY_HL || arg1_location->kind == LOC_PHY_BC)
    return NO;

  if (tac->arg1_type == TAC_ARG_TYPE_TEMP)
    return YES;

  if (tac->arg1_node != NULL && tac->arg1_node->children[0]->value_double > 0.0 && tac->arg1_node->value == 0)
    return YES;

  return NO;
}


static void _load_pointer_value_at_hl_to_hl(FILE *file_out) {

  fprintf(file_out, "      LD  A,(HL)\n");
  fprintf(file_out, "      INC HL\n");
  fprintf(file_out, "      LD  H,(HL)\n");
  fprintf(file_out, "      LD  L,A\n");
}


static int _generate_asm_get_address_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0, index_materialized = NO, base_memory_pointer_load = NO, retained_bc_base_hl_index = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  base_memory_pointer_load = _get_address_arg1_needs_memory_pointer_load(t, op, &arg1_location);

  /* arg2 */

  if (op == TAC_OP_GET_ADDRESS_ARRAY) {
    if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
      return FAILED;

    if (arg1_location.kind == LOC_PHY_BC && arg2_location.kind == LOC_PHY_BC) {
      fprintf(stderr, "_generate_asm_get_address_z80(): Cannot use retained arg1 in BC with a retained BC index! Please submit a bug report!\n");
      return FAILED;
    }

    if (arg1_location.kind == LOC_PHY_BC && arg2_location.kind == LOC_PHY_HL) {
      retained_bc_base_hl_index = YES;
      index_materialized = YES;
    }
    else if (arg2_location.kind == LOC_PHY_HL || arg2_location.kind == LOC_PHY_BC) {
      if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED) {
        print_tac(t, NO, stderr);
        return FAILED;
      }
      index_materialized = YES;
    }
  }

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> hl */
  /******************************************************************************************************/

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: get_address_base function=%s kind=%s memory_pointer_load=%s offset=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location.kind), base_memory_pointer_load == YES ? "yes" : "no",
            arg1_location.offset, _get_z80_physical_register_name(arg1_location.phy));
  }
#endif

  if (arg1_location.kind == LOC_CONST) {
    fprintf(stderr, "_generate_asm_get_address_z80(): Source cannot be a value!\n");
    return FAILED;
  }
  else if (arg1_location.kind == LOC_PHY_A) {
    fprintf(stderr, "_generate_asm_get_address_z80(): Cannot use an 8-bit retained arg1 as a get-address base! Please submit a bug report!\n");
    return FAILED;
  }
  else if (arg1_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    if (retained_bc_base_hl_index == NO)
      _load_bc_to_hl(file_out);
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else {
    if (_load_location_address_to_hl(&arg1_location, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "get_address_base", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_HL, 0);
  }

  if (base_memory_pointer_load == YES)
    _load_pointer_value_at_hl_to_hl(file_out);

  if (op == TAC_OP_GET_ADDRESS_ARRAY) {
    int var_type;

    /******************************************************************************************************/
    /* index address (arg2) -> ix */
    /******************************************************************************************************/

    if (index_materialized == YES) {
    }
    else if (arg2_location.kind == LOC_CONST) {
    }
    else {
      if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
        return FAILED;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
      _trace_z80_specialized_address_materialization(function_node, "get_address_index", "arg2",
                      &arg2_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif
    }

    /******************************************************************************************************/
    /* copy data (ix) -> bc */
    /******************************************************************************************************/

    if (index_materialized == YES) {
    }
    else if (arg2_location.kind == LOC_CONST) {
      /* 16-bit */
      _load_value_to_bc(arg2_location.value, file_out);
    }
    else if (arg2_location.kind == LOC_PHY_A) {
      if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED) {
        print_tac(t, NO, stderr);
        return FAILED;
      }
      index_materialized = YES;
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
          print_tac(t, NO, stderr);
          return FAILED;
        }
      }
    }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES) {
      char *function_name;

      function_name = "<unknown>";
      if (function_node != NULL && function_node->children[1] != NULL)
        function_name = function_node->children[1]->label;

      fprintf(stderr, "register_allocator: get_address_index function=%s kind=%s size=%d phy=%s\n",
              function_name, _get_z80_location_kind_name(arg2_location.kind),
              arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
    }
#endif

    /******************************************************************************************************/
    /* add bc -> hl */
    /******************************************************************************************************/

    if (t->arg1_node != NULL)
      var_type = get_array_item_variable_type(t->arg1_node);
    else
      var_type = t->arg1_var_type;

    if (retained_bc_base_hl_index == YES) {
      fprintf(file_out, "      ; retained arg2 in HL\n");
      if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16)
        _add_hl_to_hl(file_out);
      _add_bc_to_hl(file_out);
    }
    else {
      _add_bc_to_hl(file_out);

      if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
        /* the array has 16-bit items -> add bc -> hl twice! */
        _add_bc_to_hl(file_out);
      }
    }
  }

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (!(t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16)) {
    fprintf(stderr, "_generate_asm_get_address_z80(): The target cannot be an 8-bit value!\n");
    return FAILED;
  }

  if (result_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_BC) {
    _load_hl_to_bc(file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_A) {
    fprintf(stderr, "_generate_asm_get_address_z80(): Cannot retain an address result in A! Please submit a bug report!\n");
    return FAILED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
    return FAILED;

  return SUCCEEDED;
}


static int _generate_asm_z80_in_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg2_location;
  int ix_offset = 0;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* index address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_location.kind == LOC_GLOBAL || arg2_location.kind == LOC_STACK_LOCAL || arg2_location.kind == LOC_STACK_SPILL) {
    if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "z80_in_port_address", "arg2",
                                                    &arg2_location, RA_ADDRESS_TARGET_IX, ix_offset);
  }

  /******************************************************************************************************/
  /* copy data (ix) -> c */
  /******************************************************************************************************/

  if (_materialize_z80_port_to_c(&arg2_location, ix_offset, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: z80_in_port function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
  }
#endif

  /******************************************************************************************************/
  /* in a, (?) / in a, (c) */
  /******************************************************************************************************/

  if (arg2_location.kind == LOC_CONST)
    _in_a_from_value(arg2_location.value & 0xff, file_out);
  else
    _in_a_from_c(file_out);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: z80_in_result function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy));
  }
#endif

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  L,A\n");
    _load_value_to_h(0, file_out);
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  C,A\n");
    _load_value_to_b(0, file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_C) {
    fprintf(file_out, "      LD  C,A\n");
    fprintf(file_out, "      ; retained result in C\n");
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

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


static void _load_pointer_value_at_iy_to_iy(int arg2_is_zero, FILE *file_out);


static int _array_read_arg1_needs_memory_pointer_load(struct tac *tac, struct z80_location *arg1_location) {

  if (arg1_location->kind == LOC_PHY_A || arg1_location->kind == LOC_PHY_HL || arg1_location->kind == LOC_PHY_BC)
    return NO;

  if (tac->arg1_type == TAC_ARG_TYPE_TEMP)
    return YES;

  if (tac->arg1_node != NULL && tac->arg1_node->children[0]->value_double > 0.0 && tac->arg1_node->value == 0)
    return YES;

  return NO;
}


static int _materialize_array_read_arg1_to_iy(struct tac *tac, struct z80_location *arg1_location, int base_memory_pointer_load,
                                              struct tree_node *function_node, FILE *file_out) {

  int arg2_is_zero;

  arg2_is_zero = tac->arg2_type == TAC_ARG_TYPE_CONSTANT && ((int)tac->arg2_d) == 0;

  if (arg1_location->kind == LOC_CONST) {
    fprintf(stderr, "_materialize_array_read_arg1_to_iy(): Source cannot be a value!\n");
    return FAILED;
  }
  else if (arg1_location->kind == LOC_PHY_A) {
    fprintf(stderr, "_materialize_array_read_arg1_to_iy(): Cannot use an 8-bit retained arg1 as an array-read base! Please submit a bug report!\n");
    return FAILED;
  }
  else if (arg1_location->kind == LOC_PHY_HL) {
    _load_hl_to_iy(file_out);
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location->kind == LOC_PHY_BC) {
    _load_bc_to_iy(file_out);
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else {
    if (_load_location_address_to_iy(arg1_location, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "array_read_base_address", "arg1",
                                                    arg1_location, RA_ADDRESS_TARGET_IY, 0);
  }

  if (base_memory_pointer_load == YES)
    _load_pointer_value_at_iy_to_iy(arg2_is_zero, file_out);

  return SUCCEEDED;
}


static int _spill_retained_array_read_arg1(struct tac *t, struct tree_node *function_node, struct z80_location *arg1_location, FILE *file_out) {

  struct z80_location spill_location;
  struct register_allocator_spill_emission emission;
  char *function_name;
  int ix_offset;

  if (t->store_retained_to_spill_operand != TAC_USE_ARG1)
    return SUCCEEDED;

  function_name = "<unknown>";
  if (function_node != NULL && function_node->children[1] != NULL)
    function_name = function_node->children[1]->label;

  if (t->arg1_type != TAC_ARG_TYPE_TEMP) {
    fprintf(stderr, "_spill_retained_array_read_arg1(): Expected a retained HL temp! Please submit a bug report!\n");
    return FAILED;
  }

  if (_resolve_z80_location(t->arg1_type, t->arg1_s, (int)t->arg1_d, t->arg1_node, t->arg1_var_type,
                            function_node, &spill_location, "arg1_spill", Z80_PHY_NONE) == FAILED)
    return FAILED;
  if (spill_location.kind != LOC_STACK_SPILL) {
    fprintf(stderr, "_spill_retained_array_read_arg1(): Retained multi-read temp has no spill slot! Please submit a bug report!\n");
    return FAILED;
  }

  if (register_allocator_prepare_spill_emission(function_name, YES,
      (int)t->arg1_d, Z80_PHY_NONE, t->arg1_physical_register, YES, spill_location.offset, 2,
      &emission) == FAILED)
    return FAILED;
  if (emission.emit != YES || emission.physical_register != Z80_PHY_HL ||
      emission.byte_count != 2 || arg1_location->kind != LOC_PHY_HL) {
    fprintf(stderr, "_spill_retained_array_read_arg1(): Expected a retained HL temp! Please submit a bug report!\n");
    return FAILED;
  }

  if (_load_location_address_to_ix(&spill_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  if (_store_from_hl(&spill_location, ix_offset, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: spill_insert_emit function=%s op=ARRAY_READ operand=arg1 phy=HL spill_offset=%d bytes=2\n",
            function_name, emission.destination_offset);
  }
#endif

  fprintf(file_out, "      ; preserve retained arg1 in spill slot\n");

  return SUCCEEDED;
}


static int _generate_asm_array_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int var_type, result_needs_16_bit, ix_offset = 0, base_memory_pointer_load = NO, arg2_is_zero = NO, base_materialized = NO;

  /* OVERRIDE for __z80_in */
  if (t->arg1_type == TAC_ARG_TYPE_LABEL && strcmp(t->arg1_s, "__z80_in") == 0)
    return _generate_asm_z80_in_read_z80(t, file_out, function_node);

  /* error for a read from __z80_out */
  if (t->arg1_type == TAC_ARG_TYPE_LABEL && strcmp(t->arg1_s, "__z80_out") == 0) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_arm_array_read_z80(): You cannot read from \"__z80_out\"!\n");
    return print_error_using_tac(g_error_message, ERROR_ERR, t);
  }

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  base_memory_pointer_load = _array_read_arg1_needs_memory_pointer_load(t, &arg1_location);

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  if (_spill_retained_array_read_arg1(t, function_node, &arg1_location, file_out) == FAILED)
    return FAILED;

  if (arg2_location.kind == LOC_CONST && arg2_location.value == 0)
    arg2_is_zero = YES;

  if (arg1_location.kind == LOC_PHY_BC && arg2_location.kind == LOC_PHY_BC) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Cannot use retained arg1 in BC with a retained BC index! Please submit a bug report!\n");
    return FAILED;
  }

  if (arg1_location.kind == LOC_PHY_BC && arg2_is_zero == NO) {
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES) {
      char *function_name;

      function_name = "<unknown>";
      if (function_node != NULL && function_node->children[1] != NULL)
        function_name = function_node->children[1]->label;

      fprintf(stderr, "register_allocator: array_read_base function=%s kind=%s memory_pointer_load=%s offset=%d phy=%s\n",
              function_name, _get_z80_location_kind_name(arg1_location.kind), base_memory_pointer_load == YES ? "yes" : "no",
              arg1_location.offset, _get_z80_physical_register_name(arg1_location.phy));
    }
#endif

    if (_materialize_array_read_arg1_to_iy(t, &arg1_location, base_memory_pointer_load, function_node, file_out) == FAILED)
      return FAILED;
    base_materialized = YES;
  }

  /* generate asm */

  /******************************************************************************************************/
  /* index address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_location.kind == LOC_GLOBAL || arg2_location.kind == LOC_STACK_LOCAL || arg2_location.kind == LOC_STACK_SPILL) {
    if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (ix) -> bc */
  /******************************************************************************************************/

  if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED) {
    print_tac(t, NO, stderr);
    return FAILED;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: array_read_index function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
  }
#endif

  /******************************************************************************************************/
  /* source address (arg1) -> iy */
  /******************************************************************************************************/

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES && base_materialized == NO) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: array_read_base function=%s kind=%s memory_pointer_load=%s offset=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location.kind), base_memory_pointer_load == YES ? "yes" : "no",
            arg1_location.offset, _get_z80_physical_register_name(arg1_location.phy));
  }
#endif

  if (base_materialized == NO && _materialize_array_read_arg1_to_iy(t, &arg1_location, base_memory_pointer_load, function_node, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* add bc -> iy */
  /******************************************************************************************************/

  /* NOTE! var_type is actually the type of the item we have inside arg1 array */
  if (t->arg1_node != NULL) {
    /* we come here only if arg1 has been a variable, not a temp register */
    var_type = get_array_item_variable_type(t->arg1_node);
  }
  else
    var_type = t->arg1_var_type;

  result_needs_16_bit = result_location.size == 2 ||
                        t->result_var_type_promoted == VARIABLE_TYPE_INT16 ||
                        t->result_var_type_promoted == VARIABLE_TYPE_UINT16;

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
      if (var_type == VARIABLE_TYPE_INT8 && result_needs_16_bit) {
        /* yes */
        _sign_extend_l_to_hl(file_out);
      }
      else if (var_type == VARIABLE_TYPE_UINT8 && result_needs_16_bit) {
        /* upper byte -> 0 */
        _load_value_to_h(0, file_out);
      }
      else if ((var_type == VARIABLE_TYPE_UINT8 || var_type == VARIABLE_TYPE_INT8) &&
               (t->result_var_type_promoted == VARIABLE_TYPE_INT8 || t->result_var_type_promoted == VARIABLE_TYPE_UINT8)) {
      }
      else {
        fprintf(stderr, "_generate_asm_array_read_z80(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: array_read_result function=%s kind=%s size=%d phy=%s value_bytes=%d\n",
            function_node->children[1]->label, _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy), result_needs_16_bit == YES ? 2 : 1);
  }
#endif

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A) {
    if (result_needs_16_bit) {
      fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain a 16-bit array read result in A! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      LD  A,L\n");
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_HL) {
    if (!result_needs_16_bit) {
      fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain an 8-bit array read result in HL! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_BC) {
    if (result_needs_16_bit) {
      _load_hl_to_bc(file_out);
      fprintf(file_out, "      ; retained result in BC\n");
      return SUCCEEDED;
    }
    fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain an 8-bit array read result in BC! Please submit a bug report!\n");
    return FAILED;
  }
  if (result_location.kind == LOC_PHY_C) {
    if (result_needs_16_bit) {
      fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain a 16-bit array read result in C! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      LD  C,L\n");
    fprintf(file_out, "      ; retained result in C\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_B) {
    if (result_needs_16_bit) {
      fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain a 16-bit array read result in B! Please submit a bug report!\n");
      return FAILED;
    }
    fprintf(file_out, "      LD  B,L\n");
    fprintf(file_out, "      ; retained result in B\n");
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      fprintf(stderr, "register_allocator: array_read_physical_result function=%s result_phy=B value_bytes=1 via=L store=B status=complete\n",
              function_node->children[1]->label);
#endif
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data hl -> (ix) */
  /******************************************************************************************************/

  if (result_needs_16_bit) {
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_z80_out_write_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int arg1_ix_offset = 0, arg2_ix_offset = 0;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* port address (arg2) -> c */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_PHY_BC && arg2_location.kind != LOC_CONST) {
    fprintf(stderr, "_generate_asm_z80_out_write_z80(): Cannot use retained arg1 in BC with a non-constant port! Please submit a bug report!\n");
    return FAILED;
  }

  if (arg2_location.kind == LOC_GLOBAL || arg2_location.kind == LOC_STACK_LOCAL || arg2_location.kind == LOC_STACK_SPILL) {
    if (_load_location_address_to_ix(&arg2_location, &arg2_ix_offset, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "z80_out_port_address", "arg2",
                                                    &arg2_location, RA_ADDRESS_TARGET_IX, arg2_ix_offset);
  }

  if (_materialize_z80_port_to_c(&arg2_location, arg2_ix_offset, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: z80_out_port function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
    fprintf(stderr, "register_allocator: z80_out_value function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy));
  }
#endif

  /******************************************************************************************************/
  /* source value (arg1) -> a */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST) {
    _load_value_to_a(arg1_location.value & 0xff, file_out);
  }
  else if (arg1_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      ; retained arg1 in A\n");
  }
  else if (arg1_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  A,L\n");
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  A,C\n");
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else {
    if (_load_location_address_to_ix(&arg1_location, &arg1_ix_offset, file_out) == FAILED)
      return FAILED;

    /******************************************************************************************************/
    /* copy data (ix) -> a */
    /******************************************************************************************************/

    _load_from_ix_to_a(arg1_ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* out (?), a / out (c), a */
  /******************************************************************************************************/

  if (arg2_location.kind == LOC_CONST)
    _out_a_into_value(arg2_location.value & 0xff, file_out);
  else
    _out_a_into_c(file_out);

  return SUCCEEDED;
}


static int _array_write_result_needs_memory_pointer_load(struct tac *tac, struct z80_location *result_location) {

  if (result_location->kind == LOC_PHY_A || result_location->kind == LOC_PHY_HL || result_location->kind == LOC_PHY_BC)
    return NO;

  if (tac->result_type == TAC_ARG_TYPE_TEMP)
    return YES;

  if (tac->result_node != NULL && tac->result_node->children[0]->value_double > 0.0 && tac->result_node->value == 0)
    return YES;

  return NO;
}


static void _load_pointer_value_at_iy_to_iy(int arg2_is_zero, FILE *file_out) {

  if (arg2_is_zero == NO)
    _push_bc(file_out);

  _load_from_iy_to_c(0, file_out);
  _load_from_iy_to_b(1, file_out);

  _load_value_to_iy(0, file_out);
  _add_bc_to_iy(file_out);

  if (arg2_is_zero == NO)
    _pop_bc(file_out);
}


static int _materialize_array_write_result_to_iy(struct tac *tac, struct z80_location *result_location, int arg2_is_zero, struct tree_node *function_node, FILE *file_out) {

  int memory_pointer_load;

  memory_pointer_load = _array_write_result_needs_memory_pointer_load(tac, result_location);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: array_write_base function=%s kind=%s memory_pointer_load=%s offset=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(result_location->kind), memory_pointer_load == YES ? "yes" : "no",
            result_location->offset, _get_z80_physical_register_name(result_location->phy));
  }
#endif

  if (result_location->kind == LOC_CONST) {
    _load_value_to_iy(result_location->value, file_out);
  }
  else if (result_location->kind == LOC_PHY_A) {
    fprintf(stderr, "_materialize_array_write_result_to_iy(): Cannot use an 8-bit retained result as an array-write base! Please submit a bug report!\n");
    return FAILED;
  }
  else if (result_location->kind == LOC_PHY_HL) {
    _load_hl_to_iy(file_out);
    fprintf(file_out, "      ; retained result in HL\n");
  }
  else if (result_location->kind == LOC_PHY_BC) {
    if (arg2_is_zero == NO) {
      fprintf(stderr, "_materialize_array_write_result_to_iy(): Cannot use retained result in BC with a non-zero index! Please submit a bug report!\n");
      return FAILED;
    }
    _load_bc_to_iy(file_out);
    fprintf(file_out, "      ; retained result in BC\n");
  }
  else if (result_location->kind == LOC_GLOBAL || result_location->kind == LOC_STACK_LOCAL || result_location->kind == LOC_STACK_SPILL) {
    if (_load_location_address_to_iy(result_location, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "array_write_base_address", "result",
                                                    result_location, RA_ADDRESS_TARGET_IY, 0);

    if (memory_pointer_load == YES)
      _load_pointer_value_at_iy_to_iy(arg2_is_zero, file_out);
  }
  else {
    fprintf(stderr, "_materialize_array_write_result_to_iy(): Unknown result location kind %d! Please submit a bug report!\n", result_location->kind);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _reload_array_write_result_from_spill(struct tac *t, struct tree_node *function_node, struct z80_location *result_location, FILE *file_out) {

  struct z80_location spill_location;
  struct register_allocator_reload_emission emission;
  char *function_name;
  int ix_offset;

  if (t->reload_spill_to_physical_operand == -1)
    return SUCCEEDED;
  function_name = "<unknown>";
  if (function_node != NULL && function_node->children[1] != NULL)
    function_name = function_node->children[1]->label;
  if (z80_validate_array_write_reload_operand(function_name,
      t->reload_spill_to_physical_operand) == FAILED)
    return FAILED;

  if (t->result_type != TAC_ARG_TYPE_TEMP) {
    fprintf(stderr, "_reload_array_write_result_from_spill(): Expected a retained BC temp! Please submit a bug report!\n");
    return FAILED;
  }

  if (_resolve_z80_location(t->result_type, t->result_s, (int)t->result_d, t->result_node, t->result_var_type,
                            function_node, &spill_location, "result_reload", Z80_PHY_NONE) == FAILED)
    return FAILED;
  if (spill_location.kind != LOC_STACK_SPILL) {
    fprintf(stderr, "_reload_array_write_result_from_spill(): Reload temp has no spill slot! Please submit a bug report!\n");
    return FAILED;
  }

  if (register_allocator_prepare_reload_operand_emission(function_name, YES,
      (int)t->result_d, t->reload_spill_to_physical_operand,
      Z80_PHY_NONE, t->result_physical_register, YES,
      spill_location.offset, 2, &emission) == FAILED)
    return FAILED;
  if (emission.emit != YES || emission.operand != TAC_USE_RESULT ||
      emission.physical_register != Z80_PHY_BC ||
      emission.byte_count != 2 || result_location->kind != LOC_PHY_BC) {
    fprintf(stderr, "_reload_array_write_result_from_spill(): Expected a retained BC temp! Please submit a bug report!\n");
    return FAILED;
  }

  if (_load_location_address_to_ix(&spill_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  _load_from_ix_to_c(ix_offset, file_out);
  _load_from_ix_to_b(ix_offset + 1, file_out);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
            fprintf(stderr, "register_allocator: reload_insert_emit function=%s op=ARRAY_WRITE operand=result source_offset=%d bytes=2 phy=BC operand_id=%d\n",
              function_name, emission.source_offset, emission.operand);
  }
#endif

  fprintf(file_out, "      ; reload result from spill slot into BC\n");

  return SUCCEEDED;
}


static int _is_z80_16_bit_variable_type(int var_type) {

  if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16)
    return YES;

  return NO;
}


static int _is_z80_8_bit_variable_type(int var_type) {

  if (var_type == VARIABLE_TYPE_INT8 || var_type == VARIABLE_TYPE_UINT8)
    return YES;

  return NO;
}


static int _get_array_write_value_bytes(int var_type, struct z80_location *arg1_location) {

  if (_is_z80_16_bit_variable_type(var_type) == YES)
    return 2;
  if (_is_z80_8_bit_variable_type(var_type) == YES)
    return 1;

  if (arg1_location->size == 2)
    return 2;

  return 1;
}


static int _array_write_value_source_is_16_bit(struct tac *tac, struct z80_location *arg1_location) {

  if (arg1_location->size == 2)
    return YES;
  if (_is_z80_16_bit_variable_type(tac->arg1_var_type) == YES)
    return YES;

  return NO;
}


static int _materialize_array_write_value_to_iy(struct tac *tac, struct z80_location *arg1_location, int var_type, int ix_offset, struct tree_node *function_node, FILE *file_out) {

  int value;
  int value_bytes;
  int source_is_16_bit;

  value_bytes = _get_array_write_value_bytes(var_type, arg1_location);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: array_write_value function=%s kind=%s bytes=%d source_size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location->kind), value_bytes,
            arg1_location->size, _get_z80_physical_register_name(arg1_location->phy));
  }
#endif

  if (arg1_location->kind == LOC_CONST) {
    value = arg1_location->value;
    _load_value_into_iy(value & 0xff, 0, file_out);
    if (value_bytes == 2)
      _load_value_into_iy((value >> 8) & 0xff, 1, file_out);

    return SUCCEEDED;
  }
  else if (arg1_location->kind == LOC_PHY_A) {
    _load_a_into_iy(0, file_out);
    if (value_bytes == 2) {
      if (tac->arg1_var_type == VARIABLE_TYPE_INT8)
        _sign_extend_a_into_iy(1, file_out);
      else
        _load_value_into_iy(0, 1, file_out);
    }
    fprintf(file_out, "      ; retained arg1 in A\n");
    return SUCCEEDED;
  }
  else if (arg1_location->kind == LOC_PHY_HL) {
    _load_l_into_iy(0, file_out);
    if (value_bytes == 2)
      _load_h_into_iy(1, file_out);
    fprintf(file_out, "      ; retained arg1 in HL\n");
    return SUCCEEDED;
  }
  else if (arg1_location->kind == LOC_PHY_BC) {
    _load_c_into_iy(0, file_out);
    if (value_bytes == 2)
      _load_b_into_iy(1, file_out);
    fprintf(file_out, "      ; retained arg1 in BC\n");
    return SUCCEEDED;
  }

  source_is_16_bit = _array_write_value_source_is_16_bit(tac, arg1_location);
  if (source_is_16_bit == YES) {
    _load_from_ix_to_l(ix_offset, file_out);
    _load_from_ix_to_h(ix_offset + 1, file_out);
  }
  else {
    _load_from_ix_to_l(ix_offset, file_out);

    if (value_bytes == 2) {
      if (tac->arg1_var_type == VARIABLE_TYPE_INT8)
        _sign_extend_l_to_hl(file_out);
      else if (tac->arg1_var_type == VARIABLE_TYPE_UINT8)
        _load_value_to_h(0, file_out);
      else {
        fprintf(stderr, "_materialize_array_write_value_to_iy(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        print_tac(tac, NO, stderr);
        return FAILED;
      }
    }
  }

  if (value_bytes == 2) {
    _load_l_into_iy(0, file_out);
    _load_h_into_iy(1, file_out);
  }
  else {
    _load_l_into_iy(0, file_out);
  }

  return SUCCEEDED;
}


static int _generate_asm_array_write_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int var_type, arg2_is_zero, preserve_a_for_arg1, ix_offset = 0, result_materialized = NO;

  /* OVERRIDE for __z80_out */
  if (t->result_type == TAC_ARG_TYPE_LABEL && strcmp(t->result_s, "__z80_out") == 0)
    return _generate_asm_z80_out_write_z80(t, file_out, function_node);

  /* error for a write to __z80_in */
  if (t->result_type == TAC_ARG_TYPE_LABEL && strcmp(t->result_s, "__z80_in") == 0) {
    snprintf(g_error_message, sizeof(g_error_message), "_generate_arm_array_assignment_z80(): You cannot write to \"__z80_in\"!\n");
    return print_error_using_tac(g_error_message, ERROR_ERR, t);
  }

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg2_is_zero = NO;
  if (arg2_location.kind == LOC_CONST && arg2_location.value == 0)
    arg2_is_zero = YES;

  preserve_a_for_arg1 = arg1_location.kind == LOC_PHY_A ? YES : NO;

  if (arg2_is_zero == NO &&
      _reload_array_write_result_from_spill(t, function_node, &result_location, file_out) == FAILED)
    return FAILED;

  if (result_location.kind == LOC_PHY_BC && arg2_location.kind == LOC_PHY_BC) {
    fprintf(stderr, "_generate_asm_array_write_z80(): Cannot use retained result in BC with a retained BC index! Please submit a bug report!\n");
    return FAILED;
  }

  if (result_location.kind == LOC_PHY_BC && arg2_is_zero == NO) {
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES) {
      char *function_name;

      function_name = "<unknown>";
      if (function_node != NULL && function_node->children[1] != NULL)
        function_name = function_node->children[1]->label;

      fprintf(stderr, "register_allocator: array_write_base function=%s kind=%s memory_pointer_load=no offset=%d phy=%s\n",
              function_name, _get_z80_location_kind_name(result_location.kind),
              result_location.offset, _get_z80_physical_register_name(result_location.phy));
    }
#endif

    _load_bc_to_iy(file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    result_materialized = YES;
  }

  /* generate asm */

  /******************************************************************************************************/
  /* index address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_location.kind == LOC_GLOBAL || arg2_location.kind == LOC_STACK_LOCAL || arg2_location.kind == LOC_STACK_SPILL) {
    if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (ix) -> bc */
  /******************************************************************************************************/

  if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, preserve_a_for_arg1, file_out) == FAILED) {
    print_tac(t, NO, stderr);
    return FAILED;
  }

  if (arg2_is_zero == YES &&
      _reload_array_write_result_from_spill(t, function_node, &result_location, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

        fprintf(stderr, "register_allocator: array_write_index function=%s kind=%s size=%d phy=%s var_type=%d\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
          arg2_location.size, _get_z80_physical_register_name(arg2_location.phy), t->arg2_var_type);
  }
#endif

  /******************************************************************************************************/
  /* result address (result) -> iy */
  /******************************************************************************************************/

  if (result_materialized == NO && _materialize_array_write_result_to_iy(t, &result_location, arg2_is_zero, function_node, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* add index bc -> iy */
  /******************************************************************************************************/

  /* NOTE! var_type is actually the type of the item we have inside result array */
  var_type = t->result_var_type;

  if (arg2_is_zero == NO) {
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

  if (arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL) {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  return _materialize_array_write_value_to_iy(t, &arg1_location, var_type, ix_offset, function_node, file_out);
}


/* NOTE! because SAS/C on Amiga thinks that this and the next function were called the same
   prefixes "aaa" and "bbb" were added to separate them */


static int _aaa_generate_asm_shift_left_right_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_hl = NO, arg1_in_bc = NO, arg1_in_a = NO, arg1_in_b = NO, arg1_in_c = NO;
  int arg2_in_bc = NO, arg2_in_a = NO, result_in_hl = NO, result_in_bc = NO, result_in_b = NO, result_in_c = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_hl = arg1_location.kind == LOC_PHY_HL ? YES : NO;
  arg1_in_bc = arg1_location.kind == LOC_PHY_BC ? YES : NO;
  arg1_in_a = arg1_location.kind == LOC_PHY_A ? YES : NO;
  arg1_in_b = arg1_location.kind == LOC_PHY_B ? YES : NO;
  arg1_in_c = arg1_location.kind == LOC_PHY_C ? YES : NO;
  arg2_in_bc = arg2_location.kind == LOC_PHY_BC ? YES : NO;
  arg2_in_a = arg2_location.kind == LOC_PHY_A ? YES : NO;
  result_in_hl = result_location.kind == LOC_PHY_HL ? YES : NO;
  result_in_bc = result_location.kind == LOC_PHY_BC ? YES : NO;
  result_in_b = result_location.kind == LOC_PHY_B ? YES : NO;
  result_in_c = result_location.kind == LOC_PHY_C ? YES : NO;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: shift_count function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
    fprintf(stderr, "register_allocator: shift_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT",
            _get_z80_location_kind_name(result_location.kind), result_location.size,
            _get_z80_physical_register_name(result_location.phy), t->result_var_type,
            t->result_var_type_promoted);
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_in_bc == YES) {
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else if (arg1_in_a == YES) {
    fprintf(file_out, "      ; retained arg1 in A\n");
  }
  else if (arg1_in_b == YES) {
    fprintf(file_out, "      ; retained arg1 in B\n");
  }
  else if (arg1_in_c == YES) {
    fprintf(file_out, "      ; retained arg1 in C\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> hl */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
    if (t->arg1_var_type == VARIABLE_TYPE_INT8)
      _sign_extend_l_to_hl(file_out);
    else if (t->arg1_var_type == VARIABLE_TYPE_UINT8)
      _load_value_to_h(0, file_out);
  }
  else if (arg1_in_bc == YES) {
    fprintf(file_out, "      LD  L,C\n");
    if (t->arg1_var_type == VARIABLE_TYPE_INT8)
      _sign_extend_l_to_hl(file_out);
    else if (t->arg1_var_type == VARIABLE_TYPE_UINT8)
      _load_value_to_h(0, file_out);
    else
      fprintf(file_out, "      LD  H,B\n");
  }
  else if (arg1_in_a == YES) {
    fprintf(file_out, "      LD  L,A\n");
    if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                   t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16))
      _sign_extend_l_to_hl(file_out);
    else
      _load_value_to_h(0, file_out);
  }
  else if (arg1_in_b == YES || arg1_in_c == YES) {
    fprintf(file_out, "      LD  L,%c\n", arg1_in_b == YES ? 'B' : 'C');
    if (t->arg1_var_type == VARIABLE_TYPE_INT8)
      _sign_extend_l_to_hl(file_out);
    else
      _load_value_to_h(0, file_out);
  }
  else if (arg1_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_hl(arg1_location.value, file_out);
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
        fprintf(stderr, "_aaa_generate_asm_shift_left_right_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;

  if (arg2_in_bc == YES) {
    fprintf(file_out, "      ; retained arg2 in BC\n");
  }
  else if (arg2_in_a == YES) {
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else if (arg2_location.kind == LOC_PHY_HL) {
    fprintf(stderr, "_aaa_generate_asm_shift_left_right_z80_16bit(): Retained ARG2 is not supported for HL. Please submit a bug report!\n");
    return FAILED;
  }
  else if (arg2_location.kind == LOC_CONST) {
  }
  else {
    if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> bc */
  /******************************************************************************************************/

  if (arg2_in_bc == YES) {
  }
  else if (arg2_in_a == YES) {
    fprintf(file_out, "      LD  C,A\n");
    if (t->arg2_var_type == VARIABLE_TYPE_INT8)
      _sign_extend_c_to_bc(file_out);
    else
      _load_value_to_b(0, file_out);
  }
  else if (arg2_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_bc(arg2_location.value, file_out);
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
        fprintf(stderr, "_aaa_generate_asm_shift_left_right_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
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

  if (result_in_hl == YES) {
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_in_bc == YES) {
    _load_hl_to_bc(file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  if (result_in_b == YES || result_in_c == YES) {
    char result_register = result_in_b == YES ? 'B' : 'C';

    fprintf(file_out, "      LD  %c,L\n", result_register);
    fprintf(file_out, "      ; retained result in %c\n", result_register);
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      fprintf(stderr, "register_allocator: shift_physical_result function=%s op=%s result_phy=%c value_register=L transfer=LD_%c_L store=none status=complete\n",
              function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT",
              result_register, result_register);
#endif
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (result_location.kind == LOC_CONST) {
    fprintf(stderr, "_aaa_generate_asm_shift_left_right_z80_16bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (result_location.kind == LOC_GLOBAL || result_location.kind == LOC_STACK_LOCAL || result_location.kind == LOC_STACK_SPILL)
    _trace_z80_specialized_address_materialization(function_node, "shift_16bit_result_address", "result",
                                                   &result_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif

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


static int _bbb_generate_asm_shift_left_right_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_a = NO, arg1_in_c = NO, arg2_in_a = NO, result_in_a = NO, result_in_hl = NO, result_in_bc = NO, result_in_b = NO, result_in_c = NO;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_a = arg1_location.kind == LOC_PHY_A ? YES : NO;
  arg1_in_c = arg1_location.kind == LOC_PHY_C ? YES : NO;
  arg2_in_a = arg2_location.kind == LOC_PHY_A ? YES : NO;
  result_in_a = result_location.kind == LOC_PHY_A ? YES : NO;
  result_in_hl = result_location.kind == LOC_PHY_HL ? YES : NO;
  result_in_bc = result_location.kind == LOC_PHY_BC ? YES : NO;
  result_in_b = result_location.kind == LOC_PHY_B ? YES : NO;
  result_in_c = result_location.kind == LOC_PHY_C ? YES : NO;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: shift_value function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy));
    fprintf(stderr, "register_allocator: shift_count function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
    fprintf(stderr, "register_allocator: shift_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT",
            _get_z80_location_kind_name(result_location.kind), result_location.size,
            _get_z80_physical_register_name(result_location.phy), t->result_var_type,
            t->result_var_type_promoted);
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_a == YES || arg1_in_c == YES) {
    if (arg1_in_a == YES)
      fprintf(file_out, "      ; retained arg1 in A\n");
    else
      fprintf(file_out, "      ; retained arg1 in C\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> b */
  /******************************************************************************************************/

  if (arg1_in_a == YES) {
    fprintf(file_out, "      LD  B,A\n");
  }
  else if (arg1_in_c == YES) {
    fprintf(file_out, "      LD  B,C\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_b(arg1_location.value & 0xff, file_out);
  }
  else {
    /* 8-bit */
    _load_from_ix_to_b(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;

  if (arg2_in_a == YES) {
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else if (arg2_location.kind == LOC_PHY_HL || arg2_location.kind == LOC_PHY_BC) {
    fprintf(stderr, "_bbb_generate_asm_shift_left_right_z80_8bit(): Retained ARG2 is not supported for HL/BC. Please submit a bug report!\n");
    return FAILED;
  }
  else if (arg2_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg2) -> a */
  /******************************************************************************************************/

  if (arg2_in_a == YES) {
  }
  else if (arg2_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_a(arg2_location.value & 0xff, file_out);
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

  if (result_in_a == YES) {
    fprintf(file_out, "      LD  A,B\n");
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_in_hl == YES) {
    fprintf(file_out, "      LD  L,B\n");
    if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                      t->arg2_var_type == VARIABLE_TYPE_INT8))
      _sign_extend_l_to_hl(file_out);
    else
      _load_value_to_h(0, file_out);
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_in_bc == YES) {
    fprintf(file_out, "      LD  C,B\n");
    _load_value_to_b(0, file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  if (result_in_b == YES) {
    fprintf(file_out, "      ; retained result in B\n");
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      fprintf(stderr, "register_allocator: shift_physical_result function=%s op=%s result_phy=B value_register=B store=none status=complete\n",
              function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT");
#endif
    return SUCCEEDED;
  }
  if (result_in_c == YES) {
    fprintf(file_out, "      LD  C,B\n");
    fprintf(file_out, "      ; retained result in C\n");
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      fprintf(stderr, "register_allocator: shift_physical_result function=%s op=%s result_phy=C value_register=B transfer=LD_C_B store=none status=complete\n",
              function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT");
#endif
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (result_location.kind == LOC_CONST) {
    fprintf(stderr, "_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (result_location.kind == LOC_GLOBAL || result_location.kind == LOC_STACK_LOCAL || result_location.kind == LOC_STACK_SPILL)
    _trace_z80_specialized_address_materialization(function_node, "shift_8bit_result_address", "result",
                                                   &result_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif

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
    return _bbb_generate_asm_shift_left_right_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _aaa_generate_asm_shift_left_right_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_shift_left_right_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static char *_get_z80_mul_div_mod_op_name(int op) {

  if (op == TAC_OP_MUL)
    return "MUL";
  if (op == TAC_OP_DIV)
    return "DIV";
  if (op == TAC_OP_MOD)
    return "MOD";

  return "UNKNOWN";
}


static int _generate_asm_mul_div_mod_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0, was_ix_de;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind != LOC_CONST) {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg1) -> bc */
  /******************************************************************************************************/

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: mul_div_mod_arg1 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy));
  }
#endif

  if (arg1_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_bc(arg1_location.value, file_out);
  }
  else if (arg1_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  C,L\n");
    fprintf(file_out, "      LD  B,H\n");
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else if (arg1_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  C,A\n");

    if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                   t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _sign_extend_c_to_bc_preserving_a(file_out);
    }
    else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                         t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _load_value_to_b(0, file_out);
    }
    else {
      fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Cannot use retained A as 16-bit ARG1! Please submit a bug report!\n");
      print_tac(t, NO, stderr);
      return FAILED;
    }

    fprintf(file_out, "      ; retained arg1 in A\n");
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
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;

  if (arg2_location.kind == LOC_CONST || arg2_location.kind == LOC_PHY_A || arg2_location.kind == LOC_PHY_HL || arg2_location.kind == LOC_PHY_BC) {
  }
  else {
    if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    _trace_z80_specialized_address_materialization(function_node, "mul_div_mod_16_arg2", "arg2",
                            &arg2_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> de */
  /******************************************************************************************************/

  /* NOTE!!! */
  _push_de(file_out);

  was_ix_de = g_is_ix_de;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: mul_div_mod_arg2 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
  }
#endif

  if (arg2_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_de(arg2_location.value, file_out);
  }
  else if (arg2_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  E,L\n");
    fprintf(file_out, "      LD  D,H\n");
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else if (arg2_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  E,C\n");
    fprintf(file_out, "      LD  D,B\n");
    fprintf(file_out, "      ; retained arg2 in BC\n");
  }
  else if (arg2_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  E,A\n");

    if (t->arg2_var_type == VARIABLE_TYPE_INT8 && (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                   t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _sign_extend_e_to_de(file_out);
    }
    else if (t->arg2_var_type == VARIABLE_TYPE_UINT8 && (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                         t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _load_value_to_d(0, file_out);
    }
    else {
      fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Cannot use retained A as 16-bit ARG2! Please submit a bug report!\n");
      print_tac(t, NO, stderr);
      return FAILED;
    }

    fprintf(file_out, "      ; retained arg2 in A\n");
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
        print_tac(t, NO, stderr);
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

  g_is_ix_de = was_ix_de;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: mul_div_mod_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy),
            t->result_var_type, t->result_var_type_promoted);
  }
#endif

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A) {
    if (op == TAC_OP_MUL || op == TAC_OP_MOD)
      fprintf(file_out, "      LD  A,L\n");
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_HL) {
    if (op == TAC_OP_DIV) {
      fprintf(file_out, "      LD  L,A\n");
      fprintf(file_out, "      LD  H,C\n");
    }
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_BC) {
    if (op == TAC_OP_DIV) {
      fprintf(file_out, "      LD  B,C\n");
      fprintf(file_out, "      LD  C,A\n");
    }
    else {
      _load_hl_to_bc(file_out);
    }
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  ix_offset = 0;

  if (result_location.kind == LOC_CONST) {
    fprintf(stderr, "_generate_asm_mul_div_mod_z80_16bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

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

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0, was_ix_de;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind != LOC_CONST) {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg1) -> h */
  /******************************************************************************************************/

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: mul_div_mod_arg1 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy));
  }
#endif

  if (arg1_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_h(arg1_location.value & 0xff, file_out);
  }
  else if (arg1_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  H,A\n");
    fprintf(file_out, "      ; retained arg1 in A\n");
  }
  else if (arg1_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  H,L\n");
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  H,C\n");
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else {
    /* 8-bit */
    _load_from_ix_to_h(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  ix_offset = 0;

  if (arg2_location.kind == LOC_CONST || arg2_location.kind == LOC_PHY_A || arg2_location.kind == LOC_PHY_HL || arg2_location.kind == LOC_PHY_BC) {
  }
  else {
    if (_load_location_address_to_ix_uncached(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    _trace_z80_specialized_address_materialization(function_node, "mul_div_mod_8_arg2", "arg2",
                            &arg2_location, RA_ADDRESS_TARGET_IX_UNCACHED, ix_offset);
#endif
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> e */
  /******************************************************************************************************/

  /* NOTE!!! */
  _push_de(file_out);

  was_ix_de = g_is_ix_de;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: mul_div_mod_arg2 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy));
  }
#endif

  if (arg2_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_e(arg2_location.value & 0xff, file_out);
  }
  else if (arg2_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  E,A\n");
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else if (arg2_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  E,L\n");
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else if (arg2_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  E,C\n");
    fprintf(file_out, "      ; retained arg2 in BC\n");
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

  g_is_ix_de = was_ix_de;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: mul_div_mod_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy),
            t->result_var_type, t->result_var_type_promoted);
  }
#endif

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A) {
    if (op == TAC_OP_MUL)
      fprintf(file_out, "      LD  A,L\n");
    else if (op == TAC_OP_MOD)
      fprintf(file_out, "      LD  A,B\n");
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_HL) {
    if (op == TAC_OP_DIV) {
      fprintf(file_out, "      LD  L,A\n");
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8))
        _sign_extend_l_to_hl(file_out);
      else
        _load_value_to_h(0, file_out);
    }
    else if (op == TAC_OP_MOD) {
      fprintf(file_out, "      LD  L,B\n");
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8))
        _sign_extend_l_to_hl(file_out);
      else
        _load_value_to_h(0, file_out);
    }
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_BC) {
    if (op == TAC_OP_MUL) {
      _load_hl_to_bc(file_out);
    }
    else if (op == TAC_OP_DIV) {
      fprintf(file_out, "      LD  C,A\n");
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8))
        _sign_extend_c_to_bc(file_out);
      else
        _load_value_to_b(0, file_out);
    }
    else if (op == TAC_OP_MOD) {
      fprintf(file_out, "      LD  C,B\n");
      if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                        t->arg2_var_type == VARIABLE_TYPE_INT8))
        _sign_extend_c_to_bc(file_out);
      else
        _load_value_to_b(0, file_out);
    }
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_C) {
    if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
      fprintf(stderr, "_generate_asm_mul_div_mod_z80_8bit(): Cannot keep a 16-bit result in C! Please submit a bug report!\n");
      return FAILED;
    }

    if (op == TAC_OP_MUL)
      fprintf(file_out, "      LD  C,L\n");
    else if (op == TAC_OP_DIV)
      fprintf(file_out, "      LD  C,A\n");
    else if (op == TAC_OP_MOD)
      fprintf(file_out, "      LD  C,B\n");
    fprintf(file_out, "      ; retained result in C\n");
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (result_location.kind == LOC_CONST) {
    fprintf(stderr, "_bbb_generate_asm_add_sub_or_and_z80_8bit(): Target cannot be a value!\n");
    return FAILED;
  }
  else if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

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

  /* TODO(register-allocator): retained mul/div/mod operands remain disabled until the input scheduling is migrated. */
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


/* NOTE! because SAS/C on Amiga thinks that this and the next function were called the same
   prefixes "aaa" and "bbb" were added to separate them */


static int _aaa_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_hl = NO, arg2_in_hl = NO;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_hl = arg1_location.kind == LOC_PHY_HL ? YES : NO;
  arg2_in_hl = arg2_location.kind == LOC_PHY_HL ? YES : NO;

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> hl */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
  }
  else {
    if (arg2_in_hl == YES)
      _load_hl_to_bc(file_out);

    if (arg1_location.kind == LOC_CONST) {
    /* 16-bit */
      _load_value_to_hl(arg1_location.value, file_out);
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
          fprintf(stderr, "_aaa_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
          print_tac(t, NO, stderr);
          return FAILED;
        }
      }
    }
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_in_hl == YES) {
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else {
    ix_offset = 0;

    if (arg2_location.kind == LOC_CONST) {
    }
    else if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> bc */
  /******************************************************************************************************/

  if (arg2_in_hl == YES) {
  }
  else if (arg2_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_bc(arg2_location.value, file_out);
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
        fprintf(stderr, "_aaa_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
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


static char *_get_z80_compare_op_name(int op) {

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


static int _bbb_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_a = NO, arg1_in_c = NO;
  int arg2_in_a = NO;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_a = arg1_location.kind == LOC_PHY_A ? YES : NO;
  arg1_in_c = arg1_location.kind == LOC_PHY_C ? YES : NO;
  arg2_in_a = arg2_location.kind == LOC_PHY_A ? YES : NO;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: compare_8bit function=%s op=%d op_name=%s arg1_kind=%s arg1_phy=%s arg2_kind=%s arg2_phy=%s\n",
            function_node->children[1]->label, op, _get_z80_compare_op_name(op),
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy),
            _get_z80_location_kind_name(arg2_location.kind), _get_z80_physical_register_name(arg2_location.phy));
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_a == YES || arg1_in_c == YES) {
    if (arg1_in_a == YES)
      fprintf(file_out, "      ; retained arg1 in A\n");
    else
      fprintf(file_out, "      ; retained arg1 in C\n");
  }
  else if (arg1_location.kind == LOC_CONST) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> a */
  /******************************************************************************************************/

  if (arg1_in_a == YES) {
  }
  else {
    if (arg2_in_a == YES)
      _load_a_to_b(file_out);

    if (arg1_in_c == YES)
      fprintf(file_out, "      LD  A,C\n");
    else if (arg1_location.kind == LOC_CONST) {
    /* 8-bit */
      _load_value_to_a(arg1_location.value & 0xff, file_out);
    }
    else {
    /* 8-bit */
      _load_from_ix_to_a(ix_offset, file_out);
    }
  }

  /******************************************************************************************************/
  /* source address (arg2) -> ix */
  /******************************************************************************************************/

  if (arg2_in_a == YES) {
    fprintf(file_out, "      ; retained arg2 in A\n");
  }
  else {
    ix_offset = 0;

    if (arg2_location.kind == LOC_CONST) {
    }
    else if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> b */
  /******************************************************************************************************/

  if (arg2_in_a == YES) {
  }
  else if (arg2_location.kind == LOC_CONST) {
    /* 8-bit */
    _load_value_to_b(arg2_location.value & 0xff, file_out);
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
    return _bbb_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_8bit(t, file_out, function_node, op);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _aaa_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(t, file_out, function_node, op);

  fprintf(stderr, "_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


static int _generate_asm_return_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  int frame_end, return_sp_offset, saved_fp_low_offset, saved_fp_high_offset;

  if (_get_z80_call_frame_prefix(&frame_end, &return_sp_offset, &saved_fp_low_offset, &saved_fp_high_offset) == FAILED) {
    fprintf(stderr, "_generate_asm_return_z80(): Target policy has an invalid call-frame prefix! Please submit a bug report!\n");
    return FAILED;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    fprintf(stderr, "register_allocator: target_call_frame_prefix function=%s role=callee_return target=z80 frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
            function_node->children[1]->label, frame_end, return_sp_offset, saved_fp_low_offset, saved_fp_high_offset);
#endif

  /* ret */

  /* bytes -1 and 0 of stack frame contain the return address */
  fprintf(file_out, "      ; bytes -1 and 0 of stack frame contain the return address\n");

  _load_value_to_hl(return_sp_offset, file_out);
  _add_de_to_hl(file_out);
  _load_hl_to_sp(file_out);

  _ret(file_out);

  return SUCCEEDED;
}


static int _generate_asm_return_value_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location arg1_location;
  int return_var_type, return_value_size, return_low_offset, return_high_offset, ix_offset = 0;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* return */

  return_var_type = tree_node_get_max_var_type(function_node->children[0]);
  if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16)
    return_value_size = 2;
  else
    return_value_size = 1;

  if (_get_z80_return_value_slot_offsets(return_value_size, &return_low_offset, &return_high_offset) == FAILED) {
    fprintf(stderr, "_generate_asm_return_value_z80(): Target policy cannot locate a %d-byte return value! Please submit a bug report!\n", return_value_size);
    return FAILED;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    fprintf(stderr, "register_allocator: target_return_slot_access function=%s role=callee_store size=%d target=z80 low_offset=%d high_offset=%d\n",
            function_node->children[1]->label, return_value_size, return_low_offset, return_high_offset);
    fprintf(stderr, "register_allocator: return_value_source function=%s kind=%s size=%d phy=%s arg_type=%d return_type=%d\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg1_location.kind), arg1_location.size,
            _get_z80_physical_register_name(arg1_location.phy), t->arg1_var_type, return_var_type);
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_A ||
      arg1_location.kind == LOC_PHY_HL || arg1_location.kind == LOC_PHY_BC ||
      arg1_location.kind == LOC_PHY_B || arg1_location.kind == LOC_PHY_C) {
  }
  else {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL)
      _trace_z80_specialized_address_materialization(function_node, "return_value_source_address", "arg1",
                                                     &arg1_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif
  }

  /******************************************************************************************************/
  /* copy data (arg1) -> hl/l */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST) {
    if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      _load_value_to_hl(arg1_location.value, file_out);
    }
    else {
      /* 8-bit */
      _load_value_to_l(arg1_location.value & 0xff, file_out);
    }
  }
  else if (arg1_location.kind == LOC_PHY_HL) {
  }
  else if (arg1_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  L,A\n");

    if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16) {
      if (t->arg1_var_type == VARIABLE_TYPE_INT8) {
        _sign_extend_l_to_hl(file_out);
      }
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8) {
        _load_value_to_h(0, file_out);
      }
      else {
        fprintf(stderr, "_generate_asm_return_value_z80(): Cannot return 16-bit value from A! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  L,C\n");
    if (return_var_type == VARIABLE_TYPE_INT16 ||
        return_var_type == VARIABLE_TYPE_UINT16)
      fprintf(file_out, "      LD  H,B\n");
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else if (arg1_location.kind == LOC_PHY_B ||
      arg1_location.kind == LOC_PHY_C) {
    fprintf(file_out, "      LD  L,%s\n",
        arg1_location.kind == LOC_PHY_B ? "B" : "C");
    if (return_var_type == VARIABLE_TYPE_INT16 ||
        return_var_type == VARIABLE_TYPE_UINT16) {
      if (t->arg1_var_type == VARIABLE_TYPE_INT8)
        _sign_extend_l_to_hl(file_out);
      else if (t->arg1_var_type == VARIABLE_TYPE_UINT8)
        _load_value_to_h(0, file_out);
      else {
        fprintf(stderr, "_generate_asm_return_value_z80(): Cannot return 16-bit value from %s! Please submit a bug report!\n",
            arg1_location.kind == LOC_PHY_B ? "B" : "C");
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }
    fprintf(file_out, "      ; retained arg1 in %s\n",
        arg1_location.kind == LOC_PHY_B ? "B" : "C");
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
          print_tac(t, NO, stderr);
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
    _load_l_into_ix(return_low_offset, file_out);
    _load_h_into_ix(return_high_offset, file_out);
  }
  else {
    /* 8-bit */
    _load_l_into_ix(return_low_offset, file_out);
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
  int frame_end, return_sp_offset, saved_fp_low_offset, saved_fp_high_offset;
  char return_label[32];

  /* __pureasm function calls are very simple */
  if ((t->arg1_node->flags & TREE_NODE_FLAG_PUREASM) == TREE_NODE_FLAG_PUREASM) {
    _call_to(t->arg1_node->children[1]->label, file_out);

    return SUCCEEDED;
  }

  if (_get_z80_call_frame_prefix(&frame_end, &return_sp_offset, &saved_fp_low_offset, &saved_fp_high_offset) == FAILED) {
    fprintf(stderr, "_generate_asm_function_call_z80(): Target policy has an invalid call-frame prefix! Please submit a bug report!\n");
    return FAILED;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: target_call_frame_prefix function=%s callee=%s role=caller_setup target=z80 frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
            function_name, t->arg1_node->children[1]->label, frame_end, return_sp_offset,
            saved_fp_low_offset, saved_fp_high_offset);
  }
#endif

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
      int source_var_type, source_offset, target_var_type, target_offset, argument_transport;
      int source_low_offset, source_high_offset, target_low_offset, target_high_offset;
      struct tree_node *name_node = t->arg1_node->children[j+1];
      struct local_variable *local_variable;
      struct function_argument *arg;
      struct z80_location source_location;

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

      /* find the source location in the old stack frame */
      if (_resolve_z80_location(arg->type, arg->label, (int)arg->value, arg->node, arg->var_type, function_node, &source_location, "call_arg", Z80_PHY_NONE) == FAILED)
        return FAILED;

      source_var_type = arg->var_type;
      source_offset = source_location.offset;
      target_var_type = tree_node_get_max_var_type(local_variable->node->children[0]);
      target_offset = local_variable->offset_to_fp;
      argument_transport = get_register_allocator_argument_transport(source_var_type, target_var_type);

      if (argument_transport == RA_ARGUMENT_TRANSPORT_NONE) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Target policy cannot transport argument %d from type %d to type %d! Please submit a bug report!\n", argument + 1, source_var_type, target_var_type);
        return FAILED;
      }
      if (_get_z80_argument_byte_offsets(argument_transport, &source_low_offset, &source_high_offset,
                                         &target_low_offset, &target_high_offset) == FAILED) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Target policy has invalid byte access for argument %d transport %d! Please submit a bug report!\n", argument + 1, argument_transport);
        return FAILED;
      }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
      if (g_allocator_enabled == YES) {
        char *function_name;

        function_name = "<unknown>";
        if (function_node != NULL && function_node->children[1] != NULL)
          function_name = function_node->children[1]->label;

        fprintf(stderr, "register_allocator: target_argument_transport function=%s callee=%s argument=%d source_type=%d target_type=%d source_kind=%s transport=%s target=z80\n",
                function_name, t->arg1_node->children[1]->label, argument + 1,
                source_var_type, target_var_type, _get_z80_location_kind_name(source_location.kind),
                _get_register_allocator_argument_transport_name(argument_transport));
        fprintf(stderr, "register_allocator: target_argument_byte_access function=%s callee=%s argument=%d transport=%s source_low=%d source_high=%d target_low=%d target_high=%d target=z80\n",
          function_name, t->arg1_node->children[1]->label, argument + 1,
          _get_register_allocator_argument_transport_name(argument_transport), source_low_offset,
          source_high_offset, target_low_offset, target_high_offset);
      }
#endif

      if (init_ix == NO) {
        /* load the new stack frame address -> ix */
        init_ix = YES;

        fprintf(file_out, "      ; new stack frame -> IX\n");

        _load_value_to_ix(0, file_out);
        _load_hl_to_bc(file_out);
        _add_bc_to_ix(file_out);
      }

      if (source_location.kind == LOC_CONST) {
      }
      else if (source_location.kind == LOC_GLOBAL || source_location.kind == LOC_PHY_A || source_location.kind == LOC_PHY_HL) {
      }
      else if (_load_call_argument_address_to_iy(&source_location, &source_offset, &reset_iy,
                                                 function_node, t->arg1_node->children[1]->label,
                                                 argument + 1, file_out) == FAILED)
        return FAILED;

      /******************************************************************************************************/
      /* copy data (reg) -> bc */
      /******************************************************************************************************/

      fprintf(file_out, "      ; copy argument %d\n", argument + 1);

      if (source_location.kind == LOC_GLOBAL) {
        if (_load_location_address_to_iy(&source_location, file_out) == FAILED)
          return FAILED;
        _trace_z80_specialized_address_materialization(function_node, "function_call_argument_source", "argument",
                                                        &source_location, RA_ADDRESS_TARGET_IY, 0);
        reset_iy = YES;
        source_offset = 0;
      }

      if (source_location.kind == LOC_CONST) {
      }
      else if (source_location.kind == LOC_PHY_A) {
        if (argument_transport == RA_ARGUMENT_TRANSPORT_WORD || argument_transport == RA_ARGUMENT_TRANSPORT_TRUNCATE) {
          fprintf(stderr, "_generate_asm_function_call_z80(): Cannot copy 16-bit argument from A! Please submit a bug report!\n");
          print_tac(t, NO, stderr);
          return FAILED;
        }

        if (argument_transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND) {
          _sign_extend_a_to_bc(file_out);
        }
        else {
          fprintf(file_out, "      LD  C,A\n");

          if (argument_transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND)
            _load_value_to_b(0, file_out);
        }
      }
      else if (source_location.kind == LOC_PHY_HL) {
        _load_hl_to_bc(file_out);
      }
      else {
        if (argument_transport == RA_ARGUMENT_TRANSPORT_WORD || argument_transport == RA_ARGUMENT_TRANSPORT_TRUNCATE) {
          /* 16-bit */
          _load_from_iy_to_c(source_offset + source_low_offset, file_out);
          _load_from_iy_to_b(source_offset + source_high_offset, file_out);
        }
        else {
          /* 8-bit */
          _load_from_iy_to_c(source_offset + source_low_offset, file_out);

          /* sign extend 8-bit -> 16-bit? */
          if (argument_transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND) {
            /* yes */
            _sign_extend_c_to_bc(file_out);
          }
          else if (argument_transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND) {
            /* upper byte -> 0 */
            _load_value_to_b(0, file_out);
          }
        }
      }

      /******************************************************************************************************/
      /* copy data bc/c -> (ix) */
      /******************************************************************************************************/

      if (source_location.kind == LOC_CONST) {
        if (argument_transport == RA_ARGUMENT_TRANSPORT_WORD ||
          argument_transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND ||
          argument_transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND) {
          /* 16-bit */
          _load_value_into_ix(source_location.value & 0xff, target_offset + target_low_offset, file_out);
          _load_value_into_ix((source_location.value >> 8) & 0xff, target_offset + target_high_offset, file_out);
        }
        else {
          /* 8-bit */
          _load_value_into_ix(source_location.value & 0xff, target_offset + target_low_offset, file_out);
        }
      }
      else {
        if (argument_transport == RA_ARGUMENT_TRANSPORT_WORD ||
          argument_transport == RA_ARGUMENT_TRANSPORT_SIGN_EXTEND ||
          argument_transport == RA_ARGUMENT_TRANSPORT_ZERO_EXTEND) {
          /* 16-bit */
          _load_c_into_ix(target_offset + target_low_offset, file_out);
          _load_b_into_ix(target_offset + target_high_offset, file_out);
        }
        else {
          /* 8-bit */
          _load_c_into_ix(target_offset + target_low_offset, file_out);
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
    int return_value_size, return_low_offset, return_high_offset;

    if (return_var_type == VARIABLE_TYPE_NONE || return_var_type == VARIABLE_TYPE_VOID) {
      fprintf(stderr, "_generate_asm_function_call_z80(): Function \"%s\" doesn't return a value yet we are trying to use the return value!\n", t->arg1_node->children[1]->label);
      return FAILED;
    }

    if (return_var_type == VARIABLE_TYPE_INT16 || return_var_type == VARIABLE_TYPE_UINT16)
      return_value_size = 2;
    else
      return_value_size = 1;

    if (_get_z80_return_value_slot_offsets(return_value_size, &return_low_offset, &return_high_offset) == FAILED) {
      fprintf(stderr, "_generate_asm_function_call_z80(): Target policy cannot locate a %d-byte return value! Please submit a bug report!\n", return_value_size);
      return FAILED;
    }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES) {
      char *function_name;

      function_name = "<unknown>";
      if (function_node != NULL && function_node->children[1] != NULL)
        function_name = function_node->children[1]->label;

      fprintf(stderr, "register_allocator: target_return_slot_access function=%s callee=%s role=caller_load size=%d target=z80 low_offset=%d high_offset=%d\n",
              function_name, t->arg1_node->children[1]->label, return_value_size,
              return_low_offset, return_high_offset);
    }
#endif

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
      _load_from_ix_to_l(return_low_offset, file_out);
      _load_from_ix_to_h(return_high_offset, file_out);
    }
    else {
      /* 8-bit */
      _load_from_ix_to_l(return_low_offset, file_out);

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

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: target_call_frame_prefix function=%s callee=%s role=caller_restore target=z80 frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
            function_name, t->arg1_node->children[1]->label, frame_end, return_sp_offset,
            saved_fp_low_offset, saved_fp_high_offset);
  }
#endif

  /* old stack frame address -> de */
  _load_from_ix_to_e(saved_fp_low_offset, file_out);
  _load_from_ix_to_d(saved_fp_high_offset, file_out);

  if (op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE) {
    struct z80_location result_location;

    if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
      return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES) {
      char *function_name;
      char *callee_name;

      function_name = "<unknown>";
      if (function_node != NULL && function_node->children[1] != NULL)
        function_name = function_node->children[1]->label;

      callee_name = "<unknown>";
      if (t->arg1_node != NULL && t->arg1_node->children[1] != NULL)
        callee_name = t->arg1_node->children[1]->label;

            fprintf(stderr, "register_allocator: function_call_return function=%s callee=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
              function_name, callee_name, _get_z80_location_kind_name(result_location.kind),
              result_location.size, _get_z80_physical_register_name(result_location.phy),
              t->result_var_type, t->result_var_type_promoted);
    }
#endif

    fprintf(file_out, "      ; copy return value to its destination\n");

    if (result_location.kind == LOC_PHY_A) {
      if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Cannot keep 16-bit return value in A! Please submit a bug report!\n");
        return FAILED;
      }

      fprintf(file_out, "      LD  A,L\n");
      fprintf(file_out, "      ; retained call result in A\n");
      return SUCCEEDED;
    }
    else if (result_location.kind == LOC_PHY_HL) {
      fprintf(file_out, "      ; retained call result in HL\n");
      return SUCCEEDED;
    }
    else if (result_location.kind == LOC_PHY_BC) {
      fprintf(file_out, "      LD  C,L\n");
      if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16)
        fprintf(file_out, "      LD  B,H\n");
      else
        _load_value_to_b(0, file_out);
      fprintf(file_out, "      ; retained call result in BC\n");
      return SUCCEEDED;
    }
    else if (result_location.kind == LOC_PHY_C) {
      if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
        fprintf(stderr, "_generate_asm_function_call_z80(): Cannot keep 16-bit return value in C! Please submit a bug report!\n");
        return FAILED;
      }

      fprintf(file_out, "      LD  C,L\n");
      fprintf(file_out, "      ; retained call result in C\n");
      return SUCCEEDED;
    }

    /******************************************************************************************************/
    /* target address -> iy */
    /******************************************************************************************************/

    if (_load_location_address_to_iy(&result_location, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "function_call_result_store", "result",
                                                    &result_location, RA_ADDRESS_TARGET_IY, 0);

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


static int _resolve_inline_asm_variable_location(struct tree_node *function_node, struct asm_line *al, struct z80_location *location, char *operand_name, char *access_name) {

  int var_type;
  char *function_name;

  var_type = tree_node_get_max_var_type(al->variable->children[0]);

  if (_resolve_z80_location(TAC_ARG_TYPE_LABEL, al->variable->children[1]->label, 0, al->variable, var_type, function_node, location, operand_name, Z80_PHY_NONE) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    function_name = "<global>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: inline_asm_operand function=%s access=%s variable=%s kind=%s offset=%d size=%d policy=stack_location\n",
            function_name, access_name, al->variable->children[1]->label,
            _get_z80_location_kind_name(location->kind), location->offset, location->size);
  }
#endif

  return SUCCEEDED;
}


static int _generate_asm_inline_asm_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, struct asm_line *al, int file_id) {

  struct z80_location source_location;
  int ix_offset;

  ix_offset = 0;

  if (_resolve_inline_asm_variable_location(function_node, al, &source_location, "inline_asm_read", "read") == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* source address -> ix */
  /******************************************************************************************************/

  if (_load_location_address_to_ix(&source_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  _trace_z80_specialized_address_materialization(function_node, "inline_asm_read_address", "variable",
                                                  &source_location, RA_ADDRESS_TARGET_IX, ix_offset);

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

  struct z80_location target_location;
  int ix_offset;

  ix_offset = 0;

  if (_resolve_inline_asm_variable_location(function_node, al, &target_location, "inline_asm_write", "write") == FAILED)
    return FAILED;

  /* generate asm */

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (_load_location_address_to_ix(&target_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  _trace_z80_specialized_address_materialization(function_node, "inline_asm_write_address", "variable",
                                                  &target_location, RA_ADDRESS_TARGET_IX, ix_offset);

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


static int _generate_asm_inline_asm_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int indentation) {

  struct inline_asm *ia;
  struct asm_line *al;

  ia = inline_asm_find((int)t->result_d);
  if (ia == NULL)
    return FAILED;

  al = ia->asm_line_first;
  while (al != NULL) {
    if (al->flags == 0) {
      /* no outside access -> output as it is */
      int i, length, j;

      /* skip white space */
      length = (int)strlen(al->line);
      for (i = 0; i < length && (al->line[i] == ' ' || al->line[i] == '\t'); i++)
        ;

      for (j = 0; j < indentation; j++)
        fprintf(file_out, " ");

      fprintf(file_out, "%s\n", &al->line[i]);
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

    if (element_type == VARIABLE_TYPE_STRUCT || element_type == VARIABLE_TYPE_UNION) {
      int current_element_size = 0, added_items = 0;

      /* check element types */
      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == TREE_NODE_FLAG_DATA_IS_CONST) {
        for (j = 2; j < node->added_children; j++) {
          if (node->children[j]->type == TREE_NODE_TYPE_SYMBOL)
            continue;
          if (node->children[j]->type != TREE_NODE_TYPE_VALUE_INT &&
              node->children[j]->type != TREE_NODE_TYPE_VALUE_DOUBLE &&
              node->children[j]->type != TREE_NODE_TYPE_BYTES) {
            snprintf(g_error_message, sizeof(g_error_message), "_add_const_variables(): Const variable (\"%s\") can only be initialized with an immediate number!\n", node->children[1]->label);
            return print_error_using_tree_node(g_error_message, ERROR_ERR, node);
          }
        }
      }

      for (j = 2; j < node->added_children; j++) {
        /* skip ',', '{' and '}' */
        if (node->children[j]->type == TREE_NODE_TYPE_SYMBOL)
          continue;

        element_size = get_variable_type_size(node->children[j]->struct_item->variable_type);

        if (element_size != current_element_size) {
          if (current_element_size != 0)
            fprintf(file_out, "\n");

          current_element_size = element_size;
          added_items = 0;

          if (element_size == 8)
            fprintf(file_out, "      .DB ");
          else if (element_size == 16)
            fprintf(file_out, "      .DW ");
        }
        else if (added_items > 0)
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
        else {
          /* non-constants are 0 here, but they are overwritten later */
          fprintf(file_out, "%d", 0);
        }

        added_items++;
      }
    }
    else {
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
            snprintf(g_error_message, sizeof(g_error_message), "_add_const_variables(): Const variable (\"%s\") can only be initialized with an immediate number!\n", node->children[1]->label);
            return print_error_using_tree_node(g_error_message, ERROR_ERR, node);
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
    }

    fprintf(file_out, "\n");
  }

  return SUCCEEDED;
}


static int _copy_non_const_array_constants(struct tac *t, struct tree_node *node, int items, struct tree_node *function_node, FILE *file_out) {

  char copy_function_name[MAX_NAME_LENGTH+1];
  struct z80_location target_location;
  int size, var_type;

  var_type = tree_node_get_max_var_type(node->children[0]);

  if (_resolve_z80_location(TAC_ARG_TYPE_LABEL, node->children[1]->label, 0, node, var_type, function_node, &target_location, "array_init_target", Z80_PHY_NONE) == FAILED)
    return FAILED;

  size = get_array_initialized_size_in_bytes(node);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    fprintf(stderr, "register_allocator: array_init_copy function=%s variable=%s kind=%s offset=%d size=%d bytes=%d const_items=%d policy=location_address\n",
            function_name, node->children[1]->label, _get_z80_location_kind_name(target_location.kind),
            target_location.offset, target_location.size, size, items);
  }
#endif

  _push_de(file_out);

  /* target address -> hl */
  if (_load_location_address_to_hl(&target_location, file_out) == FAILED)
    return FAILED;
  _trace_z80_specialized_address_materialization(function_node, "array_initializer_target", "target",
                                                  &target_location, RA_ADDRESS_TARGET_HL, 0);

  /* source address -> de */
  _load_label_to_de(node->children[1]->label, file_out);

  /* counter -> bc */
  _load_value_to_bc(size, file_out);

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

      if ((function_node->flags & TREE_NODE_FLAG_ORG_DEFINED) == TREE_NODE_FLAG_ORG_DEFINED) {
        fprintf(file_out, "  .ORG $%x\n", (unsigned int)function_node->value_double);
        fprintf(file_out, "  .SECTION \"%s\" FORCE\n", function_node->children[1]->label);
      }
      else if ((function_node->flags & TREE_NODE_FLAG_ORGA_DEFINED) == TREE_NODE_FLAG_ORGA_DEFINED) {
        fprintf(file_out, "  .ORGA $%x\n", (unsigned int)function_node->value_double);
        fprintf(file_out, "  .SECTION \"%s\" FORCE\n", function_node->children[1]->label);
      }
      else
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

        /* list temp register info */
        if (function_node->local_variables != NULL) {
          struct local_variables *local_variables = function_node->local_variables;
          int k;

          if (local_variables->temp_registers_count > 0) {
            fprintf(file_out, "      ; =================================================================\n");
            fprintf(file_out, "      ; temp registers\n");
            fprintf(file_out, "      ; =================================================================\n");
          }

          /* Keep allocator diagnostic comments stable; byte tests live under tests/z80/sms/allocator_*. */
          for (k = 0; k < local_variables->temp_registers_count; k++) {
            if (g_allocator_enabled == YES) {
              fprintf(file_out, "      ; register %d size %d spill %s phy %s\n", local_variables->temp_registers[k].register_index,
                      local_variables->temp_registers[k].size / 8, local_variables->temp_registers[k].spill_required == YES ? "yes" : "no",
                      _get_z80_physical_register_name(local_variables->temp_registers[k].physical_register));
              if (local_variables->temp_registers[k].spill_reason != Z80_SPILL_REASON_NONE)
                fprintf(file_out, "      ; spill: %s r%d boundary_tac %d\n", _get_z80_spill_reason_name(local_variables->temp_registers[k].spill_reason), local_variables->temp_registers[k].register_index, local_variables->temp_registers[k].spill_boundary_tac);
            }
            else {
              fprintf(file_out, "      ; register %d size %d offset %d\n", local_variables->temp_registers[k].register_index,
                      local_variables->temp_registers[k].size / 8, local_variables->temp_registers[k].offset_to_fp);
            }
          }
        }

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
        else if (op == TAC_OP_ADD || op == TAC_OP_SUB || op == TAC_OP_OR || op == TAC_OP_AND || op == TAC_OP_XOR) {
          if (_generate_asm_add_sub_or_xor_and_z80(t, file_out, function_node, op) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_COMPLEMENT) {
          if (_generate_asm_complement_z80(t, file_out, function_node) == FAILED)
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
        else if (op == TAC_OP_ARRAY_WRITE) {
          if (_generate_asm_array_write_z80(t, file_out, function_node) == FAILED)
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
                if (_copy_non_const_array_constants(t, node, size, function_node, file_out) == FAILED)
                  return FAILED;

                /* the constants in this array need to be available for a bulk copy at the end of the function, just like
                   constant variables... */
                const_variables[const_variables_count++] = node;
              }
            }
          }
        }
        else if (op == TAC_OP_REGISTER_SPILL) {
          if (z80_emit_register_spill(t, function_node, file_out) == FAILED)
            return FAILED;
          g_is_ix_de = t->result_d == 0 ? YES : NO;
        }
        else if (op == TAC_OP_ASM) {
          if (_generate_asm_inline_asm_z80(t, file_out, function_node, 6) == FAILED)
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
    else if (op == TAC_OP_ASM) {
      if (_generate_asm_inline_asm_z80(t, file_out, NULL, 2) == FAILED)
        return FAILED;
      fprintf(file_out, "\n");
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

  if ((node->children[0]->value == VARIABLE_TYPE_STRUCT || node->children[0]->value == VARIABLE_TYPE_UNION) && node->children[0]->value_double == 0) {
    /* struct/union (can be an array as well) */
    struct struct_item *si;

    si = find_struct_item(node->children[0]->children[0]->label);
    if (si == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", node->children[0]->children[0]->label);
      print_error_using_tree_node(g_error_message, ERROR_ERR, node);
      exit(1);
    }

    *bytes = si->size * elements;
  }
  else
    *bytes = element_size * elements;

  if (elements == max_elements)
    *is_fully_initialized = YES;
  else
    *is_fully_initialized = NO;
}


int generate_global_variables_z80(char *file_name, FILE *file_out) {

  int i, j, global_variables_rom = 0, global_variables_ram = 0, length, max_length = 0, elements, element_size, element_type, bytes, is_fully_initialized, accumulated_items, accumulated_sizes[1024];
  char label_tmp[MAX_NAME_LENGTH + 1];
  struct tree_node *node_1st = NULL;

  /* kill __z80_out and __z80_in as they are not normal arrays */
  _kill_z80_in_out();

  fprintf(file_out, "\n");

  for (i = 0; i < g_global_nodes->added_children; i++) {
    struct tree_node *node = g_global_nodes->children[i];
    if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
      if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
        continue;

      if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
        length = (int)strlen(node->children[1]->label);
        if (length > max_length)
          max_length = length;
        global_variables_ram++;
      }
      else
        global_variables_rom++;
    }
  }

  if (global_variables_ram > 0) {
    /* create .RAMSECTION for global variables */
    fprintf(file_out, "  .RAMSECTION \"global_variables_%s_ram\" BANK %d SLOT %d FREE\n", file_name, g_ram_bank, g_ram_slot);

    for (i = 0; i < g_global_nodes->added_children; i++) {
      struct tree_node *node = g_global_nodes->children[i];
      if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
        if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
          continue;

        if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
          length = (int)strlen(node->children[1]->label);

          fprintf(file_out, "    %s", node->children[1]->label);

          for (j = length; j < max_length + 1; j++)
            fprintf(file_out, " ");

          elements = node->value;
          if (elements == 0)
            elements = 1;
          element_type = tree_node_get_max_var_type(node->children[0]);

          if (element_type == VARIABLE_TYPE_STRUCT) {
            struct struct_item *si;

            si = find_struct_item(node->children[0]->children[0]->label);
            if (si == NULL) {
              fprintf(stderr, "generate_global_variables_z80(): Cannot find struct/union \"%s\"! Please submit a bug report!\n", node->children[0]->children[0]->label);
              return FAILED;
            }

            element_size = si->size * 8;
          }
          else {
            element_size = get_variable_type_size(element_type);

            if (element_size != 8 && element_size != 16) {
              fprintf(stderr, "generate_global_variables_z80(): Unsupported global variable \"%s\" size %d! Please submit a bug report!\n", node->children[1]->label, element_size);
              return FAILED;
            }
          }

          /* check element types */
          for (j = 2; j < node->added_children; j++) {
            if (node->children[j]->type != TREE_NODE_TYPE_VALUE_INT &&
                node->children[j]->type != TREE_NODE_TYPE_VALUE_DOUBLE &&
                node->children[j]->type != TREE_NODE_TYPE_BYTES &&
                node->children[j]->type != TREE_NODE_TYPE_SYMBOL) {
              snprintf(g_error_message, sizeof(g_error_message), "generate_global_variables_z80(): Global variable (\"%s\") can only be initialized with an immediate number!\n", node->children[1]->label);
              return print_error_using_tree_node(g_error_message, ERROR_ERR, node);
            }
          }

          if (element_type == VARIABLE_TYPE_STRUCT)
            fprintf(file_out, "DSB %d*%d", element_size / 8, elements);
          else if (node->value == 1) {
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
  }

  if (global_variables_ram > 0 || global_variables_rom > 0) {
    fprintf(file_out, "\n");
    fprintf(file_out, "  .BANK %d SLOT %d\n", g_bank, g_slot);
    fprintf(file_out, "  .ORG $0000\n");
    fprintf(file_out, "\n");

    /* create .SECTION for global variables */
    fprintf(file_out, "  .SECTION \"global_variables_%s_rom\" FREE\n", file_name);

    for (i = 0; i < g_global_nodes->added_children; i++) {
      struct tree_node *node = g_global_nodes->children[i];
      if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
        if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
          continue;

        if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0)
          snprintf(label_tmp, sizeof(label_tmp), "global_variable_rom_%s", node->children[1]->label);
        else
          snprintf(label_tmp, sizeof(label_tmp), "%s", node->children[1]->label);
        _add_label(label_tmp, file_out, NO);

        elements = node->added_children - 2;
        if (elements > 0) {
          element_type = tree_node_get_max_var_type(node->children[0]);

          if (element_type == VARIABLE_TYPE_STRUCT || element_type == VARIABLE_TYPE_UNION) {
            int current_element_size = 0, added_items = 0;

            /* struct / union */
            for (j = 2; j < node->added_children; j++) {
              /* skip ',', '{' and '}' */
              if (node->children[j]->type == TREE_NODE_TYPE_SYMBOL)
                continue;

              element_size = get_variable_type_size(node->children[j]->struct_item->variable_type);

              if (element_size != current_element_size) {
                if (current_element_size != 0)
                  fprintf(file_out, "\n");

                current_element_size = element_size;
                added_items = 0;

                if (element_size == 8)
                  fprintf(file_out, "      .DB ");
                else if (element_size == 16)
                  fprintf(file_out, "      .DW ");
              }
              else if (added_items > 0)
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

              added_items++;
            }

            fprintf(file_out, "\n");
          }
          else {
            element_size = get_variable_type_size(element_type);

            /* normal array */
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
    }

    fprintf(file_out, "  .ENDS\n\n");
  }

  if (global_variables_ram > 0) {
    /* create .SECTION that copies data from .SECTION to .RAMSECTION */
    fprintf(file_out, "  .BANK %d SLOT %d\n", g_bank, g_slot);
    fprintf(file_out, "  .ORG $0000\n");
    fprintf(file_out, "\n");

    fprintf(file_out, "  .SECTION \"global_variables_%s_init\" FREE\n", file_name);

    snprintf(label_tmp, sizeof(label_tmp), "global_variables_%s_init", file_name);
    _add_label(label_tmp, file_out, NO);

    /* 1. copy the data of all fully initialized non-const global variables in one call */
    accumulated_items = 0;
    for (i = 0; i < g_global_nodes->added_children; i++) {
      struct tree_node *node = g_global_nodes->children[i];
      if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
        if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
          continue;

        if ((node->flags & TREE_NODE_FLAG_DATA_IS_CONST) == 0) {
          if (node_1st == NULL)
            node_1st = node;

          _get_variable_initialized_size(node, &bytes, &is_fully_initialized);
          if (is_fully_initialized == NO)
            break;

          accumulated_sizes[accumulated_items++] = bytes;
          if (accumulated_items >= 1024)
            return print_error_using_tree_node("generate_global_variables_z80(): accumulated_sizes array is full! Please resize and recompile!\n", ERROR_ERR, node);
        }
      }
    }

    if (accumulated_items > 0) {
      char copy_function_name[MAX_NAME_LENGTH+1];

      fprintf(file_out, "      ; copy all fully initialized global variables in a single call\n");

      /* target address -> hl */
      _load_label_to_hl(node_1st->children[1]->label, file_out);

      /* source address -> de */
      snprintf(label_tmp, sizeof(label_tmp), "global_variable_rom_%s", node_1st->children[1]->label);
      _load_label_to_de(label_tmp, file_out);

      /* counter -> bc */
      _load_value_to_bc_array(accumulated_sizes, accumulated_items, file_out);

      /* call */
      snprintf(copy_function_name, sizeof(copy_function_name), "copy_bytes_bank_%.3d", g_bank);
      _call_to(copy_function_name, file_out);
    }

    /* 2. copy the data of all partially initialized non_const global variables in multiple calls */
    for (; i < g_global_nodes->added_children; i++) {
      struct tree_node *node = g_global_nodes->children[i];
      if (node != NULL && node->type == TREE_NODE_TYPE_CREATE_VARIABLE) {
        if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
          continue;

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
  }

  return SUCCEEDED;
}
