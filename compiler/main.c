
/*
  bilibali-compiler - by ville helin <ville.helin@iki.fi>. this is gpl software.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "defines.h"
#include "printf.h"
#include "parse.h"
#include "include_file.h"
#include "source_line_manager.h"
#include "definition.h"
#include "symbol_table.h"
#include "inline_asm.h"
#include "token.h"
#include "tac.h"
#include "pass_1.h"
#include "pass_2.h"
#include "pass_3.h"
#include "pass_4.h"
#include "pass_5.h"
#include "pass_6_z80.h"


/* amiga specific definitions */
#ifdef AMIGA
__near long __stack = 200000;
#endif

char g_version_string[] = "$VER: bilibali-compiler 1.0a (28.6.2021)";
char g_bilibali_version[] = "1.0";

char *g_tmp_name = NULL;

extern struct incbin_file_data *g_incbin_file_data_first, *g_ifd_tmp;
extern struct file_name_info *g_file_name_info_first;
extern struct map_t *g_global_unique_label_map;
extern struct macro_static *g_macros_first;
extern struct map_t *g_defines_map;
extern struct export_def *g_export_first, *g_export_last;
extern struct stack *g_stacks_first, *g_stacks_tmp, *g_stacks_last, *g_stacks_header_first, *g_stacks_header_last;
extern struct map_t *g_namespace_map;
extern struct append_section *g_append_sections;
extern struct block_name *g_block_names;
extern struct stringmaptable *g_stringmaptables;
extern struct active_file_info *g_active_file_info_first, *g_active_file_info_last, *g_active_file_info_tmp;
extern struct token *g_token_first, *g_token_last;
extern struct definition *g_definitions_first;
extern struct inline_asm *g_inline_asm_first, *g_inline_asm_last;
extern char *g_include_in_tmp, *g_tmp_a;
extern char *g_include_dir, *g_buffer, *g_full_name;
extern int g_include_in_tmp_size, g_tmp_a_size, g_newline_beginning;
extern int g_current_filename_id, g_current_line_number;
extern int *g_register_reads, *g_register_writes;

extern struct tree_node *g_open_expression[256];

extern struct tac *g_tacs;
extern int g_tacs_count;

int g_line_count_status = ON;
int g_verbose_mode = OFF, g_test_mode = OFF;
int g_extra_definitions = OFF, g_commandline_parsing = ON, g_makefile_rules = NO;
int g_listfile_data = NO, g_quiet = NO, g_use_incdir = NO;
int g_create_sizeof_definitions = YES;
int g_output = OUTPUT_NONE, g_backend = BACKEND_NONE;

char *g_final_name = NULL, *g_asm_name = NULL;
char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

struct definition *g_tmp_def = NULL;
struct ext_include_collection g_ext_incdirs;
struct label_def *g_label_tmp = NULL, *g_labels = NULL;



static int _get_plain_filename(char *output, int output_size, char *input) {

  int length, i, start, j;

  length = strlen(input);

  /* find the starting point */
  start = 0;
  for (i = 0; i < length; i++) {
    if (input[i] == '/' || input[i] == '\\')
      start = i + 1;
  }

  for (j = 0, i = start; j < output_size && i < length; i++) {
    if (input[i] == '.')
      break;
    output[j++] = input[i];
  }

  if (j == output_size) {
    fprintf(stderr, "_get_plain_filename(): Output buffer is too small. Please submit a bug report!\n");
    return FAILED;
  }

  output[j] = 0;

  return SUCCEEDED;
}


int main(int argc, char *argv[]) {

  int parse_flags_result, include_size = 0;
  char final_name_no_extension[MAX_NAME_LENGTH + 1];
  FILE *file_out;
  
  if (sizeof(double) != 8) {
    fprintf(stderr, "MAIN: sizeof(double) == %d != 8. BILIBALI-COMPILER will not work properly.\n", (int)sizeof(double));
    return 1;
  }

  atexit(procedures_at_exit);

  /* initialize our external include dir collection */
  g_ext_incdirs.count = 0;
  g_ext_incdirs.names = NULL;
  g_ext_incdirs.max_name_size_bytes = MAX_NAME_LENGTH + 1;

  parse_flags_result = FAILED;
  if (argc >= 5)
    parse_flags_result = parse_flags(argv, argc);
  
  if (argc < 5 || parse_flags_result == FAILED || g_backend == BACKEND_NONE) {
    printf("\nBILIBALI Compiler v1.0a. Written by Ville Helin in 2021+\n");
#ifdef BILIBALI_DEBUG
    printf("*** BILIBALI_DEBUG defined - this executable is running in DEBUG mode ***\n");
#endif
    printf("%s\n\n", g_version_string);
    printf("USAGE: %s <ARCHITECTURE> [OPTIONS] -o <ASM FILE> <SOURCE FILE>\n\n", argv[0]);
    printf("Options:\n");
    printf("-q       Quiet\n");
    printf("-v       Verbose messages\n");
    printf("-I <DIR> Include directory\n");
    printf("-D <DEF> Declare definition\n\n");
    printf("Achitectures:\n");
    printf("-aZ80    Compile for Z80 CPU\n\n");
    printf("EXAMPLE: %s -aZ80 -D VERSION=1 -D TWO=2 -v -o main.asm main.blb\n\n", argv[0]);
    return 0;
  }

  if (strcmp(g_asm_name, g_final_name) == 0) {
    fprintf(stderr, "MAIN: Input and output files have the same name!\n");
    return 1;
  }

  g_commandline_parsing = OFF;

  /* start the process */
  if (include_file(g_asm_name, &include_size, NULL) == FAILED)
    return 1;

  /* tokenize */
  if (pass_1() == FAILED)
    return 1;
  /* parse assembly code */
  if (pass_2() == FAILED)
    return 1;

  /* free the tokens from pass_1() that pass_2() processed */
  while (g_token_first != NULL) {
    struct token *t = g_token_first->next;
    free(g_token_first->label);
    free(g_token_first);
    g_token_first = t;
  }

  /* simplify expressions */
  if (pass_3() == FAILED)
    return 1;
  /* generate IL */
  if (pass_4() == FAILED)
    return 1;
  /* optimize IL, make IL ready to be turned into ASM */
  if (pass_5() == FAILED)
    return 1;

  if (_get_plain_filename(final_name_no_extension, sizeof(final_name_no_extension), g_final_name) == FAILED)
    return 1;
  
  file_out = fopen(g_final_name, "wb");

  /* generate ASM */
  if (g_backend == BACKEND_Z80) {
    if (pass_6_z80(final_name_no_extension, file_out) == FAILED) {
      fclose(file_out);
      return FAILED;
    }
  }

  fclose(file_out);
  
  return 0;
}


int parse_flags(char **flags, int flagc) {

  int asm_name_def = 0, count;
  char *str_build;
  
  for (count = 1; count < flagc; count++) {
    if (!strcmp(flags[count], "-o")) {
      if (g_output != OUTPUT_NONE)
        return FAILED;
      g_output = OUTPUT_ASM;
      if (count + 1 < flagc) {
        /* set output */
        g_final_name = calloc(strlen(flags[count+1])+1, 1);
        strcpy(g_final_name, flags[count+1]);
      }
      else
        return FAILED;

      count++;
      continue;
    }
    else if (!strcmp(flags[count], "-D")) {
      if (count + 1 < flagc) {
        if (count + 3 < flagc) {
          if (!strcmp(flags[count+2], "=")) {
            int length = (int)strlen(flags[count+1])+(int)strlen(flags[count+3])+2;
            str_build = calloc(length, 1);
            snprintf(str_build, length, "%s=%s", flags[count+1], flags[count+3]);
            parse_and_add_definition(str_build, NO);
            free(str_build);
            count += 2;
          }
          else
            parse_and_add_definition(flags[count+1], NO);
        }
        else
          parse_and_add_definition(flags[count+1], NO);
      }
      else
        return FAILED;

      count++;
      continue;
    }
    else if (!strcmp(flags[count], "-I")) {
      if (count + 1 < flagc) {
        /* get arg */
        parse_and_add_incdir(flags[count+1], NO);
      }
      else
        return FAILED;

      count++;
      continue;
    }
    else if (!strcmp(flags[count], "-v")) {
      g_verbose_mode = ON;
      continue;
    }
    else if (!strcmp(flags[count], "-t")) {
      g_test_mode = ON;
      continue;
    }
    else if (!strcmp(flags[count], "-q")) {
      g_quiet = YES;
      continue;
    }
    else if (!strcmp(flags[count], "-aZ80")) {
      g_backend = BACKEND_Z80;
      continue;
    }    
    else {
      if (count == flagc - 1) {
        g_asm_name = calloc(strlen(flags[count]) + 1, 1);
        strcpy(g_asm_name, flags[count]);
        count++;
        asm_name_def++;
      }
      else {
        /* legacy support? */
        if (strncmp(flags[count], "-D", 2) == 0) {
          /* old define */
          parse_and_add_definition(flags[count], YES);
          continue;
        }
        else if (strncmp(flags[count], "-I", 2) == 0) {
          /* old include directory */
          parse_and_add_incdir(flags[count], YES);
          continue;
        }
        else
          return FAILED;
      }
    }
  }
  
  if (asm_name_def <= 0)
    return FAILED;
  
  return SUCCEEDED;
}


void procedures_at_exit(void) {

  struct file_name_info *f, *ft;
  int index;
  
  /* free all the dynamically allocated data structures and close open files */
  free(g_final_name);
  free(g_asm_name);
  free(g_include_dir);
  free(g_full_name);

  g_label_tmp = g_labels;
  while (g_label_tmp != NULL) {
    g_labels = g_label_tmp->next;
    free(g_label_tmp);
    g_label_tmp = g_labels;
  }

  g_ifd_tmp = g_incbin_file_data_first;
  while(g_ifd_tmp != NULL) {
    g_incbin_file_data_first = g_ifd_tmp->next;
    free(g_ifd_tmp->data);
    free(g_ifd_tmp->name);
    free(g_ifd_tmp);
    g_ifd_tmp = g_incbin_file_data_first;
  }

  g_stacks_tmp = g_stacks_first;
  while (g_stacks_tmp != NULL) {
    free(g_stacks_tmp->stack);
    g_stacks_first = g_stacks_tmp->next;
    free(g_stacks_tmp);
    g_stacks_tmp = g_stacks_first;
  }

  g_stacks_tmp = g_stacks_header_first;
  while (g_stacks_tmp != NULL) {
    free(g_stacks_tmp->stack);
    g_stacks_first = g_stacks_tmp->next;
    free(g_stacks_tmp);
    g_stacks_tmp = g_stacks_first;
  }

  free(g_buffer);
  free(g_include_in_tmp);
  free(g_tmp_a);

  f = g_file_name_info_first;
  while (f != NULL) {
    free(f->name);
    ft = f->next;
    free(f);
    f = ft;
  }

  while (g_inline_asm_first != NULL) {
    struct inline_asm *ia = g_inline_asm_first->next;
    inline_asm_free(g_inline_asm_first);
    g_inline_asm_first = ia;
  }
  
  while (g_definitions_first != NULL) {
    struct definition *d = g_definitions_first->next;
    definition_free(g_definitions_first);
    g_definitions_first = d;
  }

  while (g_token_first != NULL) {
    struct token *t = g_token_first->next;
    token_free(g_token_first);
    g_token_first = t;
  }

  for (index = 0; index < 256; index++)
    free(g_open_expression[index]);

  free_symbol_table();

  for (index = 0; index < g_tacs_count; index++)
    free_tac_contents(&g_tacs[index]);
  free(g_tacs);

  free(g_register_reads);
  free(g_register_writes);

  source_files_free();
  
  /* remove the tmp files */
  if (g_tmp_name != NULL)
    remove(g_tmp_name);

  /* cleanup any incdirs we added */
  for (index = 0; index < g_ext_incdirs.count; index++)
    free(g_ext_incdirs.names[index]);
  free(g_ext_incdirs.names);
}


static int _add_a_new_definition(char *name, double d, char *s, int type, int s_length) {

  return FAILED;
}


int parse_and_add_definition(char *c, int contains_flag) {

  char n[MAX_NAME_LENGTH + 1], *value;
  int i;

  /* skip the flag? */
  if (contains_flag == YES)
    c += 2;
  
  for (i = 0; i < MAX_NAME_LENGTH && *c != 0 && *c != '='; i++, c++)
    n[i] = *c;
  n[i] = 0;

  if (*c == 0)
    return _add_a_new_definition(n, 0.0, NULL, DEFINITION_TYPE_VALUE, 0);
  else if (*c == '=') {
    c++;
    if (*c == 0)
      return FAILED;

    value = c;
    
    /* hexadecimal value? */
    if (*c == '$' || ((c[strlen(c)-1] == 'h' || c[strlen(c)-1] == 'H') && (*c >= '0' && *c <= '9'))) {
      if (*c == '$')
        c++;
      for (i = 0; *c != 0; c++) {
        if (*c >= '0' && *c <= '9')
          i = (i << 4) + *c - '0';
        else if (*c >= 'a' && *c <= 'f')
          i = (i << 4) + *c - 'a' + 10;
        else if (*c >= 'A' && *c <= 'F')
          i = (i << 4) + *c - 'A' + 10;
        else if ((*c == 'h' || *c == 'H') && *(c+1) == 0)
          break;
        else {
          fprintf(stderr, "PARSE_AND_ADD_DEFINITION: Error in value (%s).\n", value);
          return FAILED;
        }
      }
      return _add_a_new_definition(n, (double)i, NULL, DEFINITION_TYPE_VALUE, 0);
    }

    /* decimal value? */
    if (*c >= '0' && *c <= '9') {
      for (i = 0; *c != 0; c++) {
        if (*c >= '0' && *c <= '9')
          i = (i * 10) + *c - '0';
        else {
          fprintf(stderr, "PARSE_AND_ADD_DEFINITION: Error in value (%s).\n", value);
          return FAILED;
        }
      }
      return _add_a_new_definition(n, (double)i, NULL, DEFINITION_TYPE_VALUE, 0);
    }

    /* quoted string? */
    if (*c == '"' && c[strlen(c) - 1] == '"') {
      int t;
      char *s = calloc(strlen(c) + 1, 1);
      int result;

      c++;
      for (t = 0; *c != 0; c++, t++) {
        if (*c == '\\' && *(c + 1) == '"') {
          c++;
          s[t] = '"';
        }
        else if (*c == '"') {
          c++;
          break;
        }
        else
          s[t] = *c;
      }
      s[t] = 0;
      
      if (*c == 0)
        result = _add_a_new_definition(n, 0.0, s, DEFINITION_TYPE_STRING, (int)strlen(s));
      else {
        fprintf(stderr, "PARSE_AND_ADD_DEFINITION: Incorrectly terminated quoted string (%s).\n", value);
        result = FAILED;
      }
      
      free(s);
      return result;
    }

    /* unquoted string */
    return _add_a_new_definition(n, 0.0, c, DEFINITION_TYPE_STRING, (int)strlen(c));
  }

  return FAILED;
}


int parse_and_add_incdir(char* c, int contains_flag) {

  int old_count = g_ext_incdirs.count, index, buffer_size, j;
  char **new_array;
  char n[MAX_NAME_LENGTH + 1];

  /* increment for the new entry, then re-allocate the array */
  g_ext_incdirs.count++;
  new_array = calloc(g_ext_incdirs.count * sizeof(char*), 1);
  for (index = 0; index < old_count; index++)
    new_array[index] = g_ext_incdirs.names[index];
 
  free(g_ext_incdirs.names);
  g_ext_incdirs.names = new_array;
  buffer_size = g_ext_incdirs.max_name_size_bytes;
  g_ext_incdirs.names[old_count] = calloc(buffer_size, 1);

  /* skip the flag? */
  if (contains_flag == YES)
    c += 2;

  for (j = 0; j < MAX_NAME_LENGTH && *c != 0; j++, c++)
    n[j] = *c;
  n[j] = 0;

  if (*c != 0)
    return FAILED;

  localize_path(n);

#if defined(MSDOS)
  snprintf(g_ext_incdirs.names[old_count], buffer_size, "%s\\", n);
#else
  snprintf(g_ext_incdirs.names[old_count], buffer_size, "%s/", n);
#endif

  g_use_incdir = YES;

  return SUCCEEDED;
}


int localize_path(char *path) {

  int i;

  if (path == NULL)
    return FAILED;

  for (i = 0; path[i] != 0; i++) {
#if defined(MSDOS)
    /* '/' -> '\' */
    if (path[g_source_pointer] == '/')
      path[g_source_pointer] = '\\';
#else
    /* '\' -> '/' */
    if (path[i] == '\\')
      path[i] = '/';
#endif
  }

  return SUCCEEDED;
}


void print_error(char *error, int type) {

  char error_dir[] = "DIRECTIVE_ERROR:";
  char error_unf[] = "UNFOLD_ALIASES:";
  char error_num[] = "INPUT_NUMBER:";
  char error_inc[] = "INCLUDE_FILE:";
  char error_inb[] = "INCBIN_FILE:";
  char error_inp[] = "INPUT_ERROR:";
  char error_log[] = "LOGIC_ERROR:";
  char error_stc[] = "STACK_CALCULATE:";
  char error_wrn[] = "WARNING:";
  char error_err[] = "ERROR:";
  char *t = NULL;
  int filename_id, line_current;

  if (g_active_file_info_last != NULL) {
    filename_id = g_active_file_info_last->filename_id;
    line_current = g_active_file_info_last->line_current;
  }
  else {
    filename_id = g_current_filename_id;
    line_current = g_current_line_number;
  }
  
  switch (type) {
  case ERROR_LOG:
    t = error_log;
    break;
  case ERROR_UNF:
    t = error_unf;
    break;
  case ERROR_INC:
    t = error_inc;
    break;
  case ERROR_INB:
    t = error_inb;
    break;
  case ERROR_DIR:
    t = error_dir;
    break;
  case ERROR_INP:
    t = error_inp;
    break;
  case ERROR_NUM:
    t = error_num;
    break;
  case ERROR_STC:
    t = error_stc;
    break;
  case ERROR_WRN:
    t = error_wrn;
    break;
  case ERROR_ERR:
    t = error_err;
    break;
  case ERROR_NONE:
    fprintf(stderr, "%s:%d: %s", get_file_name(filename_id), line_current, error);
    return;
  }

  fprintf(stderr, "%s:%d: %s %s", get_file_name(filename_id), line_current, t, error);
  fflush(stderr);

  return;
}


void next_line(void) {

  g_newline_beginning = ON;

  if (g_line_count_status == OFF)
    return;

  if (g_active_file_info_last == NULL)
    return;

  g_active_file_info_last->line_current++;
}


int strcaselesscmp(char *s1, char *s2) {

  if (s1 == NULL || s2 == NULL)
    return 0;

  while (*s1 != 0) {
    if (toupper((int)*s1) != toupper((int)*s2))
      return 1;
    s1++;
    s2++;
  }

  if (*s2 != 0)
    return 1;

  return 0;
}
