
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "printf.h"
#include "struct_item.h"
#include "main.h"


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024], g_label[MAX_NAME_LENGTH + 1];
extern int g_current_filename_id, g_current_line_number;

struct struct_item *g_struct_items_first = NULL, *g_struct_items_last = NULL;


int free_struct_item(struct struct_item *s) {

  int i;

  for (i = 0; i < s->added_children; i++)
    free_struct_item(s->children[i]);

  free(s->children);
  free(s);

  return SUCCEEDED;
}


int free_struct_items(void) {

  struct struct_item *s, *next;

  s = g_struct_items_first;
  while (s != NULL) {
    next = s->next;
    free_struct_item(s);
    s = next;
  }

  return SUCCEEDED;
}


int add_struct_item(struct struct_item *s) {

  s->next = NULL;
  
  if (g_struct_items_first == NULL) {
    g_struct_items_first = s;
    g_struct_items_last = s;
  }
  else {
    g_struct_items_last->next = s;
    g_struct_items_last = s;
  }

  return SUCCEEDED;
}


struct struct_item *find_struct_item(char *name) {

  struct struct_item *s;

  s = g_struct_items_first;
  while (s != NULL) {
    if (strcmp(s->name, name) == 0)
      return s;
    s = s->next;
  }

  return NULL;
}


struct struct_item *allocate_struct_item(char *name, int type) {

  struct struct_item *s;

  s = (struct struct_item *)calloc(sizeof(struct struct_item), 1);
  if (s == NULL) {
    print_error("Out of memory error while allocating a new struct_item.\n", ERROR_DIR);
    return FAILED;
  }

  s->type = type;
  s->variable_type = VARIABLE_TYPE_NONE;
  s->pointer_depth = 0;
  s->array_items = 0;
  strcpy(s->name, name);
  s->offset = 0;
  s->size = 0;
  s->children = NULL;
  s->max_children = 0;
  s->added_children = 0;
  s->next = NULL;

  return s;
}


int struct_item_add_child(struct struct_item *s, struct struct_item *child) {

  if (s->added_children >= s->max_children) {
    if (s->added_children == 0)
      s->children = (struct struct_item **)calloc(sizeof(struct struct_item *) * (s->max_children + 8), 1);
    else
      s->children = (struct struct_item **)realloc(s->children, sizeof(struct struct_item *) * (s->max_children + 8));

    if (s->children == NULL) {
      print_error("Out of memory error while allocating children for a struct_item.\n", ERROR_DIR);
      return FAILED;
    }

    s->max_children += 8;
  }

  s->children[s->added_children++] = child;

  return SUCCEEDED;
}
