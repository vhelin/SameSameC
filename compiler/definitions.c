
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "definitions.h"
#include "main.h"
#include "printf.h"


extern int g_commandline_parsing;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];

struct definition *g_definitions_first = NULL;


int add_a_new_definition(char *name, double value, char *string, int type, int size) {

  struct definition *d;

  d = calloc(sizeof(struct definition), 1);
  if (d == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to add a new definition (\"%s\").\n", name);
    if (g_commandline_parsing == OFF)
      print_error(g_error_message, ERROR_DIR);
    else
      fprintf(stderr, "ADD_A_NEW_DEFINITION: %s", g_error_message);
    return FAILED;
  }

  strcpy(d->alias, name);
  d->type = type;

  if (type == DEFINITION_TYPE_VALUE)
    d->value = value;
  else if (type == DEFINITION_TYPE_STACK)
    d->value = value;
  else if (type == DEFINITION_TYPE_STRING || type == DEFINITION_TYPE_ADDRESS_LABEL) {
    memcpy(d->string, string, size);
    d->string[size] = 0;
    d->size = size;
  }

  d->next = g_definitions_first;
  g_definitions_first = d;
  
  return SUCCEEDED;
}


void definition_get(char *label, struct definition **definition) {

  struct definition *d = g_definitions_first;

  while (d != NULL) {
    if (strcmp(d->alias, label) == 0) {
      *definition = d;
      return;
    }
    d = d->next;
  }

  *definition = NULL;
}


int redefine(char *name, double value, char *string, int type, int size) {

  struct definition *d;
  
  definition_get(name, &d);
  
  /* it wasn't defined previously */
  if (d == NULL)
    return add_a_new_definition(name, value, string, type, size);

  d->type = type;

  if (type == DEFINITION_TYPE_VALUE)
    d->value = value;
  else if (type == DEFINITION_TYPE_STACK)
    d->value = value;
  else if (type == DEFINITION_TYPE_STRING || type == DEFINITION_TYPE_ADDRESS_LABEL) {
    memcpy(d->string, string, size);
    d->string[size] = 0;
    d->size = size;
  }

  return SUCCEEDED;
}


int undefine(char *name) {

  struct definition *d = g_definitions_first, *previous = NULL;

  while (d != NULL) {
    if (strcmp(d->alias, name) == 0) {
      /* found it! */
      struct definition *next = d->next;
      
      if (previous == NULL) {
        free(g_definitions_first);
        g_definitions_first = next;
      }
      else {
        free(d);
        previous->next = next;
      }

      return SUCCEEDED;
    }
    
    previous = d;
    d = d->next;
  }

  return FAILED;
}
