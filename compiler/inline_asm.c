
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"

#include "printf.h"
#include "main.h"
#include "inline_asm.h"


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

struct inline_asm *g_inline_asm_first = NULL, *g_inline_asm_last = NULL;

static int g_inline_asm_id = 0;


struct inline_asm *inline_asm_add(void) {

  struct inline_asm *ia;
  
  ia = calloc(sizeof(struct inline_asm), 1);
  if (ia == NULL) {
    print_error("Out of memory error while allocating a new inline_asm struct.\n", ERROR_ERR);
    return NULL;
  }

  ia->id = g_inline_asm_id++;
  ia->file_id = -1;
  ia->line_number = -1;
  ia->asm_line_first = NULL;
  ia->asm_line_last = NULL;
  ia->next = NULL;

  if (g_inline_asm_first == NULL) {
    g_inline_asm_first = ia;
    g_inline_asm_last = ia;
  }
  else {
    g_inline_asm_last->next = ia;
    g_inline_asm_last = ia;
  }
  
  return ia;
}


void inline_asm_free(struct inline_asm *ia) {

  struct asm_line *al;

  al = ia->asm_line_first;
  while (al != NULL) {
    struct asm_line *next = al->next;
    free(al->line);
    free(al);
    al = next;
  }

  free(ia);
}


struct inline_asm *inline_asm_find(int id) {

  struct inline_asm *ia;

  ia = g_inline_asm_first;
  while (ia != NULL) {
    if (ia->id == id)
      return ia;
    ia = ia->next;
  }

  fprintf(stderr, "inline_asm_find(): Could not find inline_asm with id %d! Should not happen! Please submit a bug report!\n", id);
  
  return NULL;
}


int inline_asm_add_asm_line(struct inline_asm *ia, char *line, int length, int line_number) {

  struct asm_line *al;

  al = calloc(sizeof(struct asm_line), 1);
  if (al == NULL) {
    print_error("Out of memory error while allocating a new asm_line struct.\n", ERROR_ERR);
    return FAILED;
  }
  
  al->line = calloc(length, 1);
  if (al->line == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while allocating room for string \"%s\"\n", line);
    print_error(g_error_message, ERROR_ERR);
    free(al);
    return FAILED;
  }

  memcpy(al->line, line, length);

  al->flags = 0;
  al->line_number = line_number;
  al->variable = NULL;
  al->next = NULL;

  if (ia->asm_line_first == NULL) {
    ia->asm_line_first = al;
    ia->asm_line_last = al;
  }
  else {
    ia->asm_line_last->next = al;
    ia->asm_line_last = al;
  }
  
  return SUCCEEDED;
}
