
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"

#include "printf.h"
#include "main.h"
#include "include_file.h"
#include "source_line_manager.h"


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

struct source_file *g_source_files_first = NULL, *g_source_files_last = NULL;


static void _show_out_of_memory_error(char *file_name) {

  snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying allocate info structure for file \"%s\".\n", file_name);
  print_error(g_error_message, ERROR_INC);
}


static void _free_source_file(struct source_file *s) {

  free(s->file_name);
  free(s->source);
  free(s->line_pointers);
  free(s);
}


static int _count_lines(struct source_file *s) {

  int lines = 1, i;

  for (i = 0; i < s->file_size; i++) {
    if (s->source[i] == 0xA)
      lines++;
  }

  s->lines_total = lines;
  
  return SUCCEEDED;
}


static int _calculate_line_pointers(struct source_file *s) {

  int line = 0, i;

  s->line_pointers[0] = 0;
  
  for (i = 0; i < s->file_size; i++) {
    if (s->source[i] == 0xD) {
    }
    else if (s->source[i] == 0xA) {
      line++;
      s->line_pointers[line] = i + 1;
    }
  }

  return SUCCEEDED;
}


int get_source_line(int file_id, int line_number, FILE *file_out) {

  struct source_file *s = g_source_files_first;
  int i, spaces = 1;
  
  while (s != NULL) {
    if (s->id == file_id)
      break;
    s = s->next;
  }

  if (s == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Cannot find source file \"%s\"! Please submit a bug report!\n", get_file_name(file_id));
    print_error(g_error_message, ERROR_INC);
    return FAILED;
  }

  if (line_number < 1 || line_number > s->lines_total) {
    snprintf(g_error_message, sizeof(g_error_message), "Line number %d of source file \"%s\" is out of bounds [1, %d]! Please submit a bug report!\n", line_number, get_file_name(file_id), s->lines_total);
    print_error(g_error_message, ERROR_INC);
    return FAILED;
  }

  i = s->line_pointers[line_number - 1];

  while (i < s->file_size) {
    char c = s->source[i];

    if (c == 0xA)
      break;
    else if (c == 0xD) {
    }
    else if (c == ' ' || c == '\t') {
      if (spaces == 0)
        fprintf(file_out, " ");
      spaces++;
    }
    else {
      fprintf(file_out, "%c", c);
      spaces = 0;
    }

    i++;
  }

  return SUCCEEDED;
}


int load_source_file(char *file_name, int file_id) {

  struct source_file *s;
  int file_size;
  FILE *f;
  
  f = fopen(file_name, "rb");
  if (f == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Cannot open file \"%s\" for reading.\n", file_name);
    print_error(g_error_message, ERROR_INC);
    return FAILED;
  }
    
  fseek(f, 0, SEEK_END);
  file_size = (int)ftell(f);
  fseek(f, 0, SEEK_SET);

  s = (struct source_file *)calloc(sizeof(struct source_file), 1);
  if (s == NULL) {
    _show_out_of_memory_error(file_name);
    fclose(f);
    return FAILED;
  }

  s->file_name = NULL;
  s->file_size = 0;
  s->id = -1;
  s->source = NULL;
  s->lines_total = 0;
  s->line_pointers = NULL;
  s->next = NULL;

  s->file_name = (char *)calloc(strlen(file_name) + 1, 1);
  if (s->file_name == NULL) {
    fclose(f);
    _show_out_of_memory_error(file_name);
    _free_source_file(s);
    return FAILED;
  }

  s->source = (char *)calloc(file_size, 1);
  if (s->source == NULL) {
    fclose(f);
    _show_out_of_memory_error(file_name);
    _free_source_file(s);
    return FAILED;
  }

  strcpy(s->file_name, file_name);
  s->id = file_id;
  s->file_size = file_size;
  
  /* read the whole file into a buffer */
  fread(s->source, 1, file_size, f);
  fclose(f);

  if (_count_lines(s) == FAILED) {
    _show_out_of_memory_error(file_name);
    _free_source_file(s);
    return FAILED;
  }

  s->line_pointers = (int *)calloc(sizeof(int) * s->lines_total, 1);
  if (s->line_pointers == NULL) {
    _show_out_of_memory_error(file_name);
    _free_source_file(s);
    return FAILED;
  }

  if (_calculate_line_pointers(s) == FAILED) {
    _show_out_of_memory_error(file_name);
    _free_source_file(s);
    return FAILED;
  }

  if (g_source_files_first == NULL) {
    g_source_files_first = s;
    g_source_files_last = s;
  }
  else {
    g_source_files_last->next = s;
    g_source_files_last = s;
  }
  
  return SUCCEEDED;
}


int source_files_free(void) {

  struct source_file *s1 = g_source_files_first, *s2;

  while (s1 != NULL) {
    s2 = s1->next;
    _free_source_file(s1);
    s1 = s2;
  }

  return SUCCEEDED;
}
