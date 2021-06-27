
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#include "defines.h"

#include "main.h"
#include "include_file.h"
#include "parse.h"
#include "pass_1.h"
#include "stack.h"
#include "printf.h"
#include "definitions.h"


extern int g_verbose_mode, g_source_pointer, g_ss, g_input_float_mode, g_parsed_int, g_open_files, g_expect_calculations;
extern int g_latest_stack;
extern double g_parsed_double;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern struct stack *g_stacks_first, *g_stacks_tmp, *g_stacks_last, *g_stacks_header_first, *g_stacks_header_last;
extern char g_xyz[512], *g_buffer, g_tmp[4096], g_label[MAX_NAME_LENGTH + 1];
extern struct active_file_info *g_active_file_info_first, *g_active_file_info_last, *g_active_file_info_tmp;
extern struct file_name_info *g_file_name_info_first, *g_file_name_info_last, *g_file_name_info_tmp;

char g_current_directive[MAX_NAME_LENGTH + 1];

struct token *g_token_first = NULL, *g_token_last = NULL;


int pass_1(void) {

  int q;

  if (g_verbose_mode == ON)
    printf("Pass 1...\n");

  /* start from the very first character, this is the index to the source file we are about to parse... */
  g_source_pointer = 0;

  while (1) {
    q = get_next_token();
    if (q == FAILED)
      break;

    /*
    if (q == GET_NEXT_TOKEN_INT)
      fprintf(stderr, "%d: TOKEN: %d\n", q, g_parsed_int);
    else if (q == GET_NEXT_TOKEN_DOUBLE)
      fprintf(stderr, "%d: TOKEN: %f\n", q, g_parsed_double);
    else if (q == GET_NEXT_TOKEN_STACK)
      fprintf(stderr, "%d: TOKEN: %d\n", q, g_latest_stack);
    else
      fprintf(stderr, "%d: TOKEN: %s\n", q, g_tmp);
    */

    q = evaluate_token(q);

    if (q == SUCCEEDED)
      continue;
    else if (q == EVALUATE_TOKEN_EOP) {
      /* add 8 no ops so that parsing later will be easier (no need to check for null pointers all the time */
      for (q = 0; q < 8; q++)
        add_token(TOKEN_ID_NO_OP, 0, 0.0, NULL);
      return SUCCEEDED;
    }
    else if (q == FAILED) {
      snprintf(g_error_message, sizeof(g_error_message), "Couldn't parse \"%s\".\n", g_tmp);
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }
    else {
      printf("PASS_1: Internal error, unknown return type %d.\n", q);
      return FAILED;
    }
  }

  return FAILED;
}


int add_token(int id, int value, double value_double, char *label) {

  struct token *t;

  t = calloc(sizeof(struct token), 1);
  if (t == NULL) {
    print_error("Out of memory while allocating a new token.\n", ERROR_DIR);
    return FAILED;
  }

  t->id = id;
  t->value = value;
  t->value_double = value_double;
  t->next = NULL;
  if (g_active_file_info_last != NULL) {
    t->file_id = g_active_file_info_last->filename_id;
    t->line_number = g_active_file_info_last->line_current;
  }
  else {
    t->file_id = -1;
    t->line_number = -1;
  }

  if (label == NULL)
    t->label = NULL;
  else {
    t->label = calloc(strlen(label)+1, 1);
    if (t->label == NULL) {
      print_error("Out of memory while allocating a new token.\n", ERROR_DIR);
      return FAILED;
    }
    strncpy(t->label, label, strlen(label));
  }

  if (g_token_first == NULL) {
    g_token_first = t;
    g_token_last = t;
  }
  else {
    g_token_last->next = t;
    g_token_last = t;
  }

  return SUCCEEDED;
}


int evaluate_token(int type) {

  int q;

  /* a symbol */
  if (type == GET_NEXT_TOKEN_SYMBOL) {
    int value = (int)g_tmp[0];
    
    if (g_ss == 2) {
      /* turn a two char symbol into a definition value */
      if (g_tmp[0] == '&' && g_tmp[1] == '&')
        value = SYMBOL_LOGICAL_AND;
      else if (g_tmp[0] == '|' && g_tmp[1] == '|')
        value = SYMBOL_LOGICAL_OR;
      else if (g_tmp[0] == '<' && g_tmp[1] == '=')
        value = SYMBOL_LTE;
      else if (g_tmp[0] == '>' && g_tmp[1] == '=')
        value = SYMBOL_GTE;
      else if (g_tmp[0] == '=' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL;
      else if (g_tmp[0] == '!' && g_tmp[1] == '=')
        value = SYMBOL_UNEQUAL;
      else if (g_tmp[0] == '<' && g_tmp[1] == '<')
        value = SYMBOL_SHIFT_LEFT;
      else if (g_tmp[0] == '>' && g_tmp[1] == '>')
        value = SYMBOL_SHIFT_RIGHT;
      else if (g_tmp[0] == '+' && g_tmp[1] == '+')
        value = SYMBOL_INCREMENT;
      else if (g_tmp[0] == '-' && g_tmp[1] == '-')
        value = SYMBOL_DECREMENT;
      else {
        snprintf(g_error_message, sizeof(g_error_message), "Unhandled two char symbol \"%c%c\"! Please submit a bug report!\n", g_tmp[0], g_tmp[1]);
        print_error(g_error_message, ERROR_ERR);
        return FAILED;
      }
    }

    return add_token(TOKEN_ID_SYMBOL, value, 0.0, NULL);
  }

  /* a double */
  if (type == GET_NEXT_TOKEN_DOUBLE)
    return add_token(TOKEN_ID_VALUE_DOUBLE, 0, g_parsed_double, NULL);

  /* an int */
  if (type == GET_NEXT_TOKEN_INT)
    return add_token(TOKEN_ID_VALUE_INT, g_parsed_int, 0.0, NULL);

  /* a string */
  if (type == GET_NEXT_TOKEN_STRING) {
    /* check reserved strings */
    
    /* int8 */
    if (strcaselesscmp(g_tmp, "int8") == 0)
      return add_token(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_INT8, 0.0, NULL);

    /* uint8 */
    if (strcaselesscmp(g_tmp, "uint8") == 0)
      return add_token(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_UINT8, 0.0, NULL);

    /* int16 */
    if (strcaselesscmp(g_tmp, "int16") == 0)
      return add_token(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_INT16, 0.0, NULL);

    /* uint16 */
    if (strcaselesscmp(g_tmp, "uint16") == 0)
      return add_token(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_UINT16, 0.0, NULL);

    /* void */
    if (strcaselesscmp(g_tmp, "void") == 0)
      return add_token(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_VOID, 0.0, NULL);

    /* return */
    if (strcaselesscmp(g_tmp, "return") == 0)
      return add_token(TOKEN_ID_RETURN, 0, 0.0, NULL);

    /* else */
    if (strcaselesscmp(g_tmp, "else") == 0)
      return add_token(TOKEN_ID_ELSE, 0, 0.0, NULL);

    /* if */
    if (strcaselesscmp(g_tmp, "if") == 0)
      return add_token(TOKEN_ID_IF, 0, 0.0, NULL);

    /* while */
    if (strcaselesscmp(g_tmp, "while") == 0)
      return add_token(TOKEN_ID_WHILE, 0, 0.0, NULL);

    /* for */
    if (strcaselesscmp(g_tmp, "for") == 0)
      return add_token(TOKEN_ID_FOR, 0, 0.0, NULL);
    
    /* break */
    if (strcaselesscmp(g_tmp, "break") == 0)
      return add_token(TOKEN_ID_BREAK, 0, 0.0, NULL);

    /* continue */
    if (strcaselesscmp(g_tmp, "continue") == 0)
      return add_token(TOKEN_ID_CONTINUE, 0, 0.0, NULL);
    
    /* switch */
    if (strcaselesscmp(g_tmp, "switch") == 0)
      return add_token(TOKEN_ID_SWITCH, 0, 0.0, NULL);

    /* case */
    if (strcaselesscmp(g_tmp, "case") == 0)
      return add_token(TOKEN_ID_CASE, 0, 0.0, NULL);

    /* default */
    if (strcaselesscmp(g_tmp, "default") == 0)
      return add_token(TOKEN_ID_DEFAULT, 0, 0.0, NULL);
    
    /* .DEFINE/.DEF/.EQU */
    if (strcaselesscmp(g_tmp, ".DEFINE") == 0 || strcaselesscmp(g_tmp, ".DEF") == 0 || strcaselesscmp(g_tmp, ".EQU") == 0)
      return add_token(TOKEN_ID_DEFINE, 0, 0.0, g_tmp);
      
    /* .CHANGEFILE (INTERNAL) */
    if (strcaselesscmp(g_tmp, ".CHANGEFILE") == 0) {
      q = get_next_token();
      if (q != GET_NEXT_TOKEN_INT) {
        print_error("Internal error in (internal) .CHANGEFILE. Please submit a bug report...\n", ERROR_DIR);
        return FAILED;
      }

      g_active_file_info_tmp = calloc(sizeof(struct active_file_info), 1);
      if (g_active_file_info_tmp == NULL) {
        snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying allocate error tracking data structure.\n");
        print_error(g_error_message, ERROR_DIR);
        return FAILED;
      }
      g_active_file_info_tmp->next = NULL;

      if (g_active_file_info_first == NULL) {
        g_active_file_info_first = g_active_file_info_tmp;
        g_active_file_info_last = g_active_file_info_tmp;
        g_active_file_info_tmp->prev = NULL;
      }
      else {
        g_active_file_info_tmp->prev = g_active_file_info_last;
        g_active_file_info_last->next = g_active_file_info_tmp;
        g_active_file_info_last = g_active_file_info_tmp;
      }

      g_active_file_info_tmp->line_current = 0;
      g_active_file_info_tmp->filename_id = g_parsed_int;
      
      g_open_files++;

      if (compare_next_token("NAMESPACE") == SUCCEEDED) {
        get_next_token();

        q = get_next_token();

        if (q != GET_NEXT_TOKEN_STRING) {
          print_error("Internal error: Namespace string is missing.\n", ERROR_DIR);
          return FAILED;
        }

        strcpy(g_active_file_info_tmp->namespace, g_label);
      }
      else if (compare_next_token("NONAMESPACE") == SUCCEEDED) {
        get_next_token();

        g_active_file_info_tmp->namespace[0] = 0;
      }
      else {
        print_error("Internal error: NAMESPACE/NONAMESPACE is missing.\n", ERROR_DIR);
        return FAILED;
      }

      return SUCCEEDED;
    }
  
    /* .E (INTERNAL) */
    if (strcaselesscmp(g_tmp, ".E") == 0) {
      if (g_active_file_info_last != NULL) {
        g_active_file_info_tmp = g_active_file_info_last;
        g_active_file_info_last = g_active_file_info_last->prev;
        free(g_active_file_info_tmp);

        if (g_active_file_info_last == NULL)
          g_active_file_info_first = NULL;
      }

      /* fix the line */
      if (g_active_file_info_last != NULL)
        g_active_file_info_last->line_current--;

      g_open_files--;
      if (g_open_files == 0)
        return EVALUATE_TOKEN_EOP;
      
      return SUCCEEDED;
    }

    /* not a known string -> add it as a string token */
    return add_token(TOKEN_ID_VALUE_STRING, 0, 0.0, g_tmp);
  }

  return FAILED;
}
