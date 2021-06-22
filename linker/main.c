
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
#include "file.h"


extern struct file *g_files_first;

char g_version_string[] = "$VER: bilibali-linker 1.0a (22.6.2021)";
char g_bilibali_version[] = "1.0";

int g_verbose = NO, g_quiet = NO, g_target = TARGET_NONE, g_final_name_index = -1;



int main(int argc, char *argv[]) {

  int parse_flags_result, i;
  FILE *file_out;
  
  if (sizeof(double) != 8) {
    fprintf(stderr, "MAIN: sizeof(double) == %d != 8. BILIBALI-LINKER will not work properly.\n", (int)sizeof(double));
    return -1;
  }

  atexit(procedures_at_exit);

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
    printf("-lSMS    Link a SMS ROM\n\n");
    printf("EXAMPLE: %s -lSMS -v -o game.rom main.asm menu.asm music.asm\n\n", argv[0]);
    return 0;
  }

  /* load the ASM files */
  for (i = g_final_name_index + 1; i < argc; i++) {
    if (file_load(argv[i]) == FAILED)
      return 1;
  }

  if (file_has_main() == FAILED) {
    fprintf(stderr, "MAIN: The ASM files don't contain \"main\" function. One of your source files must contain function \"void main(void)\"!\n");
    return 1;
  }
  

  file_out = fopen(argv[g_final_name_index], "wb");

  fclose(file_out);
  
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
    else if (!strcmp(flags[count], "-lSMS")) {
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
    free(f1);
    f1 = f2;
  }
}
