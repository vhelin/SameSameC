
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "printf.h"
#include "symbol_table.h"
#include "main.h"
#include "struct_item.h"


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024], g_label[MAX_NAME_LENGTH + 1];
extern int g_current_filename_id, g_current_line_number;

static struct symbol_table_item **g_symbol_table = NULL;
static int g_symbol_table_size = 0;


int is_symbol_table_empty(void) {

  int i;
  
  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] != NULL)
      return NO;
  }

  return YES;
}


struct symbol_table_item *symbol_table_find_symbol(char *name) {

  int i;

  if (name == NULL) {
    print_error("Trying to find a symbol from symbol table with name NULL! Please submit a bug report!\n", ERROR_DIR);
    return NULL;
  }

  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] == NULL)
      continue;

    if (strcmp(g_symbol_table[i]->label, name) == 0)
      return g_symbol_table[i];
  }

  return NULL;
}


int symbol_table_add_symbol(struct tree_node *node, char *name, int level, int line_number, int file_id) {

  struct symbol_table_item *item;
  int i, j, symbols = 0;

  if (name == NULL) {
    g_current_filename_id = file_id;
    g_current_line_number = line_number;
    return print_error("Adding a NULL symbol to symbol table! Please submit a bug report!\n", ERROR_DIR);
  }

  if (node == NULL) {
    g_current_filename_id = file_id;
    g_current_line_number = line_number;
    return print_error("Adding a NULL node to symbol table! Please submit a bug report!\n", ERROR_DIR);
  }
  
  /* try to find the name in the symbol table */
  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] == NULL)
      continue;
    if (strcmp(name, g_symbol_table[i]->label) == 0)
      break;
    symbols++;
  }

  if (i == g_symbol_table_size) {
    /* symbol name not found -> add it to the table */
    if (symbols == g_symbol_table_size) {
      int new_symbol_table_size = g_symbol_table_size + 64;
      
      /* grow the symbol table */
      g_symbol_table = realloc(g_symbol_table, sizeof(struct symbol_table_item *) * new_symbol_table_size);
      if (g_symbol_table == NULL) {
        g_current_filename_id = file_id;
        g_current_line_number = line_number;
        return print_error("Out of memory while enlarging symbol table!\n", ERROR_DIR);
      }

      for (j = g_symbol_table_size; j < new_symbol_table_size; j++)
        g_symbol_table[j] = NULL;

      g_symbol_table_size = new_symbol_table_size;
    }

    /* find an empty location for the symbol */
    for (j = 0; j < g_symbol_table_size; j++) {
      if (g_symbol_table[j] == NULL) {
        i = j;
        break;
      }
    }

    if (j == g_symbol_table_size) {
      g_current_filename_id = file_id;
      g_current_line_number = line_number;
      return print_error("No empty slot for a new symbol in the symbol table even though there should be room for it! Please submit a bug report!\n", ERROR_DIR);
    }
  }

  /* i is the index for the symbol in the symbol table */

  if (g_symbol_table[i] != NULL && g_symbol_table[i]->level == level) {
    g_current_filename_id = file_id;
    g_current_line_number = line_number;
    snprintf(g_error_message, sizeof(g_error_message), "\"%s\" is already defined on this level.\n", name);
    return print_error(g_error_message, ERROR_ERR);
  }

  item = calloc(sizeof(struct symbol_table_item), 1);
  if (item == NULL) {
    g_current_filename_id = file_id;
    g_current_line_number = line_number;
    return print_error("Out of memory while allocating a new symbol table item.\n", ERROR_DIR);
  }

  item->label = calloc(strlen(name) + 1, 1);
  if (item->label == NULL) {
    free(item);
    g_current_filename_id = file_id;
    g_current_line_number = line_number;
    return print_error("Out of memory while allocating a new symbol table item.\n", ERROR_DIR);
  }

  item->level = level;
  strcpy(item->label, name);
  item->node = node;
  item->next = g_symbol_table[i];
  g_symbol_table[i] = item;
  
  return SUCCEEDED;
}


void free_symbol_table(void) {

  struct symbol_table_item *item;
  int i;

  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] == NULL)
      continue;

    while (g_symbol_table[i] != NULL) {
      item = g_symbol_table[i]->next;

      free(g_symbol_table[i]->label);
      free(g_symbol_table[i]);

      g_symbol_table[i] = item;
    }
  }

  free(g_symbol_table);
  
  g_symbol_table = NULL;
  g_symbol_table_size = 0;
}


void free_symbol_table_items(int level) {

  struct symbol_table_item *item;
  int i;

  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] == NULL)
      continue;
    if (g_symbol_table[i]->level < level)
      continue;

    item = g_symbol_table[i]->next;

    free(g_symbol_table[i]->label);
    free(g_symbol_table[i]);

    g_symbol_table[i] = item;
  }
}


void print_symbol_table_items(void) {

  struct symbol_table_item *item;
  int i;

  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] == NULL)
      continue;

    item = g_symbol_table[i];
    while (item != NULL) {
      fprintf(stderr, "LEVEL %d: %s\n", item->level, item->label);
      item = item->next;
    }
  }
}
