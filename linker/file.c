
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "file.h"
#include "printf.h"


struct file *g_files_first = NULL, *g_files_last = NULL;


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


int file_load(char *name) {

  struct file *f;
  int file_size;
  FILE *fp;
  
  fp = fopen(name, "rb");
  if (fp == NULL) {
    fprintf(stderr, "FILE_LOAD: Cannot open file \"%s\" for reading.\n", name);
    return FAILED;
  }

  f = calloc(sizeof(struct file), 1);
  if (f == NULL) {
    fprintf(stderr, "FILE_LOAD: Out of memory error.\n");
    fclose(fp);
    return FAILED;
  }

  f->name[0] = 0;
  f->name_no_extension[0] = 0;
  f->global_variables_init_function[0] = 0;
  f->data = NULL;
  f->size = 0;
  f->next = NULL;
  f->position_sections = 0;
  f->position_ramsections = 0;
  
  if (strlen(name) >= MAX_NAME_LENGTH) {
    fprintf(stderr, "FILE_LOAD: The file name \"%s\" is too long. Please make defines.h/MAX_NAME_LENGTH larger, recompile and relink.\n", name);
    free(f);
    fclose(fp);
    return FAILED;
  }
  strcpy(f->name, name);

  /* get just the file name without path and extension */
  if (_get_plain_filename(f->name_no_extension, MAX_NAME_LENGTH+1, name) == FAILED) {
    free(f);
    fclose(fp);
    return FAILED;
  }
  
  fseek(fp, 0, SEEK_END);
  file_size = (int)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  f->size = file_size;

  f->data = calloc(file_size, 1);
  if (f->data == NULL) {
    fprintf(stderr, "FILE_LOAD: Out of memory error.\n");
    free(f);
    fclose(fp);
    return FAILED;
  }

  fread(f->data, 1, file_size, fp);  
  fclose(fp);

  f->next = NULL;
  
  if (g_files_first == NULL) {
    g_files_first = f;
    g_files_last = f;
  }
  else {
    g_files_last->next = f;
    g_files_last = f;
  }
  
  return SUCCEEDED;
}


static int _find_function(const char *name, struct file *f) {

  int i = 0, j, length;
  char *d;

  d = f->data;
  length = strlen(name);

  while (i < f->size) {
    if (i < f->size - 4) {
      if (d[i+0] == ' ' && d[i+1] == ' ' && d[i+2] == ' ' && d[i+3] == ' ' && i+4+length < f->size) {
        for (j = 0; j < length; j++) {
          if (d[i+4+j] != name[j])
            break;
        }
        if (j == length)
          return SUCCEEDED;
      }
    }
    i++;
  }
  
  return FAILED;
}


int file_has_main(void) {

  struct file *f;

  f = g_files_first;
  while (f != NULL) {
    if (_find_function("mainmain:", f) == SUCCEEDED)
      return SUCCEEDED;
    f = f->next;
  }
  
  return FAILED;
}


int file_write_all_asm_files_into_tmp_file(FILE *fp) {

  struct file *f;

  f = g_files_first;
  while (f != NULL) {
    fprintf(fp, "\n");
    fprintf(fp, "; ######################################################################\n");
    fprintf(fp, "; %s\n", f->name);
    fprintf(fp, "; ######################################################################\n");
    fwrite(f->data, 1, f->size, fp);
    f = f->next;
  }

  return SUCCEEDED;
}


int file_find_global_variable_inits(void) {

  char function_name[MAX_NAME_LENGTH+1];
  struct file *f;

  f = g_files_first;
  while (f != NULL) {
    snprintf(function_name, sizeof(function_name), "global_variables_%s_init:", f->name_no_extension);
    if (_find_function(function_name, f) == SUCCEEDED)
      snprintf(f->global_variables_init_function, MAX_NAME_LENGTH+1, "global_variables_%s_init", f->name_no_extension);
    f = f->next;
  }
  
  return SUCCEEDED;
}


static int _find_next_section(struct file *f, const char *directive, int *position, char *section_name, int length) {

  int i = *position, j, k, l;
  char *data = f->data;

  l = strlen(directive);
  
  while (i < f->size) {
    if (data[i] == directive[0]) {
      for (j = 0, k = i; j < l && k < f->size; j++, k++) {
        if (data[k] != directive[j])
          break;
      }

      if (j == l) {
        while (data[k] != '"')
          k++;
        k++;

        for (j = 0; j < length && k < f->size && data[k] != '"'; j++, k++)
          section_name[j] = data[k];
        section_name[j] = 0;

        *position = k;
        
        return SUCCEEDED;
      }
    }

    i++;
  }

  *position = i;
  
  return FAILED;
}


int file_find_next_section(struct file *f, char *section_name, int length) {

  return _find_next_section(f, ".SECTION", &f->position_sections, section_name, length);
}


int file_find_next_ramsection(struct file *f, char *section_name, int length) {

  return _find_next_section(f, ".RAMSECTION", &f->position_ramsections, section_name, length);
}
