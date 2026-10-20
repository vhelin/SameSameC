
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "file.h"
#include "printf.h"


struct file *g_files_first = NULL, *g_files_last = NULL;
unsigned char g_copy_bytes_bank[256];
extern int g_share_arithmetic;


static int _get_plain_filename(char *output, int output_size, char *input) {

  int length, i, start, j;

  length = (int)strlen(input);

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
  f->bank = -1;
  f->data_copy_bytes[0] = 0;
  f->data_copy_bytes_size = 0;
  f->next = NULL;
  f->position_sections = 0;
  f->position_ramsections = 0;
  
  if ((int)strlen(name) >= MAX_NAME_LENGTH) {
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

  f->data = calloc((size_t)file_size + 1, 1);
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
  length = (int)strlen(name);

  while (i < f->size) {
    for (j = 0; i+j < f->size && j < length; j++) {
      if (d[i+j] != name[j])
        break;
    }
    if (j == length)
      return SUCCEEDED;

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


int file_find_copy_bytes_calls(void) {

  char copy_function_name[MAX_NAME_LENGTH+1];
  struct file *f;
  int i;

  for (i = 0; i < 256; i++)
    g_copy_bytes_bank[i] = NO;

  /* TODO: optimize */
  
  f = g_files_first;
  while (f != NULL) {
    for (i = 0; i < 256; i++) {
      if (g_copy_bytes_bank[i] == YES)
        continue;
      
      snprintf(copy_function_name, sizeof(copy_function_name), "copy_bytes_bank_%.3d", i);
      if (_find_function(copy_function_name, f) == SUCCEEDED) {
        g_copy_bytes_bank[i] = YES;
        f->bank = i;
        break;
      }
    }
    
    f = f->next;
  }

  return SUCCEEDED;
}


int file_write_all_asm_files_into_tmp_file(FILE *fp) {

  struct arithmetic_body {
    struct file *file;
    char *start, *body, *end;
    int kind, bank, slot, length, uses, id;
    struct arithmetic_body *helper, *next;
  } *bodies = NULL, *last = NULL, *body, *match, *next;
  const char *begin_marker = "      ; ssc_arithmetic_begin ";
  const char *end_marker = "      ; ssc_arithmetic_end";
  char *cursor, *start, *end, *code;
  int kind, bank, slot, consumed, id = 0, status = FAILED;
  struct file *f;

  if (g_share_arithmetic == YES) {
    for (f = g_files_first; f != NULL; f = f->next) {
      cursor = f->data;
      while ((start = strstr(cursor, begin_marker)) != NULL) {
        cursor = start + strlen(begin_marker);
        if (start != f->data && start[-1] != '\n')
          continue;
        end = strchr(cursor, '\n');
        if (end == NULL || sscanf(cursor, "%d %d %d%n", &kind, &bank, &slot, &consumed) != 3)
          goto invalid_body;
        for (cursor += consumed; cursor < end && strchr(" \t\r", *cursor) != NULL; cursor++)
          ;
        if (cursor != end || kind < 0 || kind > 3 || bank < 0 || bank > 255 || slot < 0 || slot > 3)
          goto invalid_body;
        code = end + 1;
        end = strstr(code, end_marker);
        cursor = strstr(code, begin_marker);
        if (end == NULL || end == code || end[-1] != '\n' || (cursor != NULL && cursor < end))
          goto invalid_body;
        cursor = end + strlen(end_marker);
        if (*cursor == '\r')
          cursor++;
        if (*cursor != '\n' && *cursor != 0)
          goto invalid_body;
        if (*cursor == '\n')
          cursor++;
        /* RAM code cannot call an ordinary ROM-bank-local helper. */
        if (slot == 3)
          continue;
        body = calloc(1, sizeof(*body));
        if (body == NULL) {
          fprintf(stderr, "FILE_WRITE_ALL_ASM_FILES_INTO_TMP_FILE: Out of memory error.\n");
          goto cleanup;
        }
        body->file = f;
        body->start = start;
        body->body = code;
        body->end = cursor;
        body->kind = kind;
        body->bank = bank;
        body->slot = slot;
        body->length = (int)(end - code);
        body->uses = 1;
        body->id = id++;
        body->helper = body;
        for (match = bodies; match != NULL; match = match->next) {
          if (match->helper == match && match->kind == kind && match->bank == bank && match->slot == slot &&
              match->length == body->length && memcmp(match->body, code, body->length) == 0) {
            body->helper = match;
            match->uses++;
            break;
          }
        }
        if (last == NULL)
          bodies = body;
        else
          last->next = body;
        last = body;
      }
    }
  }

  f = g_files_first;
  while (f != NULL) {
    fprintf(fp, "\n");
    fprintf(fp, "; ######################################################################\n");
    fprintf(fp, "; %s\n", f->name);
    fprintf(fp, "; ######################################################################\n");
    cursor = f->data;
    for (body = bodies; body != NULL; body = body->next) {
      if (body->file != f || body->helper->uses < 2)
        continue;
      fwrite(cursor, 1, body->start - cursor, fp);
      fprintf(fp, "      CALL ssc.arithmetic_%d_bank_%.3d_slot_%d\n",
              body->helper->id, body->bank, body->slot);
      cursor = body->end;
    }
    fwrite(cursor, 1, f->data + f->size - cursor, fp);

    /* copy_bytes_bank_??? function? */
    if (f->data_copy_bytes_size > 0)
      fwrite(f->data_copy_bytes, 1, f->data_copy_bytes_size, fp);
    
    f = f->next;
  }

  for (body = bodies; body != NULL; body = body->next) {
    /* All four emitted kernels save bytes from their second use, including RET. */
    if (body->helper != body || body->uses < 2)
      continue;
    /* Leading underscores are section-local in WLA; dots cannot collide with C identifiers. */
    fprintf(fp, "\n  .BANK %d SLOT %d\n", body->bank, body->slot);
    fprintf(fp, "  .SECTION \"ssc.arithmetic_%d_bank_%.3d_slot_%d\" FREE\n", body->id, body->bank, body->slot);
    fprintf(fp, "    ssc.arithmetic_%d_bank_%.3d_slot_%d:\n", body->id, body->bank, body->slot);
    fwrite(body->body, 1, body->length, fp);
    fprintf(fp, "      RET\n  .ENDS\n");
  }
  status = SUCCEEDED;
  if (fflush(fp) != 0 || ferror(fp)) {
    fprintf(stderr, "FILE_WRITE_ALL_ASM_FILES_INTO_TMP_FILE: Cannot write combined assembly.\n");
    status = FAILED;
  }
  goto cleanup;

invalid_body:
  fprintf(stderr, "FILE_WRITE_ALL_ASM_FILES_INTO_TMP_FILE: Invalid arithmetic body in \"%s\".\n", f->name);
cleanup:
  for (body = bodies; body != NULL; body = next) {
    next = body->next;
    free(body);
  }
  return status;
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

  l = (int)strlen(directive);
  
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
