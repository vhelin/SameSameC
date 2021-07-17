
/*
  bilibali-linker - by ville helin <ville.helin@iki.fi>. this is gpl software.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "defines.h"
#include "printf.h"
#include "file.h"
#include "arch_z80.h"


/* define this if you want to keep all temp files */

#define KEEP_TMP_FILES 1


extern struct file *g_files_first;

char g_version_string[] = "$VER: bilibali-linker 1.0a (28.6.2021)";
char g_bilibali_version[] = "1.0";

int g_verbose = NO, g_quiet = NO, g_target = TARGET_NONE, g_final_name_index = -1;
char g_asm_file_name[MAX_NAME_LENGTH+1], g_object_file_name[MAX_NAME_LENGTH+1], g_link_file_name[MAX_NAME_LENGTH+1];



int main(int argc, char *argv[]) {

  int parse_flags_result, i, command_result = 0;
  char command[MAX_NAME_LENGTH+1];
  FILE *file_out;
  
  if (sizeof(double) != 8) {
    fprintf(stderr, "MAIN: sizeof(double) == %d != 8. BILIBALI-LINKER will not work properly.\n", (int)sizeof(double));
    return 1;
  }

  atexit(procedures_at_exit);

  /* init tmp file names */
  g_asm_file_name[0] = 0;
  g_object_file_name[0] = 0;
  g_link_file_name[0] = 0;
  
  parse_flags_result = FAILED;
  if (argc >= 5)
    parse_flags_result = parse_flags(argv, argc);
  
  if (argc < 5 || parse_flags_result == FAILED || g_target == TARGET_NONE || g_final_name_index < 0 || g_final_name_index == argc) {
    printf("\nBILIBALI Linker v1.0a. Written by Ville Helin in 2021+\n");
#ifdef BILIBALI_DEBUG
    printf("*** BILIBALI_DEBUG defined - this executable is running in DEBUG mode ***\n");
#endif
    printf("%s\n\n", g_version_string);
    printf("USAGE: %s <ARCHITECTURE> [OPTIONS] -o <ROM/PRG FILE> <ASM FILES>\n\n", argv[0]);
    printf("Options:\n");
    printf("-q       Quiet\n");
    printf("-v       Verbose messages\n\n");
    printf("Achitectures:\n");
    printf("-lsms    Link a SMS ROM\n\n");
    printf("EXAMPLE: %s -lsms -v -o game.rom main.asm menu.asm music.asm\n\n", argv[0]);
    return 0;
  }

  /* load the ASM files */
  for (i = g_final_name_index + 1; i < argc; i++) {
    if (file_load(argv[i]) == FAILED)
      return 1;
  }

  /* check that one of the files has "void main(void)" function */
  if (file_has_main() == FAILED) {
    fprintf(stderr, "MAIN: The ASM files don't contain \"main\" function. One of your source files must contain function \"void main(void)\"!\n");
    return 1;
  }

  /* find all functions that initialize global variables */
  if (file_find_global_variable_inits() == FAILED)
    return 1;
  
  /* create an ASM file that will contain all the user made ASM files */

  tmpnam(g_asm_file_name);

#if defined(KEEP_TMP_FILES)
  fprintf(stderr, "ASM ---> %s\n", g_asm_file_name);
#endif
  
  file_out = fopen(g_asm_file_name, "wb");
  if (file_out == NULL) {
    fprintf(stderr, "MAIN: Could not open file \"%s\" for writing.\n", g_asm_file_name);
    return 1;
  }

  /* write system initializing ASM code */
  if (g_target == TARGET_SMS) {
    if (arch_z80_write_system_init(g_target, file_out) == FAILED) {
      fclose(file_out);
      return 1;
    }
  }
  
  /* find all copy_bytes_bank_??? calls */
  if (file_find_copy_bytes_calls() == FAILED) {
    fclose(file_out);
    return 1;
  }
  
  /* ... and create the functions */
  if (g_target == TARGET_SMS) {
    if (arch_z80_create_copy_bytes_functions() == FAILED) {
      fclose(file_out);
      return 1;
    }
  }
  
  /* write all ASM files into the one ASM file */
  if (file_write_all_asm_files_into_tmp_file(file_out) == FAILED) {
    fclose(file_out);
    return 1;
  }

  fclose(file_out);

  /* assemble */

  tmpnam(g_object_file_name);

#if defined(KEEP_TMP_FILES)
  fprintf(stderr, "OBJ ---> %s\n", g_object_file_name);
#endif
  
  if (g_target == TARGET_SMS) {
    snprintf(command, sizeof(command), "wla-z80 -o %s %s", g_object_file_name, g_asm_file_name);
    command_result = system(command);
  }

  if (command_result != 0) {
    fprintf(stderr, "MAIN: Could not issue command \"%s\"! Make sure you have WLA DX v10.0+ in the path...\n", command);
    return 1;
  }

  /* create linkfile for WLALINK */

  tmpnam(g_link_file_name);

#if defined(KEEP_TMP_FILES)
  fprintf(stderr, "LNK ---> %s\n", g_link_file_name);
#endif
  
  file_out = fopen(g_link_file_name, "wb");
  if (file_out == NULL) {
    fprintf(stderr, "MAIN: Could not open file \"%s\" for writing.\n", g_link_file_name);
    return 1;
  }

  /* write a link file that will place all the sections in correct banks and slots */
  if (g_target == TARGET_SMS) {
    if (arch_z80_write_link_file(g_target, g_object_file_name, file_out) == FAILED) {
      fclose(file_out);
      return 1;
    }
  }

  fclose(file_out);

  /* link */

  if (g_target == TARGET_SMS) {
    snprintf(command, sizeof(command), "wlalink -v %s %s", g_link_file_name, argv[g_final_name_index]);
    command_result = system(command);
  }

  if (command_result != 0) {
    fprintf(stderr, "MAIN: Could not issue command \"%s\"! Make sure you have WLA DX v10.0+ in the path...\n", command);
    return 1;
  }  
  
  return 0;
}


int parse_flags(char **flags, int flagc) {

  int count;
  
  for (count = 1; count < flagc; count++) {
    if (strcmp(flags[count], "-o") == 0) {
      if (count + 1 < flagc) {
        /* set output */
        g_final_name_index = count + 1;
        return SUCCEEDED;
      }
      else
        return FAILED;
    }
    else if (!strcmp(flags[count], "-v")) {
      g_verbose = YES;
      continue;
    }
    else if (!strcmp(flags[count], "-q")) {
      g_quiet = YES;
      continue;
    }
    else if (!strcmp(flags[count], "-lsms")) {
      g_target = TARGET_SMS;
      continue;
    }    
    else
      return FAILED;
  }
  
  return SUCCEEDED;
}


void procedures_at_exit(void) {

  struct file *f1, *f2;
  
  /* free all the dynamically allocated data structures and close open files */

  f1 = g_files_first;
  while (f1 != NULL) {
    f2 = f1->next;
    free(f1->data);
    free(f1);
    f1 = f2;
  }

#if !defined(KEEP_TMP_FILES)
  /* delete tmp files */
  if (g_asm_file_name[0] != 0)
    remove(g_asm_file_name);
  if (g_object_file_name[0] != 0)
    remove(g_object_file_name);
  if (g_link_file_name[0] != 0)
    remove(g_link_file_name);
#endif
}
