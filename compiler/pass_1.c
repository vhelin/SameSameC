
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
#include "definition.h"
#include "inline_asm.h"
#include "token.h"


extern int g_verbose_mode, g_source_pointer, g_ss, g_input_float_mode, g_parsed_int, g_open_files, g_expect_calculations;
extern int g_latest_stack, g_get_next_token_return_linefeed;
extern double g_parsed_double;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern struct stack *g_stacks_first, *g_stacks_tmp, *g_stacks_last, *g_stacks_header_first, *g_stacks_header_last;
extern char g_xyz[512], *g_buffer, g_tmp[4096], g_label[MAX_NAME_LENGTH + 1];
extern struct active_file_info *g_active_file_info_first, *g_active_file_info_last, *g_active_file_info_tmp;
extern struct file_name_info *g_file_name_info_first, *g_file_name_info_last, *g_file_name_info_tmp;
extern struct token *g_latest_token, *g_token_first, *g_token_last, *g_token_current;

char g_current_directive[MAX_NAME_LENGTH + 1], g_skip_elifs[256];

int g_inside_define = NO, g_ifdef_index = -1;


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
      /* add 8 no ops so that parsing later will be easier (no need to check for null pointers all the time) */
      for (q = 0; q < 8; q++)
        token_add(TOKEN_ID_NO_OP, 0, 0.0, NULL);
      return SUCCEEDED;
    }
    else if (q == FAILED) {
      snprintf(g_error_message, sizeof(g_error_message), "Couldn't parse \"%s\".\n", g_tmp);
      return print_error(g_error_message, ERROR_ERR);
    }
    else {
      snprintf(g_error_message, sizeof(g_error_message), "PASS_1: Internal error, unknown return type %d.\n", q);
      return print_error(g_error_message, ERROR_ERR);
    }
  }

  return FAILED;
}


int create_mainmain_tokens(void) {

  int sum = 0, q;
  
  /* we'll call this from pass_2.c when we've parsed function "void main(void)"... */

  /* generate "void mainmain(void)" */
  sum += token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_VOID, 0.0, NULL);  
  sum += token_add(TOKEN_ID_VALUE_STRING, (int)(strlen("mainmain") + 1), 0.0, "mainmain");
  sum += token_add(TOKEN_ID_SYMBOL, '(', 0.0, NULL);
  sum += token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_VOID, 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, ')', 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, '{', 0.0, NULL);
  sum += token_add(TOKEN_ID_VALUE_STRING, (int)(strlen("main") + 1), 0.0, "main");
  sum += token_add(TOKEN_ID_SYMBOL, '(', 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, ')', 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, ';', 0.0, NULL);
  sum += token_add(TOKEN_ID_WHILE, 0, 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, '(', 0.0, NULL);
  sum += token_add(TOKEN_ID_VALUE_INT, 1, 0.0, NULL);  
  sum += token_add(TOKEN_ID_SYMBOL, ')', 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, '{', 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, '}', 0.0, NULL);
  sum += token_add(TOKEN_ID_SYMBOL, '}', 0.0, NULL);
  
  if (sum != 17)
    return FAILED;

  /* add 8 no ops so that parsing later will be easier (no need to check for null pointers all the time) */
  for (q = 0; q < 8; q++)
    token_add(TOKEN_ID_NO_OP, 0, 0.0, NULL);
  
  return SUCCEEDED;
}


static int _parse_filesize(void) {

  int token, file_size;
  FILE *f;

  token = get_next_token();
  if (token != GET_NEXT_TOKEN_SYMBOL || g_tmp[0] != '(')
    return print_error("\"__filesize()\" is missing its first parenthesis.\n", ERROR_DIR);

  token = get_next_token();
  if (token != GET_NEXT_TOKEN_STRING_DATA)
    return print_error("\"__filesize()\" needs a file name.\n", ERROR_DIR);

  f = fopen(g_tmp, "rb");
  if (f == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "\"__filesize() cannot open file \"%s\".\n", g_tmp);
    return print_error(g_error_message, ERROR_DIR);
  }

  fseek(f, 0, SEEK_END);
  file_size = (int)ftell(f);
  fclose(f);

  token = get_next_token();
  if (token != GET_NEXT_TOKEN_SYMBOL || g_tmp[0] != ')')
    return print_error("\"__filesize()\" is missing its second parenthesis.\n", ERROR_DIR);

  return token_add(TOKEN_ID_VALUE_INT, file_size, 0.0, NULL);
}


static int _parse_incbin(void) {

  int token, file_size;
  unsigned char *data;
  FILE *f;

  token = get_next_token();
  if (token != GET_NEXT_TOKEN_SYMBOL || g_tmp[0] != '(')
    return print_error("\"__incbin()\" is missing its first parenthesis.\n", ERROR_DIR);

  token = get_next_token();
  if (token != GET_NEXT_TOKEN_STRING_DATA)
    return print_error("\"__incbin()\" needs a file name.\n", ERROR_DIR);

  f = fopen(g_tmp, "rb");
  if (f == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "\"__incbin() cannot open file \"%s\".\n", g_tmp);
    return print_error(g_error_message, ERROR_DIR);
  }

  fseek(f, 0, SEEK_END);
  file_size = (int)ftell(f);
  fseek(f, 0, SEEK_SET);

  data = calloc(file_size, 1);
  if (data == NULL) {
    fclose(f);
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while allocating memory for __incbin()'ed \"%s\".\n", g_tmp);
    return print_error(g_error_message, ERROR_DIR);
  }

  fread(data, 1, file_size, f);
  fclose(f);

  token = get_next_token();
  if (token != GET_NEXT_TOKEN_SYMBOL || g_tmp[0] != ')')
    return print_error("\"__incbin()\" is missing its second parenthesis.\n", ERROR_DIR);

  /* NOTE! in the case of TOKEN_ID_BYTES token_add() will eat the memory block "data" and it'll be assigned to a "token" */
  return token_add(TOKEN_ID_BYTES, file_size, 0.0, (char *)data);
}


static int _parse_asm(void) {

  struct inline_asm *ia;
  int token;
  
  token = get_next_token();
  if (token != GET_NEXT_TOKEN_SYMBOL || g_tmp[0] != '(')
    return print_error("\"__asm()\" is missing its first parenthesis.\n", ERROR_DIR);

  ia = inline_asm_add();
  if (ia == NULL)
    return FAILED;

  if (g_active_file_info_last != NULL) {
    ia->file_id = g_active_file_info_last->filename_id;
    ia->line_number = g_active_file_info_last->line_current;
  }
  
  if (token_add(TOKEN_ID_ASM, ia->id, 0.0, NULL) == FAILED)
    return FAILED;
  
  /* collect the asm lines */
  while (1) {
    token = get_next_token();

    if (token == GET_NEXT_TOKEN_STRING_DATA) {
      int line_number = -1;

      if (g_active_file_info_last != NULL)
        line_number = g_active_file_info_last->line_current;

      if (inline_asm_add_asm_line(ia, g_tmp, g_ss + 1, line_number) == FAILED)
        return FAILED;
    }
    else if (token == GET_NEXT_TOKEN_SYMBOL && g_tmp[0] == ',') {
    }
    else if (token == GET_NEXT_TOKEN_SYMBOL && g_tmp[0] == ')')
      break;
    else
      return print_error("Expected a string, ',' or ')', but got something else.\n", ERROR_DIR);
  }

  /* the end */
  token = get_next_token();
  if (token == GET_NEXT_TOKEN_SYMBOL && g_tmp[0] == ';')
    return SUCCEEDED;

  return print_error("Expected a ';', but got something else.\n", ERROR_DIR);
}


static int _parse_define(void) {

  struct definition *definition;
  int token_count = 0, q;

  q = get_next_token();
  if (q != GET_NEXT_TOKEN_STRING)
    return print_error("#define needs a definition name\n", ERROR_DIR);
      
  /* get rid of an old definition with the same name, if one exists */
  undefine(g_tmp);

  /*
    if (definition_get(g_tmp) != NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Definition \"%s\" already exists.\n", g_tmp);
    return print_error(g_error_message, ERROR_DIR);
    }
  */

  definition = define(g_tmp);

  if (definition == NULL)
    return FAILED;

  /* make get_next_token() return linefeeds */
  g_get_next_token_return_linefeed = YES;

  /* for token_add() */
  g_inside_define = YES;

  /* collect all tokens that make this definition */
  while (1) {
    q = get_next_token();

    if (q == FAILED)
      break;
    if (q == GET_NEXT_TOKEN_LINEFEED) {
      if (token_count == 0) {
        /* no tokens? */
        snprintf(g_error_message, sizeof(g_error_message), "Definition \"%s\" cannot be empty.\n", definition->name);
        return print_error(g_error_message, ERROR_DIR);
      }
          
      break;
    }

    token_count++;

    q = evaluate_token(q);
    if (q == FAILED)
      return FAILED;

    /* g_latest_token has the token evaluate_token() created */
    if (definition_add_token(definition, g_latest_token, token_count) == FAILED)
      return FAILED;
  }

  /* make get_next_token() not to return linefeeds */
  g_get_next_token_return_linefeed = NO;
  
  g_inside_define = NO;

  if (q == FAILED)
    return FAILED;
      
  return SUCCEEDED;
}


static int _parse_definition(void) {

  struct definition *definition;
  struct token *t;
  int q, argument;

  definition = definition_get(g_tmp);

  if (definition->arguments_first == NULL) {
    /* no arguments */
    t = definition->tokens_first;
    while (t != NULL) {
      token_clone_and_add(t);
      t = t->next;
    }
  }
  else {
    /* parse arguments */

    /* make get_next_token() return linefeeds */
    g_get_next_token_return_linefeed = YES;

    /* for token_add() inside evaluate_token() */
    g_inside_define = YES;

    q = get_next_token();

    if (q != GET_NEXT_TOKEN_SYMBOL || g_tmp[0] != '(')
      return print_error("_parse_definition(): Was expecting '(', but got something else instead.\n", ERROR_DIR);

    t = definition->arguments_first;
    argument = 0;
    while (1) {
      q = get_next_token();
      
      if (q == FAILED)
        return FAILED;
      if (q == GET_NEXT_TOKEN_LINEFEED) {
        snprintf(g_error_message, sizeof(g_error_message), "Definition \"%s\" is missing arguments.\n", definition->name);
        return print_error(g_error_message, ERROR_DIR);
      }
      if (q == GET_NEXT_TOKEN_SYMBOL && g_tmp[0] == ')') {
        if (t->next != NULL) {
          snprintf(g_error_message, sizeof(g_error_message), "Definition \"%s\" was not called with enough arguments.\n", definition->name);
          return print_error(g_error_message, ERROR_DIR);
        }

        break;
      }
      if (q == GET_NEXT_TOKEN_SYMBOL && g_tmp[0] == ',') {
        t = t->next;
        
        if (t == NULL) {
          snprintf(g_error_message, sizeof(g_error_message), "Trying to call definition \"%s\" with too many arguments.\n", definition->name);
          return print_error(g_error_message, ERROR_DIR);
        }

        argument++;

        continue;
      }

      q = evaluate_token(q);
      if (q == FAILED)
        return FAILED;

      g_latest_token->next = NULL;
      if (definition->arguments_data_first[argument] == NULL) {
        definition->arguments_data_first[argument] = g_latest_token;
        definition->arguments_data_last[argument] = g_latest_token;
      }
      else {
        definition->arguments_data_last[argument]->next = g_latest_token;
        definition->arguments_data_last[argument] = g_latest_token;
      }
    }

    /* make get_next_token() not to return linefeeds */
    g_get_next_token_return_linefeed = NO;
  
    g_inside_define = NO;

    /* expand the definition */
    t = definition->tokens_first;
    while (t != NULL) {
      int expanded = NO;
      
      if (t->id == TOKEN_ID_VALUE_STRING) {
        struct token *arg;
        
        /* is the token one of the arguments? */
        arg = definition->arguments_first;
        argument = 0;
        while (arg != NULL) {
          if (strcmp(arg->label, t->label) == 0)
            break;
          arg = arg->next;
          argument++;
        }

        /* yes! */
        if (arg != NULL) {
          struct token *t2;
          
          /* expand the argument */
          t2 = definition->arguments_data_first[argument];
          while (t2 != NULL) {
            token_clone_and_add(t2);
            t2 = t2->next;
          }

          expanded = YES;
        }
      }

      if (expanded == NO)
        token_clone_and_add(t);

      t = t->next;
    }
    
    /* free the arguments data as we've processed the definition call */
    definition_free_arguments_data(definition);
  }

  return SUCCEEDED;
}


static int _parse_if_elif(int *result) {

  struct token *token_1st = NULL, *token_last, *token_current, *t;
  int tokens = 0, q, file_id, line_number;
  
  /* make get_next_token() return linefeeds */
  g_get_next_token_return_linefeed = YES;

  /* store the currently last token */
  token_last = g_token_last;
  token_current = g_token_current;

  line_number = g_active_file_info_tmp->line_current;
  file_id = g_active_file_info_tmp->filename_id;

  while (1) {
    q = get_next_token();
      
    if (q == FAILED)
      return FAILED;
    /* the end of the condition? */
    if (q == GET_NEXT_TOKEN_LINEFEED)
      break;

    /* evaluate and add token to the tokens list */
    q = evaluate_token(q);

    if (tokens == 0) {
      /* this is the token where the condition starts */
      token_1st = g_latest_token;
    }

    tokens++;

    if (q == FAILED)
      return FAILED;
  }

  /* tokens of the condition have been created -> try to solve the condition */
  g_token_current = token_1st;
  
  if (stack_calculate_tokens(result) != SUCCEEDED) {
    g_active_file_info_tmp->line_current = line_number;
    g_active_file_info_tmp->filename_id = file_id;
    return print_error("_parse_if(): The condition must be solvable and the result must be an integer.\n", ERROR_DIR);
  }

  /* make get_next_token() not to return linefeeds */
  g_get_next_token_return_linefeed = NO;

  /* free the condition's tokens */
  while (token_1st != NULL) {
    t = token_1st->next;
    token_free(token_1st);
    token_1st = t;
  }

  /* repair g_token_last etc. */
  if (token_last == NULL) {
    /* the tokens of the condition were the first tokens */
    g_token_first = NULL;
    g_token_last = NULL;
    g_latest_token = NULL;
  }
  else {
    g_token_last = token_last;
    g_token_last->next = NULL;
    g_latest_token = token_last;
  }

  g_token_current = token_current;

  return SUCCEEDED;
}


static int _fast_forward_to_next_elif_else_endif(void) {

  int q, depth = 0, line_number, file_id;
  
  line_number = g_active_file_info_tmp->line_current;
  file_id = g_active_file_info_tmp->filename_id;

  while (1) {
    q = get_next_token();
    if (q == FAILED)
      return FAILED;

    if (q == GET_NEXT_TOKEN_STRING) {
      if (strcaselesscmp(g_tmp, "#if") == 0) {
	depth++;
      }
      else if (strcaselesscmp(g_tmp, "#else") == 0) {
	if (depth == 0)
	  return SUCCEEDED;
      }
      else if (strcaselesscmp(g_tmp, "#elif") == 0) {
	if (depth == 0)
	  return SUCCEEDED;
      }
      else if (strcaselesscmp(g_tmp, "#endif") == 0) {
	if (depth == 0)
	  return SUCCEEDED;
	depth--;
      }
      else if (strcaselesscmp(g_tmp, "@E") == 0) {
	g_active_file_info_tmp->line_current = line_number;
	g_active_file_info_tmp->filename_id = file_id;
	return print_error("#endif is missing!\n", ERROR_DIR);
      }
    }
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
      else if (g_tmp[0] == '-' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_SUB;
      else if (g_tmp[0] == '+' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_ADD;
      else if (g_tmp[0] == '*' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_MUL;
      else if (g_tmp[0] == '/' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_DIV;
      else if (g_tmp[0] == '|' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_OR;
      else if (g_tmp[0] == '&' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_AND;
      else if (g_tmp[0] == '%' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_MOD;
      else if (g_tmp[0] == '^' && g_tmp[1] == '=')
        value = SYMBOL_EQUAL_XOR;
      else if (g_tmp[0] == '-' && g_tmp[1] == '>')
        value = SYMBOL_POINTER;
      else {
        snprintf(g_error_message, sizeof(g_error_message), "Unhandled two character symbol \"%c%c\"! Please submit a bug report!\n", g_tmp[0], g_tmp[1]);
        return print_error(g_error_message, ERROR_ERR);
      }
    }
    else if (g_ss == 3) {
      /* turn a three char symbol into a definition value */
      if (g_tmp[0] == '>' && g_tmp[1] == '>' && g_tmp[2] == '=')
        value = SYMBOL_EQUAL_SHIFT_RIGHT;
      else if (g_tmp[0] == '<' && g_tmp[1] == '<' && g_tmp[2] == '=')
        value = SYMBOL_EQUAL_SHIFT_LEFT;
      else {
        snprintf(g_error_message, sizeof(g_error_message), "Unhandled three character symbol \"%c%c%c\"! Please submit a bug report!\n", g_tmp[0], g_tmp[1], g_tmp[2]);
        return print_error(g_error_message, ERROR_ERR);
      }
    }

    return token_add(TOKEN_ID_SYMBOL, value, 0.0, NULL);
  }

  /* a double */
  if (type == GET_NEXT_TOKEN_DOUBLE)
    return token_add(TOKEN_ID_VALUE_DOUBLE, 0, g_parsed_double, NULL);

  /* an int */
  if (type == GET_NEXT_TOKEN_INT)
    return token_add(TOKEN_ID_VALUE_INT, g_parsed_int, 0.0, NULL);

  /* a "string" */
  if (type == GET_NEXT_TOKEN_STRING_DATA) {
    char *tmp;

    tmp = calloc(g_ss + 1, 1);
    if (tmp == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Out of memory while allocating room for string \"%s\"\n", g_tmp);
      return print_error(g_error_message, ERROR_ERR);
    }

    memcpy(tmp, g_tmp, g_ss + 1);
    
    return token_add(TOKEN_ID_BYTES, g_ss + 1, -1.0, tmp);
  }
  
  /* a string */
  if (type == GET_NEXT_TOKEN_STRING) {
    /* check reserved strings */
    
    /* int8 */
    if (strcaselesscmp(g_tmp, "int8") == 0 || strcaselesscmp(g_tmp, "s8") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_INT8, 0.0, NULL);

    /* uint8 */
    if (strcaselesscmp(g_tmp, "uint8") == 0 || strcaselesscmp(g_tmp, "u8") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_UINT8, 0.0, NULL);

    /* int16 */
    if (strcaselesscmp(g_tmp, "int16") == 0 || strcaselesscmp(g_tmp, "s16") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_INT16, 0.0, NULL);

    /* uint16 */
    if (strcaselesscmp(g_tmp, "uint16") == 0 || strcaselesscmp(g_tmp, "u16") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_UINT16, 0.0, NULL);

    /* void */
    if (strcaselesscmp(g_tmp, "void") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_VOID, 0.0, NULL);

    /* const */
    if (strcaselesscmp(g_tmp, "const") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_CONST, 0.0, NULL);

    /* struct */
    if (strcaselesscmp(g_tmp, "struct") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_STRUCT, 0.0, NULL);

    /* union */
    if (strcaselesscmp(g_tmp, "union") == 0)
      return token_add(TOKEN_ID_VARIABLE_TYPE, VARIABLE_TYPE_UNION, 0.0, NULL);

    /* sizeof */
    if (strcaselesscmp(g_tmp, "sizeof") == 0)
      return token_add(TOKEN_ID_SIZEOF, 0, 0.0, NULL);

    /* return */
    if (strcaselesscmp(g_tmp, "return") == 0)
      return token_add(TOKEN_ID_RETURN, 0, 0.0, NULL);

    /* extern */
    if (strcaselesscmp(g_tmp, "extern") == 0)
      return token_add(TOKEN_ID_EXTERN, 0, 0.0, NULL);

    /* static */
    if (strcaselesscmp(g_tmp, "static") == 0)
      return token_add(TOKEN_ID_STATIC, 0, 0.0, NULL);

    /* else */
    if (strcaselesscmp(g_tmp, "else") == 0)
      return token_add(TOKEN_ID_ELSE, 0, 0.0, NULL);

    /* if */
    if (strcaselesscmp(g_tmp, "if") == 0)
      return token_add(TOKEN_ID_IF, 0, 0.0, NULL);

    /* while */
    if (strcaselesscmp(g_tmp, "while") == 0)
      return token_add(TOKEN_ID_WHILE, 0, 0.0, NULL);

    /* do */
    if (strcaselesscmp(g_tmp, "do") == 0)
      return token_add(TOKEN_ID_DO, 0, 0.0, NULL);

    /* for */
    if (strcaselesscmp(g_tmp, "for") == 0)
      return token_add(TOKEN_ID_FOR, 0, 0.0, NULL);
    
    /* break */
    if (strcaselesscmp(g_tmp, "break") == 0)
      return token_add(TOKEN_ID_BREAK, 0, 0.0, NULL);

    /* continue */
    if (strcaselesscmp(g_tmp, "continue") == 0)
      return token_add(TOKEN_ID_CONTINUE, 0, 0.0, NULL);
    
    /* switch */
    if (strcaselesscmp(g_tmp, "switch") == 0)
      return token_add(TOKEN_ID_SWITCH, 0, 0.0, NULL);

    /* case */
    if (strcaselesscmp(g_tmp, "case") == 0)
      return token_add(TOKEN_ID_CASE, 0, 0.0, NULL);

    /* default */
    if (strcaselesscmp(g_tmp, "default") == 0)
      return token_add(TOKEN_ID_DEFAULT, 0, 0.0, NULL);

    /* NULL */
    if (strcaselesscmp(g_tmp, "NULL") == 0)
      return token_add(TOKEN_ID_VALUE_INT, 0, 0.0, NULL);
    
    /* __pureasm */
    if (strcaselesscmp(g_tmp, "__pureasm") == 0)
      return token_add(TOKEN_ID_HINT, 0, 0.0, g_tmp);

    /* __org */
    if (strcaselesscmp(g_tmp, "__org") == 0)
      return token_add(TOKEN_ID_HINT, 0, 0.0, g_tmp);

    /* __orga */
    if (strcaselesscmp(g_tmp, "__orga") == 0)
      return token_add(TOKEN_ID_HINT, 0, 0.0, g_tmp);
    
    /* __asm() */
    if (strcaselesscmp(g_tmp, "__asm") == 0)
      return _parse_asm();
        
    /* __filesize() */
    if (strcaselesscmp(g_tmp, "__filesize") == 0)
      return _parse_filesize();

    /* __incbin() */
    if (strcaselesscmp(g_tmp, "__incbin") == 0)
      return _parse_incbin();

    /* #if */
    if (strcaselesscmp(g_tmp, "#if") == 0) {
      int result;

      while (1) {
	if (_parse_if_elif(&result) == FAILED)
	  return FAILED;

	if (result != 0) {
	  /* condition is true */
	  if (g_ifdef_index >= 255)
	    return print_error("Out of #ifdef stack.\n", ERROR_DIR);
	  
	  g_skip_elifs[++g_ifdef_index] = YES;

	  return SUCCEEDED;
	}
	else {
	  /* condition is false */
	  if (_fast_forward_to_next_elif_else_endif() == FAILED)
	    return FAILED;

	  if (strcaselesscmp(g_tmp, "#else") == 0) {
	    if (g_ifdef_index >= 255)
	      return print_error("Out of #ifdef stack.\n", ERROR_DIR);
	    
	    g_skip_elifs[++g_ifdef_index] = NO;

	    return SUCCEEDED;
	  }
	  else if (strcaselesscmp(g_tmp, "#endif") == 0)
	    return SUCCEEDED;
	}
      }
    }

    /* #else */
    if (strcaselesscmp(g_tmp, "#else") == 0) {
      if (g_ifdef_index < 0)
	return print_error("Encountered #else without parsing #if before it.\n", ERROR_DIR);
    
      if (g_skip_elifs[g_ifdef_index] == YES) {
	if (_fast_forward_to_next_elif_else_endif() == FAILED)
	  return FAILED;

	if (strcaselesscmp(g_tmp, "#endif") != 0) {
	  snprintf(g_error_message, sizeof(g_error_message), "Expected #endif, but got %s instead.\n", g_tmp);
	  return print_error(g_error_message, ERROR_DIR);
	}
      }

      g_ifdef_index--;

      return SUCCEEDED;
    }

    /* #elif */
    if (strcaselesscmp(g_tmp, "#elif") == 0) {
      if (g_ifdef_index < 0)
	return print_error("Encountered #elif without parsing #if before it.\n", ERROR_DIR);

      if (g_skip_elifs[g_ifdef_index] == YES) {
	while (1) {
	  if (_fast_forward_to_next_elif_else_endif() == FAILED)
	    return FAILED;

	  if (strcaselesscmp(g_tmp, "#endif") == 0) {
	    g_ifdef_index--;
	    return SUCCEEDED;
	  }
	}
      }
      else
	return print_error("We have an internal error with #elif skipping state. Please submit a bug report!\n", ERROR_DIR);
    }
    
    /* #endif */
    if (strcaselesscmp(g_tmp, "#endif") == 0) {
      if (g_ifdef_index < 0)
	return print_error("Encountered #endif without parsing #if before it.\n", ERROR_DIR);

      g_ifdef_index--;

      return SUCCEEDED;
    }
    
    /* #include */
    if (strcaselesscmp(g_tmp, "#include") == 0) {
      int include_size;
      
      q = get_next_token();
      if (q != GET_NEXT_TOKEN_STRING_DATA)
        return print_error("#include needs a file name.\n", ERROR_DIR);

      if (include_file(g_tmp, &include_size, NULL) == FAILED)
        return FAILED;

      return SUCCEEDED;
    }

    /* #undefine & #undef */
    if (strcaselesscmp(g_tmp, "#undefine") == 0 || strcaselesscmp(g_tmp, "#undef") == 0) {
      char name[MAX_NAME_LENGTH + 1];

      strcpy(name, g_tmp);

      q = get_next_token();
      if (q != GET_NEXT_TOKEN_STRING) {
        snprintf(g_error_message, sizeof(g_error_message), "%s needs a definition name.\n", name);
        return print_error(g_error_message, ERROR_DIR);
      }

      undefine(g_tmp);

      return SUCCEEDED;
    }
    
    /* #define */
    if (strcaselesscmp(g_tmp, "#define") == 0)
      return _parse_define();
      
    /* @CHANGEFILE (INTERNAL) */
    if (strcaselesscmp(g_tmp, "@CHANGEFILE") == 0) {
      q = get_next_token();
      if (q != GET_NEXT_TOKEN_INT)
        return print_error("Internal error in (internal) @CHANGEFILE. Please submit a bug report...\n", ERROR_DIR);

      g_active_file_info_tmp = calloc(sizeof(struct active_file_info), 1);
      if (g_active_file_info_tmp == NULL) {
        snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying allocate error tracking data structure.\n");
        return print_error(g_error_message, ERROR_DIR);
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

        if (q != GET_NEXT_TOKEN_STRING)
          return print_error("Internal error: Namespace string is missing.\n", ERROR_DIR);

        strcpy(g_active_file_info_tmp->namespace, g_label);
      }
      else if (compare_next_token("NONAMESPACE") == SUCCEEDED) {
        get_next_token();

        g_active_file_info_tmp->namespace[0] = 0;
      }
      else
        return print_error("Internal error: NAMESPACE/NONAMESPACE is missing.\n", ERROR_DIR);

      return SUCCEEDED;
    }
  
    /* @E (INTERNAL) */
    if (strcaselesscmp(g_tmp, "@E") == 0) {
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

    /* is the string a definition call? */
    if (definition_get(g_tmp) != NULL)
      return _parse_definition();
    
    /* not a known string -> add it as a string token */
    return token_add(TOKEN_ID_VALUE_STRING, (int)(strlen(g_tmp) + 1), 0.0, g_tmp);
  }

  return FAILED;
}
