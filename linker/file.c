
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "file.h"


struct file *g_files_first = NULL, *g_files_last = NULL;


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

  if (strlen(name) >= MAX_NAME_LENGTH) {
    fprintf(stderr, "FILE_LOAD: The file name \"%s\" is too long. Please make defines.h/MAX_NAME_LENGTH larger, recompile and relink.\n", name);
    fclose(fp);
    return FAILED;
  }
  strcpy(f->name, name);
  
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
    if (_find_function("main:", f) == SUCCEEDED)
      return SUCCEEDED;
    f = f->next;
  }
  
  return FAILED;
}
