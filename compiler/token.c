
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "printf.h"
#include "main.h"
#include "token.h"
#include "include_file.h"


extern struct active_file_info *g_active_file_info_first, *g_active_file_info_last, *g_active_file_info_tmp;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern int g_inside_define;
extern char *g_variable_types[9];
extern char *g_two_char_symbols[17];

struct token *g_token_first = NULL, *g_token_last = NULL, *g_latest_token = NULL;

static char g_token_in_simple_form[MAX_NAME_LENGTH + 1];


char *get_token_simple(struct token *t) {

  if (t->id == TOKEN_ID_VARIABLE_TYPE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", g_variable_types[t->value]);
  else if (t->id == TOKEN_ID_SYMBOL) {
    if (t->value <= SYMBOL_POINTER)
      snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", g_two_char_symbols[t->value]);
    else
      snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%c'", t->value);
  }
  else if (t->id == TOKEN_ID_VALUE_INT)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%d'", t->value);
  else if (t->id == TOKEN_ID_VALUE_DOUBLE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%f'", t->value_double);
  else if (t->id == TOKEN_ID_VALUE_STRING)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", t->label);
  else if (t->id == TOKEN_ID_RETURN)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "return");
  else if (t->id == TOKEN_ID_NO_OP)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "NO-OP");
  else if (t->id == TOKEN_ID_ELSE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "else");
  else if (t->id == TOKEN_ID_IF)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "if");
  else if (t->id == TOKEN_ID_WHILE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "while");
  else if (t->id == TOKEN_ID_FOR)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "for");
  else if (t->id == TOKEN_ID_BREAK)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "break");
  else if (t->id == TOKEN_ID_CONTINUE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "continue");
  else if (t->id == TOKEN_ID_SWITCH)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "switch");
  else if (t->id == TOKEN_ID_CASE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "case");
  else if (t->id == TOKEN_ID_DEFAULT)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "default");
  else if (t->id == TOKEN_ID_HINT)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", t->label);
  else
    fprintf(stderr, "%s:%d: get_token_simple(): Unknown token %d!\n", get_file_name(t->file_id), t->line_number, t->id);
    
  return g_token_in_simple_form;
}


int token_add(int id, int value, double value_double, char *label) {

  struct token *t;

  t = calloc(sizeof(struct token), 1);
  if (t == NULL)
    return print_error("token_add(): Out of memory while allocating a new token.\n", ERROR_DIR);

  t->id = id;
  t->value = value;
  t->value_double = value_double;
  t->next = NULL;

  if (g_active_file_info_last != NULL) {
    t->file_id = g_active_file_info_last->filename_id;
    t->line_number = g_active_file_info_last->line_current;
  }
  else {
    /*
    if (id != TOKEN_ID_NO_OP) {
      if (label == NULL)
        snprintf(g_error_message, sizeof(g_error_message), "We are adding a token ID %d to the system, but there is no open file! Please submit a bug report!\n", id);
      else
        snprintf(g_error_message, sizeof(g_error_message), "We are adding token \"%s\" to the system, but there is no open file! Please submit a bug report!\n", label);
      print_error(g_error_message, ERROR_WRN);
    }
    */
    
    t->file_id = -1;
    t->line_number = -1;
  }

  if (id == TOKEN_ID_BYTES) {
    /* NOTE! in the case of TOKEN_ID_BYTES we'll capture label */
    t->label = label;
  }
  else {
    if (label == NULL)
      t->label = NULL;
    else {
      t->label = calloc(strlen(label)+1, 1);
      if (t->label == NULL) {
        token_free(t);
        return print_error("Out of memory while allocating a new token.\n", ERROR_DIR);
      }
      strncpy(t->label, label, strlen(label));
    }
  }

  if (g_inside_define == NO) {
    if (g_token_first == NULL) {
      g_token_first = t;
      g_token_last = t;
    }
    else {
      g_token_last->next = t;
      g_token_last = t;
    }
  }

  g_latest_token = t;

  return SUCCEEDED;
}


int token_clone_and_add(struct token *t) {

  struct token *clone = token_clone(t);

  if (clone == NULL)
    return FAILED;
  
  clone->next = NULL;
  
  if (g_inside_define == NO) {
    if (g_token_first == NULL) {
      g_token_first = clone;
      g_token_last = clone;
    }
    else {
      g_token_last->next = clone;
      g_token_last = clone;
    }
  }

  g_latest_token = clone;

  return SUCCEEDED;
}


struct token *token_clone(struct token *t) {

  struct token *clone;
  
  clone = calloc(sizeof(struct token), 1);
  if (clone == NULL) {
    print_error("token_clone(): Out of memory while allocating a new token.\n", ERROR_DIR);
    return NULL;
  }

  if (t->label != NULL) {
    clone->label = calloc(t->value, 1);
    if (clone->label == NULL) {
      print_error("token_clone(): Out of memory while allocating a new token.\n", ERROR_DIR);
      free(clone);
      return NULL;
    }

    memcpy(clone->label, t->label, t->value);
  }
  else
    clone->label = NULL;

  clone->id = t->id;
  clone->value = t->value;
  clone->value_double = t->value_double;
  clone->file_id = t->file_id;
  clone->line_number = t->line_number;

  /* NOTE! */
  clone->next = NULL;

  return clone;
}


void token_free(struct token *t) {

  free(t->label);
  free(t);
}
