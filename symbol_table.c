
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


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024], g_label[MAX_NAME_LENGTH + 1];

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
  
  for (i = 0; i < g_symbol_table_size; i++) {
    if (g_symbol_table[i] == NULL)
      continue;
    
    if (strcmp(g_symbol_table[i]->label, name) == 0)
      return g_symbol_table[i];
  }

  return NULL;
}


int symbol_table_add_symbol(struct tree_node *node, char *name, int level) {

  struct symbol_table_item *item;
  int i, j, symbols = 0;

  if (name == NULL) {
    print_error("Adding a NULL symbol to symbol table! Please submit a bug report!\n", ERROR_DIR);
    return FAILED;
  }

  if (node == NULL) {
    print_error("Adding a NULL node to symbol table! Please submit a bug report!\n", ERROR_DIR);
    return FAILED;
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
        print_error("Out of memory while enlarging symbol table!\n", ERROR_DIR);
        return FAILED;
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
      print_error("No empty slot for a new symbol in the symbol table even though there should be room for it! Please submit a bug report\n", ERROR_DIR);
      return FAILED;
    }
  }

  /* i is the index for the symbol in the symbol table */

  if (g_symbol_table[i] != NULL && g_symbol_table[i]->level == level) {
    snprintf(g_error_message, sizeof(g_error_message), "\"%s\" is already defined on this level.\n", name);
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  item = calloc(sizeof(struct symbol_table_item), 1);

  if (item == NULL) {
    print_error("Out of memory while allocating a new symbol table item.\n", ERROR_DIR);
    return FAILED;
  }

  item->label = calloc(strlen(name) + 1, 1);
  if (item->label == NULL) {
    print_error("Out of memory while allocating a new symbol table item.\n", ERROR_DIR);
    free(item);
    return FAILED;
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
