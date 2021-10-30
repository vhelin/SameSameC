
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "file.h"
#include "printf.h"


extern struct file *g_files_first, *g_files_last;
extern unsigned char g_copy_bytes_bank[256];


int arch_z80_write_system_init(int target, FILE *file_out) {

  int global_variables_init_sections = 0;
  struct file *f;

  f = g_files_first;
  while (f != NULL) {
    if (f->global_variables_init_function[0] != 0)
      global_variables_init_sections++;
    f = f->next;
  }

  /* write .MEMORYMAP & .ROMBANKMAP */

  if (target == TARGET_SMS) {
    fprintf(file_out, "\n");
    fprintf(file_out, "  .MEMORYMAP\n");
    fprintf(file_out, "    DEFAULTSLOT 0\n");
    fprintf(file_out, "    ; ROM\n");
    fprintf(file_out, "    SLOT 0 START $0000 SIZE $4000 NAME \"ROMSlot1\"\n");
    fprintf(file_out, "    SLOT 1 START $4000 SIZE $4000 NAME \"ROMSlot2\"\n");
    fprintf(file_out, "    SLOT 2 START $8000 SIZE $4000 NAME \"ROMSlot3\"\n");
    fprintf(file_out, "    ; RAM\n");
    fprintf(file_out, "    SLOT 3 START $C000 SIZE $2000 NAME \"RAMSlot1\"\n");
    fprintf(file_out, "  .ENDME\n");
    fprintf(file_out, "\n");
    
    fprintf(file_out, "  .ROMBANKMAP\n");
    fprintf(file_out, "    BANKSTOTAL 1\n");
    fprintf(file_out, "    BANKSIZE $4000\n");
    fprintf(file_out, "    BANKS 1\n");
    fprintf(file_out, "  .ENDRO\n");
    fprintf(file_out, "\n");
    
    fprintf(file_out, "  .SMSTAG\n");
    fprintf(file_out, "\n");

    fprintf(file_out, "  .BANK 0 SLOT \"ROMSlot1\"\n");
    fprintf(file_out, "  .ORG $0000\n");
    fprintf(file_out, "\n");

    fprintf(file_out, "    DI\n");
    fprintf(file_out, "    IM  1\n");
    fprintf(file_out, "    JP  _init_sms\n");
    fprintf(file_out, "\n");

    fprintf(file_out, "  .ORG $0080\n");
    fprintf(file_out, "\n");
    
    fprintf(file_out, "  _init_table:\n");
    fprintf(file_out, "    .DB 0, 0, 1, 2\n");
    fprintf(file_out, "\n");

    fprintf(file_out, "  _init_sms:\n");
    fprintf(file_out, "    ; map the first 48KB of ROM to $0000-$BFFF\n");
    fprintf(file_out, "    LD  DE,$FFFC\n");
    fprintf(file_out, "    LD  HL,_init_table\n");
    fprintf(file_out, "    LD  BC,$0004\n");
    fprintf(file_out, "    LDIR\n");
    fprintf(file_out, "\n");
    fprintf(file_out, "    ; init SP\n");
    fprintf(file_out, "    LD  SP,$DFF0\n");
    fprintf(file_out, "\n");

    if (global_variables_init_sections > 0) {
      fprintf(file_out, "    ; initialize global variables\n");

      f = g_files_first;
      while (f != NULL) {
        if (f->global_variables_init_function[0] != 0) {
          fprintf(file_out, "    CALL %s\n", f->global_variables_init_function);
        }
        f = f->next;
      }
  
      fprintf(file_out, "\n");
    }

    fprintf(file_out, "    ; init DE (frame pointer)\n");
    fprintf(file_out, "    LD  DE,$DFF0\n");
    fprintf(file_out, "\n");

    fprintf(file_out, "    ; jump to mainmain() (that jumps to main())\n");
    fprintf(file_out, "    JP  mainmain\n");
  }
  
  return SUCCEEDED;
}


int arch_z80_create_copy_bytes_functions(void) {

  struct file *f;

  f = g_files_first;
  while (f != NULL) {
    if (f->bank >= 0 && g_copy_bytes_bank[f->bank] == YES) {
      /* we only need one of these functions per bank */
      g_copy_bytes_bank[f->bank] = NO;

      /* create the copy function */
      snprintf(f->data_copy_bytes, sizeof(f->data_copy_bytes),
               "  .SECTION \"copy_bytes_bank_%.3d\" FREE\n" \
               "    copy_bytes_bank_%.3d:\n" \
               "      LD  A,(DE)\n" \
               "      LD  (HL),A\n" \
               "      INC DE\n" \
               "      INC HL\n" \
               "      DEC BC\n" \
               "      LD  A,B\n" \
               "      OR  A,C\n" \
               "      JR  NZ,copy_bytes_bank_%.3d\n" \
               "      RET\n" \
               "  .ENDS\n", f->bank, f->bank, f->bank);

      f->data_copy_bytes_size = strlen(f->data_copy_bytes);
    }
    
    f = f->next;
  }

  return SUCCEEDED;
}
