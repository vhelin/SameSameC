
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "parse.h"
#include "main.h"
#include "pass_2.h"
#include "printf.h"
#include "definitions.h"
#include "stack.h"
#include "include_file.h"
#include "tree_node.h"


extern int g_input_float_mode, g_parsed_int;
extern int g_verbose_mode, g_latest_stack;
extern char g_mem_insert_action[MAX_NAME_LENGTH*3 + 1024], g_tmp[4096];
extern double g_parsed_double;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024], g_label[MAX_NAME_LENGTH + 1];

extern struct stack *g_stacks_first, *g_stacks_tmp, *g_stacks_last, *g_stacks_header_first, *g_stacks_header_last;
extern struct token *g_token_first, *g_token_last;

int g_current_filename_id = -1, g_current_line_number = -1;
char *g_variable_types[4] = { "none", "void", "int8", "int16" };
char *g_two_char_symbols[10] = { "||", "&&", "<=", ">=", "==", "!=", "<<", ">>", "++", "--" };

static struct tree_node *g_open_function_definition = NULL;
struct token *g_token_current = NULL, *g_token_previous = NULL;

struct tree_node *g_global_nodes = NULL;

struct tree_node *g_open_expression[256];
int g_open_expression_current_id = -1;

struct tree_node *g_open_condition[256];
int g_open_condition_current_id = -1;

struct tree_node *g_open_block[256];
int g_open_block_current_id = -1;

static char g_token_in_simple_form[MAX_NAME_LENGTH + 1];


char *_get_token_simple(struct token *t) {

  if (t->id == TOKEN_ID_VARIABLE_TYPE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", g_variable_types[t->value]);
  else if (t->id == TOKEN_ID_SYMBOL) {
    if (t->value <= SYMBOL_DECREMENT)
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
  else if (t->id == TOKEN_ID_DEFINE)
    snprintf(g_token_in_simple_form, sizeof(g_token_in_simple_form), "'%s'", "define");
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

  return g_token_in_simple_form;
}


static void _print_token(struct token *t) {

  /*
  fprintf(stderr, "FILE %d: LINE %.3d: ", t->file_id, t->line_number);
  */
  
  if (t->id == TOKEN_ID_VARIABLE_TYPE)
    fprintf(stderr, "TOKEN: VARIABLE_TYPE: %s\n", g_variable_types[t->value]);
  else if (t->id == TOKEN_ID_SYMBOL) {
    if (t->value <= SYMBOL_DECREMENT)
      fprintf(stderr, "TOKEN: SYMBOL       : %s\n", g_two_char_symbols[t->value]);
    else
      fprintf(stderr, "TOKEN: SYMBOL       : %c\n", t->value);
  }
  else if (t->id == TOKEN_ID_VALUE_INT)
    fprintf(stderr, "TOKEN: VALUE_INT    : %d\n", t->value);
  else if (t->id == TOKEN_ID_VALUE_DOUBLE)
    fprintf(stderr, "TOKEN: VALUE_DOUBLE : %f\n", t->value_double);
  else if (t->id == TOKEN_ID_VALUE_STRING)
    fprintf(stderr, "TOKEN: VALUE_STRING : %s\n", t->label);
  else if (t->id == TOKEN_ID_RETURN)
    fprintf(stderr, "TOKEN: RETURN       : return\n");
  else if (t->id == TOKEN_ID_DEFINE)
    fprintf(stderr, "TOKEN: DEFINE       : .define\n");
  else if (t->id == TOKEN_ID_NO_OP)
    fprintf(stderr, "TOKEN: NO-OP        : n/a\n");
  else if (t->id == TOKEN_ID_ELSE)
    fprintf(stderr, "TOKEN: ELSE         : else\n");
  else if (t->id == TOKEN_ID_IF)
    fprintf(stderr, "TOKEN: IF           : if\n");
  else if (t->id == TOKEN_ID_WHILE)
    fprintf(stderr, "TOKEN: WHILE        : while\n");
  else if (t->id == TOKEN_ID_FOR)
    fprintf(stderr, "TOKEN: FOR          : for\n");
  else if (t->id == TOKEN_ID_BREAK)
    fprintf(stderr, "TOKEN: BREAK        : break\n");
  else if (t->id == TOKEN_ID_CONTINUE)
    fprintf(stderr, "TOKEN: CONTINUE     : continue\n");
  /*
  else if (t->id == TOKEN_ID_CHANGE_FILE)
    fprintf(stderr, "TOKEN: CHANGE FILE  : %s\n", get_file_name(t->value));
  else if (t->id == TOKEN_ID_LINE_NUMBER)
    fprintf(stderr, "TOKEN: LINE NUMBER  : %d\n", t->value);
  */
}


static int _print_loose_token_error(struct token *t) {

  if (t->id == TOKEN_ID_SYMBOL) {
    if (t->value <= SYMBOL_DECREMENT)
      snprintf(g_error_message, sizeof(g_error_message), "The loose symbol '%s' doesn't make any sense.\n", g_two_char_symbols[t->value]);
    else
      snprintf(g_error_message, sizeof(g_error_message), "The loose symbol '%c' doesn't make any sense.\n", t->value);
    print_error(g_error_message, ERROR_ERR);
  }
  else if (t->id == TOKEN_ID_VALUE_INT) {
    snprintf(g_error_message, sizeof(g_error_message), "The loose value %d doesn't make any sense.\n", t->value);
    print_error(g_error_message, ERROR_ERR);
  }
  else if (t->id == TOKEN_ID_VALUE_DOUBLE) {
    snprintf(g_error_message, sizeof(g_error_message), "The loose value %d doesn't make any sense.\n", t->value_double);
    print_error(g_error_message, ERROR_ERR);
  }
  else if (t->id == TOKEN_ID_VALUE_STRING) {
    snprintf(g_error_message, sizeof(g_error_message), "The loose string \"%s\" doesn't make any sense.\n", t->label);
    print_error(g_error_message, ERROR_ERR);
  }
  else if (t->id == TOKEN_ID_RETURN) {
    snprintf(g_error_message, sizeof(g_error_message), "The loose \"return\" doesn't make any sense.\n");
    print_error(g_error_message, ERROR_ERR);
  }
  else {
    snprintf(g_error_message, sizeof(g_error_message), "Unhandled loose TOKEN ID %d. Please send a bug report!\n", t->id);
    print_error(g_error_message, ERROR_ERR);
  }
  
  return FAILED;
}


static int _open_expression_push() {

  if (g_open_expression_current_id >= 256)
    return FAILED;

  g_open_expression_current_id++;
  g_open_expression[g_open_expression_current_id] = allocate_tree_node_with_children(TREE_NODE_TYPE_EXPRESSION, 256);

  if (g_open_expression[g_open_expression_current_id] == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _open_expression_pop() {

  g_open_expression[g_open_expression_current_id] = NULL;
  
  if (g_open_expression_current_id >= 0) {
    g_open_expression_current_id--;

    return SUCCEEDED;
  }

  return FAILED;
}


static struct tree_node *_get_current_open_expression() {

  if (g_open_expression_current_id < 0) {
    print_error("Trying to get the current open expression, but there is none! Please submit a bug report!\n", ERROR_DIR);
    return NULL;
  }
  
  return g_open_expression[g_open_expression_current_id];
}


static int _open_condition_push() {

  if (g_open_condition_current_id >= 256)
    return FAILED;

  g_open_condition_current_id++;
  g_open_condition[g_open_condition_current_id] = allocate_tree_node_with_children(TREE_NODE_TYPE_CONDITION, 256);

  if (g_open_condition[g_open_condition_current_id] == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _open_condition_pop() {

  g_open_condition[g_open_condition_current_id] = NULL;
  
  if (g_open_condition_current_id >= 0) {
    g_open_condition_current_id--;

    return SUCCEEDED;
  }

  return FAILED;
}


static struct tree_node *_get_current_open_condition() {

  if (g_open_condition_current_id < 0) {
    print_error("Trying to get the current open condition, but there is none! Please submit a bug report!\n", ERROR_DIR);
    return NULL;
  }
  
  return g_open_condition[g_open_condition_current_id];
}


static int _open_block_push() {

  if (g_open_block_current_id >= 256)
    return FAILED;

  g_open_block_current_id++;
  g_open_block[g_open_block_current_id] = allocate_tree_node_with_children(TREE_NODE_TYPE_BLOCK, 256);

  if (g_open_block[g_open_block_current_id] == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _open_block_pop() {

  g_open_block[g_open_block_current_id] = NULL;
  
  if (g_open_block_current_id >= 0) {
    g_open_block_current_id--;

    return SUCCEEDED;
  }

  return FAILED;
}


static struct tree_node *_get_current_open_block() {

  if (g_open_block_current_id < 0) {
    print_error("Trying to get the current open block, but there is none! Please submit a bug report!\n", ERROR_DIR);
    return NULL;
  }
  
  return g_open_block[g_open_block_current_id];
}


int pass_2(void) {

  struct tree_node *node;
  int i;
  
  if (g_verbose_mode == ON)
    printf("Pass 2...\n");

  /* init */
  for (i = 0; i < 256; i++)
    g_open_expression[i] = NULL;
  
  g_global_nodes = allocate_tree_node_with_children(TREE_NODE_TYPE_GLOBAL_NODES, 256);
  if (g_global_nodes == NULL)
    return FAILED;
  
  g_token_current = g_token_first;
  while (g_token_current != NULL) {
    if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
      _print_token(g_token_current);
      node = create_variable_or_function();
      if (node == NULL)
        return FAILED;
      if (tree_node_add_child(g_global_nodes, node) == FAILED)
        return FAILED;
    }
    else if (g_token_current->id == TOKEN_ID_DEFINE) {
      _print_token(g_token_current);
      if (create_definition() == FAILED)
        return FAILED;
    }
    else if (g_token_current->id == TOKEN_ID_NO_OP) {
      /* no operation */
      _print_token(g_token_current);
    
      /* next token */
      g_token_current = g_token_current->next;
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
      /* skip ';' */
      
      /* next token */
      g_token_current = g_token_current->next;
    }
    else {
      _print_token(g_token_current);
      return _print_loose_token_error(g_token_current);
    }
  }

  if (g_open_expression_current_id != -1)
    fprintf(stderr, "WARNING: We have an open expression even though parsing has ended. Please submit a bug report!\n");
  if (g_open_function_definition != NULL)
    fprintf(stderr, "WARNING: We have an open function definition even though parsing has ended. Please submit a bug report!\n");
  if (g_open_block_current_id != -1)
    fprintf(stderr, "WARNING: We have an open block even though parsing has ended. Please submit a bug report!\n");
  
  return SUCCEEDED;
}


static void _next_token() {

  g_token_previous = g_token_current;
  g_token_current = g_token_current->next;
  g_current_filename_id = g_token_current->file_id;
  g_current_line_number = g_token_current->line_number;

  _print_token(g_token_current);
}


int create_factor(void) {

  struct tree_node *node = NULL;
  int result;
  
  fprintf(stderr, "+ create_factor()\n");

  _print_token(g_token_current);  
  
  if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
    /* a loose variable name, a function call, var++ or var-- */
    node = allocate_tree_node_value_string(g_token_current->label);
    if (node == NULL)
      return FAILED;

    /* next token */
    _next_token();

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '(') {
      /* function call */
      struct tree_node *function_name_node = node;

      /* next token */
      _next_token();

      node = allocate_tree_node_with_children(TREE_NODE_TYPE_FUNCTION_CALL, 32);
      if (node == NULL) {
        free_tree_node(function_name_node);
        return FAILED;
      }

      tree_node_add_child(node, function_name_node);

      fprintf(stderr, "FUNCTION CALL\n");

      while (1) {
        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')') {
          fprintf(stderr, "create_factor(): GOT ')' - 1!\n");
          
          break;
        }
      
        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        fprintf(stderr, "create_factor():\n");
            
        /* possibly parse a calculation */
        result = create_expression();
        if (result == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        fprintf(stderr, "- create_expression() - create_factor()\n");

        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();
        }
      }

      fprintf(stderr, "ABC: FUNCTION CALL: %s %d\n", node->children[0]->label, node->added_children);
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == SYMBOL_INCREMENT || g_token_current->value == SYMBOL_DECREMENT)) {
      /* var++ or var-- */
      int symbol = g_token_current->value;

      /* next token */
      _next_token();

      /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_INCREMENT_DECREMENT */
      node->type = TREE_NODE_TYPE_INCREMENT_DECREMENT;
      node->value = symbol;

      /* post */
      node->value_double = 1.0;

      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      return SUCCEEDED;
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '[') {
      /* var[index] */

      /* next token */
      _next_token();

      /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_ARRAY_ITEM */
      node->type = TREE_NODE_TYPE_ARRAY_ITEM;

      fprintf(stderr, "create_factor():\n");

      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }
    
      result = create_expression();
      if (result != SUCCEEDED) {
        free_tree_node(node);
        return result;
      }

      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();
    
      fprintf(stderr, "- create_expression() - create_factor()\n");

      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        free_tree_node(node);
        return FAILED;
      }

       /* next token */
      _next_token();

      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      return SUCCEEDED;
    }
    else {
      /* loose variable name */
      struct definition *d;

      /* ... but is it actually a definition? */
      definition_get(node->label, &d);

      if (d != NULL) {
        /* yes! substitute! */
        fprintf(stderr, "%s is a definition! SUBSTITUTE!\n", node->label);

        free_tree_node(node);

        if (d->type == DEFINITION_TYPE_VALUE) {
          /* is it an integer or a double? */
          if (ceil(d->value) == d->value)
            node = allocate_tree_node_value_int(ceil(d->value));
          else
            node = allocate_tree_node_value_double(d->value);
        }
        else if (d->type == DEFINITION_TYPE_STRING || d->type == DEFINITION_TYPE_ADDRESS_LABEL)
          node = allocate_tree_node_value_string(d->string);
        else if (d->type == DEFINITION_TYPE_STACK)
          node = allocate_tree_node_stack(d->value);
        else
          node = NULL;
      }

      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      return SUCCEEDED;
    }
  }
  else if (g_token_current->id == TOKEN_ID_VALUE_INT)
    node = allocate_tree_node_value_int(g_token_current->value);
  else if (g_token_current->id == TOKEN_ID_VALUE_DOUBLE)
    node = allocate_tree_node_value_double(g_token_current->value_double);
  else if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == SYMBOL_INCREMENT || g_token_current->value == SYMBOL_DECREMENT)) {
    /* --var or ++var */
    int symbol = g_token_current->value;

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a variable name, but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    node = allocate_tree_node_value_string(g_token_current->label);
    if (node == NULL)
      return FAILED;

    /* next token */
    _next_token();

    /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_INCREMENT_DECREMENT */
    node->type = TREE_NODE_TYPE_INCREMENT_DECREMENT;
    node->value = symbol;

    /* pre */
    node->value_double = -1.0;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '(') {
    struct tree_node *expression;
    
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();

    fprintf(stderr, "create_factor():\n");

    /* create_expression() will put all tree_nodes it parses to g_open_expression */
    if (_open_expression_push() == FAILED)
      return FAILED;
    
    result = create_expression();
    if (result != SUCCEEDED)
      return result;

    expression = _get_current_open_expression();
    _open_expression_pop();
    
    fprintf(stderr, "- create_expression() - create_factor()\n");

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(expression);
      return FAILED;
    }

    fprintf(stderr, "create_factor(): GOT ')' - 2!\n");
    
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL) {
      free_tree_node(expression);
      return FAILED;
    }

    tree_node_add_child(_get_current_open_expression(), expression);
    tree_node_add_child(_get_current_open_expression(), node);
    
    /* next token */
    _next_token();
    
    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
    /* next token */
    _next_token();
    
    return FINISHED;
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']') {
    return FINISHED;
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
    return FINISHED;
  }  
  else {
    _print_loose_token_error(g_token_current);
    return FAILED;
  }

  if (node == NULL)
    return FAILED;

  if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
    return FAILED;

  /* next token */
  _next_token();
    
  return SUCCEEDED;
}


int create_term(void) {

  struct tree_node *node = NULL;
  int result;

  fprintf(stderr, "+ create_term()\n");
    
  result = create_factor();
  if (result != SUCCEEDED)
    return result;

  fprintf(stderr, "- create_factor() - create_term()\n");

  while (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '*' || g_token_current->value == '/' || g_token_current->value == SYMBOL_SHIFT_LEFT || g_token_current->value == SYMBOL_SHIFT_RIGHT)) {
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();

    result = create_factor();
    if (result != SUCCEEDED)
      return result;

    fprintf(stderr, "- create_factor() - create_term()\n");
  }

  return SUCCEEDED;
}


int create_expression(void) {

  struct tree_node *node = NULL;
  int result;

  fprintf(stderr, "+ create_expression()\n");

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
    /* next token */
    _next_token();
    
    return FINISHED;
  }

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
    /* next token */
    _next_token();
    
    return THEEND;
  }

  if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '-' || g_token_current->value == '+')) {
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();
  }

  fprintf(stderr, "create_expression():\n");
  
  result = create_term();
  if (result != SUCCEEDED)
    return result;

  fprintf(stderr, "- create_term() - create_expression()\n");
    
  while (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '-' || g_token_current->value == '+')) {
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();

    fprintf(stderr, "create_expression():\n");
      
    result = create_term();
    if (result != SUCCEEDED)
      return result;

    fprintf(stderr, "- create_term() - create_expression()\n");
  }  
  
  return SUCCEEDED;
}


int create_condition(void) {

  struct tree_node *node;

  while (1) {
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '(') {
      /* next token */
      _next_token();

      /* create_condition() will put all tree_nodes it parses to g_open_condition */
      if (_open_condition_push() == FAILED) {
        return FAILED;
      }

      fprintf(stderr, "create_condition(): IF\n");
      
      if (create_condition() == FAILED) {
        _open_condition_pop();
        return FAILED;
      }

      fprintf(stderr, "- create_condition() - create_condition()\n");

      node = _get_current_open_condition();
      _open_condition_pop();

      tree_node_add_child(_get_current_open_condition(), node);

      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        free_tree_node(node);
        _open_condition_pop();      
        return FAILED;
      }
    
      /* next token */
      _next_token();
    }
    else {
      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED) {
        return FAILED;
      }

      fprintf(stderr, "create_condition():\n");

      /* possibly parse a calculation */
      if (create_expression() == FAILED) {
        return FAILED;
      }

      fprintf(stderr, "- create_expression() - create_condition()\n");

      node = _get_current_open_expression();
      _open_expression_pop();
    
      tree_node_add_child(_get_current_open_condition(), node);

      /* expect a comparison */
    
      if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != SYMBOL_LTE &&
                                                     g_token_current->value != SYMBOL_GTE &&
                                                     g_token_current->value != SYMBOL_EQUAL &&
                                                     g_token_current->value != SYMBOL_UNEQUAL &&
                                                     g_token_current->value != '<' &&
                                                     g_token_current->value != '>')) {
        snprintf(g_error_message, sizeof(g_error_message), "Expected '<=' / '>=' / '==' / '!=' / '<' / '>', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return FAILED;      
      }

      node = allocate_tree_node_symbol(g_token_current->value);
      if (node == NULL)
        return FAILED;
      
      tree_node_add_child(_get_current_open_condition(), node);

      /* next token */
      _next_token();

      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED) {
        return FAILED;
      }

      fprintf(stderr, "create_condition():\n");

      /* possibly parse a calculation */
      if (create_expression() == FAILED) {
        return FAILED;
      }

      fprintf(stderr, "- create_expression() - create_condition()\n");

      node = _get_current_open_expression();
      _open_expression_pop();
      
      tree_node_add_child(_get_current_open_condition(), node);    
    }

    /* && or || ? */
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != SYMBOL_LOGICAL_OR &&
                                                   g_token_current->value != SYMBOL_LOGICAL_AND))
      break;

    /* yes -> continue */
    
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;
      
    tree_node_add_child(_get_current_open_condition(), node);

    /* next token */
    _next_token();
  }
  
  return SUCCEEDED;
}


int create_statement(void) {

  struct tree_node *node;
  int symbol;
  
  fprintf(stderr, "+ create_statement()\n");

  if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
    /* variable creation */
    int variable_type = g_token_current->value;
    char name[MAX_NAME_LENGTH + 1];
    int pointer_depth = 0;

    /* next token */
    _next_token();

    while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
      pointer_depth++;

      /* next token */
      _next_token();
    }
    
    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "%s must be followed by a variable name.\n", g_variable_types[variable_type]);
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                   g_token_current->value != '[' &&
                                                   g_token_current->value != ';')) {
      snprintf(g_error_message, sizeof(g_error_message), "%s must be followed by a ';', '[' or '='.\n", name);
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    symbol = g_token_current->value;

    if (symbol != ';') {
      /* next token */
      _next_token();
    }
    
    /* create_expression() will put all tree_nodes it parses to g_open_expression */
    if (_open_expression_push() == FAILED)
      return FAILED;

    if (symbol == '=') {
      /* possibly parse a calculation */
      fprintf(stderr, "create_statement():\n");

      if (create_expression() == FAILED)
        return FAILED;

      fprintf(stderr, "- create_expression() - create_statement()\n");

      /*
      result = stack_calculate_tokens(&g_parsed_int);

      fprintf(stderr, "GOT FROM STACK CALCULATE: ");

      if (result == SUCCEEDED)
        fprintf(stderr, "INT: %d\n", g_parsed_int);
      else if (result == INPUT_NUMBER_FLOAT)
        fprintf(stderr, "DOUBLE: %g\n", g_parsed_double);
      else if (result == INPUT_NUMBER_STRING)
        fprintf(stderr, "STRING: %s\n", g_label);
      else if (result == INPUT_NUMBER_STACK)
        fprintf(stderr, "STACK: %d\n", g_latest_stack);
      */
    }
    else if (symbol == '[') {
      /* array */

      /* TODO: do this nicer */
      free_tree_node(_get_current_open_expression());
      _open_expression_pop();

      node = create_array(pointer_depth, variable_type, name);
      if (node == NULL)
        return FAILED;

      tree_node_add_child(_get_current_open_block(), node);

      if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return FAILED;
      }

      /* next token */
      _next_token();
    
      return SUCCEEDED;      
    }
    else {
      if (variable_type == VARIABLE_TYPE_INT8 || variable_type == VARIABLE_TYPE_INT16) {
        tree_node_add_child(_get_current_open_expression(), allocate_tree_node_value_int(0));
        if (_get_current_open_expression()->children[0] == NULL)
          return FAILED;
      }
    }

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 3);
    if (node == NULL)
      return FAILED;

    tree_node_add_child(node, allocate_tree_node_variable_type(variable_type));
    tree_node_add_child(node, allocate_tree_node_value_string(name));
    tree_node_add_child(node, _get_current_open_expression());
    _open_expression_pop();

    /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
    node->children[0]->value_double = pointer_depth;
    
    if (node->children[0] == NULL || node->children[1] == NULL || node->children[2] == NULL) {
      free_tree_node(node);
      return FAILED;
    }
    
    tree_node_add_child(_get_current_open_block(), node);

    if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    /* next token */
    _next_token();
    
    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
    /* assignment, a function call, increment or decrement */
    char name[MAX_NAME_LENGTH + 1];

    strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                   g_token_current->value != '(' &&
                                                   g_token_current->value != '[' &&
                                                   g_token_current->value != SYMBOL_INCREMENT &&
                                                   g_token_current->value != SYMBOL_DECREMENT)) {
      snprintf(g_error_message, sizeof(g_error_message), "%s must be followed by '=' / '[' / '(' / '++' / '--'.\n", name);
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    symbol = g_token_current->value;

    /* next token */
    _next_token();

    if (symbol == SYMBOL_INCREMENT || symbol == SYMBOL_DECREMENT) {
      /* increment / decrement */

      if (!(g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == ';' || g_token_current->value == ')')))  {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ')' / ';', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return FAILED;
      }

      /* next token */
      _next_token();

      node = allocate_tree_node_value_string(name);
      if (node == NULL)
        return FAILED;

      /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_INCREMENT_DECREMENT */
      node->type = TREE_NODE_TYPE_INCREMENT_DECREMENT;
      node->value = symbol;

      /* post */
      node->value_double = 1.0;
      
      tree_node_add_child(_get_current_open_block(), node);
      
      return SUCCEEDED;
    }
    else if (symbol == '=' || symbol == '[') {
      /* assignment */
      struct tree_node *node_index = NULL;

      if (symbol == '[') {
        /* array assignment */

        /* next token */
        _next_token();

        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED)
          return FAILED;    

        fprintf(stderr, "create_statement():\n");

        /* possibly parse a calculation */
        if (create_expression() == FAILED)
          return FAILED;

        fprintf(stderr, "- create_expression() - create_statement()\n");

        node_index = _get_current_open_expression();
        _open_expression_pop();

        if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
          snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", _get_token_simple(g_token_current));
          print_error(g_error_message, ERROR_ERR);
          free_tree_node(node_index);
          return FAILED;
        }
    
        /* next token */
        _next_token();

        if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '=') {
          snprintf(g_error_message, sizeof(g_error_message), "Expected '=', but got %s.\n", _get_token_simple(g_token_current));
          print_error(g_error_message, ERROR_ERR);
          free_tree_node(node_index);
          return FAILED;
        }
    
        /* next token */
        _next_token();
      }
      
      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED)
        return FAILED;    

      fprintf(stderr, "create_statement():\n");

      /* possibly parse a calculation */
      if (create_expression() == FAILED)
        return FAILED;

      fprintf(stderr, "- create_expression() - create_statement()\n");

      node = allocate_tree_node_with_children(TREE_NODE_TYPE_ASSIGNMENT, 3);
      if (node == NULL)
        return FAILED;
      
      tree_node_add_child(node, allocate_tree_node_value_string(name));
      if (node_index != NULL)
        tree_node_add_child(node, node_index);
      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();
      
      if (node->children[0] == NULL || node->children[1] == NULL) {
        free_tree_node(node);
        return FAILED;
      }

      tree_node_add_child(_get_current_open_block(), node);
    }
    else {
      /* function call */

      node = allocate_tree_node_with_children(TREE_NODE_TYPE_FUNCTION_CALL, 32);
      if (node == NULL)
        return FAILED;

      tree_node_add_child(node, allocate_tree_node_value_string(name));

      while (1) {
        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')') {
          /* next token */
          _next_token();

          break;
        }

        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        fprintf(stderr, "create_statement():\n");

        /* possibly parse a calculation */
        if (create_expression() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        fprintf(stderr, "- create_expression() - create_statement()\n");
              
        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();
        }
      }

      fprintf(stderr, "XYZ: FUNCTION CALL %s %d\n", node->children[0]->label, node->added_children);

      tree_node_add_child(_get_current_open_block(), node);
    }

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
      /* the end of the function */
      return FINISHED;
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
    }
    else {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '}' / ',' / ';', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    /* next token */
    _next_token();

    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_IF) {
    /* if */

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_IF, 1);
    if (node == NULL)
      return FAILED;

    /* next token */
    _next_token();

    while (1) {
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        free_tree_node(node);
        return FAILED;
      }

      /* next token */
      _next_token();

      /* create_condition() will put all tree_nodes it parses to g_open_condition */
      if (_open_condition_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      fprintf(stderr, "create_statement(): IF\n");
      
      if (create_condition() == FAILED) {
        _open_condition_pop();
        free_tree_node(node);
        return FAILED;
      }

      fprintf(stderr, "- create_condition() - create_statement()\n");

      tree_node_add_child(node, _get_current_open_condition());
      _open_condition_pop();
      
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        free_tree_node(node);
        return FAILED;
      }

      /* next token */
      _next_token();

      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        free_tree_node(node);
        return FAILED;
      }

      /* start parsing a block */
      
      if (_open_block_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      fprintf(stderr, "create_statement(): IF\n");
      
      if (create_block() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      fprintf(stderr, "- create_block() - create_statement()\n");
            
      tree_node_add_child(node, _get_current_open_block());
      _open_block_pop();

      if (g_token_current->id != TOKEN_ID_ELSE) {
        /* the if ends here */
        tree_node_add_child(_get_current_open_block(), node);

        return SUCCEEDED;
      }

      /* next token */
      _next_token();
      
      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{') {
        /* the final block */

        if (_open_block_push() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        fprintf(stderr, "create_statement(): ELSE\n");
              
        if (create_block() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        fprintf(stderr, "- create_block() - create_statement()\n");

        tree_node_add_child(node, _get_current_open_block());
        _open_block_pop();

        /* the if ends here */
        tree_node_add_child(_get_current_open_block(), node);

        return SUCCEEDED;
      }

      if (g_token_current->id != TOKEN_ID_IF) {
        snprintf(g_error_message, sizeof(g_error_message), "Expected 'if', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        free_tree_node(node);
        return FAILED;
      }

      /* next token */
      _next_token();
    }
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == SYMBOL_INCREMENT || g_token_current->value == SYMBOL_DECREMENT)) {
    /* increment / decrement */
    char name[MAX_NAME_LENGTH + 1];
    int symbol = g_token_current->value;

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a variable name, but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      return FAILED;
    }

    /* next token */
    _next_token();
    
    node = allocate_tree_node_value_string(name);
    if (node == NULL)
      return FAILED;

    /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_INCREMENT_DECREMENT */
    node->type = TREE_NODE_TYPE_INCREMENT_DECREMENT;
    node->value = symbol;

    /* pre */
    node->value_double = -1.0;
      
    tree_node_add_child(_get_current_open_block(), node);
      
    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_WHILE) {
    /* while */
    
    /* next token */
    _next_token();

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_WHILE, 2);
    if (node == NULL)
      return FAILED;

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return FAILED;
    }

    /* next token */
    _next_token();

    /* create_condition() will put all tree_nodes it parses to g_open_condition */
    if (_open_condition_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "create_statement(): WHILE\n");
      
    if (create_condition() == FAILED) {
      _open_condition_pop();
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "- create_condition() - create_statement()\n");

    tree_node_add_child(node, _get_current_open_condition());
    _open_condition_pop();
      
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return FAILED;
    }

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return FAILED;
    }

    /* start parsing a block */
      
    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "create_statement(): WHILE\n");
      
    if (create_block() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "- create_block() - create_statement()\n");

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();

    tree_node_add_child(_get_current_open_block(), node);

    return SUCCEEDED;    
  }
  else if (g_token_current->id == TOKEN_ID_FOR) {
    /* for */

    /* next token */
    _next_token();

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_FOR, 4);
    if (node == NULL)
      return FAILED;

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return FAILED;
    }

    /* next token */
    _next_token();

    /* start parsing the initializer block */

    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    while (1) {
      int result;
      
      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
        /* next token */
        _next_token();

        break;
      }
      if (g_token_previous->id == TOKEN_ID_SYMBOL && g_token_previous->value == ';')
        break;

      fprintf(stderr, "create_statement():\n");
    
      result = create_statement();

      fprintf(stderr, "- create_statement() - create_statement()\n");
      
      if (result == FAILED) {
        free_tree_node(node);
        return result;
      }
      if (result == FINISHED)
        break;
    }

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();

    /* start parsing the condition */

    /* create_condition() will put all tree_nodes it parses to g_open_condition */
    if (_open_condition_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "create_statement(): IF\n");
      
    if (create_condition() == FAILED) {
      _open_condition_pop();
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "- create_condition() - create_statement()\n");

    tree_node_add_child(node, _get_current_open_condition());
    _open_condition_pop();
      
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return FAILED;
    }

    /* next token */
    _next_token();

    /* start parsing the loop block */

    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    while (1) {
      int result;
      
      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')') {
        /* next token */
        _next_token();

        break;
      }
      if (g_token_previous->id == TOKEN_ID_SYMBOL && g_token_previous->value == ')')
        break;
      
      fprintf(stderr, "create_statement():\n");
    
      result = create_statement();

      fprintf(stderr, "- create_statement() - create_statement()\n");
      
      if (result == FAILED) {
        free_tree_node(node);
        return result;
      }
      if (result == FINISHED)
        break;
    }

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return FAILED;
    }

    /* start parsing a block */
      
    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "create_statement(): IF\n");
      
    if (create_block() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    fprintf(stderr, "- create_block() - create_statement()\n");
            
    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();

    tree_node_add_child(_get_current_open_block(), node);

    return SUCCEEDED;    
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
    /* the end of the block */
    return FINISHED;
  }
  else if (g_token_current->id == TOKEN_ID_RETURN) {
    /* return */

    /* next token */
    _next_token();

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
      /* return; */
      
      /* next token */
      _next_token();

      node = allocate_tree_node(TREE_NODE_TYPE_RETURN);
      if (node == NULL)
        return FAILED;

      tree_node_add_child(_get_current_open_block(), node);

      return SUCCEEDED;
    }
    else {
      /* return {expression}; */
      
      node = allocate_tree_node_with_children(TREE_NODE_TYPE_RETURN, 1);
      if (node == NULL)
        return FAILED;

      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      fprintf(stderr, "create_statement():\n");

      /* possibly parse a calculation */
      if (create_expression() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      fprintf(stderr, "- create_expression() - create_statement()\n");

      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();

      tree_node_add_child(_get_current_open_block(), node);

      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return FAILED;
      }

      /* next token */
      _next_token();

      return SUCCEEDED;
    }
  }

  snprintf(g_error_message, sizeof(g_error_message), "Expected a statement, but got %s.\n", _get_token_simple(g_token_current));
  print_error(g_error_message, ERROR_ERR);
  
  return FAILED;
}


int create_block(void) {

  int result;

  fprintf(stderr, "+ create_block()\n");

  if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
    snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", _get_token_simple(g_token_current));
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  /* next token */
  _next_token();

  while (1) {
    fprintf(stderr, "create_block():\n");
    
    result = create_statement();

    fprintf(stderr, "- create_statement() - create_block()\n");

    if (result == FAILED)
      return result;
    if (result == FINISHED)
      break;
  }
  
  if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '}') {
    snprintf(g_error_message, sizeof(g_error_message), "Expected '}', but got %s.\n", _get_token_simple(g_token_current));
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  /* next token */
  _next_token();
  
  return FINISHED;
}


struct tree_node *create_array(double pointer_depth, int variable_type, char *name) {

  struct tree_node *node;
  int array_size = 0, added_items = 0;

  node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 16);
  if (node == NULL)
    return NULL;

  tree_node_add_child(node, allocate_tree_node_variable_type(variable_type));
  tree_node_add_child(node, allocate_tree_node_value_string(name));

  if (node->children[0] == NULL || node->children[1] == NULL) {
    free_tree_node(node);
    return NULL;
  }
  
  /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
  node->children[0]->value_double = pointer_depth;

  if (g_token_current->id == TOKEN_ID_VALUE_INT) {
    array_size = g_token_current->value;

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return NULL;
    }

    /* next token */
    _next_token();
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']') {
    /* next token */
    _next_token();
  }
  else {
    snprintf(g_error_message, sizeof(g_error_message), "Expected array size or ']', but got %s.\n", _get_token_simple(g_token_current));
    print_error(g_error_message, ERROR_ERR);
    free_tree_node(node);
    return NULL;
  }

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
  }
  else {
    /* read the items */
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '=') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '=', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return NULL;
    }

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", _get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      free_tree_node(node);
      return NULL;
    }

    /* next token */
    _next_token();

    while (1) {
      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
        /* next token */
        _next_token();

        break;
      }

      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED) {
        free_tree_node(node);
        return NULL;
      }

      /* possibly parse a calculation */
      fprintf(stderr, "create_array():\n");

      if (create_expression() == FAILED) {
        free_tree_node(_get_current_open_expression());
        _open_expression_pop();
        free_tree_node(node);
        return NULL;
      }

      fprintf(stderr, "- create_expression() - create_array()\n");

      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();

      added_items++;

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
        /* next token */
        _next_token();
      }      
    }
  }

  if (array_size == 0 && added_items == 0) {
    print_error("An array without any items doesn't make sense.\n", ERROR_ERR);
    free_tree_node(node);
    return NULL;
  }

  if (array_size == 0)
    array_size = added_items;

  if (array_size < added_items) {
    snprintf(g_error_message, sizeof(g_error_message), "Specified array size is %d, but defined %d items in the array -> setting array size to %d.\n", array_size, added_items, added_items);
    print_error(g_error_message, ERROR_WRN);
    array_size = added_items;
  }

  /* empty fill the rest of the array */
  while (added_items < array_size) {
    struct tree_node *empty_node = allocate_tree_node_value_int(0);

    if (empty_node == NULL) {
      free_tree_node(node);
      return NULL;
    }

    tree_node_add_child(node, empty_node);
    added_items++;
  }

  /* store the size of the array */
  node->value = array_size;
  
  return node;
}


struct tree_node *create_variable_or_function(void) {

  int variable_type = g_token_current->value;
  char name[MAX_NAME_LENGTH + 1];
  struct tree_node *node = NULL;
  int pointer_depth = 0;

  /* next token */
  _next_token();

  while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
    pointer_depth++;

    /* next token */
    _next_token();
  }

  if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
    snprintf(g_error_message, sizeof(g_error_message), "%s must be followed by a variable or function name.\n", g_variable_types[variable_type]);
    print_error(g_error_message, ERROR_ERR);
    return NULL;
  }

  strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

  /* next token */
  _next_token();

  if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                 g_token_current->value != ';' &&
                                                 g_token_current->value != '[' &&
                                                 g_token_current->value != '(')) {
    snprintf(g_error_message, sizeof(g_error_message), "%s must be followed by a ';', '=', '[' or '('.\n", name);
    print_error(g_error_message, ERROR_ERR);
    return NULL;
  }

  if (g_token_current->value == '=' || g_token_current->value == ';' || g_token_current->value == '[') {
    /* a variable definition */
    int symbol = g_token_current->value;

    /* next token */
    _next_token();

    /* create_expression() will put all tree_nodes it parses to g_open_expression */
    if (_open_expression_push() == FAILED)
      return NULL;

    if (symbol == '=') {
      /* possibly parse a calculation */
      if (create_expression() == FAILED)
        return NULL;

      fprintf(stderr, "- create_expression() - create_variable_or_function()\n");

      /*
      result = stack_calculate_tokens(&g_parsed_int);

      fprintf(stderr, "GOT FROM STACK CALCULATE: ");

      if (result == SUCCEEDED)
        fprintf(stderr, "INT: %d\n", g_parsed_int);
      else if (result == INPUT_NUMBER_FLOAT)
        fprintf(stderr, "DOUBLE: %g\n", g_parsed_double);
      else if (result == INPUT_NUMBER_STRING)
        fprintf(stderr, "STRING: %s\n", g_label);
      else if (result == INPUT_NUMBER_STACK)
        fprintf(stderr, "STACK: %d\n", g_latest_stack);
      */
    }
    else if (symbol == '[') {
      /* array */

      /* TODO: do this nicer */
      free_tree_node(_get_current_open_expression());
      _open_expression_pop();

      return create_array(pointer_depth, variable_type, name);
    }
    else {
      if (variable_type == VARIABLE_TYPE_INT8 || variable_type == VARIABLE_TYPE_INT16) {
        tree_node_add_child(_get_current_open_expression(), allocate_tree_node_value_int(0));
        if (_get_current_open_expression()->children[0] == NULL)
          return NULL;
      }
    }

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 3);
    if (node == NULL)
      return NULL;

    tree_node_add_child(node, allocate_tree_node_variable_type(variable_type));
    tree_node_add_child(node, allocate_tree_node_value_string(name));
    tree_node_add_child(node, _get_current_open_expression());
    _open_expression_pop();

    if (node->children[0] == NULL || node->children[1] == NULL || node->children[2] == NULL) {
      free_tree_node(node);
      return NULL;
    }

    /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
    node->children[0]->value_double = pointer_depth;
    
    return node;
  }
  else {
    /* a function definition */
    
    /* this token is '(' */
    _next_token();

    /* start collecting the argument types and names */

    g_open_function_definition = allocate_tree_node_with_children(TREE_NODE_TYPE_FUNCTION_DEFINITION, 16);
    if (g_open_function_definition == NULL)
      return NULL;

    tree_node_add_child(g_open_function_definition, allocate_tree_node_variable_type(variable_type));
    tree_node_add_child(g_open_function_definition, allocate_tree_node_value_string(name));

    if (g_open_function_definition->children[0] == NULL || g_open_function_definition->children[1] == NULL) {
      free_tree_node(g_open_function_definition);
      g_open_function_definition = NULL;
      return NULL;
    }

    /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
    g_open_function_definition->children[0]->value_double = pointer_depth;
    
    while (1) {
      if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
        struct tree_node *node;

        node = allocate_tree_node_variable_type(g_token_current->value);
        if (node == NULL) {
          free_tree_node(g_open_function_definition);
          g_open_function_definition = NULL;
          return NULL;
        }

        tree_node_add_child(g_open_function_definition, node);
        
        /* next token */
        _next_token();

        pointer_depth = 0;
        
        while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
          pointer_depth++;

          /* next token */
          _next_token();
        }

        /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
        node->value_double = pointer_depth;
        
        if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
          snprintf(g_error_message, sizeof(g_error_message), "Expected an argument name, but got %s.\n", _get_token_simple(g_token_current));
          print_error(g_error_message, ERROR_ERR);
          free_tree_node(g_open_function_definition);
          g_open_function_definition = NULL;
          return NULL;
        }

        node = allocate_tree_node_value_string(g_token_current->label);
        if (node == NULL) {
          free_tree_node(g_open_function_definition);
          g_open_function_definition = NULL;
          return NULL;          
        }

        tree_node_add_child(g_open_function_definition, node);

        /* next token */
        _next_token();

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();
        }
      }
      else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')') {
        /* the end of arguments */
        
        /* this token is ')' */
        _next_token();

        /* start parsing the function */

        if (_open_block_push() == FAILED) {
          free_tree_node(g_open_function_definition);
          g_open_function_definition = NULL;
          return NULL;
        }

        /* parse the function */
        if (create_block() == FAILED) {
          free_tree_node(_get_current_open_block());
          _open_block_pop();
          return NULL;
        }

        tree_node_add_child(g_open_function_definition, _get_current_open_block());
        _open_block_pop();

        fprintf(stderr, "- create_block() - create_variable_or_function()\n");
            
        node = g_open_function_definition;
        g_open_function_definition = NULL;

        return node;
      }
      else {
        snprintf(g_error_message, sizeof(g_error_message), "Expected a variable type or a ')', but got %s.\n", _get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return NULL;
      }
    }
  }
  
  return NULL;
}


int create_definition() {

  char name[MAX_NAME_LENGTH + 1];
  int q;

  strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

  /* next token */
  _next_token();
  
  if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
    snprintf(g_error_message, sizeof(g_error_message), "%s must be followed by a definition name.\n", name);
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

  /* next token */
  _next_token();

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '=') {
    /* next token */
    _next_token();
  }

  /* now we need the definition data - it can be a string, an integer or even a calculation stack */

  q = stack_calculate_tokens(&g_parsed_int);

  if (q == SUCCEEDED)
    q = add_a_new_definition(name, (double)g_parsed_int, NULL, DEFINITION_TYPE_VALUE, 0);
  else if (q == INPUT_NUMBER_FLOAT)
    q = add_a_new_definition(name, g_parsed_double, NULL, DEFINITION_TYPE_VALUE, 0);
  else if (q == INPUT_NUMBER_STRING)
    q = add_a_new_definition(name, 0.0, g_label, DEFINITION_TYPE_STRING, strlen(g_label));
  else if (q == INPUT_NUMBER_STACK)
    q = add_a_new_definition(name, (double)g_latest_stack, NULL, DEFINITION_TYPE_STACK, 0);
  else if (q == INPUT_NUMBER_EOL)
    q = add_a_new_definition(name, 0.0, NULL, DEFINITION_TYPE_VALUE, 0);

  return q;
}
