
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "definition.h"
#include "main.h"
#include "printf.h"
#include "token.h"


extern int g_commandline_parsing, g_current_filename_id, g_current_line_number;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern struct token *g_latest_token;

struct definition *g_definitions_first = NULL;


struct definition *define(char *name) {

  struct definition *d;

  d = calloc(sizeof(struct definition), 1);
  if (d == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to add a new definition (\"%s\").\n", name);
    if (g_commandline_parsing == OFF)
      print_error(g_error_message, ERROR_DIR);
    else
      fprintf(stderr, "ADD_A_NEW_DEFINITION: %s", g_error_message);
    return NULL;
  }

  strcpy(d->name, name);
  d->had_parenthesis = NO;
  d->parenthesis = 0;
  d->has_arguments = NO;
  d->arguments = 0;
  d->tokens_first = NULL;
  d->tokens_last = NULL;
  d->arguments_first = NULL;
  d->arguments_last = NULL;
  d->arguments_data_first = NULL;
  d->arguments_data_last = NULL;

  d->next = g_definitions_first;
  g_definitions_first = d;
  
  return d;
}


void definition_free(struct definition *d) {

  struct token *t;

  t = d->tokens_first;
  while (t != NULL) {
    struct token *next = t->next;
    token_free(t);
    t = next;
  }

  t = d->arguments_first;
  while (t != NULL) {
    struct token *next = t->next;
    token_free(t);
    t = next;
  }

  definition_free_arguments_data(d);
  free(d->arguments_data_first);
  free(d->arguments_data_last);

  free(d);
}


void definition_free_arguments_data(struct definition *d) {
  
  struct token *t;
  int i;

  for (i = 0; i < d->arguments; i++) {
    t = d->arguments_data_first[i];
    while (t != NULL) {
      struct token *next = t->next;

      token_free(t);
      t = next;
    }
    d->arguments_data_first[i] = NULL;
    d->arguments_data_last[i] = NULL;
  }
}


struct definition *definition_get(char *label) {

  struct definition *d = g_definitions_first;

  while (d != NULL) {
    if (strcmp(d->name, label) == 0)
      return d;
    d = d->next;
  }

  return NULL;
}


static int _collect_arguments_from_tokens(struct definition *d) {

  struct token *t, *next;
  int arguments = 0, i;

  t = d->tokens_first;

  if (t->id != TOKEN_ID_SYMBOL || t->value != '(') {
    g_current_filename_id = t->file_id;
    g_current_line_number = t->line_number;
    return print_error("_collect_arguments_from_tokens(): '(' should be the first token on the argument tokens list.\n", ERROR_DIR);
  }

  next = t->next;
  token_free(t);
  t = next;

  while (t != NULL) {
    if (t->id == TOKEN_ID_VALUE_STRING) {
      next = t->next;
      t->next = NULL;
      
      if (d->arguments_first == NULL) {
        d->arguments_first = t;
        d->arguments_last = t;
      }
      else {
        d->arguments_last->next = t;
        d->arguments_last = t;
      }

      arguments++;
      t = next;
    }
    else if (t->id == TOKEN_ID_SYMBOL && t->value == ',') {
      next = t->next;
      token_free(t);
      t = next;
      continue;
    }
    else if (t->id == TOKEN_ID_SYMBOL && t->value == ')' && t->next == NULL) {
      token_free(t);
      break;
    }
    else {
      g_current_filename_id = t->file_id;
      g_current_line_number = t->line_number;
      snprintf(g_error_message, sizeof(g_error_message), "_collect_arguments_from_tokens(): Unexpected token %s in the argument token list.\n", get_token_simple(t));
      return print_error(g_error_message, ERROR_ERR);
    }
    
    t = t->next;
  }

  d->tokens_first = NULL;
  d->tokens_last = NULL;

  d->arguments_data_first = calloc(sizeof(struct token *) * arguments, 1);
  if (d->arguments_data_first == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to allocate argument data list for definition \"%s\".\n", d->name);
    return print_error(g_error_message, ERROR_DIR);
  }

  d->arguments_data_last = calloc(sizeof(struct token *) * arguments, 1);
  if (d->arguments_data_last == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to allocate argument data list for definition \"%s\".\n", d->name);
    return print_error(g_error_message, ERROR_DIR);
  }

  for (i = 0; i < arguments; i++) {
    d->arguments_data_first[i] = NULL;
    d->arguments_data_last[i] = NULL;
  }

  d->arguments = arguments;

  return SUCCEEDED;
}


int definition_add_token(struct definition *d, struct token *t) {

  t->next = NULL;

  if (d->had_parenthesis == YES && d->parenthesis == 0) {
    /* #define was of form "define abc(...)" and we are still getting tokens ->
       the first tokens inside the parenthesis were arguments! let's convert them
       to arguments... */
    if (_collect_arguments_from_tokens(d) == FAILED)
      return FAILED;
  }
  
  /* are the previous tokens arguments? */
  if (g_latest_token != NULL) {
    if (g_latest_token->id == TOKEN_ID_SYMBOL && g_latest_token->value == '(') {
      d->parenthesis++;
      d->had_parenthesis = YES;
    }
    else if (g_latest_token->id == TOKEN_ID_SYMBOL && g_latest_token->value == ')')
      d->parenthesis--;
  }
  
  if (d->tokens_first == NULL) {
    d->tokens_first = t;
    d->tokens_last = t;
  }
  else {
    d->tokens_last->next = t;
    d->tokens_last = t;
  }
  
  return SUCCEEDED;
}


int undefine(char *name) {

  struct definition *d = g_definitions_first, *previous = NULL;

  while (d != NULL) {
    if (strcmp(d->name, name) == 0) {
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
