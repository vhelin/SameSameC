
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
extern int g_allocator_enabled, g_allocator_force_all_spill;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

#define DEBUG_PASS_6_REGISTER_ALLOCATOR 1

#define Z80_GLOBAL_OFFSET 999999

static int g_return_id = 1, g_ix_frame_offset = Z80_GLOBAL_OFFSET;

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

    RA_TRACE((stderr, "register_allocator: target_specialized_address_materialization function=%s emitter=%s operand=%s kind=%s offset=%d mode=%s folded_offset=%d address_target=%s target=z80\n",
      function_name, emitter, operand, _get_z80_location_kind_name(location->kind), location->offset,
      _get_z80_address_mode_name(address_mode), folded_offset, _get_z80_address_target_name(address_target)));
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

  int delta = offset - g_ix_frame_offset;
  int steps = delta < 0 ? -delta : delta;
  int reuse = g_ix_frame_offset != Z80_GLOBAL_OFFSET &&
    (offset == 0 || (g_allocator_enabled == YES && g_allocator_force_all_spill == NO));
  int adjust = reuse == YES && g_allocator_enabled == YES && g_allocator_force_all_spill == NO &&
    steps * 10 < 29 && steps * 2 <= 6;
  int i;

  if (reuse == YES && delta == 0) {
  }
  else if (adjust == YES) {
    for (i = 0; i < steps; i++)
      fprintf(file_out, "      %s IX\n", delta < 0 ? "DEC" : "INC");
  }
  else {
    fprintf(file_out, "      LD  IX,%d\n", offset);
    fprintf(file_out, "      ADD IX,DE\n");
  }
  if (g_allocator_enabled == YES)
    RA_TRACE((stderr, "register_allocator: ix_frame_address previous=%d requested=%d action=%s setup_cycles=%d setup_bytes=%d objective=cycles_no_byte_growth frequency=local\n",
      g_ix_frame_offset, offset, reuse == YES && delta == 0 ? "reuse" : adjust == YES ? "adjust" : "materialize",
      reuse == YES && delta == 0 ? 0 : adjust == YES ? steps * 10 : 29,
      reuse == YES && delta == 0 ? 0 : adjust == YES ? steps * 2 : 6));
  g_ix_frame_offset = offset == 0 || (g_allocator_enabled == YES && g_allocator_force_all_spill == NO) ?
    offset : Z80_GLOBAL_OFFSET;
}


static void _load_value_to_ix(int value, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%d\n", value);

  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
}


static void _load_value_to_iy(int value, FILE *file_out) {

  fprintf(file_out, "      LD  IY,%d\n", value);
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
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
}


static void _load_label_to_hl(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  HL,%s\n", label);
}


static void _load_label_to_ix(char *label, FILE *file_out) {

  fprintf(file_out, "      LD  IX,%s\n", label);

  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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

  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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

  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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


static void _bitwise_hl(FILE *file_out, int op, int result_in_bc, struct z80_location *constant) {

  char *mnemonic = op == TAC_OP_OR ? "OR " : op == TAC_OP_XOR ? "XOR" : "AND";
  int byte_offset;

  for (byte_offset = 1; byte_offset >= 0; byte_offset--) {
    if (constant == NULL)
      fprintf(file_out, "      LD  A,%s\n", byte_offset == 1 ? "B" : "C");
    else
      fprintf(file_out, "      LD  A,%d\n", (constant->value >> (byte_offset * 8)) & 255);
    fprintf(file_out, "      %s A,%s\n", mnemonic, byte_offset == 1 ? "H" : "L");
    fprintf(file_out, "      LD  %s,A\n", result_in_bc == YES ?
        (byte_offset == 1 ? "B" : "C") : (byte_offset == 1 ? "H" : "L"));
  }
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


static void _load_hl_to_bc(FILE *file_out) {

  fprintf(file_out, "      LD  B,H\n");
  fprintf(file_out, "      LD  C,L\n");
}


static void _load_a_to_b(FILE *file_out) {

  fprintf(file_out, "      LD  B,A\n");
}


static void _load_hl_to_sp(FILE *file_out) {

  fprintf(file_out, "      LD  SP,HL\n");
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
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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

  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
}


static void _load_value_to_e(int value, FILE *file_out) {

  fprintf(file_out, "      LD  E,%d\n", value);

  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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

  if (is_local == NO)
    g_ix_frame_offset = Z80_GLOBAL_OFFSET;
}


static void _sign_extend_c_to_bc_preserving_a(FILE *file_out) {

  fprintf(file_out, "      ; sign extend 8-bit C -> 16-bit BC preserving A\n");
  fprintf(file_out, "      LD  B,0\n");
  fprintf(file_out, "      BIT 7,C\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      DEC B\n");

  _add_label("+", file_out, YES);
}


static void _shift_hl_by_bc(const struct z80_location *location, int op, FILE *file_out) {

  int count, bytes, loop, byte_shift = NO;

  fprintf(file_out, "      ; shift %s HL by BC\n", op == TAC_OP_SHIFT_LEFT ? "left" : "right");

  if (location->kind == LOC_CONST) {
    count = location->value & 0xffff;
    if (count >= 16) {
      _load_value_to_hl(0, file_out);
      return;
    }
    if (count >= 8) {
      fprintf(file_out, "      LD  %c,%c\n", op == TAC_OP_SHIFT_LEFT ? 'H' : 'L', op == TAC_OP_SHIFT_LEFT ? 'L' : 'H');
      fprintf(file_out, "      LD  %c,0\n", op == TAC_OP_SHIFT_LEFT ? 'L' : 'H');
      count -= 8;
      byte_shift = YES;
    }

    /* Unroll only while the whole kernel fits the original 8/11-byte loop. */
    bytes = count * (op == TAC_OP_SHIFT_LEFT ? 1 : byte_shift == YES ? 2 : 4) + (byte_shift == YES ? 3 : 0);
    loop = bytes > (op == TAC_OP_SHIFT_LEFT ? 8 : 11);
    if (loop) {
      _load_value_to_b(count, file_out);
      _add_label("-", file_out, YES);
      count = 1;
    }
    for (; count > 0; count--) {
      if (op == TAC_OP_SHIFT_LEFT)
        fprintf(file_out, "      ADD HL,HL\n");
      else if (byte_shift == YES)
        fprintf(file_out, "      SRL L\n");
      else {
        fprintf(file_out, "      SRL H\n");
        fprintf(file_out, "      RR  L\n");
      }
    }
    if (loop)
      fprintf(file_out, "      DJNZ -\n");
    return;
  }

  _add_label("-", file_out, YES);

  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      OR  A,C\n");
  fprintf(file_out, "      JR  Z,+\n");
  if (op == TAC_OP_SHIFT_LEFT)
    fprintf(file_out, "      ADD HL,HL\n");
  else {
    fprintf(file_out, "      SRL H\n");
    fprintf(file_out, "      RR  L\n");
  }
  fprintf(file_out, "      DEC BC\n");
  fprintf(file_out, "      JR  -\n");

  _add_label("+", file_out, YES);
}


static void _shift_b_by_a(struct z80_location *count_location, int op, int word_count, FILE *file_out) {

  int count, i;

  if (count_location->kind == LOC_CONST) {
    count = count_location->value & (word_count == YES ? 65535 : 255);
    fprintf(file_out, "      ; constant byte shift %s by %d\n", op == TAC_OP_SHIFT_LEFT ? "left" : "right", count);
    if (count >= 8) {
      _load_value_to_b(0, file_out);
      return;
    }
    if (count >= (op == TAC_OP_SHIFT_LEFT ? 5 : 4)) {
      fprintf(file_out, "      LD  A,B\n");
      for (i = 0; i < 8 - count; i++)
        fprintf(file_out, "      %s\n", op == TAC_OP_SHIFT_LEFT || count == 4 ? "RRCA" : "RLCA");
      fprintf(file_out, "      AND %d\n", op == TAC_OP_SHIFT_LEFT ? (255 << count) & 255 : 255 >> count);
      fprintf(file_out, "      LD  B,A\n");
    }
    else if (op == TAC_OP_SHIFT_LEFT && count >= 3) {
      fprintf(file_out, "      LD  A,B\n");
      for (i = 0; i < count; i++)
        fprintf(file_out, "      ADD A,A\n");
      fprintf(file_out, "      LD  B,A\n");
    }
    else {
      for (i = 0; i < count; i++)
        fprintf(file_out, "      %s B\n", op == TAC_OP_SHIFT_LEFT ? "SLA" : "SRL");
    }
    return;
  }

  fprintf(file_out, "      ; shift %s B by A\n", op == TAC_OP_SHIFT_LEFT ? "left" : "right");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      OR  A,A\n");
  fprintf(file_out, "      JR  Z,+\n");
  fprintf(file_out, "      %s B\n", op == TAC_OP_SHIFT_LEFT ? "SLA" : "SRL");
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

  fprintf(file_out, "      ; multiply BC * DE -> HL\n");

  /* Shift the multiplier through A:C, leaving B available for DJNZ. */
  fprintf(file_out, "      LD  A,B\n");
  fprintf(file_out, "      LD  B,16 ; number of bits to process\n");
  fprintf(file_out, "      LD  HL,0\n");

  _add_label("-", file_out, YES);

  fprintf(file_out, "      ADD HL,HL\n");
  fprintf(file_out, "      SLA C\n");
  fprintf(file_out, "      RLA\n");
  fprintf(file_out, "      JR  NC,+\n");
  fprintf(file_out, "      ADD HL,DE\n");

  _add_label("+", file_out, YES);

  fprintf(file_out, "      DJNZ -\n");
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

  int offset, source_kind, bits;
  struct temp_register *temp_register;
  char *function_name;

  source_kind = RA_LOCATION_SOURCE_CONSTANT;
  location->kind = RA_LOCATION_NONE;
  location->offset = 0;
  location->phy = Z80_PHY_NONE;
  bits = get_variable_type_size(var_type);
  location->size = bits > 0 ? bits / 8 : 0;
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

    RA_TRACE((stderr, "register_allocator: resolve function=%s operand=%s kind=%s offset=%d size=%d phy=%s\n",
            function_name, operand_name, _get_z80_location_kind_name(location->kind),
            location->offset, location->size, _get_z80_physical_register_name(location->phy)));
        RA_TRACE((stderr, "register_allocator: target_location_materialization function=%s operand=%s source=%s kind=%s offset=%d size=%d phy=%s target=z80\n",
          function_name, operand_name, _get_register_allocator_location_source_name(source_kind),
          _get_z80_location_kind_name(location->kind), location->offset, location->size,
          _get_z80_physical_register_name(location->phy)));
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
  offset = location->offset;
  if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS || address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    int displacement = offset - g_ix_frame_offset;
    int adjustment = displacement < -128 ? displacement + 128 : displacement > 126 ? displacement - 126 : 0;
    int steps = adjustment < 0 ? -adjustment : adjustment;
    if (g_allocator_enabled == YES && g_allocator_force_all_spill == NO &&
        g_ix_frame_offset != Z80_GLOBAL_OFFSET && steps * 10 < 29 && steps * 2 <= 6) {
      /* Reserve the high-byte displacement even when the source is a byte. */
      *ix_offset = displacement - adjustment;
      offset = g_ix_frame_offset + adjustment;
      address_mode = RA_ADDRESS_MODE_FRAME_DISPLACEMENT;
    }
    else if (address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
      *ix_offset = offset;
      offset = 0;
    }
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    RA_TRACE((stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=%d target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_IX), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode), *ix_offset));
#endif

  if (address_mode == RA_ADDRESS_MODE_NONE) {
  }
  else if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL) {
    _load_label_to_ix(location->label, file_out);
  }
  else if (address_mode == RA_ADDRESS_MODE_FRAME_ADDRESS || address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    fprintf(file_out, "      ; offset %d\n", location->offset);
    _load_de_with_offset_to_ix(offset, file_out);
  }
  else {
    fprintf(stderr, "_load_location_address_to_ix(): Location kind %d has no IX address materialization! Please submit a bug report!\n", location->kind);
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
    RA_TRACE((stderr, "register_allocator: target_address_materialization target=IY_OLD_FRAME kind=%s offset=%d mode=%s folded_offset=%d target_policy=z80\n",
            _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode),
            address_mode == RA_ADDRESS_MODE_FRAME_DISPLACEMENT ? location->offset : 0));
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

    RA_TRACE((stderr, "register_allocator: target_call_old_frame_address function=%s callee=%s argument=%d kind=%s offset=%d mode=%s folded_offset=%d cache_before=%s cache_action=%s cache_after=%s address_target=IY_OLD_FRAME target=z80\n",
            function_name, callee_name, argument, _get_z80_location_kind_name(location->kind),
            location->offset, _get_z80_address_mode_name(address_mode), *source_offset,
            cache_was_reset == YES ? "reset" : "ready", cache_action,
            *reset_iy == YES ? "reset" : "ready"));
  }
#endif

  return SUCCEEDED;
}


static int _load_location_address_to_iy(struct z80_location *location, FILE *file_out) {

  int address_mode;

  address_mode = get_register_allocator_address_materialization_mode(location->kind, RA_ADDRESS_TARGET_IY, location->offset);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES)
    RA_TRACE((stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=0 target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_IY), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode)));
#endif

  if (address_mode == RA_ADDRESS_MODE_NONE) {
  }
  else if (address_mode == RA_ADDRESS_MODE_GLOBAL_LABEL) {
    fprintf(file_out, "      LD  IY,%s\n", location->label);
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
    RA_TRACE((stderr, "register_allocator: target_address_materialization target=%s kind=%s offset=%d mode=%s folded_offset=0 target_policy=z80\n",
            _get_z80_address_target_name(RA_ADDRESS_TARGET_HL), _get_z80_location_kind_name(location->kind), location->offset,
            _get_z80_address_mode_name(address_mode)));
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
  else if (location->kind == LOC_PHY_A || location->kind == LOC_PHY_B || location->kind == LOC_PHY_C) {
    fprintf(file_out, "      LD  L,%s\n", _get_z80_physical_register_name(location->phy));
    _load_value_to_h(0, file_out);
  }
  else if (location->kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  L,C\n");
    fprintf(file_out, "      LD  H,B\n");
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
  else if (location->kind == LOC_PHY_A || location->kind == LOC_PHY_HL ||
           location->kind == LOC_PHY_B || location->kind == LOC_PHY_C) {
    if (location->kind == LOC_PHY_HL)
      fprintf(file_out, "      LD  C,L\n");
    else if (location->kind != LOC_PHY_C)
      fprintf(file_out, "      LD  C,%s\n", _get_z80_physical_register_name(location->phy));

    if (var_type == VARIABLE_TYPE_INT8)
      _sign_extend_c_to_bc_preserving_a(file_out);
    else if (var_type == VARIABLE_TYPE_UINT8)
      _load_value_to_b(0, file_out);
    else if (location->kind == LOC_PHY_HL && (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16))
      fprintf(file_out, "      LD  B,H\n");
    else {
      if (location->kind == LOC_PHY_HL)
        fprintf(stderr, "_materialize_array_index_to_bc(): Unknown retained HL index type %d! Please submit a bug report!\n", var_type);
      else
        fprintf(stderr, "_materialize_array_index_to_bc(): Cannot read a 16-bit index from %s! Please submit a bug report!\n",
                _get_z80_physical_register_name(location->phy));
      return FAILED;
    }
    fprintf(file_out, "      ; retained arg2 in %s\n", _get_z80_physical_register_name(location->phy));
  }
  else if (location->kind == LOC_PHY_BC) {
    fprintf(file_out, "      ; retained arg2 in BC\n");
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


static int _multiply_constant_to_hl(struct tac *t, struct z80_location *arg1, struct z80_location *arg2,
                                    int byte_operands, int *selected, FILE *file_out) {

  struct z80_location *source;
  int multiplier, var_type, mask, bit = 1, bytes = 2, ix_offset = 0;
  int negative, magnitude, shifts, subtract, best_bytes = 16;
  int best_magnitude = 0, best_bit = 0, best_negative = NO, best_subtract = NO;

  *selected = NO;
  if (arg2->kind == LOC_CONST) {
    multiplier = arg2->value;
    source = arg1;
    var_type = t->arg1_var_type;
  }
  else if (arg1->kind == LOC_CONST) {
    multiplier = arg1->value;
    source = arg2;
    var_type = t->arg2_var_type;
  }
  else
    return SUCCEEDED;

  multiplier &= byte_operands == YES ? 255 : 65535;
  if (t->result_var_type == VARIABLE_TYPE_INT8 || t->result_var_type == VARIABLE_TYPE_UINT8) {
    multiplier &= 255;
    for (negative = NO; negative <= YES && multiplier != 0; negative++) {
      magnitude = negative == YES ? 256 - multiplier : multiplier;
      bytes = 1 + 2 * negative;
      bit = 1;
      shifts = 0;
      for (mask = magnitude; mask > 1; mask >>= 1) {
        bytes += 1 + (mask & 1);
        bit <<= 1;
        shifts++;
      }
      if ((magnitude & (magnitude - 1)) != 0)
        bytes++;
      subtract = NO;
      if (magnitude > 3 && (magnitude & (magnitude + 1)) == 0 && shifts + 4 + 2 * negative < bytes) {
        bytes = shifts + 4 + 2 * negative;
        bit <<= 1;
        subtract = YES;
      }
      /* Include the saved byte, final transfer and two-byte NEG in the 15-byte budget. */
      if (bytes < best_bytes) {
        best_bytes = bytes;
        best_magnitude = magnitude;
        best_bit = bit;
        best_negative = negative;
        best_subtract = subtract;
      }
    }
    if (multiplier != 0 && best_magnitude == 0)
      return SUCCEEDED;
  }
  else {
    for (mask = multiplier; mask > 1; mask >>= 1) {
      bytes += 1 + (mask & 1);
      bit <<= 1;
    }
    /* Byte operands need zero extension; their original multiply has a smaller setup budget. */
    if (multiplier != 0 && bytes > (byte_operands == YES ? 13 : 18))
      return SUCCEEDED;
  }

  *selected = YES;
  fprintf(file_out, "      ; constant %s multiply by %d without DE setup\n",
          t->result_var_type == VARIABLE_TYPE_INT8 || t->result_var_type == VARIABLE_TYPE_UINT8 ? "byte" : "word", multiplier);
  if (multiplier == 0) {
    _load_value_to_hl(0, file_out);
    return SUCCEEDED;
  }
  if (_load_location_address_to_ix(source, &ix_offset, file_out) == FAILED)
    return FAILED;

  if (t->result_var_type == VARIABLE_TYPE_INT8 || t->result_var_type == VARIABLE_TYPE_UINT8) {
    if (source->kind == LOC_GLOBAL || source->kind == LOC_STACK_LOCAL || source->kind == LOC_STACK_SPILL)
      _load_from_ix_to_a(ix_offset, file_out);
    else if (_materialize_to_a(source, 0, file_out) == FAILED)
      return FAILED;
    if ((best_magnitude & (best_magnitude - 1)) != 0)
      fprintf(file_out, "      LD  H,A\n");
    for (bit = best_bit >> 1; bit > 0; bit >>= 1) {
      fprintf(file_out, "      ADD A,A\n");
      if (best_subtract == NO && (best_magnitude & bit) != 0)
        fprintf(file_out, "      ADD A,H\n");
    }
    if (best_subtract == YES)
      fprintf(file_out, "      SUB H\n");
    if (best_negative == YES)
      fprintf(file_out, "      NEG\n");
    fprintf(file_out, "      LD  L,A\n");
  }
  else {
    if (source->kind == LOC_CONST)
      _load_value_to_bc(source->value & (byte_operands == YES ? 255 : 65535), file_out);
    else if (_materialize_array_index_to_bc(source, byte_operands == YES ? VARIABLE_TYPE_UINT8 : var_type,
                                            ix_offset, NO, file_out) == FAILED)
      return FAILED;
    if (byte_operands == YES && source->kind == LOC_PHY_BC)
      _load_value_to_b(0, file_out);
    fprintf(file_out, "      LD  H,B\n");
    fprintf(file_out, "      LD  L,C\n");
    for (bit >>= 1; bit > 0; bit >>= 1) {
      fprintf(file_out, "      ADD HL,HL\n");
      if ((multiplier & bit) != 0)
        fprintf(file_out, "      ADD HL,BC\n");
    }
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


static int _transfer_array_base_spill(struct tac *t, struct tree_node *function_node, struct z80_location *base_location, int is_reload, FILE *file_out);


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
    RA_TRACE((stderr, "register_allocator: assignment function=%s arg1_kind=%s arg1_phy=%s arg1_size=%d result_kind=%s result_phy=%s result_size=%d\n",
            function_node->children[1]->label,
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy), arg1_location.size,
            _get_z80_location_kind_name(result_location.kind), _get_z80_physical_register_name(result_location.phy), result_location.size));
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

  if (g_allocator_enabled == YES && g_allocator_force_all_spill == NO &&
      result_location.kind == LOC_PHY_HL && t->reload_spill_to_physical_operand == -1 &&
      (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) &&
      (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) &&
      get_register_allocator_address_materialization_mode(arg1_location.kind, RA_ADDRESS_TARGET_IX,
        arg1_location.offset) == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "assignment_arg1_address", "arg1",
        &arg1_location, RA_ADDRESS_TARGET_IX, ix_offset);
    _load_from_ix_to_l(ix_offset, file_out);
    _load_from_ix_to_h(ix_offset + 1, file_out);
    return SUCCEEDED;
  }

  if (_load_location_address_to_iy(&arg1_location, file_out) == FAILED)
    return FAILED;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL)
    _trace_z80_specialized_address_materialization(function_node, "assignment_arg1_address", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_IY, 0);
#endif
  if (_transfer_array_base_spill(t, function_node, &arg1_location, YES, file_out) == FAILED)
    return FAILED;

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
        if (result_location.kind == LOC_PHY_HL &&
            (arg1_location.kind == LOC_PHY_HL || arg1_location.kind == LOC_GLOBAL ||
             arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL)) {
          if (_materialize_to_hl(&arg1_location, file_out) == FAILED ||
              _store_from_hl(&result_location, ix_offset, file_out) == FAILED)
            return FAILED;
        }
        else if (result_location.kind == LOC_PHY_BC &&
            (arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL)) {
          _load_from_iy_to_c(0, file_out);
          _load_from_iy_to_b(1, file_out);
          fprintf(file_out, "      ; retained result in BC\n");
        }
        else {
          if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED ||
              _store_from_a(&result_location, 0, ix_offset, file_out) == FAILED ||
              _materialize_to_a(&arg1_location, 1, file_out) == FAILED ||
              _store_from_a(&result_location, 1, ix_offset, file_out) == FAILED)
            return FAILED;
        }
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
    RA_TRACE((stderr, "register_allocator: assignment_physical_result function=%s source_kind=%s source_size=%d result_phy=B result_size=%d via=A store=B status=complete\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, result_location.size));
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
  int word_sub = op == TAC_OP_SUB && t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16;

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

  /* Capture borrow before IX address materialization can change carry. */
  if (word_sub == YES)
    fprintf(file_out, "      LD  C,A\n      SBC A,A\n");

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
    if (word_sub == YES && result_location.kind != LOC_PHY_BC)
      _load_c_into_ix(ix_offset, file_out);
    else if (word_sub == NO && _store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
      return FAILED;

    /* borrow or sign extend 8-bit -> 16-bit? */
    if (word_sub == YES) {
      if (_store_from_a(&result_location, 1, ix_offset, file_out) == FAILED)
        return FAILED;
      if (result_location.kind == LOC_PHY_BC)
        fprintf(file_out, "      ; retained result in BC\n");
    }
    else if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
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


static int _is_z80_bounded_counter(struct tac *t, struct tree_node *function_node, int maximum) {

  struct temp_register *counter = NULL;
  struct tac *initial, *header = NULL, *bound = NULL, *update = NULL, *backedge, *exit;
  int index, i, j, end, last;

  if (g_allocator_enabled == NO || g_allocator_force_all_spill == YES ||
      (function_node->flags & TREE_NODE_FLAG_PUREASM) != 0 ||
      strcmp(function_node->children[1]->label, "mainmain") == 0 ||
      (t->op != TAC_OP_ASSIGNMENT && t->op != TAC_OP_ADD && t->op != TAC_OP_JUMP_GTE))
    return NO;
  if ((t->op == TAC_OP_ASSIGNMENT ? t->result_type : t->arg1_type) == TAC_ARG_TYPE_TEMP) {
    index = (int)(t->op == TAC_OP_ASSIGNMENT ? t->result_d : t->arg1_d);
    counter = _find_z80_temp_register(function_node, index);
    if (counter == NULL || counter->write_count != 2 || counter->live_start < 0 ||
        counter->live_end >= g_tacs_count || counter->live_end <= counter->live_start)
      return NO;
    initial = &g_tacs[counter->live_start];
    last = counter->live_end;
  }
  else {
    if (maximum != 65535 || t->op != TAC_OP_ASSIGNMENT || t->result_type != TAC_ARG_TYPE_LABEL ||
        t->result_node == NULL || t->result_node->type != TREE_NODE_TYPE_CREATE_VARIABLE ||
        (t->result_node->flags & (TREE_NODE_FLAG_GLOBAL | TREE_NODE_FLAG_STATIC)) != 0 ||
        t->result_node->value != 0 || t->result_node->children[0]->value_double != 0.0)
      return NO;
    initial = t;
    index = -1;
    last = g_tacs_count - 1;
  }
  if (initial->op != TAC_OP_ASSIGNMENT || initial->function_node != function_node ||
      initial->arg1_type != TAC_ARG_TYPE_CONSTANT || initial->arg1_d < 0.0 ||
      initial->arg1_d > maximum || initial->arg1_d != (int)initial->arg1_d ||
      (counter != NULL && (initial->result_type != TAC_ARG_TYPE_TEMP || (int)initial->result_d != index)) ||
      initial->result_var_type != VARIABLE_TYPE_UINT16 ||
      initial->result_var_type_promoted != VARIABLE_TYPE_UINT16)
    return NO;
  for (i = (int)(initial - g_tacs) + 1; i <= last; i++) {
    struct tac *current = &g_tacs[i];
    if (current->op == TAC_OP_DEAD)
      continue;
    if (current->function_node != function_node || current->is_function == YES)
      return NO;
    if (header == NULL) {
      if (current->op != TAC_OP_LABEL)
        return NO;
      header = current;
    }
    else if (bound == NULL) {
      if (current->op != TAC_OP_JUMP_GTE || current->arg1_type != initial->result_type ||
          (counter != NULL ? (int)current->arg1_d != index : current->arg1_node != initial->result_node) ||
          (counter == NULL && current->arg2_node == initial->result_node) ||
          current->arg1_var_type != VARIABLE_TYPE_UINT16 ||
          current->arg1_var_type_promoted != VARIABLE_TYPE_UINT16 ||
          current->arg2_var_type_promoted != VARIABLE_TYPE_UINT16 ||
          (current->arg2_type == TAC_ARG_TYPE_CONSTANT ?
           (current->arg2_d < 0.0 || current->arg2_d > maximum || current->arg2_d != (int)current->arg2_d) :
           (maximum != 65535 || current->arg2_type != TAC_ARG_TYPE_LABEL ||
            current->arg2_var_type != VARIABLE_TYPE_UINT16)))
        return NO;
      bound = current;
    }
    else if (counter == NULL && current->op == TAC_OP_JUMP &&
             strcmp(current->result_s, header->result_s) == 0)
      break;
    else if (current->op != TAC_OP_ARRAY_WRITE && current->result_type == initial->result_type &&
             (counter != NULL ? (int)current->result_d == index : current->result_node == initial->result_node)) {
      if (update != NULL || current->op != TAC_OP_ADD ||
          current->arg1_type != initial->result_type ||
          (counter != NULL ? (int)current->arg1_d != index : current->arg1_node != initial->result_node) ||
          current->arg1_var_type != VARIABLE_TYPE_UINT16 ||
          current->arg1_var_type_promoted != VARIABLE_TYPE_UINT16 ||
          current->arg2_type != TAC_ARG_TYPE_CONSTANT || current->arg2_d != 1.0 ||
          current->arg2_var_type_promoted != VARIABLE_TYPE_UINT16 ||
          current->result_var_type != VARIABLE_TYPE_UINT16 ||
          current->result_var_type_promoted != VARIABLE_TYPE_UINT16)
        return NO;
      update = current;
    }
  }
  if (update == NULL || (t != initial && t != bound && t != update))
    return NO;
  for (i = (int)(update - g_tacs) + 1; i < g_tacs_count && g_tacs[i].op == TAC_OP_DEAD; i++)
    ;
  if (i >= g_tacs_count || g_tacs[i].op != TAC_OP_JUMP ||
      g_tacs[i].function_node != function_node || strcmp(g_tacs[i].result_s, header->result_s) != 0)
    return NO;
  backedge = &g_tacs[i];
  for (end = i + 1; end < g_tacs_count && g_tacs[end].op == TAC_OP_DEAD; end++)
    ;
  if (end >= g_tacs_count || g_tacs[end].op != TAC_OP_LABEL ||
      g_tacs[end].is_function == YES || g_tacs[end].function_node != function_node ||
      strcmp(g_tacs[end].result_s, bound->result_s) != 0)
    return NO;
  exit = &g_tacs[end];
  for (i = 0; i < g_tacs_count; i++) {
    struct tac *current = &g_tacs[i];
    if (current->function_node != function_node || current->op == TAC_OP_DEAD)
      continue;
    if (current->op == TAC_OP_ASM ||
        ((current->op == TAC_OP_GET_ADDRESS || current->op == TAC_OP_GET_ADDRESS_ARRAY) &&
         current->arg1_type == initial->result_type &&
         (counter != NULL ? (int)current->arg1_d == index : current->arg1_node == initial->result_node)) ||
        (current > bound && current < exit &&
         (current->op == TAC_OP_FUNCTION_CALL || current->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE)))
      return NO;
    if (current->op < TAC_OP_JUMP || current->op > TAC_OP_JUMP_GTE)
      continue;
    for (j = (int)(header - g_tacs); j < end; j++) {
      if (g_tacs[j].op != TAC_OP_LABEL || strcmp(current->result_s, g_tacs[j].result_s) != 0)
        continue;
      if (current <= bound || current >= exit || (&g_tacs[j] == header && current != backedge))
        return NO;
    }
  }

  return YES;
}


static int _aaa_generate_asm_add_sub_or_xor_and_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  struct z80_location *constant_operand = NULL;
  int ix_offset = 0;
  int arg1_in_hl = NO, arg2_in_hl = NO, result_in_hl = NO, constant_step = 0;
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

  if (is_z80_register_allocator_byte_transparent_tac(t, Z80_PHY_BC) == YES) {
    if (arg2_location.kind != LOC_PHY_HL) {
      if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
        return FAILED;
      _load_from_ix_to_l(ix_offset, file_out);
    }
    if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      ; in-place word addition preserves BC\n      LD  A,L\n");
    _add_from_ix_to_a(ix_offset, file_out);
    _load_a_into_ix(ix_offset, file_out);
    fprintf(file_out, "      JR  NC,+\n      INC (IX%+d)\n+:\n", ix_offset + 1);
    return SUCCEEDED;
  }

  if ((op == TAC_OP_ADD || op == TAC_OP_SUB) && arg2_location.kind == LOC_CONST && arg2_location.value == 1 &&
      arg1_location.kind == LOC_PHY_BC && result_location.kind == LOC_PHY_BC) {
    int byte_counter = op == TAC_OP_ADD && _is_z80_bounded_counter(t, function_node, 255) == YES;
    if (byte_counter)
      fprintf(file_out, "      ; bounded word counter increment; high byte preserved\n");
    fprintf(file_out, "      ; retained arg1 in BC\n      %s %s\n      ; retained result in BC\n",
        op == TAC_OP_ADD ? "INC" : "DEC", byte_counter ? "C" : "BC");
    return SUCCEEDED;
  }
  if (op == TAC_OP_ADD && arg2_location.kind == LOC_CONST && arg2_location.value == 1 &&
      (arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL) &&
      result_location.kind == arg1_location.kind && result_location.offset == arg1_location.offset &&
      (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16) &&
      (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) &&
      arg1_location.size == 2 && result_location.size == 2) {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    if (_is_z80_bounded_counter(t, function_node, 255) == YES)
      fprintf(file_out, "      ; bounded word counter increment; high byte preserved\n      INC (IX%+d)\n", ix_offset);
    else
      fprintf(file_out, "      INC (IX%+d)\n      JR  NZ,+\n      INC (IX%+d)\n+:\n", ix_offset, ix_offset + 1);
    return SUCCEEDED;
  }

  arg1_in_hl = arg1_location.kind == LOC_PHY_HL ? YES : NO;
  arg2_in_hl = arg2_location.kind == LOC_PHY_HL ? YES : NO;
  result_in_hl = result_location.kind == LOC_PHY_HL ? YES : NO;
  arg1_in_bc = arg1_location.kind == LOC_PHY_BC ? YES : NO;
  arg2_in_bc = arg2_location.kind == LOC_PHY_BC ? YES : NO;
  result_in_bc = result_location.kind == LOC_PHY_BC ? YES : NO;
  if (op == TAC_OP_AND || op == TAC_OP_OR || op == TAC_OP_XOR) {
    if (arg2_location.kind == LOC_CONST)
      constant_operand = &arg2_location;
    else if (arg1_location.kind == LOC_CONST && arg2_in_hl == YES)
      constant_operand = &arg1_location;
  }

  if (result_in_bc == YES && (op == TAC_OP_AND || op == TAC_OP_OR || op == TAC_OP_XOR) &&
      (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_HL || arg1_location.kind == LOC_PHY_BC ||
       arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL) &&
      (t->arg2_physical_register == Z80_PHY_NONE || arg2_in_hl == YES) &&
      (t->arg2_type == TAC_ARG_TYPE_CONSTANT || t->arg2_var_type == VARIABLE_TYPE_UINT8 ||
       t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16)) {
    int byte_offset;
    char *mnemonic = op == TAC_OP_OR ? "OR " : op == TAC_OP_XOR ? "XOR" : "AND";
    if ((arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL) &&
      _load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    if (arg1_location.kind == LOC_CONST)
      _load_value_to_bc(arg1_location.value, file_out);
    else if (arg1_in_hl == YES)
      _load_hl_to_bc(file_out);
    else if (arg1_in_bc == NO && _materialize_array_index_to_bc(&arg1_location, t->arg1_var_type, ix_offset, NO, file_out) == FAILED)
      return FAILED;
    if (arg1_in_hl == YES || arg1_in_bc == YES)
      fprintf(file_out, "      ; retained arg1 in %s\n", arg1_in_hl == YES ? "HL" : "BC");
    if ((arg2_location.kind == LOC_GLOBAL || arg2_location.kind == LOC_STACK_LOCAL || arg2_location.kind == LOC_STACK_SPILL) &&
        _load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    for (byte_offset = 1; byte_offset >= 0; byte_offset--) {
      if (byte_offset == 1 && t->arg2_var_type == VARIABLE_TYPE_UINT8 && arg2_location.kind != LOC_CONST)
        _load_value_to_a(0, file_out);
      else if (arg2_location.kind == LOC_GLOBAL || arg2_location.kind == LOC_STACK_LOCAL || arg2_location.kind == LOC_STACK_SPILL)
        _load_from_ix_to_a(ix_offset + byte_offset, file_out);
      else if (_materialize_to_a(&arg2_location, byte_offset, file_out) == FAILED)
        return FAILED;
      fprintf(file_out, "      %s A,%s\n      LD  %s,A\n", mnemonic, byte_offset == 1 ? "B" : "C", byte_offset == 1 ? "B" : "C");
    }
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }

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
  else if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_A ||
           arg1_location.kind == LOC_PHY_B || arg1_location.kind == LOC_PHY_C) {
    if (_materialize_to_hl(&arg1_location, file_out) == FAILED)
      return FAILED;
    if (arg1_location.kind != LOC_CONST && t->arg1_var_type == VARIABLE_TYPE_INT8) {
      if (arg2_location.kind == LOC_PHY_A)
        fprintf(file_out, "      PUSH AF\n");
      _sign_extend_l_to_hl(file_out);
      if (arg2_location.kind == LOC_PHY_A)
        fprintf(file_out, "      POP AF\n");
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

  if (constant_operand != NULL) {
  }
  else if (arg2_in_bc == YES) {
  }
  else if (arg2_in_hl == YES) {
    if (arg1_location.kind == LOC_CONST) {
      /* 16-bit */
      if (op == TAC_OP_ADD && (arg1_location.value == 1 || arg1_location.value == 256))
        constant_step = arg1_location.value;
      else
        _load_value_to_bc(arg1_location.value, file_out);
    }
    else if (arg1_in_bc == YES) {
    }
    else {
      if (_materialize_array_index_to_bc(&arg1_location, t->arg1_var_type, ix_offset, NO, file_out) == FAILED)
        return FAILED;
    }
  }
  else if (arg2_location.kind == LOC_CONST) {
    /* 16-bit */
    if ((op == TAC_OP_ADD || op == TAC_OP_SUB) && (arg2_location.value == 1 || arg2_location.value == 256)) {
      constant_step = arg2_location.value;
    }
    else
      _load_value_to_bc(arg2_location.value, file_out);
  }
  else {
    if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* add/sub/or/xor/and data bc -> hl */
  /******************************************************************************************************/

  if (op == TAC_OP_ADD) {
    if (constant_step == 1)
      _inc_hl(file_out);
    else if (constant_step == 256)
      fprintf(file_out, "      INC H\n");
    else
      _add_bc_to_hl(file_out);
  }
  else if (op == TAC_OP_SUB) {
    if (constant_step == 1)
      _dec_hl(file_out);
    else if (constant_step == 256)
      fprintf(file_out, "      DEC H\n");
    else
      _sub_bc_from_hl(file_out);
  }
  else if (op == TAC_OP_OR || op == TAC_OP_XOR || op == TAC_OP_AND)
    _bitwise_hl(file_out, op, result_in_bc, constant_operand);
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
    if (op != TAC_OP_OR && op != TAC_OP_XOR && op != TAC_OP_AND &&
        _store_from_hl(&result_location, ix_offset, file_out) == FAILED)
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


static int _generate_asm_add_sub_or_xor_and_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op, int *tac_index) {

  if (op == TAC_OP_AND && t->result_type == TAC_ARG_TYPE_TEMP &&
      (t->arg1_type == TAC_ARG_TYPE_CONSTANT || t->arg2_type == TAC_ARG_TYPE_CONSTANT)) {
    struct temp_register *temporary = _find_z80_temp_register(function_node, (int)t->result_d);
    int next = *tac_index + 1;

    while (next < g_tacs_count && g_tacs[next].op == TAC_OP_DEAD)
      next++;
    if (temporary != NULL && temporary->read_count == 1 && temporary->write_count == 1 &&
        next < g_tacs_count && g_tacs[next].function_node == function_node &&
        (g_tacs[next].op == TAC_OP_JUMP_EQ || g_tacs[next].op == TAC_OP_JUMP_NEQ) &&
        ((g_tacs[next].arg1_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[next].arg1_d == (int)t->result_d &&
          g_tacs[next].arg2_type == TAC_ARG_TYPE_CONSTANT && g_tacs[next].arg2_d == 0) ||
         (g_tacs[next].arg2_type == TAC_ARG_TYPE_TEMP && (int)g_tacs[next].arg2_d == (int)t->result_d &&
          g_tacs[next].arg1_type == TAC_ARG_TYPE_CONSTANT && g_tacs[next].arg1_d == 0))) {
      int constant_first = t->arg1_type == TAC_ARG_TYPE_CONSTANT;
      int mask = (int)(constant_first ? t->arg1_d : t->arg2_d) & 65535;
      int byte_offset = (mask & 255) == 0 ? 1 : 0;
      int source_type = constant_first ? t->arg2_var_type : t->arg1_var_type;
      int promoted_type = constant_first ? t->arg2_var_type_promoted : t->arg1_var_type_promoted;

      /* Read just the selected byte; extension or truncation must not supply it. */
      if (mask != 0 && ((mask & 255) == 0 || mask < 256) &&
          get_variable_type_size(source_type) > byte_offset * 8 &&
          get_variable_type_size(promoted_type) > byte_offset * 8 &&
          get_variable_type_size(t->result_var_type) > byte_offset * 8) {
        struct z80_location source;
        int ix_offset = 0, bit = 0, byte_mask = (mask >> (byte_offset * 8)) & 255;

        if (_resolve_tac_location(t, constant_first ? TAC_USE_ARG2 : TAC_USE_ARG1, function_node, &source) == FAILED)
          return FAILED;
        if (source.kind == LOC_STACK_LOCAL || source.kind == LOC_STACK_SPILL ||
            source.kind == LOC_PHY_A || source.kind == LOC_PHY_B || source.kind == LOC_PHY_C ||
            source.kind == LOC_PHY_HL || source.kind == LOC_PHY_BC) {
          int memory = source.kind == LOC_STACK_LOCAL || source.kind == LOC_STACK_SPILL;
          char *byte_name = source.kind == LOC_PHY_HL ? (byte_offset ? "H" : "L") :
            source.kind == LOC_PHY_BC ? (byte_offset ? "B" : "C") : _get_z80_physical_register_name(source.phy);

          if (memory && _load_location_address_to_ix(&source, &ix_offset, file_out) == FAILED)
            return FAILED;
          while ((1 << bit) < byte_mask)
            bit++;
          fprintf(file_out, "      ; folded single-use mask/zero branch\n");
          if ((1 << bit) == byte_mask) {
            if (memory)
              fprintf(file_out, "      BIT %d,(IX%+d)\n", bit, ix_offset + byte_offset);
            else
              fprintf(file_out, "      BIT %d,%s\n", bit, byte_name);
          }
          else {
            if (memory)
              _load_from_ix_to_a(ix_offset + byte_offset, file_out);
            else if (_materialize_to_a(&source, byte_offset, file_out) == FAILED)
              return FAILED;
            _and_value_from_a(byte_mask, file_out);
          }
          print_tac(&g_tacs[next], YES, file_out);
          if (g_tacs[next].op == TAC_OP_JUMP_EQ)
            _jump_z_to(g_tacs[next].result_s, file_out);
          else
            _jump_nz_to(g_tacs[next].result_s, file_out);
          *tac_index = next;
          return SUCCEEDED;
        }
      }
    }
  }

  /* 8-bit or 16-bit? */
  if (is_z80_register_allocator_byte_transparent_tac(t, Z80_PHY_HL) == YES ||
      ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
       (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8)))
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

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    RA_TRACE((stderr, "register_allocator: complement_8bit function=%s arg1_kind=%s arg1_phy=%s result_kind=%s result_phy=%s\n",
            function_node->children[1]->label,
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy),
            _get_z80_location_kind_name(result_location.kind), _get_z80_physical_register_name(result_location.phy)));
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_A || arg1_location.kind == LOC_PHY_C) {
    if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
      return FAILED;
  }
  else {
    if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "complement_8bit_arg1_address", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_IX, ix_offset);
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

  if (result_location.kind == LOC_PHY_A || result_location.kind == LOC_PHY_B || result_location.kind == LOC_PHY_C) {
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

  if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
    return FAILED;

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
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

  return SUCCEEDED;
}


static int _generate_asm_complement_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  int ix_offset = 0;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    RA_TRACE((stderr, "register_allocator: complement_16bit function=%s arg1_kind=%s arg1_phy=%s result_kind=%s result_phy=%s\n",
            function_node->children[1]->label,
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy),
            _get_z80_location_kind_name(result_location.kind), _get_z80_physical_register_name(result_location.phy)));
  }
#endif

  /* generate asm */

  if (result_location.kind == LOC_PHY_BC) {
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

      if (t->arg1_var_type != VARIABLE_TYPE_INT16 && t->arg1_var_type != VARIABLE_TYPE_UINT16 &&
          !((t->arg1_var_type == VARIABLE_TYPE_INT8 || t->arg1_var_type == VARIABLE_TYPE_UINT8) &&
          (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16))) {
        fprintf(stderr, "_generate_asm_complement_z80_16bit(): Unhandled 8-bit ARG1 for BC result! Please submit a bug report!\n");
        print_tac(t, NO, stderr);
        return FAILED;
      }
      if (_materialize_array_index_to_bc(&arg1_location, t->arg1_var_type, ix_offset, NO, file_out) == FAILED)
        return FAILED;
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

  if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_HL) {
    if (_materialize_to_hl(&arg1_location, file_out) == FAILED)
      return FAILED;
  }
  else {
    if (arg1_location.kind == LOC_PHY_A)
      fprintf(file_out, "      LD  L,A\n");
    else
      _load_from_ix_to_l(ix_offset, file_out);

    if (arg1_location.kind != LOC_PHY_A &&
        (t->arg1_var_type == VARIABLE_TYPE_INT16 || t->arg1_var_type == VARIABLE_TYPE_UINT16)) {
      _load_from_ix_to_h(ix_offset + 1, file_out);
    }
    else if (t->arg1_var_type == VARIABLE_TYPE_INT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                       t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _sign_extend_l_to_hl(file_out);
    }
    else if (t->arg1_var_type == VARIABLE_TYPE_UINT8 && (t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 ||
                                                         t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16)) {
      _load_value_to_h(0, file_out);
    }
    else {
      fprintf(stderr, "_generate_asm_complement_z80_16bit(): Unhandled %s ARG1! Please submit a bug report!\n",
              arg1_location.kind == LOC_PHY_A ? "A" : "8-bit");
      print_tac(t, NO, stderr);
      return FAILED;
    }
  }

  /******************************************************************************************************/
  /* complement hl */
  /******************************************************************************************************/

  _complement_hl(file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_HL) {
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


static int _base_needs_memory_pointer_load(int base_type, struct tree_node *base_node, struct z80_location *base_location) {

  if (base_location->kind == LOC_PHY_A || base_location->kind == LOC_PHY_HL || base_location->kind == LOC_PHY_BC)
    return NO;

  if (base_type == TAC_ARG_TYPE_TEMP)
    return YES;

  if (base_node != NULL && base_node->children[0]->value_double > 0.0 && base_node->value == 0)
    return YES;

  return NO;
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

  if (op == TAC_OP_GET_ADDRESS_ARRAY)
    base_memory_pointer_load = _base_needs_memory_pointer_load(t->arg1_type, t->arg1_node, &arg1_location);

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

    RA_TRACE((stderr, "register_allocator: get_address_base function=%s kind=%s memory_pointer_load=%s offset=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location.kind), base_memory_pointer_load == YES ? "yes" : "no",
            arg1_location.offset, _get_z80_physical_register_name(arg1_location.phy)));
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
    if (retained_bc_base_hl_index == NO) {
      fprintf(file_out, "      LD  H,B\n");
      fprintf(file_out, "      LD  L,C\n");
    }
    fprintf(file_out, "      ; retained arg1 in BC\n");
  }
  else {
    if (_load_location_address_to_hl(&arg1_location, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node, "get_address_base", "arg1",
                                                    &arg1_location, RA_ADDRESS_TARGET_HL, 0);
  }

  if (base_memory_pointer_load == YES) {
    fprintf(file_out, "      LD  A,(HL)\n");
    _inc_hl(file_out);
    fprintf(file_out, "      LD  H,(HL)\n");
    fprintf(file_out, "      LD  L,A\n");
  }

  if (op == TAC_OP_GET_ADDRESS_ARRAY) {
    int var_type;

    /******************************************************************************************************/
    /* index address (arg2) -> ix */
    /******************************************************************************************************/

    if (index_materialized == NO && arg2_location.kind != LOC_CONST) {
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

    if (index_materialized == NO) {
      if (arg2_location.kind == LOC_CONST)
        _load_value_to_bc(arg2_location.value, file_out);
      else if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED) {
        print_tac(t, NO, stderr);
        return FAILED;
      }
    }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES) {
      char *function_name;

      function_name = "<unknown>";
      if (function_node != NULL && function_node->children[1] != NULL)
        function_name = function_node->children[1]->label;

      RA_TRACE((stderr, "register_allocator: get_address_index function=%s kind=%s size=%d phy=%s\n",
              function_name, _get_z80_location_kind_name(arg2_location.kind),
              arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
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

    RA_TRACE((stderr, "register_allocator: z80_in_port function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
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
    RA_TRACE((stderr, "register_allocator: z80_in_result function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy)));
  }
#endif

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A || result_location.kind == LOC_PHY_HL ||
      result_location.kind == LOC_PHY_BC || result_location.kind == LOC_PHY_C) {
    if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
      return FAILED;
    if (result_location.kind == LOC_PHY_HL)
      _load_value_to_h(0, file_out);
    else if (result_location.kind == LOC_PHY_BC)
      _load_value_to_b(0, file_out);
    fprintf(file_out, "      ; retained result in %s\n", _get_z80_physical_register_name(result_location.phy));
    return SUCCEEDED;
  }

  ix_offset = 0;

  if (_load_location_address_to_ix(&result_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data a -> (ix) */
  /******************************************************************************************************/

  if (_store_from_a(&result_location, 0, ix_offset, file_out) == FAILED)
    return FAILED;
  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16)
    _load_value_into_ix(0, ix_offset + 1, file_out);

  return SUCCEEDED;
}


static int _materialize_array_base(struct tac *tac, struct z80_location *base_location, int base_memory_pointer_load,
                                   int arg2_is_zero, int keep_hl, struct tree_node *function_node, FILE *file_out) {

  int is_write, materialized_hl = NO, ix_offset = base_location->offset - g_ix_frame_offset;
  char *operand_name;

  is_write = tac->op == TAC_OP_ARRAY_WRITE;
  operand_name = _get_z80_tac_operand_name(is_write == YES ? TAC_USE_RESULT : TAC_USE_ARG1);

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: array_%s_base function=%s kind=%s memory_pointer_load=%s offset=%d phy=%s\n",
            is_write == YES ? "write" : "read", function_name,
            _get_z80_location_kind_name(base_location->kind), base_memory_pointer_load == YES ? "yes" : "no",
            base_location->offset, _get_z80_physical_register_name(base_location->phy)));
  }
#endif

  if (base_location->kind == LOC_CONST) {
    if (is_write == NO) {
      fprintf(stderr, "_materialize_array_base(): Source cannot be a value!\n");
      return FAILED;
    }
    _load_value_to_iy(base_location->value, file_out);
  }
  else if (base_location->kind == LOC_PHY_A) {
    fprintf(stderr, "_materialize_array_base(): Cannot use an 8-bit retained array base! Please submit a bug report!\n");
    return FAILED;
  }
  else if (base_location->kind == LOC_PHY_HL || base_location->kind == LOC_PHY_BC) {
    if (base_location->kind == LOC_PHY_HL)
      materialized_hl = YES;
    else {
      if (is_write == YES && arg2_is_zero == NO) {
        fprintf(stderr, "_materialize_array_base(): Cannot use retained result in BC with a non-zero index! Please submit a bug report!\n");
        return FAILED;
      }
      _load_bc_to_iy(file_out);
    }
    fprintf(file_out, "      ; retained %s in %s\n", operand_name, base_location->kind == LOC_PHY_HL ? "HL" : "BC");
  }
  else if (base_memory_pointer_load == YES && (is_write == NO || keep_hl == YES) &&
           (arg2_is_zero == NO || keep_hl == YES) &&
           g_allocator_enabled == YES && g_allocator_force_all_spill == NO && g_ix_frame_offset != Z80_GLOBAL_OFFSET &&
           get_register_allocator_address_materialization_mode(base_location->kind, RA_ADDRESS_TARGET_IX,
             ix_offset) == RA_ADDRESS_MODE_FRAME_DISPLACEMENT) {
    _trace_z80_specialized_address_materialization(function_node,
        is_write == YES ? "array_write_base_address" : "array_read_base_address",
        operand_name, base_location, RA_ADDRESS_TARGET_IX, ix_offset);
    _load_from_ix_to_l(ix_offset, file_out);
    _load_from_ix_to_h(ix_offset + 1, file_out);
    materialized_hl = YES;
  }
  else {
    if (_load_location_address_to_iy(base_location, file_out) == FAILED)
      return FAILED;
    _trace_z80_specialized_address_materialization(function_node,
        is_write == YES ? "array_write_base_address" : "array_read_base_address",
        operand_name, base_location, RA_ADDRESS_TARGET_IY, 0);

    if (base_memory_pointer_load == YES && (is_write == NO || keep_hl == YES) &&
        (arg2_is_zero == NO || keep_hl == YES) &&
        g_allocator_enabled == YES && g_allocator_force_all_spill == NO) {
      if (_materialize_to_hl(base_location, file_out) == FAILED)
        return FAILED;
      materialized_hl = YES;
    }
    else if (base_memory_pointer_load == YES) {
      if (arg2_is_zero == NO)
        _push_bc(file_out);
      _load_from_iy_to_c(0, file_out);
      _load_from_iy_to_b(1, file_out);
      _load_value_to_iy(0, file_out);
      _add_bc_to_iy(file_out);
      if (arg2_is_zero == NO)
        _pop_bc(file_out);
    }
  }

  if (materialized_hl == YES && keep_hl == NO) {
    fprintf(file_out, "      PUSH HL\n");
    fprintf(file_out, "      POP IY\n");
  }

  return SUCCEEDED;
}


static int _transfer_array_base_spill(struct tac *t, struct tree_node *function_node, struct z80_location *base_location, int is_reload, FILE *file_out) {

  struct tac spill_tac;
  struct z80_location spill_location;
  struct register_allocator_reload_emission reload;
  struct register_allocator_spill_emission spill;
  char *function_name, *operand_name, *op_name;
  int ix_offset, operand, physical_register, temp, byte_count, scalar;

  scalar = t->op == TAC_OP_ADD || t->op == TAC_OP_SUB || t->op == TAC_OP_AND || t->op == TAC_OP_OR || t->op == TAC_OP_XOR;
  operand = is_reload == YES ? t->reload_spill_to_physical_operand : t->store_retained_to_spill_operand;
  if (operand == -1)
    return SUCCEEDED;
  function_name = "<unknown>";
  if (function_node != NULL && function_node->children[1] != NULL)
    function_name = function_node->children[1]->label;
  if (scalar == YES && (operand == TAC_USE_ARG1 || operand == TAC_USE_ARG2) &&
      (operand == TAC_USE_ARG1 ? t->arg1_type : t->arg2_type) == TAC_ARG_TYPE_TEMP) {
    temp = (int)(operand == TAC_USE_ARG1 ? t->arg1_d : t->arg2_d);
    physical_register = base_location->phy;
    operand_name = operand == TAC_USE_ARG1 ? "arg1" : "arg2";
    op_name = "SCALAR";
  }
  else if ((t->op == TAC_OP_ARRAY_READ || (is_reload == YES && t->op == TAC_OP_ASSIGNMENT)) && operand == TAC_USE_ARG1 &&
      t->arg1_type == TAC_ARG_TYPE_TEMP && base_location->kind == LOC_PHY_HL &&
      t->arg1_physical_register == Z80_PHY_HL) {
    temp = (int)t->arg1_d;
    physical_register = Z80_PHY_HL;
    operand_name = "arg1";
    op_name = t->op == TAC_OP_ARRAY_READ ? "ARRAY_READ" : "ASSIGNMENT";
  }
  else if (is_reload == YES && t->op == TAC_OP_ARRAY_WRITE && operand == TAC_USE_RESULT &&
      t->result_type == TAC_ARG_TYPE_TEMP && base_location->kind == LOC_PHY_BC &&
      t->result_physical_register == Z80_PHY_BC) {
    if (z80_validate_array_write_reload_operand(function_name, operand) == FAILED)
      return FAILED;
    temp = (int)t->result_d;
    physical_register = Z80_PHY_BC;
    operand_name = "result";
    op_name = "ARRAY_WRITE";
  }
  else {
    fprintf(stderr, "register_allocator_z80: array_base_spill function=%s operand=%d reload=%s status=unsupported_operand\n",
        function_name, operand, is_reload == YES ? "yes" : "no");
    return FAILED;
  }
  spill_tac = *t;
  spill_tac.arg1_physical_register = spill_tac.arg2_physical_register = spill_tac.result_physical_register = Z80_PHY_NONE;
  if (_resolve_tac_location(&spill_tac, operand, function_node, &spill_location) == FAILED ||
      spill_location.kind != LOC_STACK_SPILL)
    return FAILED;
  byte_count = scalar == YES ? spill_location.size : 2;
  if (is_reload == YES) {
    if (register_allocator_prepare_reload_operand_emission(function_name, YES,
      temp, operand, Z80_PHY_NONE, physical_register, YES,
        spill_location.offset, byte_count, &reload) == FAILED || reload.emit != YES ||
        reload.operand != operand || reload.physical_register != physical_register || reload.byte_count != byte_count)
      return FAILED;
  }
  else if (register_allocator_prepare_spill_emission(function_name, YES,
      temp, Z80_PHY_NONE, physical_register, YES, spill_location.offset, byte_count, &spill) == FAILED ||
      spill.emit != YES || spill.physical_register != physical_register || spill.byte_count != byte_count)
    return FAILED;
  if (is_reload == YES && byte_count == 2) {
    int definition_index;

    for (definition_index = (int)(t - g_tacs) - 1; definition_index >= 0; definition_index--) {
      struct tac *definition;

      definition = &g_tacs[definition_index];
      if (definition->function_node != function_node || definition->op == TAC_OP_LABEL ||
          definition->op == TAC_OP_ASM || definition->op == TAC_OP_FUNCTION_CALL ||
          definition->op == TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE ||
          (definition->op >= TAC_OP_JUMP && definition->op <= TAC_OP_JUMP_GTE) ||
          definition->op == TAC_OP_RETURN || definition->op == TAC_OP_RETURN_VALUE)
        break;
      if (definition->op == TAC_OP_DEAD || definition->result_type != TAC_ARG_TYPE_TEMP ||
          (int)definition->result_d != temp)
        continue;
      if (definition->op == TAC_OP_GET_ADDRESS_ARRAY && definition->arg1_type == TAC_ARG_TYPE_LABEL &&
          definition->arg1_node != NULL && definition->arg1_node->type == TREE_NODE_TYPE_CREATE_VARIABLE &&
          (definition->arg1_node->flags & TREE_NODE_FLAG_GLOBAL) != 0 && definition->arg1_node->value > 0 &&
          definition->arg2_type == TAC_ARG_TYPE_CONSTANT && definition->arg2_d >= 0.0 &&
          definition->arg2_d < definition->arg1_node->value && definition->arg2_d <= 32767.0 &&
          definition->arg2_d == (double)(int)definition->arg2_d) {
        int displacement = (int)definition->arg2_d;
        int element_type = get_array_item_variable_type(definition->arg1_node);
        if (element_type == VARIABLE_TYPE_INT16 || element_type == VARIABLE_TYPE_UINT16)
          displacement *= 2;
        if (displacement == 0)
          fprintf(file_out, "      LD  %s,%s\n", _get_z80_physical_register_name(physical_register), definition->arg1_s);
        else
          fprintf(file_out, "      LD  %s,%s+%d\n", _get_z80_physical_register_name(physical_register), definition->arg1_s, displacement);
        fprintf(file_out, "      ; rematerialize %s from stable global address into %s\n",
            operand_name, _get_z80_physical_register_name(physical_register));
        return SUCCEEDED;
      }
      break;
    }
  }
  fprintf(file_out, "      ; allocator transfer begin kind=%s bytes=%d\n",
      is_reload == YES ? "reload" : "preserve", byte_count);
  if (_load_location_address_to_ix(&spill_location, &ix_offset, file_out) == FAILED)
    return FAILED;
  if (is_reload == NO) {
    if (physical_register == Z80_PHY_A)
      _load_a_into_ix(ix_offset, file_out);
    else if (physical_register == Z80_PHY_B)
      _load_b_into_ix(ix_offset, file_out);
    else if (physical_register == Z80_PHY_C || physical_register == Z80_PHY_BC) {
      _load_c_into_ix(ix_offset, file_out);
      if (byte_count == 2)
        _load_b_into_ix(ix_offset + 1, file_out);
    }
    else if (_store_from_hl(&spill_location, ix_offset, file_out) == FAILED)
      return FAILED;
  }
  else {
    if (physical_register == Z80_PHY_A)
      _load_from_ix_to_a(ix_offset, file_out);
    else if (physical_register == Z80_PHY_B)
      _load_from_ix_to_b(ix_offset, file_out);
    else if (physical_register == Z80_PHY_C || physical_register == Z80_PHY_BC)
      _load_from_ix_to_c(ix_offset, file_out);
    else if (physical_register == Z80_PHY_HL)
      _load_from_ix_to_l(ix_offset, file_out);
    if (byte_count == 2) {
      if (physical_register == Z80_PHY_HL)
        _load_from_ix_to_h(ix_offset + 1, file_out);
      else
        _load_from_ix_to_b(ix_offset + 1, file_out);
    }
  }
  if (scalar == YES)
    RA_TRACE((stderr, "register_allocator: scalar_split_transfer function=%s tac=%d operand=%d phy=%s bytes=%d kind=%s\n",
      function_name, (int)(t - g_tacs), operand, _get_z80_physical_register_name(physical_register),
      byte_count, is_reload == YES ? "reload" : "store"));

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES && scalar == NO) {
    if (is_reload == YES)
      RA_TRACE((stderr, "register_allocator: reload_insert_emit function=%s op=%s operand=%s source_offset=%d bytes=2 phy=%s operand_id=%d\n",
          function_name, op_name, operand_name, reload.source_offset,
          _get_z80_physical_register_name(physical_register), reload.operand));
    else
      RA_TRACE((stderr, "register_allocator: spill_insert_emit function=%s op=ARRAY_READ operand=arg1 phy=HL spill_offset=%d bytes=2\n",
          function_name, spill.destination_offset));
  }
#endif
  if (is_reload == YES)
    fprintf(file_out, "      ; reload %s from spill slot into %s\n", operand_name, _get_z80_physical_register_name(physical_register));
  else
    fprintf(file_out, "      ; preserve retained %s in spill slot\n", operand_name);
  return SUCCEEDED;
}


static int _generate_asm_array_read_z80(struct tac *t, FILE *file_out, struct tree_node *function_node) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  char address_label[MAX_NAME_LENGTH + 32];
  int constant_address = NO, byte_offset;
  int var_type, result_needs_16_bit, ix_offset = 0, base_memory_pointer_load = NO, arg2_is_zero = NO, base_materialized = NO, base_in_hl;

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

  base_memory_pointer_load = _base_needs_memory_pointer_load(t->arg1_type, t->arg1_node, &arg1_location);

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  if (_transfer_array_base_spill(t, function_node, &arg1_location, NO, file_out) == FAILED)
    return FAILED;

  if (t->arg1_node != NULL)
    var_type = get_array_item_variable_type(t->arg1_node);
  else
    var_type = t->arg1_var_type;

  if (base_memory_pointer_load == NO && arg2_location.kind == LOC_CONST &&
      (arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL) &&
      (var_type == VARIABLE_TYPE_UINT8 || var_type == VARIABLE_TYPE_INT8 ||
       var_type == VARIABLE_TYPE_UINT16 || var_type == VARIABLE_TYPE_INT16)) {
    byte_offset = arg2_location.value & 65535;
    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16)
      byte_offset *= 2;
    byte_offset &= 65535;

    if (arg1_location.kind == LOC_GLOBAL) {
      if (byte_offset != 0) {
        snprintf(address_label, sizeof(address_label), "%s+%d&65535", arg1_location.label, byte_offset);
        arg1_location.label = address_label;
      }
    }
    else {
      arg1_location.offset = ((arg1_location.offset & 65535) + byte_offset) & 65535;
      if (arg1_location.offset > 32767)
        arg1_location.offset -= 65536;
    }
    arg2_location.value = 0;
    constant_address = YES;
  }

  if (arg2_location.kind == LOC_CONST && arg2_location.value == 0)
    arg2_is_zero = YES;

  if (arg1_location.kind == LOC_PHY_BC && arg2_location.kind == LOC_PHY_BC) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Cannot use retained arg1 in BC with a retained BC index! Please submit a bug report!\n");
    return FAILED;
  }

  base_in_hl = g_allocator_enabled == YES && g_allocator_force_all_spill == NO &&
               var_type == VARIABLE_TYPE_UINT8 && arg1_location.kind == LOC_PHY_BC &&
               arg2_location.kind == LOC_PHY_HL && arg2_location.size == 2 &&
               (t->arg2_var_type == VARIABLE_TYPE_INT16 || t->arg2_var_type == VARIABLE_TYPE_UINT16);

  if (arg1_location.kind == LOC_PHY_BC && arg2_is_zero == NO) {
    if (base_in_hl == NO &&
        _materialize_array_base(t, &arg1_location, base_memory_pointer_load, arg2_is_zero, NO, function_node, file_out) == FAILED)
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

  if (base_in_hl == NO && _materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED) {
    print_tac(t, NO, stderr);
    return FAILED;
  }

  if (_transfer_array_base_spill(t, function_node, &arg1_location, YES, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: array_read_index function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
  }
#endif

  /******************************************************************************************************/
  /* source address (arg1) -> hl/iy */
  /******************************************************************************************************/

  result_needs_16_bit = result_location.size == 2 ||
                        t->result_var_type_promoted == VARIABLE_TYPE_INT16 ||
                        t->result_var_type_promoted == VARIABLE_TYPE_UINT16;

  if (constant_address == YES && arg1_location.kind == LOC_GLOBAL) {
    if (var_type == VARIABLE_TYPE_UINT8 || var_type == VARIABLE_TYPE_INT8) {
      if (_load_location_address_to_hl(&arg1_location, file_out) == FAILED)
        return FAILED;
      base_in_hl = YES;
    }
    base_materialized = YES;
  }

  base_in_hl = base_in_hl == YES || (g_allocator_enabled == YES && g_allocator_force_all_spill == NO &&
               base_materialized == NO && (var_type == VARIABLE_TYPE_UINT8 ||
                 var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) &&
               (arg1_location.kind == LOC_PHY_HL || base_memory_pointer_load == YES));
  if (base_materialized == NO && _materialize_array_base(t, &arg1_location, base_memory_pointer_load,
      arg2_is_zero, base_in_hl, function_node, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* add bc -> hl/iy */
  /******************************************************************************************************/

  if (arg2_is_zero == NO) {
    if (base_in_hl == YES)
      _add_bc_to_hl(file_out);
    else
      _add_bc_to_iy(file_out);

    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      if (base_in_hl == YES)
        _add_bc_to_hl(file_out);
      else
        _add_bc_to_iy(file_out);
    }
  }

  /******************************************************************************************************/
  /* copy data -> hl */
  /******************************************************************************************************/

  if (t->arg1_type == TAC_ARG_TYPE_CONSTANT) {
    fprintf(stderr, "_generate_asm_array_read_z80(): Source cannot be a value!\n");
    return FAILED;
  }
  else {
    if (var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16) {
      /* 16-bit */
      if (constant_address == YES && arg1_location.kind == LOC_GLOBAL)
        fprintf(file_out, "      LD  HL,(%s)\n", arg1_location.label);
      else if (base_in_hl == YES) {
        fprintf(file_out, "      LD  A,(HL)\n");
        _inc_hl(file_out);
        fprintf(file_out, "      LD  H,(HL)\n");
        fprintf(file_out, "      LD  L,A\n");
      }
      else {
        _load_from_iy_to_l(0, file_out);
        _load_from_iy_to_h(1, file_out);
      }
    }
    else {
      /* 8-bit */
      if (base_in_hl == YES)
        fprintf(file_out, "      LD  L,(HL)\n");
      else
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
    RA_TRACE((stderr, "register_allocator: array_read_result function=%s kind=%s size=%d phy=%s value_bytes=%d\n",
            function_node->children[1]->label, _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy), result_needs_16_bit == YES ? 2 : 1));
  }
#endif

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A || result_location.kind == LOC_PHY_B || result_location.kind == LOC_PHY_C) {
    if (result_needs_16_bit) {
      fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain a 16-bit array read result in %s! Please submit a bug report!\n",
              _get_z80_physical_register_name(result_location.phy));
      return FAILED;
    }
    fprintf(file_out, "      LD  %s,L\n", _get_z80_physical_register_name(result_location.phy));
    fprintf(file_out, "      ; retained result in %s\n", _get_z80_physical_register_name(result_location.phy));
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES && result_location.kind == LOC_PHY_B)
      RA_TRACE((stderr, "register_allocator: array_read_physical_result function=%s result_phy=B value_bytes=1 via=L store=B status=complete\n",
              function_node->children[1]->label));
#endif
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_HL || result_location.kind == LOC_PHY_BC) {
    if (!result_needs_16_bit) {
      fprintf(stderr, "_generate_asm_array_read_z80(): Cannot retain an 8-bit array read result in %s! Please submit a bug report!\n",
              _get_z80_physical_register_name(result_location.phy));
      return FAILED;
    }
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      ; retained result in %s\n", _get_z80_physical_register_name(result_location.phy));
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

    RA_TRACE((stderr, "register_allocator: z80_out_port function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
    RA_TRACE((stderr, "register_allocator: z80_out_value function=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy)));
  }
#endif

  /******************************************************************************************************/
  /* source value (arg1) -> a */
  /******************************************************************************************************/

  if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_A ||
      arg1_location.kind == LOC_PHY_HL || arg1_location.kind == LOC_PHY_BC) {
    if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
      return FAILED;
    if (arg1_location.kind != LOC_CONST)
      fprintf(file_out, "      ; retained arg1 in %s\n", _get_z80_physical_register_name(arg1_location.phy));
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


static int _materialize_array_write_value(struct tac *tac, struct z80_location *arg1_location, int var_type, int ix_offset, int base_in_hl, struct tree_node *function_node, FILE *file_out) {

  int value;
  int value_bytes;
  int source_is_16_bit;

  value_bytes = arg1_location->size == 2 ? 2 : 1;
  if (var_type == VARIABLE_TYPE_INT8 || var_type == VARIABLE_TYPE_UINT8 ||
      var_type == VARIABLE_TYPE_INT16 || var_type == VARIABLE_TYPE_UINT16)
    value_bytes = get_variable_type_size(var_type) / 8;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: array_write_value function=%s kind=%s bytes=%d source_size=%d phy=%s\n",
            function_name, _get_z80_location_kind_name(arg1_location->kind), value_bytes,
            arg1_location->size, _get_z80_physical_register_name(arg1_location->phy)));
  }
#endif

  if (base_in_hl == YES) {
    if (value_bytes != 1) {
      fprintf(stderr, "_materialize_array_write_value(): Cannot store a word through HL! Please submit a bug report!\n");
      return FAILED;
    }
    if (arg1_location->kind == LOC_GLOBAL || arg1_location->kind == LOC_STACK_LOCAL || arg1_location->kind == LOC_STACK_SPILL)
      _load_from_ix_to_a(ix_offset, file_out);
    else if (_materialize_to_a(arg1_location, 0, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      LD  (HL),A\n");
    if (arg1_location->kind == LOC_PHY_A)
      fprintf(file_out, "      ; retained arg1 in A\n");
    return SUCCEEDED;
  }

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

  source_is_16_bit = arg1_location->size == 2 ||
                    tac->arg1_var_type == VARIABLE_TYPE_INT16 || tac->arg1_var_type == VARIABLE_TYPE_UINT16;
  if (source_is_16_bit == YES && value_bytes == 2) {
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
        fprintf(stderr, "_materialize_array_write_value(): Unhandled 8-bit ARG1! Please submit a bug report!\n");
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
  int base_in_hl, base_memory_pointer_load;

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
      _transfer_array_base_spill(t, function_node, &result_location, YES, file_out) == FAILED)
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

      RA_TRACE((stderr, "register_allocator: array_write_base function=%s kind=%s memory_pointer_load=no offset=%d phy=%s\n",
              function_name, _get_z80_location_kind_name(result_location.kind),
              result_location.offset, _get_z80_physical_register_name(result_location.phy)));
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
      _transfer_array_base_spill(t, function_node, &result_location, YES, file_out) == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

        RA_TRACE((stderr, "register_allocator: array_write_index function=%s kind=%s size=%d phy=%s var_type=%d\n",
            function_name, _get_z80_location_kind_name(arg2_location.kind),
          arg2_location.size, _get_z80_physical_register_name(arg2_location.phy), t->arg2_var_type));
  }
#endif

  /******************************************************************************************************/
  /* result address (result) -> hl/iy */
  /******************************************************************************************************/

  var_type = t->result_var_type;
  base_memory_pointer_load = _base_needs_memory_pointer_load(t->result_type, t->result_node, &result_location);
  base_in_hl = g_allocator_enabled == YES && g_allocator_force_all_spill == NO && result_materialized == NO &&
      (var_type == VARIABLE_TYPE_UINT8 || var_type == VARIABLE_TYPE_INT8) &&
      (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_A ||
       arg1_location.kind == LOC_GLOBAL || arg1_location.kind == LOC_STACK_LOCAL || arg1_location.kind == LOC_STACK_SPILL) &&
      (result_location.kind == LOC_PHY_HL ||
       (base_memory_pointer_load == YES && (result_location.kind == LOC_GLOBAL ||
        result_location.kind == LOC_STACK_LOCAL || result_location.kind == LOC_STACK_SPILL)));
  if (result_materialized == NO && _materialize_array_base(t, &result_location,
      base_memory_pointer_load, arg2_is_zero, base_in_hl, function_node, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* add index bc -> hl/iy */
  /******************************************************************************************************/

  if (arg2_is_zero == NO) {
    if (base_in_hl == YES)
      _add_bc_to_hl(file_out);
    else
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

  return _materialize_array_write_value(t, &arg1_location, var_type, ix_offset, base_in_hl, function_node, file_out);
}


/* NOTE! because SAS/C on Amiga thinks that this and the next function were called the same
   prefixes "aaa" and "bbb" were added to separate them */


static int _aaa_generate_asm_shift_left_right_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_hl = NO, arg1_in_bc = NO, arg1_in_a = NO, arg1_in_b = NO, arg1_in_c = NO;
  int arg2_in_bc = NO, arg2_in_a = NO, result_in_b = NO, result_in_c = NO;

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
  result_in_b = result_location.kind == LOC_PHY_B ? YES : NO;
  result_in_c = result_location.kind == LOC_PHY_C ? YES : NO;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    RA_TRACE((stderr, "register_allocator: shift_count function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
    RA_TRACE((stderr, "register_allocator: shift_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT",
            _get_z80_location_kind_name(result_location.kind), result_location.size,
            _get_z80_physical_register_name(result_location.phy), t->result_var_type,
            t->result_var_type_promoted));
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_hl == YES || arg1_in_bc == YES || arg1_in_a == YES || arg1_in_b == YES || arg1_in_c == YES) {
    fprintf(file_out, "      ; retained arg1 in %s\n", _get_z80_physical_register_name(arg1_location.phy));
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
  else if (arg2_location.kind != LOC_CONST) {
    if (t->arg2_var_type != VARIABLE_TYPE_INT16 && t->arg2_var_type != VARIABLE_TYPE_UINT16 &&
        !((t->arg2_var_type == VARIABLE_TYPE_INT8 || t->arg2_var_type == VARIABLE_TYPE_UINT8) &&
        (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))) {
      fprintf(stderr, "_aaa_generate_asm_shift_left_right_z80_16bit(): Unhandled 8-bit ARG2! Please submit a bug report!\n");
      print_tac(t, NO, stderr);
      return FAILED;
    }
    if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* hl <</>> bc */
  /******************************************************************************************************/

  _shift_hl_by_bc(&arg2_location, op, file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_HL || result_location.kind == LOC_PHY_BC) {
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      ; retained result in %s\n", _get_z80_physical_register_name(result_location.phy));
    return SUCCEEDED;
  }
  if (result_in_b == YES || result_in_c == YES) {
    char result_register = result_in_b == YES ? 'B' : 'C';

    fprintf(file_out, "      LD  %c,L\n", result_register);
    fprintf(file_out, "      ; retained result in %c\n", result_register);
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      RA_TRACE((stderr, "register_allocator: shift_physical_result function=%s op=%s result_phy=%c value_register=L transfer=LD_%c_L store=none status=complete\n",
              function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT",
              result_register, result_register));
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
    if (_store_from_hl(&result_location, ix_offset, file_out) == FAILED)
      return FAILED;
  }
  else {
    /* 8-bit */
    _load_l_into_ix(ix_offset, file_out);
  }

  return SUCCEEDED;
}


static int _bbb_generate_asm_shift_left_right_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op, int word_count) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0;
  int arg1_in_a = NO, arg1_in_c = NO, arg2_in_a = NO;

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

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    RA_TRACE((stderr, "register_allocator: shift_value function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy)));
    RA_TRACE((stderr, "register_allocator: shift_count function=%s kind=%s size=%d phy=%s\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
    RA_TRACE((stderr, "register_allocator: shift_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT",
            _get_z80_location_kind_name(result_location.kind), result_location.size,
            _get_z80_physical_register_name(result_location.phy), t->result_var_type,
            t->result_var_type_promoted));
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_a == YES || arg1_in_c == YES) {
    fprintf(file_out, "      ; retained arg1 in %s\n", _get_z80_physical_register_name(arg1_location.phy));
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
  else if (arg1_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  B,L\n");
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  B,C\n");
  }
  else if (arg1_location.kind == LOC_PHY_B) {
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

  if (arg2_location.kind == LOC_CONST) {
  }
  else if (arg2_in_a == YES) {
    if (_materialize_to_a(&arg2_location, 0, file_out) == FAILED)
      return FAILED;
  }
  else {
    /* 8-bit */
    _load_from_ix_to_a(ix_offset, file_out);
  }

  /******************************************************************************************************/
  /* b <</>> a */
  /******************************************************************************************************/

  _shift_b_by_a(&arg2_location, op, word_count, file_out);

  /******************************************************************************************************/
  /* target address -> ix */
  /******************************************************************************************************/

  if (result_location.kind == LOC_PHY_A) {
    fprintf(file_out, "      LD  A,B\n");
    fprintf(file_out, "      ; retained result in A\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_HL) {
    fprintf(file_out, "      LD  L,B\n");
    if (t->result_var_type == VARIABLE_TYPE_INT16 && (t->arg1_var_type == VARIABLE_TYPE_INT8 ||
                                                      t->arg2_var_type == VARIABLE_TYPE_INT8))
      _sign_extend_l_to_hl(file_out);
    else
      _load_value_to_h(0, file_out);
    fprintf(file_out, "      ; retained result in HL\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  C,B\n");
    _load_value_to_b(0, file_out);
    fprintf(file_out, "      ; retained result in BC\n");
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_B) {
    fprintf(file_out, "      ; retained result in B\n");
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      RA_TRACE((stderr, "register_allocator: shift_physical_result function=%s op=%s result_phy=B value_register=B store=none status=complete\n",
              function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT"));
#endif
    return SUCCEEDED;
  }
  if (result_location.kind == LOC_PHY_C) {
    fprintf(file_out, "      LD  C,B\n");
    fprintf(file_out, "      ; retained result in C\n");
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    if (g_allocator_enabled == YES)
      RA_TRACE((stderr, "register_allocator: shift_physical_result function=%s op=%s result_phy=C value_register=B transfer=LD_C_B store=none status=complete\n",
              function_node->children[1]->label, op == TAC_OP_SHIFT_LEFT ? "SHIFT_LEFT" : "SHIFT_RIGHT"));
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

  _load_b_into_ix(ix_offset, file_out);

  if (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16) {
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

  return SUCCEEDED;
}


static int _generate_asm_shift_left_right_z80(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _bbb_generate_asm_shift_left_right_z80_8bit(t, file_out, function_node, op, NO);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16)) {
    if (t->arg2_type == TAC_ARG_TYPE_CONSTANT &&
        (t->result_var_type == VARIABLE_TYPE_INT8 || t->result_var_type == VARIABLE_TYPE_UINT8) &&
        (t->arg1_var_type == VARIABLE_TYPE_INT8 || t->arg1_var_type == VARIABLE_TYPE_UINT8) &&
        (op == TAC_OP_SHIFT_LEFT || t->arg1_var_type == VARIABLE_TYPE_UINT8))
      return _bbb_generate_asm_shift_left_right_z80_8bit(t, file_out, function_node, op, YES);
    return _aaa_generate_asm_shift_left_right_z80_16bit(t, file_out, function_node, op);
  }

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


static int _generate_asm_mul_div_mod_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op, int reuse_remainder) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0, saved_ix_frame_offset, selected;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  if (reuse_remainder == YES)
    goto store_result;

  if (op == TAC_OP_MUL) {
    if (_multiply_constant_to_hl(t, &arg1_location, &arg2_location, NO, &selected, file_out) == FAILED)
      return FAILED;
    if (selected == YES)
      goto store_result;
  }

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

    RA_TRACE((stderr, "register_allocator: mul_div_mod_arg1 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy)));
  }
#endif

  if (arg1_location.kind == LOC_CONST) {
    /* 16-bit */
    _load_value_to_bc(arg1_location.value, file_out);
  }
  else if (_materialize_array_index_to_bc(&arg1_location, t->arg1_var_type, ix_offset, NO, file_out) == FAILED)
    return FAILED;

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

  saved_ix_frame_offset = g_ix_frame_offset;
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: mul_div_mod_arg2 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
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

  if (op == TAC_OP_MUL)
    _multiply_bc_and_de_to_hl(file_out);

  /******************************************************************************************************/
  /* bc / de -> ca (result), hl (remainder) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV || op == TAC_OP_MOD)
    _divide_bc_by_de_to_ca_hl(file_out);

  /* NOTE!!! */
  _pop_de(file_out);

  g_ix_frame_offset = saved_ix_frame_offset;

store_result:

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: mul_div_mod_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy),
            t->result_var_type, t->result_var_type_promoted));
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


static int _generate_asm_mul_div_mod_z80_8bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op, int reuse_remainder) {

  struct z80_location result_location;
  struct z80_location arg1_location;
  struct z80_location arg2_location;
  int ix_offset = 0, saved_ix_frame_offset, selected;

  /* result */

  if (_resolve_tac_location(t, TAC_USE_RESULT, function_node, &result_location) == FAILED)
    return FAILED;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  if (reuse_remainder == YES)
    goto store_result;

  if (op == TAC_OP_MUL) {
    if (_multiply_constant_to_hl(t, &arg1_location, &arg2_location, YES, &selected, file_out) == FAILED)
      return FAILED;
    if (selected == YES)
      goto store_result;
  }

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

    RA_TRACE((stderr, "register_allocator: mul_div_mod_arg1 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg1_location.kind),
            arg1_location.size, _get_z80_physical_register_name(arg1_location.phy)));
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
    /* Preserve the byte helper's existing uncached operand contract. */
    g_ix_frame_offset = Z80_GLOBAL_OFFSET;
    if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
    g_ix_frame_offset = Z80_GLOBAL_OFFSET;
#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
    _trace_z80_specialized_address_materialization(function_node, "mul_div_mod_8_arg2", "arg2",
                            &arg2_location, RA_ADDRESS_TARGET_IX, ix_offset);
#endif
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> e */
  /******************************************************************************************************/

  /* NOTE!!! */
  _push_de(file_out);

  saved_ix_frame_offset = g_ix_frame_offset;
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: mul_div_mod_arg2 function=%s op=%s kind=%s size=%d phy=%s\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(arg2_location.kind),
            arg2_location.size, _get_z80_physical_register_name(arg2_location.phy)));
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

  if (op == TAC_OP_MUL)
    _multiply_h_and_e_to_hl(file_out);

  /******************************************************************************************************/
  /* h / e -> a (result) and b (reminder) */
  /******************************************************************************************************/

  if (op == TAC_OP_DIV || op == TAC_OP_MOD)
    _divide_h_by_e_to_a_b(file_out);

  /* NOTE!!! */
  _pop_de(file_out);

  g_ix_frame_offset = saved_ix_frame_offset;

store_result:

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    char *function_name;

    function_name = "<unknown>";
    if (function_node != NULL && function_node->children[1] != NULL)
      function_name = function_node->children[1]->label;

    RA_TRACE((stderr, "register_allocator: mul_div_mod_result function=%s op=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
            function_name, _get_z80_mul_div_mod_op_name(op), _get_z80_location_kind_name(result_location.kind),
            result_location.size, _get_z80_physical_register_name(result_location.phy),
            t->result_var_type, t->result_var_type_promoted));
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

  int previous = (int)(t - g_tacs) - 1, reuse_remainder = NO;

  /* Only a memory quotient preserves the divider's remainder without an extra save. */
  if (op == TAC_OP_MOD && t->arg1_type == TAC_ARG_TYPE_LABEL && t->arg2_type == TAC_ARG_TYPE_LABEL &&
      t->arg1_node != NULL && t->arg2_node != NULL &&
      ((t->arg1_node->flags | t->arg2_node->flags) & TREE_NODE_FLAG_GLOBAL) == 0) {
    while (previous >= 0 && g_tacs[previous].function_node == function_node &&
           (g_tacs[previous].op == TAC_OP_DEAD ||
            (g_tacs[previous].op == TAC_OP_CREATE_VARIABLE && g_tacs[previous].result_node != NULL &&
             g_tacs[previous].result_node->value == 0)))
      previous--;
    if (previous >= 0 && g_tacs[previous].function_node == function_node) {
      struct tac *division = &g_tacs[previous];
      struct z80_location quotient_location;

      if (division->op == TAC_OP_DIV && division->arg1_type == t->arg1_type && division->arg2_type == t->arg2_type &&
          division->arg1_node == t->arg1_node && division->arg2_node == t->arg2_node &&
          division->arg1_var_type == t->arg1_var_type && division->arg2_var_type == t->arg2_var_type &&
          division->arg1_var_type_promoted == t->arg1_var_type_promoted &&
          division->arg2_var_type_promoted == t->arg2_var_type_promoted &&
          division->result_node != t->arg1_node && division->result_node != t->arg2_node) {
        if (_resolve_tac_location(division, TAC_USE_RESULT, function_node, &quotient_location) == FAILED)
          return FAILED;
        if (quotient_location.kind == LOC_STACK_LOCAL || quotient_location.kind == LOC_STACK_SPILL) {
          reuse_remainder = YES;
          fprintf(file_out, "      ; reuse remainder from preceding DIV\n");
        }
      }
    }
  }

  /* TODO(register-allocator): retained mul/div/mod operands remain disabled until the input scheduling is migrated. */
  /* 8-bit or 16-bit? */
  if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT8) &&
      (t->arg2_var_type_promoted == VARIABLE_TYPE_INT8 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT8))
    return _generate_asm_mul_div_mod_z80_8bit(t, file_out, function_node, op, reuse_remainder);
  else if ((t->arg1_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg1_var_type_promoted == VARIABLE_TYPE_UINT16) &&
           (t->arg2_var_type_promoted == VARIABLE_TYPE_INT16 || t->arg2_var_type_promoted == VARIABLE_TYPE_UINT16))
    return _generate_asm_mul_div_mod_z80_16bit(t, file_out, function_node, op, reuse_remainder);

  fprintf(stderr, "_generate_asm_mul_div_mod_z80(): 8-bit + 16-bit, this shouldn't happen. Please submit a bug report!\n");

  return FAILED;
}


/* NOTE! because SAS/C on Amiga thinks that this and the next function were called the same
   prefixes "aaa" and "bbb" were added to separate them */


static int _aaa_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_16bit(struct tac *t, FILE *file_out, struct tree_node *function_node, int op) {

  struct z80_location arg1_location, swapped_location;
  struct z80_location arg2_location;
  struct tac swapped;
  int ix_offset = 0;
  int arg1_in_hl = NO, arg2_in_hl = NO;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  /* Reverse the comparison rather than overwriting an incoming BC value. */
  if (arg1_location.kind == LOC_PHY_BC && arg2_location.kind != LOC_PHY_BC) {
    swapped = *t;
    tac_swap_args(&swapped);
    t = &swapped;
    swapped_location = arg1_location;
    arg1_location = arg2_location;
    arg2_location = swapped_location;
    if (op == TAC_OP_JUMP_LT)
      op = TAC_OP_JUMP_GT;
    else if (op == TAC_OP_JUMP_GT)
      op = TAC_OP_JUMP_LT;
    else if (op == TAC_OP_JUMP_LTE)
      op = TAC_OP_JUMP_GTE;
    else if (op == TAC_OP_JUMP_GTE)
      op = TAC_OP_JUMP_LTE;
  }
  arg1_in_hl = arg1_location.kind == LOC_PHY_HL ? YES : NO;
  arg2_in_hl = arg2_location.kind == LOC_PHY_HL ? YES : NO;

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
    fprintf(file_out, "      ; retained arg1 in HL\n");
  }
  else if (arg1_location.kind == LOC_CONST || arg1_location.kind == LOC_PHY_BC) {
  }
  else if (_load_location_address_to_ix(&arg1_location, &ix_offset, file_out) == FAILED)
    return FAILED;

  /******************************************************************************************************/
  /* copy data (arg1) -> hl */
  /******************************************************************************************************/

  if (arg1_in_hl == YES) {
  }
  else if (arg1_location.kind == LOC_PHY_BC) {
    if (_materialize_to_hl(&arg1_location, file_out) == FAILED)
      return FAILED;
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

  if (arg2_location.kind == LOC_CONST && arg2_location.value == 0 &&
      (op == TAC_OP_JUMP_EQ || op == TAC_OP_JUMP_NEQ)) {
    fprintf(file_out, "      LD  A,H\n      OR  L\n");
    if (op == TAC_OP_JUMP_EQ)
      _jump_z_to(t->result_s, file_out);
    else
      _jump_nz_to(t->result_s, file_out);
    return SUCCEEDED;
  }
  if (arg2_in_hl == YES) {
    fprintf(file_out, "      ; retained arg2 in HL\n");
  }
  else {
    ix_offset = 0;

    if (arg2_location.kind == LOC_CONST || arg2_location.kind == LOC_PHY_BC) {
    }
    else if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> bc */
  /******************************************************************************************************/

  if (arg2_in_hl == NO || arg1_in_hl == YES) {
    if (arg2_location.kind == LOC_CONST && arg2_location.value == 0)
      _load_value_to_bc(0, file_out);
    else if (_materialize_array_index_to_bc(&arg2_location, t->arg2_var_type, ix_offset, NO, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* hl = hl - bc */
  /******************************************************************************************************/

  _sub_bc_from_hl(file_out);

  /******************************************************************************************************/
  /* jumps */
  /******************************************************************************************************/

  if (get_max_variable_type_2(t->arg1_var_type_promoted, t->arg2_var_type_promoted) == VARIABLE_TYPE_INT16 &&
      op != TAC_OP_JUMP_EQ && op != TAC_OP_JUMP_NEQ) {
    if (op == TAC_OP_JUMP_GT)
      _jump_z_to("++", file_out);
    else if (op == TAC_OP_JUMP_LTE)
      _jump_z_to(t->result_s, file_out);
    fprintf(file_out, "      JP  PO,+\n      LD  A,H\n");
    _xor_value_to_a(128, file_out);
    fprintf(file_out, "+:\n      JP  %s,%s\n",
        op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_LTE ? "M" : "P", t->result_s);
    if (op == TAC_OP_JUMP_GT)
      fprintf(file_out, "++:\n");
    return SUCCEEDED;
  }

  if (op == TAC_OP_JUMP_EQ)
    _jump_z_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_NEQ)
    _jump_nz_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_LT)
    _jump_c_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_GT) {
    _jump_z_to("+", file_out);
    _jump_nc_to(t->result_s, file_out);
    fprintf(file_out, "+:\n");
  }
  else if (op == TAC_OP_JUMP_LTE) {
    _jump_z_to(t->result_s, file_out);
    _jump_c_to(t->result_s, file_out);
  }
  else if (op == TAC_OP_JUMP_GTE) {
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
  int arg1_in_a = NO, arg1_in_register = NO;
  int arg2_in_a = NO;
  int previous_index = (int)(t - g_tacs) - 1, arg1_from_array = NO;

  /* arg1 */

  if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &arg1_location) == FAILED)
    return FAILED;

  /* arg2 */

  if (_resolve_tac_location(t, TAC_USE_ARG2, function_node, &arg2_location) == FAILED)
    return FAILED;

  arg1_in_a = arg1_location.kind == LOC_PHY_A ? YES : NO;
  arg1_in_register = arg1_location.kind == LOC_PHY_C || arg1_location.kind == LOC_PHY_HL ||
    arg1_location.kind == LOC_PHY_BC ? YES : NO;
  arg2_in_a = arg2_location.kind == LOC_PHY_A ? YES : NO;

  /* The array reader leaves its stored byte in L; other TACs, including labels, are barriers. */
  while (previous_index >= 0 && g_tacs[previous_index].op == TAC_OP_DEAD)
    previous_index--;
  if (g_allocator_enabled == YES && g_allocator_force_all_spill == NO &&
      arg1_location.kind == LOC_STACK_LOCAL && arg2_location.kind == LOC_CONST &&
      previous_index >= 0) {
    struct tac *previous = &g_tacs[previous_index];
    arg1_from_array = previous->op == TAC_OP_ARRAY_READ && previous->function_node == function_node &&
      previous->result_type == TAC_ARG_TYPE_LABEL && previous->result_node != NULL &&
      previous->result_node == t->arg1_node && previous->result_physical_register == Z80_PHY_NONE &&
      (previous->result_var_type == VARIABLE_TYPE_UINT8 || previous->result_var_type == VARIABLE_TYPE_INT8) &&
      (previous->result_var_type_promoted == VARIABLE_TYPE_UINT8 ||
       previous->result_var_type_promoted == VARIABLE_TYPE_INT8) ? YES : NO;
  }

#if defined(DEBUG_PASS_6_REGISTER_ALLOCATOR)
  if (g_allocator_enabled == YES) {
    RA_TRACE((stderr, "register_allocator: compare_8bit function=%s op=%d op_name=%s arg1_kind=%s arg1_phy=%s arg2_kind=%s arg2_phy=%s\n",
            function_node->children[1]->label, op, _get_z80_compare_op_name(op),
            _get_z80_location_kind_name(arg1_location.kind), _get_z80_physical_register_name(arg1_location.phy),
            _get_z80_location_kind_name(arg2_location.kind), _get_z80_physical_register_name(arg2_location.phy)));
  }
#endif

  /* generate asm */

  /******************************************************************************************************/
  /* source address (arg1) -> ix */
  /******************************************************************************************************/

  if (arg1_from_array == YES) {
    fprintf(file_out, "      ; reuse preceding byte array read in L\n");
  }
  else if (arg1_in_a == YES || arg1_in_register == YES) {
    fprintf(file_out, "      ; retained arg1 in %s\n", _get_z80_physical_register_name(arg1_location.phy));
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

    if (arg1_from_array == YES)
      fprintf(file_out, "      LD  A,L\n");
    else if (arg1_in_register == YES) {
      if (_materialize_to_a(&arg1_location, 0, file_out) == FAILED)
        return FAILED;
    }
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

  if (arg2_in_a == YES || arg2_location.kind == LOC_PHY_B) {
    fprintf(file_out, "      ; retained arg2 in %s\n", _get_z80_physical_register_name(arg2_location.phy));
  }
  else {
    ix_offset = 0;

    if (arg2_location.kind == LOC_CONST || arg2_location.kind == LOC_PHY_HL || arg2_location.kind == LOC_PHY_BC) {
    }
    else if (_load_location_address_to_ix(&arg2_location, &ix_offset, file_out) == FAILED)
      return FAILED;
  }

  /******************************************************************************************************/
  /* copy data (arg2) -> b */
  /******************************************************************************************************/

  if (arg2_in_a == YES || arg2_location.kind == LOC_PHY_B ||
      (g_allocator_enabled == YES && g_allocator_force_all_spill == NO && arg2_location.kind == LOC_CONST)) {
  }
  else if (arg2_location.kind == LOC_PHY_HL || arg2_location.kind == LOC_PHY_BC) {
    fprintf(file_out, "      LD  B,%s\n", arg2_location.kind == LOC_PHY_HL ? "L" : "C");
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

  if (g_allocator_enabled == YES && g_allocator_force_all_spill == NO && arg2_location.kind == LOC_CONST)
    _sub_value_from_a(arg2_location.value & 0xff, file_out);
  else
    _sub_b_from_a(file_out);

  /******************************************************************************************************/
  /* jumps */
  /******************************************************************************************************/

  if (get_max_variable_type_2(t->arg1_var_type_promoted, t->arg2_var_type_promoted) == VARIABLE_TYPE_INT8 &&
      op != TAC_OP_JUMP_EQ && op != TAC_OP_JUMP_NEQ) {
    if (op == TAC_OP_JUMP_GT)
      _jump_z_to("++", file_out);
    else if (op == TAC_OP_JUMP_LTE)
      _jump_z_to(t->result_s, file_out);
    fprintf(file_out, "      JP  PO,+\n");
    _xor_value_to_a(128, file_out);
    fprintf(file_out, "+:\n      JP  %s,%s\n",
        op == TAC_OP_JUMP_LT || op == TAC_OP_JUMP_LTE ? "M" : "P", t->result_s);
    if (op == TAC_OP_JUMP_GT)
      fprintf(file_out, "++:\n");
    return SUCCEEDED;
  }

  if (op == TAC_OP_JUMP_EQ)
    _jump_z_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_NEQ)
    _jump_nz_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_LT)
    _jump_c_to(t->result_s, file_out);
  else if (op == TAC_OP_JUMP_GT) {
    _jump_z_to("+", file_out);
    _jump_nc_to(t->result_s, file_out);
    fprintf(file_out, "+:\n");
  }
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

  if (op == TAC_OP_JUMP_GTE && _is_z80_bounded_counter(t, function_node, 255) == YES) {
    struct z80_location counter;
    if (_resolve_tac_location(t, TAC_USE_ARG1, function_node, &counter) == FAILED)
      return FAILED;
    if (counter.kind == LOC_PHY_BC || counter.kind == LOC_STACK_LOCAL || counter.kind == LOC_STACK_SPILL) {
      fprintf(file_out, "      ; bounded word counter comparison; high byte preserved\n");
      return _bbb_generate_asm_jump_eq_lt_gt_neq_lte_gte_z80_8bit(t, file_out, function_node, op);
    }
  }

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
    RA_TRACE((stderr, "register_allocator: target_call_frame_prefix function=%s role=callee_return target=z80 frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
            function_node->children[1]->label, frame_end, return_sp_offset, saved_fp_low_offset, saved_fp_high_offset));
#endif

  /* ret */

  /* bytes -1 and 0 of stack frame contain the return address */
  fprintf(file_out, "      ; bytes -1 and 0 of stack frame contain the return address\n");

  if (g_ix_frame_offset == 0 && return_sp_offset == -1) {
    fprintf(file_out, "      DEC IX\n");
    fprintf(file_out, "      LD  SP,IX\n");
    g_ix_frame_offset = Z80_GLOBAL_OFFSET;
  }
  else {
    _load_value_to_hl(return_sp_offset, file_out);
    _add_de_to_hl(file_out);
    _load_hl_to_sp(file_out);
  }

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
    RA_TRACE((stderr, "register_allocator: target_return_slot_access function=%s role=callee_store size=%d target=z80 low_offset=%d high_offset=%d\n",
            function_node->children[1]->label, return_value_size, return_low_offset, return_high_offset));
    RA_TRACE((stderr, "register_allocator: return_value_source function=%s kind=%s size=%d phy=%s arg_type=%d return_type=%d\n",
            function_node->children[1]->label, _get_z80_location_kind_name(arg1_location.kind), arg1_location.size,
            _get_z80_physical_register_name(arg1_location.phy), t->arg1_var_type, return_var_type));
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

    RA_TRACE((stderr, "register_allocator: target_call_frame_prefix function=%s callee=%s role=caller_setup target=z80 frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
            function_name, t->arg1_node->children[1]->label, frame_end, return_sp_offset,
            saved_fp_low_offset, saved_fp_high_offset));
  }
#endif

  /* sp -> hl (the new stack frame address) */
  _load_value_to_hl(-1, file_out);
  fprintf(file_out, "      ADD HL,SP\n");

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

        RA_TRACE((stderr, "register_allocator: target_argument_transport function=%s callee=%s argument=%d source_type=%d target_type=%d source_kind=%s transport=%s target=z80\n",
                function_name, t->arg1_node->children[1]->label, argument + 1,
                source_var_type, target_var_type, _get_z80_location_kind_name(source_location.kind),
                _get_register_allocator_argument_transport_name(argument_transport)));
        RA_TRACE((stderr, "register_allocator: target_argument_byte_access function=%s callee=%s argument=%d transport=%s source_low=%d source_high=%d target_low=%d target_high=%d target=z80\n",
          function_name, t->arg1_node->children[1]->label, argument + 1,
          _get_register_allocator_argument_transport_name(argument_transport), source_low_offset,
          source_high_offset, target_low_offset, target_high_offset));
      }
#endif

      if (init_ix == NO) {
        /* load the new stack frame address -> ix */
        init_ix = YES;

        fprintf(file_out, "      ; new stack frame -> IX\n");

        _load_value_to_ix(-saved_fp_low_offset, file_out);
        fprintf(file_out, "      ADD IX,SP\n");
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
          if (argument_transport == RA_ARGUMENT_TRANSPORT_WORD)
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

  /* HL is dead at the callee entry; DE becomes the new frame pointer. */
  fprintf(file_out, "      EX  DE,HL\n");
  g_ix_frame_offset = Z80_GLOBAL_OFFSET;

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

      RA_TRACE((stderr, "register_allocator: target_return_slot_access function=%s callee=%s role=caller_load size=%d target=z80 low_offset=%d high_offset=%d\n",
              function_name, t->arg1_node->children[1]->label, return_value_size,
              return_low_offset, return_high_offset));
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

    RA_TRACE((stderr, "register_allocator: target_call_frame_prefix function=%s callee=%s role=caller_restore target=z80 frame_end=%d return_sp=%d saved_fp_low=%d saved_fp_high=%d\n",
            function_name, t->arg1_node->children[1]->label, frame_end, return_sp_offset,
            saved_fp_low_offset, saved_fp_high_offset));
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

            RA_TRACE((stderr, "register_allocator: function_call_return function=%s callee=%s kind=%s size=%d phy=%s var_type=%d promoted_type=%d\n",
              function_name, callee_name, _get_z80_location_kind_name(result_location.kind),
              result_location.size, _get_z80_physical_register_name(result_location.phy),
              t->result_var_type, t->result_var_type_promoted));
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

    if (result_location.kind == LOC_GLOBAL &&
        (t->result_var_type == VARIABLE_TYPE_INT16 || t->result_var_type == VARIABLE_TYPE_UINT16)) {
      fprintf(file_out, "      LD  (%s),HL\n", result_location.label);
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

    RA_TRACE((stderr, "register_allocator: inline_asm_operand function=%s access=%s variable=%s kind=%s offset=%d size=%d policy=stack_location\n",
            function_name, access_name, al->variable->children[1]->label,
            _get_z80_location_kind_name(location->kind), location->offset, location->size));
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
      g_ix_frame_offset = Z80_GLOBAL_OFFSET;
    }
    else if ((al->flags & ASM_LINE_FLAG_READ) == ASM_LINE_FLAG_READ) {
      /* read! */
      if (_generate_asm_inline_asm_read_z80(t, file_out, function_node, al, ia->file_id) == FAILED)
        return FAILED;
      if (strchr(al->cpu_register, 'D') != NULL || strchr(al->cpu_register, 'E') != NULL)
        g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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
  int size, var_type, saved_ix_frame_offset = g_ix_frame_offset;

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

    RA_TRACE((stderr, "register_allocator: array_init_copy function=%s variable=%s kind=%s offset=%d size=%d bytes=%d const_items=%d policy=location_address\n",
            function_name, node->children[1]->label, _get_z80_location_kind_name(target_location.kind),
            target_location.offset, target_location.size, size, items));
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
  /* The linker copy helper preserves IX; POP restores its frame base. */
  g_ix_frame_offset = saved_ix_frame_offset;

  return SUCCEEDED;
}


static int _generate_asm_byte_loop_z80(int *tac_index, FILE *file_out, struct tree_node *function_node) {

  struct tac *sequence[16], *access[2] = {NULL, NULL}, *base[2], *bound, *initial, *exit;
  struct tac *reduction = NULL, *accumulator = NULL;
  struct z80_location locations[3], counter_location, final_location;
  struct temp_register *counter = NULL;
  struct tree_node *nodes[3];
  int indices[16], operands[2], displacements[2] = {0, 0};
  int i, j, k, count = 0, end, scratch = 0, offset, constant_count, reload_count = NO;
  int loop_id, copied = NO;
  char loop_label[48];

  initial = &g_tacs[*tac_index];
  if (initial->op != TAC_OP_ASSIGNMENT || initial->arg1_d != 0.0 ||
      _is_z80_bounded_counter(initial, function_node, 65535) == NO)
    return SUCCEEDED;
  for (i = *tac_index; i < g_tacs_count && count < 16; i++) {
    if (g_tacs[i].function_node != function_node || g_tacs[i].is_function == YES)
      return SUCCEEDED;
    if (g_tacs[i].op == TAC_OP_DEAD)
      continue;
    if (g_tacs[i].store_retained_to_spill_operand != -1 ||
        g_tacs[i].reload_spill_to_physical_operand != -1)
      return SUCCEEDED;
    indices[count] = i;
    sequence[count++] = &g_tacs[i];
    if (g_tacs[i].op == TAC_OP_JUMP)
      break;
  }
  if (count < 7 || sequence[1]->op != TAC_OP_LABEL ||
      sequence[2]->op != TAC_OP_JUMP_GTE || sequence[count - 3]->op != TAC_OP_LABEL ||
      sequence[count - 2]->op != TAC_OP_ADD || sequence[count - 1]->op != TAC_OP_JUMP ||
      strcmp(sequence[count - 1]->result_s, sequence[1]->result_s) != 0)
    return SUCCEEDED;
  bound = sequence[2];
  if (bound->arg2_type == TAC_ARG_TYPE_CONSTANT && bound->arg2_d == 0.0)
    return SUCCEEDED;
  for (end = indices[count - 1] + 1; end < g_tacs_count && g_tacs[end].op == TAC_OP_DEAD; end++)
    ;
  if (end >= g_tacs_count || g_tacs[end].op != TAC_OP_LABEL ||
      g_tacs[end].is_function == YES || g_tacs[end].function_node != function_node ||
      strcmp(bound->result_s, g_tacs[end].result_s) != 0)
    return SUCCEEDED;
  exit = &g_tacs[end];
  for (i = 3; i < count - 3; i++) {
    struct tac *t = sequence[i];
    if (t->op == TAC_OP_ASSIGNMENT) {
      if (t->result_type != TAC_ARG_TYPE_TEMP || t->arg1_type != TAC_ARG_TYPE_LABEL ||
          t->arg1_node == NULL || t->result_var_type != VARIABLE_TYPE_UINT16)
        return SUCCEEDED;
    }
    else if (t->op == TAC_OP_ADD) {
      if (t->result_type == TAC_ARG_TYPE_TEMP && t->arg1_type == initial->result_type &&
          (initial->result_type == TAC_ARG_TYPE_TEMP ? t->arg1_d == initial->result_d : t->arg1_node == initial->result_node) &&
          t->arg2_type == TAC_ARG_TYPE_CONSTANT && t->arg2_d >= -32768.0 &&
          t->arg2_d <= 65535.0 && t->arg2_d == (int)t->arg2_d &&
          t->result_var_type == VARIABLE_TYPE_UINT16)
        ;
      else if (reduction == NULL && t->arg2_type == TAC_ARG_TYPE_TEMP &&
          ((t->result_type == TAC_ARG_TYPE_LABEL && t->arg1_type == TAC_ARG_TYPE_LABEL &&
            t->result_node != NULL && t->result_node == t->arg1_node &&
            get_variable_type_size(t->result_var_type) == 16) ||
           (t->result_type == TAC_ARG_TYPE_TEMP && t->arg1_type == TAC_ARG_TYPE_TEMP &&
            t->result_var_type == VARIABLE_TYPE_UINT8)))
        reduction = t;
      else
        return SUCCEEDED;
    }
    else if (t->op == TAC_OP_ARRAY_READ && accumulator == NULL &&
             t->arg2_type == TAC_ARG_TYPE_CONSTANT && t->arg2_d >= 0.0 &&
             t->arg2_d <= 65535.0 && t->arg2_d == (int)t->arg2_d &&
             t->result_type == TAC_ARG_TYPE_TEMP && t->result_var_type == VARIABLE_TYPE_UINT8)
      accumulator = t;
    else if (t->op == TAC_OP_ARRAY_READ && access[1] == NULL && access[0] == NULL &&
             t->result_type == TAC_ARG_TYPE_TEMP &&
             (t->result_var_type == VARIABLE_TYPE_UINT8 || t->result_var_type == VARIABLE_TYPE_INT8 ||
              t->result_var_type == VARIABLE_TYPE_UINT16 || t->result_var_type == VARIABLE_TYPE_INT16))
      access[1] = t;
    else if (t->op == TAC_OP_ARRAY_WRITE && access[0] == NULL &&
             (t->result_var_type == VARIABLE_TYPE_UINT8 || t->result_var_type == VARIABLE_TYPE_INT8))
      access[0] = t;
    else
      return SUCCEEDED;
    if (t->op != TAC_OP_ARRAY_WRITE && t->result_type == initial->result_type &&
        (initial->result_type == TAC_ARG_TYPE_TEMP ? t->result_d == initial->result_d : t->result_node == initial->result_node))
      return SUCCEEDED;
  }
  if (reduction != NULL) {
    if (access[1] == NULL || reduction->arg2_d != access[1]->result_d ||
        (accumulator == NULL ? access[0] != NULL || reduction->result_type != TAC_ARG_TYPE_LABEL :
         (reduction->arg1_d != accumulator->result_d || accumulator > access[1] ||
          accumulator->arg1_type != TAC_ARG_TYPE_LABEL || access[0] == NULL ||
          access[0]->result_type != TAC_ARG_TYPE_LABEL || access[0]->result_node != accumulator->arg1_node ||
          access[0]->arg2_type != TAC_ARG_TYPE_CONSTANT || access[0]->arg2_d != accumulator->arg2_d ||
          access[0]->arg1_type != TAC_ARG_TYPE_TEMP || access[0]->arg1_d != reduction->result_d)))
      return SUCCEEDED;
    access[0] = access[1];
    access[1] = NULL;
  }
  else {
    if (accumulator != NULL || access[0] == NULL || (access[1] != NULL &&
        (access[0]->arg1_type != TAC_ARG_TYPE_TEMP || access[0]->arg1_d != access[1]->result_d ||
         get_variable_type_size(access[1]->result_var_type) != 8)))
      return SUCCEEDED;
    copied = access[1] != NULL;
  }
  for (i = 0; i <= copied; i++) {
    struct tac *t = access[i];
    int type = i == 0 && reduction == NULL ? t->result_type : t->arg1_type;
    double value = i == 0 && reduction == NULL ? t->result_d : t->arg1_d;

    base[i] = t;
    operands[i] = i == 0 && reduction == NULL ? TAC_USE_RESULT : TAC_USE_ARG1;
    if (type == TAC_ARG_TYPE_TEMP) {
      for (j = 3; j < count - 3; j++) {
        if (sequence[j] >= t)
          break;
        if (sequence[j]->op == TAC_OP_ASSIGNMENT && sequence[j]->result_d == value) {
          base[i] = sequence[j];
          operands[i] = TAC_USE_ARG1;
          break;
        }
      }
      if (base[i] == t)
        return SUCCEEDED;
    }
    else if (type != TAC_ARG_TYPE_LABEL)
      return SUCCEEDED;
    if (t->arg2_var_type != VARIABLE_TYPE_UINT16 ||
        t->arg2_var_type_promoted != VARIABLE_TYPE_UINT16)
      return SUCCEEDED;
    if (t->arg2_type != initial->result_type ||
        (initial->result_type == TAC_ARG_TYPE_TEMP ? t->arg2_d != initial->result_d : t->arg2_node != initial->result_node)) {
      if (t->arg2_type != TAC_ARG_TYPE_TEMP)
        return SUCCEEDED;
      for (j = 3; j < count - 3; j++) {
        if (sequence[j] >= t)
          break;
        if (sequence[j]->op == TAC_OP_ADD && sequence[j]->result_d == t->arg2_d)
          break;
      }
      if (j == count - 3 || sequence[j] >= t)
        return SUCCEEDED;
      displacements[i] = (int)sequence[j]->arg2_d;
    }
  }
  nodes[0] = bound->arg2_node;
  nodes[1] = operands[0] == TAC_USE_RESULT ? base[0]->result_node : base[0]->arg1_node;
  nodes[2] = reduction != NULL ? (accumulator == NULL ? reduction->result_node : accumulator->arg1_node) :
    copied == YES ? base[1]->arg1_node : access[0]->arg1_node;
  if (nodes[1] == NULL || (copied == YES && nodes[2] == NULL) ||
      (reduction == NULL && copied == NO && access[0]->arg1_type == TAC_ARG_TYPE_LABEL && nodes[2] == NULL) ||
      (reduction == NULL && copied == NO && access[0]->arg1_type != TAC_ARG_TYPE_LABEL &&
       access[0]->arg1_type != TAC_ARG_TYPE_CONSTANT) ||
      (reduction != NULL && (nodes[2] == NULL || nodes[0] == nodes[2])))
    return SUCCEEDED;
  for (i = 0; i < 3; i++) {
    struct tree_node *node = nodes[i];
    if (node == NULL)
      continue;
    if ((node->type != TREE_NODE_TYPE_CREATE_VARIABLE &&
         node->type != TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT) ||
        ((node->flags & (TREE_NODE_FLAG_GLOBAL | TREE_NODE_FLAG_STATIC)) != 0 && node->value == 0))
      return SUCCEEDED;
    if (i == 1 || (i == 2 && (copied == YES || accumulator != NULL))) {
      if (strcmp(node->children[1]->label, "__z80_in") == 0 ||
          strcmp(node->children[1]->label, "__z80_out") == 0 ||
          (node->value == 0 && node->children[0]->value_double != 1.0) ||
          (node->children[0]->value != VARIABLE_TYPE_UINT8 && node->children[0]->value != VARIABLE_TYPE_INT8) ||
          (reduction != NULL && node->children[0]->value != VARIABLE_TYPE_UINT8))
        return SUCCEEDED;
    }
    else if (node->value != 0 || node->children[0]->value_double != 0.0)
      return SUCCEEDED;
  }
  for (i = 0; i < function_node->local_variables->temp_registers_count; i++) {
    struct temp_register *temp = &function_node->local_variables->temp_registers[i];
    if (initial->result_type == TAC_ARG_TYPE_TEMP && temp->register_index == (int)initial->result_d)
      counter = temp;
    else if (temp->live_start < *tac_index && temp->live_end >= *tac_index)
      return SUCCEEDED;
    for (j = 3; j < count - 3; j++) {
      if (sequence[j]->op == TAC_OP_ARRAY_WRITE || sequence[j]->result_type != TAC_ARG_TYPE_TEMP ||
          sequence[j]->result_d != temp->register_index)
        continue;
      if (temp->live_start < *tac_index || temp->live_end >= end || temp->write_count != 1)
        return SUCCEEDED;
      if (temp->size == 16 && temp->spill_required == YES &&
          temp->offset_to_fp >= -128 && temp->offset_to_fp <= 126)
        scratch = temp->offset_to_fp;
    }
  }
  if (initial->result_type == TAC_ARG_TYPE_TEMP &&
      (counter == NULL || counter->live_start != *tac_index || counter->write_count != 2))
    return SUCCEEDED;
  if (counter != NULL && counter->spill_required == YES && counter->offset_to_fp >= -128 && counter->offset_to_fp <= 126)
    scratch = counter->offset_to_fp;
  if (copied == YES && scratch == 0)
    return SUCCEEDED;
  for (i = 0; i < g_tacs_count; i++) {
    struct tac *t = &g_tacs[i];
    if (t->function_node != function_node || t->op == TAC_OP_DEAD)
      continue;
    if (t->op == TAC_OP_ASM)
      return SUCCEEDED;
    if (t->op >= TAC_OP_JUMP && t->op <= TAC_OP_JUMP_GTE &&
        i != indices[2] && i != indices[count - 1] &&
        (strcmp(t->result_s, sequence[1]->result_s) == 0 ||
         strcmp(t->result_s, sequence[count - 3]->result_s) == 0 ||
         strcmp(t->result_s, exit->result_s) == 0))
      return SUCCEEDED;
    for (j = 0; j < 3; j++) {
      if (nodes[j] != NULL && nodes[j]->value == 0 &&
          (t->op == TAC_OP_GET_ADDRESS || t->op == TAC_OP_GET_ADDRESS_ARRAY) &&
          t->arg1_node == nodes[j])
        return SUCCEEDED;
    }
  }
  if (_resolve_tac_location(bound, TAC_USE_ARG2, function_node, &locations[0]) == FAILED ||
      _resolve_tac_location(base[0], operands[0], function_node, &locations[1]) == FAILED ||
      _resolve_tac_location(reduction != NULL ? (accumulator == NULL ? reduction : accumulator) :
        copied == YES ? base[1] : access[0], reduction != NULL ?
        (accumulator == NULL ? TAC_USE_RESULT : TAC_USE_ARG1) : copied == YES ?
        operands[1] : TAC_USE_ARG1, function_node, &locations[2]) == FAILED ||
      _resolve_tac_location(initial, TAC_USE_RESULT, function_node, &counter_location) == FAILED ||
      _resolve_tac_location(sequence[count - 2], TAC_USE_RESULT, function_node, &final_location) == FAILED)
    return FAILED;
  if (counter_location.kind != final_location.kind || counter_location.phy != final_location.phy ||
      counter_location.offset != final_location.offset)
    return SUCCEEDED;
  for (i = 0; i < 3; i++) {
    if (locations[i].kind != LOC_CONST && locations[i].kind != LOC_STACK_LOCAL &&
        !(i > 0 && nodes[i]->value > 0 && locations[i].kind == LOC_GLOBAL))
      return SUCCEEDED;
    if (locations[i].kind == LOC_STACK_LOCAL && nodes[i]->value == 0 &&
        (locations[i].offset < -128 || locations[i].offset + locations[i].size > 128))
      return SUCCEEDED;
  }
  if (reduction != NULL && accumulator == NULL &&
      (locations[2].kind != LOC_STACK_LOCAL || locations[2].size != 2))
    return SUCCEEDED;

  constant_count = locations[0].kind == LOC_CONST ? locations[0].value : 0;
  loop_id = g_return_id++;
  snprintf(loop_label, sizeof(loop_label), "_byte_loop_%d", loop_id);
  fprintf(file_out, "      ; advancing byte %s loop\n",
    reduction != NULL ? (accumulator == NULL ? "sum" : "sum_store") : copied == YES ? "copy" : "fill");
  _load_de_with_offset_to_ix(0, file_out);
  if (constant_count == 0) {
    if (_materialize_array_index_to_bc(&locations[0], VARIABLE_TYPE_UINT16, locations[0].offset, NO, file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      LD  A,B\n      OR  C\n");
    fprintf(file_out, "      JP  Z,%s\n", exit->result_s);
  }
  if (copied == YES)
    fprintf(file_out, "      LD  (IX%+d),E\n      LD  (IX%+d),D\n", scratch, scratch + 1);
  if (accumulator != NULL) {
    if (nodes[2]->value == 0) {
      _load_from_ix_to_l(locations[2].offset, file_out);
      _load_from_ix_to_h(locations[2].offset + 1, file_out);
    }
    else if (_load_location_address_to_hl(&locations[2], file_out) == FAILED)
      return FAILED;
    fprintf(file_out, "      EX  DE,HL\n");
    _load_value_to_iy((int)accumulator->arg2_d, file_out);
    _add_de_to_iy(file_out);
    fprintf(file_out, "      EX  DE,HL\n");
  }
  for (i = 0; i <= copied; i++) {
    struct z80_location *location = &locations[i + 1];
    offset = displacements[i];
    if (nodes[i + 1]->value == 0) {
      _load_from_ix_to_l(location->offset, file_out);
      _load_from_ix_to_h(location->offset + 1, file_out);
    }
    else if (i == 1 && location->kind == LOC_STACK_LOCAL)
      offset += location->offset; /* EX DE,HL left the frame base in HL. */
    else if (_load_location_address_to_hl(location, file_out) == FAILED)
      return FAILED;
    if (offset != 0) {
      _load_value_to_bc(offset, file_out);
      _add_bc_to_hl(file_out);
      reload_count = YES;
    }
    if (copied == YES && i == 0) {
      fprintf(file_out, "      EX  DE,HL\n");
      g_ix_frame_offset = Z80_GLOBAL_OFFSET;
    }
  }
  if (copied == YES || constant_count > 256 || constant_count == 0) {
    if (constant_count != 0 || reload_count == YES) {
      if (_materialize_array_index_to_bc(&locations[0], VARIABLE_TYPE_UINT16, locations[0].offset, NO, file_out) == FAILED)
        return FAILED;
    }
  }
  if (reduction != NULL) {
    offset = locations[2].offset;
    if (accumulator == NULL) {
      fprintf(file_out, "      EX  DE,HL\n");
      _load_value_to_iy(0, file_out);
      _add_de_to_iy(file_out);
      fprintf(file_out, "      EX  DE,HL\n");
      _load_from_ix_to_l(offset, file_out);
      _load_from_ix_to_h(offset + 1, file_out);
    }
    if (constant_count > 0 && constant_count <= 256)
      _load_value_to_b(constant_count & 255, file_out);
    _add_label(loop_label, file_out, NO);
    if (accumulator == NULL)
      fprintf(file_out, "      LD  A,(IY+0)\n      ADD A,L\n      LD  L,A\n      JR  NC,+\n      INC H\n+:\n");
    else
      fprintf(file_out, "      LD  A,(IY+0)\n      ADD A,(HL)\n      LD  (IY+0),A\n");
    fprintf(file_out, "      INC %s\n", accumulator == NULL ? "IY" : "HL");
    if (constant_count > 0 && constant_count <= 256)
      fprintf(file_out, "      DJNZ %s\n", loop_label);
    else
      fprintf(file_out, "      DEC BC\n      LD  A,B\n      OR  C\n      JP  NZ,%s\n", loop_label);
    if (accumulator == NULL) {
      fprintf(file_out, "      LD  (IX%+d),L\n      LD  (IX%+d),H\n", offset, offset + 1);
    }
  }
  else if (copied == YES) {
    if (constant_count == 1 || constant_count == 2) {
      if (constant_count == 2)
        fprintf(file_out, "      LDI\n");
      fprintf(file_out, "      LD  A,(HL)\n      LD  (DE),A\n");
    }
    else
      fprintf(file_out, "      LDIR\n");
    _load_from_ix_to_e(scratch, file_out);
    _load_from_ix_to_d(scratch + 1, file_out);
  }
  else {
    if (constant_count > 3 && constant_count <= 256)
      _load_value_to_b(constant_count & 255, file_out);
    else if (constant_count == 0 || constant_count > 256) {
      fprintf(file_out, "      LD  A,C\n      OR  A\n      JR  Z,+\n      INC B\n");
      _add_label("+", file_out, YES);
    }
    if (locations[2].kind == LOC_CONST)
      _load_value_to_a(locations[2].value & 255, file_out);
    else
      _load_from_ix_to_a(locations[2].offset, file_out);
    if (constant_count > 0 && constant_count <= 3) {
      for (k = 0; k < constant_count; k++) {
        fprintf(file_out, "      LD  (HL),A\n");
        if (k + 1 < constant_count)
          fprintf(file_out, "      INC HL\n");
      }
    }
    else {
      _add_label(loop_label, file_out, NO);
      fprintf(file_out, "      LD  (HL),A\n      INC HL\n");
      if (constant_count == 0 || constant_count > 256)
        fprintf(file_out, "      DEC C\n      JR  NZ,%s\n", loop_label);
      fprintf(file_out, "      DJNZ %s\n", loop_label);
    }
  }
  _add_label(exit->result_s, file_out, NO);
  /* Both the zero-count edge and the loop preserve IX = the restored DE. */
  g_ix_frame_offset = 0;
  if (counter == NULL || counter->live_end > indices[count - 1]) {
    struct tac update = *initial;
    update.arg1_type = bound->arg2_type;
    update.arg1_d = bound->arg2_d;
    update.arg1_s = bound->arg2_s;
    update.arg1_node = bound->arg2_node;
    update.arg1_var_type = bound->arg2_var_type;
    update.arg1_var_type_promoted = bound->arg2_var_type_promoted;
    update.arg1_physical_register = bound->arg2_physical_register;
    if (_generate_asm_assignment_z80(&update, file_out, function_node) == FAILED)
      return FAILED;
  }
  RA_TRACE((stderr, "register_allocator: byte_loop function=%s kind=%s count=%s frame_save=%d instructions=%d target=z80 status=complete\n",
    function_node->children[1]->label, reduction != NULL ? (accumulator == NULL ? "sum" : "sum_store") : copied == YES ? "copy" : "fill",
    constant_count == 0 ? "word" : "constant", copied == YES ? scratch : 0, count));
  *tac_index = end;
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
          int frame_offset = function_node->local_variables->offset_to_fp_total + 1;
          int bytes = get_register_allocator_call_frame_prefix_value(RA_CALL_FRAME_SAVED_FP_LOW_OFFSET) - frame_offset;
          int k;

          fprintf(file_out, "      ; allocate space for the stack frame bytes=%d\n", -frame_offset);
          /* DEC SP costs 1 byte/6 T-states versus 5/27 for full setup. */
          if (bytes >= 0 && bytes <= 4) {
            for (k = 0; k < bytes; k++)
              fprintf(file_out, "      DEC SP\n");
          }
          else {
            _load_value_to_hl(frame_offset, file_out);
            _add_de_to_hl(file_out);
            _load_hl_to_sp(file_out);
          }
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
          g_ix_frame_offset = Z80_GLOBAL_OFFSET;
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

        if (op == TAC_OP_ASSIGNMENT) {
          int previous_index = i;
          if (_generate_asm_byte_loop_z80(&i, file_out, function_node) == FAILED)
            return FAILED;
          if (i != previous_index)
            continue;
        }
        if (op == TAC_OP_ADD || op == TAC_OP_SUB || op == TAC_OP_AND || op == TAC_OP_OR || op == TAC_OP_XOR) {
          int is_reload;
          for (is_reload = NO; is_reload <= YES; is_reload++) {
            struct z80_location location;
            int operand = is_reload == YES ? t->reload_spill_to_physical_operand : t->store_retained_to_spill_operand;
            if (operand != -1 && (_resolve_tac_location(t, operand, function_node, &location) == FAILED ||
                _transfer_array_base_spill(t, function_node, &location, is_reload, file_out) == FAILED))
              return FAILED;
          }
        }
        if (op == TAC_OP_LABEL && t->is_function == NO)
          fprintf(file_out, "    %s:\n", t->result_s);
        else if (op == TAC_OP_LABEL)
          fprintf(file_out, "    %s:\n", t->result_s);
        else if (op == TAC_OP_ASSIGNMENT) {
          if (_generate_asm_assignment_z80(t, file_out, function_node) == FAILED)
            return FAILED;
        }
        else if (op == TAC_OP_ADD || op == TAC_OP_SUB || op == TAC_OP_OR || op == TAC_OP_AND || op == TAC_OP_XOR) {
          if (_generate_asm_add_sub_or_xor_and_z80(t, file_out, function_node, op, &i) == FAILED)
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
          g_ix_frame_offset = (g_allocator_enabled == YES && g_allocator_force_all_spill == NO) || t->result_d == 0 ?
            (int)t->result_d : Z80_GLOBAL_OFFSET;
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
