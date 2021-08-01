
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "printf.h"
#include "tree_node.h"
#include "struct_item.h"
#include "main.h"


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024], g_label[MAX_NAME_LENGTH + 1];
extern char *g_variable_types[9];
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


struct struct_item *find_struct_item_child(struct struct_item *s, char *name) {

  int i;
  
  if (s == NULL)
    return NULL;
  
  for (i = 0; i < s->added_children; i++) {
    if (strcmp(s->children[i]->name, name) == 0)
      return s->children[i];
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
  s->file_id = -1;
  s->line_number = -1;
  strcpy(s->name, name);
  s->struct_name[0] = 0;
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


static int _calculate_struct_item(struct struct_item *s) {

  int i, size = 0;

  if (s->type == STRUCT_ITEM_TYPE_STRUCT || s->type == STRUCT_ITEM_TYPE_UNION) {
    for (i = 0; i < s->added_children; i++) {
      struct struct_item *child = s->children[i];

      if (child->size == 0) {
        if (_calculate_struct_item(child) == FAILED)
          return FAILED;
      }

      if (s->type == STRUCT_ITEM_TYPE_STRUCT) {
        child->offset = s->size;
        s->size += child->size;
      }
      else if (s->type == STRUCT_ITEM_TYPE_UNION) {
        child->offset = 0;
        if (s->size < child->size)
          s->size = child->size;
      }
    }
  }
  else {
    /* calculate the unit size */
    if (s->variable_type == VARIABLE_TYPE_STRUCT || s->variable_type == VARIABLE_TYPE_UNION) {
      struct struct_item *item;

      /* find the struct/union */
      item = g_struct_items_first;
      while (item != NULL) {
        if (strcmp(item->name, s->struct_name) == 0)
          break;
        item = item->next;
      }

      if (item == NULL) {
        g_current_line_number = s->line_number;
        g_current_filename_id = s->file_id;
        snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", s->struct_name);
        print_error(g_error_message, ERROR_ERR);
        return FAILED;
      }

      if (s->pointer_depth > 0)
        size = 2;
      else {
        if (item->size == 0) {
          if (_calculate_struct_item(item) == FAILED)
            return FAILED;
        }

        size = item->size;
      }
    }
    else {
      if (s->pointer_depth > 0)
        size = 2;
      else
        size = get_variable_type_size(s->variable_type) / 8;
    }
  }
  
  if (s->type == STRUCT_ITEM_TYPE_ITEM)
    s->size = size;
  else if (s->type == STRUCT_ITEM_TYPE_ARRAY)
    s->size = size * s->array_items;

  return SUCCEEDED;
}


int calculate_struct_items(void) {

  struct struct_item *s;

  /* calculate offsets and sizes in struct items */

  s = g_struct_items_first;
  while (s != NULL) {
    if (s->size == 0) {
      if (_calculate_struct_item(s) == FAILED)
        return FAILED;
    }
    s = s->next;
  }
  
  return SUCCEEDED;
}


static int _print_struct_item(struct struct_item *s, int indentation) {

  int i;

  if (s->type == STRUCT_ITEM_TYPE_STRUCT || s->type == STRUCT_ITEM_TYPE_UNION) {
    if (indentation == 0) {
      if (s->type == STRUCT_ITEM_TYPE_STRUCT)
        fprintf(stderr, "struct %s { // SIZE %d\n", s->name, s->size);
      else
        fprintf(stderr, "union %s { // SIZE %d\n", s->name, s->size);
    }
    else {
      for (i = 0; i < indentation; i++)
        fprintf(stderr, " ");
      
      if (s->type == STRUCT_ITEM_TYPE_STRUCT)
        fprintf(stderr, "struct { // SIZE %d OFFSET %d\n", s->size, s->offset);
      else
        fprintf(stderr, "union { // SIZE %d OFFSET %d\n", s->size, s->offset);
    }

    for (i = 0; i < s->added_children; i++)
      _print_struct_item(s->children[i], indentation + 2);

    if (indentation == 0)
      fprintf(stderr, "};\n");
    else {
      for (i = 0; i < indentation; i++)
        fprintf(stderr, " ");

      if (s->name[0] == 0)
        fprintf(stderr, "};\n");
      else
        fprintf(stderr, "} %s;\n", s->name);
    }
  }
  else {
    for (i = 0; i < indentation; i++)
      fprintf(stderr, " ");
          
    if (s->variable_type == VARIABLE_TYPE_STRUCT)
      fprintf(stderr, "struct %s ", s->struct_name);
    else if (s->variable_type == VARIABLE_TYPE_UNION)
      fprintf(stderr, "union %s ", s->struct_name);
    else
      fprintf(stderr, "%s ", g_variable_types[s->variable_type]);

    for (i = 0; i < s->pointer_depth; i++)
      fprintf(stderr, "*");
    
    fprintf(stderr, "%s", s->name);

    if (s->type == STRUCT_ITEM_TYPE_ITEM)
      fprintf(stderr, "; // SIZE %d OFFSET %d\n", s->size, s->offset);
    else
      fprintf(stderr, "[%d] // SIZE %d OFFSET %d;\n", s->array_items, s->size, s->offset);
  }
  
  return SUCCEEDED;
}


int print_struct_items(void) {

  struct struct_item *s;

  s = g_struct_items_first;
  while (s != NULL) {
    if (_print_struct_item(s, 0) == FAILED)
      return FAILED;
    s = s->next;
  }
  
  return SUCCEEDED;
}
