
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "parse.h"
#include "main.h"
#include "pass_1.h"
#include "pass_2.h"
#include "pass_3.h"
#include "printf.h"
#include "stack.h"
#include "include_file.h"
#include "tree_node.h"
#include "symbol_table.h"
#include "inline_asm.h"
#include "inline_asm_z80.h"
#include "struct_item.h"


/* define this for DEBUG */

#define DEBUG_PASS_2 1


extern int g_input_float_mode, g_parsed_int;
extern int g_verbose_mode, g_latest_stack, g_backend;
extern char g_mem_insert_action[MAX_NAME_LENGTH*3 + 1024], g_tmp[4096];
extern double g_parsed_double;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024], g_label[MAX_NAME_LENGTH + 1];

extern struct stack *g_stacks_first, *g_stacks_tmp, *g_stacks_last, *g_stacks_header_first, *g_stacks_header_last;
extern struct token *g_token_first, *g_token_last;

int g_current_filename_id = -1, g_current_line_number = -1;
char *g_variable_types[9] = { "none", "void", "s8", "s16", "u8", "u16", "const", "struct", "union" };
char *g_two_char_symbols[17] = { "||", "&&", "<=", ">=", "==", "!=", "<<", ">>", "++", "--", "-=", "+=", "*=", "/=", "|=", "&=", "->" };

static struct tree_node *g_open_function_definition = NULL;
struct token *g_token_current = NULL, *g_token_previous = NULL;

struct tree_node *g_global_nodes = NULL;

struct tree_node *g_open_expression[256];
int g_open_expression_current_id = -1;

struct tree_node *g_open_condition[256];
int g_open_condition_current_id = -1;

struct tree_node *g_open_block[256];
int g_open_block_current_id = -1;

static int g_block_level = 0;

static int g_check_ast_failed = NO;
static int g_was_main_function_defined = NO;


static void _check_ast_block(struct tree_node *node);
static void _check_ast_function_call(struct tree_node *node);
static void _check_ast_simple_tree_node(struct tree_node *node);
static void _check_ast_expression(struct tree_node *node);
static void _next_token(void);


#if defined(DEBUG_PASS_2)

static void _print_token(struct token *t) {

  if (t->id == TOKEN_ID_VARIABLE_TYPE)
    fprintf(stderr, "TOKEN: VARIABLE_TYPE: %s\n", g_variable_types[t->value]);
  else if (t->id == TOKEN_ID_SYMBOL) {
    if (t->value <= SYMBOL_POINTER)
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
  else if (t->id == TOKEN_ID_SWITCH)
    fprintf(stderr, "TOKEN: SWITCH       : switch\n");
  else if (t->id == TOKEN_ID_CASE)
    fprintf(stderr, "TOKEN: CASE         : case\n");
  else if (t->id == TOKEN_ID_DEFAULT)
    fprintf(stderr, "TOKEN: DEFAULT      : default\n");
  else if (t->id == TOKEN_ID_BYTES)
    fprintf(stderr, "TOKEN: BYTES        : [bytes]\n");
  else if (t->id == TOKEN_ID_ASM)
    fprintf(stderr, "TOKEN: ASM          : __asm\n");
  else if (t->id == TOKEN_ID_EXTERN)
    fprintf(stderr, "TOKEN: EXTERN       : extern\n");
  else if (t->id == TOKEN_ID_STATIC)
    fprintf(stderr, "TOKEN: STATIC       : static\n");
  else if (t->id == TOKEN_ID_SIZEOF)
    fprintf(stderr, "TOKEN: SIZEOF       : sizeof\n");
  else if (t->id == TOKEN_ID_DO)
    fprintf(stderr, "TOKEN: DO           : do\n");
  /*
  else if (t->id == TOKEN_ID_CHANGE_FILE)
    fprintf(stderr, "TOKEN: CHANGE FILE  : %s\n", get_file_name(t->value));
  else if (t->id == TOKEN_ID_LINE_NUMBER)
    fprintf(stderr, "TOKEN: LINE NUMBER  : %d\n", t->value);
  */

  /*
  if (t->id != TOKEN_ID_NO_OP)
    fprintf(stderr, "   %s:%d\n", get_file_name(t->file_id), t->line_number);
  */
}

#endif


static int _print_loose_token_error(struct token *t) {

  if (t->id == TOKEN_ID_SYMBOL) {
    if (t->value <= SYMBOL_POINTER)
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


static int _open_expression_push(void) {

  if (g_open_expression_current_id >= 256)
    return FAILED;

  g_open_expression_current_id++;
  g_open_expression[g_open_expression_current_id] = allocate_tree_node_with_children(TREE_NODE_TYPE_EXPRESSION, 256);

  if (g_open_expression[g_open_expression_current_id] == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _open_expression_pop(void) {

  g_open_expression[g_open_expression_current_id] = NULL;
  
  if (g_open_expression_current_id >= 0) {
    g_open_expression_current_id--;

    return SUCCEEDED;
  }

  return FAILED;
}


static struct tree_node *_get_current_open_expression(void) {

  if (g_open_expression_current_id < 0) {
    print_error("Trying to get the current open expression, but there is none! Please submit a bug report!\n", ERROR_ERR);
    return NULL;
  }
  
  return g_open_expression[g_open_expression_current_id];
}


static int _open_condition_push(void) {

  if (g_open_condition_current_id >= 256)
    return FAILED;

  g_open_condition_current_id++;
  g_open_condition[g_open_condition_current_id] = allocate_tree_node_with_children(TREE_NODE_TYPE_CONDITION, 1);

  if (g_open_condition[g_open_condition_current_id] == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _open_condition_pop(void) {

  g_open_condition[g_open_condition_current_id] = NULL;
  
  if (g_open_condition_current_id >= 0) {
    g_open_condition_current_id--;

    return SUCCEEDED;
  }

  return FAILED;
}


static struct tree_node *_get_current_open_condition(void) {

  if (g_open_condition_current_id < 0) {
    print_error("Trying to get the current open condition, but there is none! Please submit a bug report!\n", ERROR_ERR);
    return NULL;
  }
  
  return g_open_condition[g_open_condition_current_id];
}


static int _open_block_push(void) {

  if (g_open_block_current_id >= 256)
    return FAILED;

  g_open_block_current_id++;
  g_open_block[g_open_block_current_id] = allocate_tree_node_with_children(TREE_NODE_TYPE_BLOCK, 256);

  if (g_open_block[g_open_block_current_id] == NULL)
    return FAILED;

  return SUCCEEDED;
}


static int _open_block_pop(void) {

  g_open_block[g_open_block_current_id] = NULL;
  
  if (g_open_block_current_id >= 0) {
    g_open_block_current_id--;

    return SUCCEEDED;
  }

  return FAILED;
}


static struct tree_node *_get_current_open_block(void) {

  if (g_open_block_current_id < 0) {
    print_error("Trying to get the current open block, but there is none! Please submit a bug report!\n", ERROR_ERR);
    return NULL;
  }
  
  return g_open_block[g_open_block_current_id];
}


static struct tree_node *_create_z80_in_out(char *name) {

  struct tree_node *node;

  node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 3);
  if (node == NULL)
    return NULL;

  tree_node_add_child(node, allocate_tree_node_variable_type(VARIABLE_TYPE_UINT8));
  tree_node_add_child(node, allocate_tree_node_value_string(name));

  /* set array size */
  node->value = 256;

  return node;
}


static int _pass_2_z80_init(void) {

  /* create global arrays __z80_out[] and __z80_in[] */
  struct tree_node *node;

  /* __z80_out */
  node = _create_z80_in_out("__z80_out");
  if (node == NULL)
    return FAILED;

  if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, -1, -1) == FAILED) {
    free_tree_node(node);
    return FAILED;
  }

  if (tree_node_add_child(g_global_nodes, node) == FAILED)
    return FAILED;

  /* __z80_in */
  node = _create_z80_in_out("__z80_in");
  if (node == NULL)
    return FAILED;

  if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, -1, -1) == FAILED) {
    free_tree_node(node);
    return FAILED;
  }

  if (tree_node_add_child(g_global_nodes, node) == FAILED)
    return FAILED;

  return SUCCEEDED;
}


int pass_2(void) {

  int i;
  
  if (g_verbose_mode == ON)
    printf("Pass 2...\n");

  /* init */
  for (i = 0; i < 256; i++)
    g_open_expression[i] = NULL;
  
  g_global_nodes = allocate_tree_node_with_children(TREE_NODE_TYPE_GLOBAL_NODES, 256);
  if (g_global_nodes == NULL)
    return FAILED;

  /* Z80 special */
  if (g_backend == BACKEND_Z80) {
    if (_pass_2_z80_init() == FAILED)
      return FAILED;
  }

  /* start from the very first token */
  g_token_current = g_token_first;
  g_token_previous = NULL;
  g_current_filename_id = g_token_current->file_id;
  g_current_line_number = g_token_current->line_number;

#if defined(DEBUG_PASS_2)
  _print_token(g_token_current);
#endif

  while (g_token_current != NULL) {
    int is_extern = NO, is_static = NO;
    
    if (g_token_current->id == TOKEN_ID_EXTERN) {
      is_extern = YES;

      /* next token */
      _next_token();
    }
    else if (g_token_current->id == TOKEN_ID_STATIC) {
      is_static = YES;

      /* next token */
      _next_token();
    }

    if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
      if (create_variable_or_function(is_extern, is_static) == FAILED)
        return FAILED;

      if (g_block_level != 0) {
        snprintf(g_error_message, sizeof(g_error_message), "g_block_level should be 0 here, but it's %d instead! Please submit a bug report!\n", g_block_level);
        return print_error(g_error_message, ERROR_ERR);
      }

      if (g_was_main_function_defined == YES) {
        /* special case! right after "void main(void)" is parsed we'll generate tokens for "void mainmain(void)".
           mainmain() is where the initializer code jumps to, and mainmain() jumps to main(). this way the compiler
           can generate a proper stack allocating jump to main() while the linker just makes a plain and simple jump
           to mainmain()... */
        create_mainmain_tokens();
        g_was_main_function_defined = NO;
      }
    }
    else if (g_token_current->id == TOKEN_ID_NO_OP) {
      /* no operation */
    
      /* next token */
      g_token_current = g_token_current->next;
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
      /* skip ';' */
      
      /* next token */
      g_token_current = g_token_current->next;
    }
    else {
      return _print_loose_token_error(g_token_current);
    }
  }

  /* calculate struct sizes and member offsets */
  if (calculate_struct_items() == FAILED)
    return FAILED;

  check_ast();
  if (g_check_ast_failed == YES)
    return FAILED;
  
  free_symbol_table_items(0);
  
  if (g_open_expression_current_id != -1) {
    fprintf(stderr, "ERROR: We have an open expression even though parsing has ended. Please submit a bug report!\n");
    return FAILED;
  }
  
  if (g_open_function_definition != NULL) {
    fprintf(stderr, "ERROR: We have an open function definition even though parsing has ended. Please submit a bug report!\n");
    return FAILED;
  }
  
  if (g_open_block_current_id != -1) {
    fprintf(stderr, "ERROR: We have an open block even though parsing has ended. Please submit a bug report!\n");
    return FAILED;
  }

  if (is_symbol_table_empty() == NO) {
    fprintf(stderr, "ERROR: The symbol table is not empty even though parsing has ended! Please submit a bug report!\n");
    return FAILED;
  }
  
  return SUCCEEDED;
}


static void _next_token(void) {

  g_token_previous = g_token_current;
  g_token_current = g_token_current->next;
  g_current_filename_id = g_token_current->file_id;
  g_current_line_number = g_token_current->line_number;

#if defined(DEBUG_PASS_2)
  _print_token(g_token_current);
#endif
}


static void _simplify_expressions_with_tree_node_type_bytes(struct tree_node *node) {

  int i;
  
  /* some items can be TREE_NODE_TYPE_EXPRESSION with single TREE_NODE_TYPE_BYTES child -> simplify! */

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION &&
        node->children[i]->added_children == 1 &&
        node->children[i]->children[0]->type == TREE_NODE_TYPE_BYTES) {
      struct tree_node *n;

      n = node->children[i]->children[0];
      node->children[i]->children[0] = NULL;
      free_tree_node(node->children[i]);
      node->children[i] = n;
    }
  }
}


static int _add_symbol_as_child(struct tree_node *node, int symbol) {

  struct tree_node *child;
  
  child = allocate_tree_node_symbol(symbol);
  if (child == NULL)
    return FAILED;

  if (tree_node_add_child(node, child) == FAILED) {
    free_tree_node(child);
    return FAILED;
  }

  return SUCCEEDED;
}


static int _parse_struct_access(struct tree_node *node) {

  struct symbol_table_item *item;
  struct tree_node *child;

  /* node is either a TREE_NODE_TYPE_VALUE_STRING or TREE_NODE_TYPE_ARRAY_ITEM,
     so let's convert that to TREE_NODE_TYPE_STRUCT_ACCESS */

  child = allocate_tree_node_value_string(node->label);
  if (child == NULL)
    return FAILED;

  item = symbol_table_find_symbol(child->label);
  if (item != NULL)
    child->definition = item->node;

  if (node->type == TREE_NODE_TYPE_ARRAY_ITEM) {
    /* we need to add '[' and ']' symbols to the node... */
    struct tree_node *expression = node->children[0];

    node->children[0] = child;

    child = allocate_tree_node_symbol('[');
    if (child == NULL) {
      free_tree_node(node);
      return FAILED;
    }

    if (tree_node_add_child(node, child) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    if (tree_node_add_child(node, expression) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    child = allocate_tree_node_symbol(']');
    if (child == NULL) {
      free_tree_node(node);
      return FAILED;
    }

    if (tree_node_add_child(node, child) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }
  }
  else {
    if (tree_node_add_child(node, child) == FAILED) {
      free_tree_node(child);
      return FAILED;
    }
  }
  
  node->type = TREE_NODE_TYPE_STRUCT_ACCESS;
    
  while (1) {
    if ((g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '.') ||
        (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_POINTER)) {
      if (_add_symbol_as_child(node, g_token_current->value) == FAILED)
        return FAILED;

      /* next token */
      _next_token();
    }
    else
      return SUCCEEDED;

    if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
      child = allocate_tree_node_value_string(g_token_current->label);
      if (child == NULL)
        return FAILED;

      if (tree_node_add_child(node, child) == FAILED) {
        free_tree_node(child);
        return FAILED;
      }
      
      /* next token */
      _next_token();

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '[') {
        /* an array! */

        if (_add_symbol_as_child(node, g_token_current->value) == FAILED)
          return FAILED;

        /* next token */
        _next_token();

        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED)
          return FAILED;

        /* possibly parse a calculation */
        if (create_expression() == FAILED)
          return FAILED;

        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();        

        if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']')) {
          snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }

        if (_add_symbol_as_child(node, g_token_current->value) == FAILED)
          return FAILED;

        /* next token */
        _next_token();
      }
    }
    else {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a struct/union member name, but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }
  }
}


int create_factor(void) {

  struct symbol_table_item *item;
  struct tree_node *node = NULL;
  int result;
  
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

      while (1) {
        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')')
          break;
      
        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        /* possibly parse a calculation */
        result = create_expression();
        if (result == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();
        }
      }

      item = symbol_table_find_symbol(node->children[0]->label);
      if (item != NULL)
        node->definition = item->node;
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

      item = symbol_table_find_symbol(node->label);
      if (item != NULL)
        node->definition = item->node;
      
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

      /* children[0] - index */
      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();
    
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

       /* next token */
      _next_token();

      item = symbol_table_find_symbol(node->label);
      if (item != NULL)
        node->definition = item->node;

      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      if ((g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '.') ||
          (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_POINTER)) {
        /* we are handling a struct here! */
        if (_parse_struct_access(node) == FAILED)
          return FAILED;

        return SUCCEEDED;
      }

      return SUCCEEDED;
    }
    else {
      /* loose variable name */
      if (node->type == TREE_NODE_TYPE_VALUE_STRING) {
        item = symbol_table_find_symbol(node->label);
        if (item != NULL)
          node->definition = item->node;
      }
      
      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      if ((g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '.') ||
          (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_POINTER)) {
        /* we are handling a struct here! */
        if (_parse_struct_access(node) == FAILED)
          return FAILED;

        return SUCCEEDED;
      }
      
      return SUCCEEDED;
    }
  }
  else if (g_token_current->id == TOKEN_ID_SIZEOF) {
    struct tree_node *delayed_node = NULL;
    int size = 0;
    
    /* next token */
    _next_token();

    if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '(')) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* skip '(' */
    _next_token();

    if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
      /* skip 'const' */
      _next_token();
    }
    
    if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
      int pointer_depth = 0;
      
      if (g_token_current->value == VARIABLE_TYPE_STRUCT || g_token_current->value == VARIABLE_TYPE_UNION) {
        int type = g_token_current->value;
        
        /* skip 'struct/union' */
        _next_token();

        if (g_token_current->id != TOKEN_ID_VALUE_STRING)
          return print_error("sizeof() needs struct/union's name.\n", ERROR_ERR);

        /* at this point struct/unions don't have valid sizes - we need to delay this calculation */

        delayed_node = allocate_tree_node_value_string(g_token_current->label);
        if (delayed_node == NULL)
          return FAILED;

        delayed_node->type = TREE_NODE_TYPE_SIZEOF;
        delayed_node->value = type;

        /* skip the name */
        _next_token();
      }
      else {
        /* built-in type */
        size = get_variable_type_size(g_token_current->value) / 8;

        /* skip built-in type */
        _next_token();
      }

      /* is it a pointer? */

      while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
        pointer_depth++;

        /* next token */
        _next_token();
      }

      /* pointer size is 2 bytes */
      if (pointer_depth > 0) {
        size = 2;

        /* no need to figure out the size of the delayed node, pointers are always worth 2 bytes */
        if (delayed_node != NULL) {
          free_tree_node(delayed_node);
          delayed_node = NULL;
        }
      }
    }
    else {
      /* it's either a variable or an expression */

      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED)
        return FAILED;

      /* possibly parse a calculation */
      if (create_expression() == FAILED)
        return FAILED;

      node = _get_current_open_expression();
      _open_expression_pop();

      if (node->added_children == 1 && node->children[0]->type == TREE_NODE_TYPE_VALUE_STRING) {
        /* variable name? */
        struct symbol_table_item *item;
        
        item = symbol_table_find_symbol(node->children[0]->label);
        if (item == NULL) {
          /* at this point we don't know the variable (it must be global) - we need to delay this calculation */

          delayed_node = allocate_tree_node_value_string(node->children[0]->label);
          if (delayed_node == NULL)
            return FAILED;

          delayed_node->type = TREE_NODE_TYPE_SIZEOF;
          delayed_node->value = VARIABLE_TYPE_NONE;
        }
        else {
          if (item->node->type == TREE_NODE_TYPE_CREATE_VARIABLE && (item->node->children[0]->value == VARIABLE_TYPE_STRUCT ||
                                                                     item->node->children[0]->value == VARIABLE_TYPE_UNION)) {
            /* at this point struct/unions don't have valid sizes - we need to delay this calculation */

            delayed_node = allocate_tree_node_value_string(item->node->children[0]->children[0]->label);
            if (delayed_node == NULL)
              return FAILED;

            delayed_node->type = TREE_NODE_TYPE_SIZEOF;
            delayed_node->value = item->node->children[0]->value;
          }
          else
            size = get_variable_node_size_in_bytes(item->node);
        }
      }
      else {
        /* expression */
        int type;
        
        type = tree_node_get_max_var_type(node);
        size = get_variable_type_size(type) / 8;
      }

      free_tree_node(node);
    }

    if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')')) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    if (delayed_node != NULL)
      node = delayed_node;
    else
      node = allocate_tree_node_value_int(size);
  }
  else if (g_token_current->id == TOKEN_ID_VALUE_INT) {
    node = allocate_tree_node_value_int(g_token_current->value);
  }
  else if (g_token_current->id == TOKEN_ID_VALUE_DOUBLE)
    node = allocate_tree_node_value_double(g_token_current->value_double);
  else if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == SYMBOL_INCREMENT || g_token_current->value == SYMBOL_DECREMENT)) {
    /* --var or ++var */
    int symbol = g_token_current->value;

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a variable name, but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
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

    item = symbol_table_find_symbol(node->label);
    if (item != NULL)
      node->definition = item->node;
    
    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '&') {
    /* &var */

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a variable name, but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    node = allocate_tree_node_value_string(g_token_current->label);
    if (node == NULL)
      return FAILED;

    /* next token */
    _next_token();

    /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_GET_ADDRESS */
    node->type = TREE_NODE_TYPE_GET_ADDRESS;

    item = symbol_table_find_symbol(node->label);
    if (item != NULL) {
      node->definition = item->node;

      /* mark references */
      node->definition->reads++;
    }

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '[') {
      /* array! */

      /* change the TREE_NODE_TYPE_GET_ADDRESS to TREE_NODE_TYPE_GET_ADDRESS_ARRAY */
      node->type = TREE_NODE_TYPE_GET_ADDRESS_ARRAY;

      /* next token */
      _next_token();

      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED)
        return FAILED;    

      /* possibly parse a calculation */
      if (create_expression() == FAILED)
        return FAILED;

      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();

      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }
    
      /* next token */
      _next_token();
    }
    
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

    /* create_expression() will put all tree_nodes it parses to g_open_expression */
    if (_open_expression_push() == FAILED)
      return FAILED;
    
    result = create_expression();
    if (result != SUCCEEDED)
      return result;

    expression = _get_current_open_expression();
    _open_expression_pop();
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
      free_tree_node(expression);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

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
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ':') {
    return FINISHED;
  }
  else if (g_token_current->id == TOKEN_ID_BYTES)
    node = allocate_tree_node_bytes(g_token_current);
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

  if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '!' || g_token_current->value == '~')) {
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();
  }
  
  result = create_factor();
  if (result != SUCCEEDED)
    return result;

  while (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '*' ||
                                                    g_token_current->value == '/' ||
                                                    g_token_current->value == '~' ||
                                                    g_token_current->value == '^' ||
                                                    g_token_current->value == '%' ||
                                                    g_token_current->value == '&' ||
                                                    g_token_current->value == '|' ||
                                                    g_token_current->value == '<' ||
                                                    g_token_current->value == '>' ||
                                                    g_token_current->value == SYMBOL_SHIFT_LEFT ||
                                                    g_token_current->value == SYMBOL_SHIFT_RIGHT ||
                                                    g_token_current->value == SYMBOL_EQUAL ||
                                                    g_token_current->value == SYMBOL_UNEQUAL ||
                                                    g_token_current->value == SYMBOL_LTE ||
                                                    g_token_current->value == SYMBOL_GTE ||
                                                    g_token_current->value == SYMBOL_LOGICAL_OR ||
                                                    g_token_current->value == SYMBOL_LOGICAL_AND)) {
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '!') {
      node = allocate_tree_node_symbol(g_token_current->value);
      if (node == NULL)
        return FAILED;

      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      /* next token */
      _next_token();
    }
    
    result = create_factor();
    if (result != SUCCEEDED)
      return result;
  }

  return SUCCEEDED;
}


int create_expression(void) {

  struct tree_node *node = NULL;
  int result;

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
    /* next token */
    _next_token();
    
    return FINISHED;
  }

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
    /* next token */
    _next_token();
    
    return THE_END;
  }

  while (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '-' || g_token_current->value == '+')) {
    node = allocate_tree_node_symbol(g_token_current->value);
    if (node == NULL)
      return FAILED;

    if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
      return FAILED;

    /* next token */
    _next_token();
  }

  result = create_term();
  if (result != SUCCEEDED)
    return result;

  while (1) {
    int got = 0;
    
    while (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == '-' || g_token_current->value == '+')) {
      got++;
      
      node = allocate_tree_node_symbol(g_token_current->value);
      if (node == NULL)
        return FAILED;

      if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
        return FAILED;

      /* next token */
      _next_token();
    }

    if (got == 0)
      break;
    
    result = create_term();
    if (result != SUCCEEDED)
      return result;
  }  
  
  return SUCCEEDED;
}


/* TODO: remove TREE_NODE_TYPE_CONDITION as it now contains just one TREE_NODE_TYPE_EXPRESSION */
int create_condition(void) {

  struct tree_node *node;

  /* create_expression() will put all tree_nodes it parses to g_open_expression */
  if (_open_expression_push() == FAILED) {
    return FAILED;
  }

  /* possibly parse a calculation */
  if (create_expression() == FAILED) {
    return FAILED;
  }

  node = _get_current_open_expression();
  _open_expression_pop();
    
  tree_node_add_child(_get_current_open_condition(), node);

  return SUCCEEDED;
}


int create_statement(void) {

  struct tree_node *node;
  int symbol;
  
  if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
    /* variable creation */
    int variable_type, pointer_depth, is_const_1 = NO;
    char name[MAX_NAME_LENGTH + 1];

    variable_type = g_token_current->value;
    
    /* next token */
    _next_token();

    if (variable_type == VARIABLE_TYPE_CONST) {
      is_const_1 = YES;

      /* get the real variable type */
      if (g_token_current->id != TOKEN_ID_VARIABLE_TYPE)
        return print_error_using_token("\"const\" must be followed by a variable type.\n", ERROR_ERR, g_token_previous);

      variable_type = g_token_current->value;

      if (variable_type == VARIABLE_TYPE_CONST)
        return print_error("\"const const\" doesn't make any sense.\n", ERROR_ERR);

      /* next token */
      _next_token();
    }
    
    while (1) {
      int is_const_2 = NO, file_id, line_number;
      
      /* it's a struct/union? */
      if (variable_type == VARIABLE_TYPE_STRUCT || variable_type == VARIABLE_TYPE_UNION) {
        if (create_struct_union(NO, NO, is_const_1, variable_type, NO) == FAILED)
          return FAILED;

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();

          continue;
        }

        if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
          snprintf(g_error_message, sizeof(g_error_message), "Expected ';' or ',', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }

        /* skip ';' */
        _next_token();
      
        return SUCCEEDED;
      }

      pointer_depth = 0;

      while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
        pointer_depth++;

        /* next token */
        _next_token();
      }
    
      if (pointer_depth > 0 && g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
        is_const_2 = YES;

        /* next token */
        _next_token();
      }
      
      if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
        snprintf(g_error_message, sizeof(g_error_message), "\"%s\" must be followed by a variable name.\n", g_variable_types[variable_type]);
        return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
      }

      strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

      /* remember these for symbol_table_add_symbol() */
      file_id = g_token_current->file_id;
      line_number = g_token_current->line_number;
      
      /* next token */
      _next_token();

      if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                     g_token_current->value != '[' &&
                                                     g_token_current->value != ',' &&
                                                     g_token_current->value != ';')) {
        snprintf(g_error_message, sizeof(g_error_message), "\"%s\" must be followed by a ';', ',', '[' or '='.\n", name);
        return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
      }

      symbol = g_token_current->value;

      /* next token */
      _next_token();
    
      if (symbol == '=') {
        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED)
          return FAILED;

        /* possibly parse a calculation */
        if (create_expression() == FAILED)
          return FAILED;

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
        node = create_array(pointer_depth, variable_type, name, line_number, file_id);
        if (node == NULL)
          return FAILED;

        tree_node_add_child(_get_current_open_block(), node);

        /* store const flags */
        if (is_const_1 == YES)
          node->flags |= TREE_NODE_FLAG_CONST_1;
        if (is_const_2 == YES)
          node->flags |= TREE_NODE_FLAG_CONST_2;

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();

          continue;
        }
        
        if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
          snprintf(g_error_message, sizeof(g_error_message), "Expected ';' or ',', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }

        /* add to symbol table */
        if (symbol_table_add_symbol(node, name, g_block_level, line_number, file_id) == FAILED)
          return FAILED;

        /* next token */
        _next_token();

        return SUCCEEDED;
      }

      node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 3);
      if (node == NULL)
        return FAILED;

      /* use the line where the name was given */
      node->file_id = file_id;
      node->line_number = line_number;

      tree_node_add_child(node, allocate_tree_node_variable_type(variable_type));
      tree_node_add_child(node, allocate_tree_node_value_string(name));
      if (symbol == '=') {
        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();
      }

      /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
      node->children[0]->value_double = pointer_depth;

      /* store const flags */
      if (is_const_1 == YES)
        node->flags |= TREE_NODE_FLAG_CONST_1;
      if (is_const_2 == YES)
        node->flags |= TREE_NODE_FLAG_CONST_2;

      if (node->children[0] == NULL || node->children[1] == NULL || (symbol == '=' && node->children[2] == NULL)) {
        free_tree_node(node);
        return print_error("_create_statement(): Out of memory error.\n", ERROR_ERR);
      }
    
      tree_node_add_child(_get_current_open_block(), node);

      /* add to symbol table */
      if (symbol_table_add_symbol(node, name, g_block_level, line_number, file_id) == FAILED)
        return FAILED;

      if (symbol == ',')
        continue;

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
        /* next token */
        _next_token();

        continue;
      }

      if (symbol == ';')
        return SUCCEEDED;
      
      if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ';' or ',', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();
      
      return SUCCEEDED;
    }
  }
  else if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
    /* assignment, a function call, increment or decrement */
    char name[MAX_NAME_LENGTH + 1];
    struct tree_node *target_node = NULL;
    
    strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

    /* next token */
    _next_token();

    if (strcmp(name, "goto") == 0) {
      /* goto */
      if (g_token_current->id != TOKEN_ID_VALUE_STRING)
        return print_error_using_token("\"goto\" needs a label to go to.\n", ERROR_ERR, g_token_previous);

      /* add prefix '_' */
      snprintf(g_error_message, sizeof(g_error_message), "_%s", g_token_current->label);
      
      /* next token */
      _next_token();

      node = allocate_tree_node_value_string(g_error_message);
      if (node == NULL)
        return FAILED;

      node->type = TREE_NODE_TYPE_GOTO;
      
      tree_node_add_child(_get_current_open_block(), node);

      if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';'))  {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
        return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
      }

      /* next token */
      _next_token();

      return SUCCEEDED;
    }
    
    if ((g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '.') ||
        (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_POINTER)) {
      target_node = allocate_tree_node_value_string(name);
      if (target_node == NULL)
        return FAILED;

      if (_parse_struct_access(target_node) == FAILED) {
        free_tree_node(target_node);
        return FAILED;
      }
    }

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                   g_token_current->value != '(' &&
                                                   g_token_current->value != '[' &&
                                                   g_token_current->value != ':' &&
                                                   g_token_current->value != SYMBOL_INCREMENT &&
                                                   g_token_current->value != SYMBOL_DECREMENT)) {
      free_tree_node(target_node);
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" must be followed by '=' / '[' / '(' / '++' / '--'.\n", name);
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    symbol = g_token_current->value;

    /* next token */
    _next_token();

    if (symbol == SYMBOL_INCREMENT || symbol == SYMBOL_DECREMENT) {
      /* increment / decrement */

      if (target_node != NULL)
        node = target_node;
      else {
        struct symbol_table_item *item;

        node = allocate_tree_node_value_string(name);
        if (node == NULL)
          return FAILED;

        item = symbol_table_find_symbol(name);
        if (item != NULL)
          node->definition = item->node;
      }
      
      /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_INCREMENT_DECREMENT */
      node->type = TREE_NODE_TYPE_INCREMENT_DECREMENT;

      /* ++ or -- */
      node->value = symbol;

      /* post */
      node->value_double = 1.0;

      tree_node_add_child(_get_current_open_block(), node);

      if (!(g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == ';' || g_token_current->value == ')')))  {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ')' / ';', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();
      
      return SUCCEEDED;
    }
    else if (symbol == ':') {
      /* label */

      if (target_node != NULL) {
        free_tree_node(target_node);
        return print_error_using_token("':' is in a wrong place.\n", ERROR_ERR, g_token_previous);
      }
      
      /* add prefix '_' */
      snprintf(g_error_message, sizeof(g_error_message), "_%s", name);
      
      node = allocate_tree_node_value_string(g_error_message);
      if (node == NULL)
        return FAILED;

      node->type = TREE_NODE_TYPE_LABEL;
      
      tree_node_add_child(_get_current_open_block(), node);

      return SUCCEEDED;
    }
    else if (symbol == '=' || symbol == '[') {
      /* assignment */
      struct tree_node *node_index = NULL;
      struct symbol_table_item *item;
    
      if (symbol == '[') {
        /* array assignment */

        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED)
          return FAILED;    

        /* possibly parse a calculation */
        if (create_expression() == FAILED)
          return FAILED;

        node_index = _get_current_open_expression();
        _open_expression_pop();

        if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
          free_tree_node(node_index);
          snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }
    
        /* next token */
        _next_token();

        if ((g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '.') ||
            (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == SYMBOL_POINTER)) {
          target_node = allocate_tree_node_value_string(name);
          if (target_node == NULL)
            return FAILED;

          target_node->type = TREE_NODE_TYPE_ARRAY_ITEM;
          tree_node_add_child(target_node, node_index);

          if (_parse_struct_access(target_node) == FAILED)
            return FAILED;

          if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == SYMBOL_INCREMENT ||
                                                         g_token_current->value == SYMBOL_DECREMENT)) {
            /* struct_accress[]++ / struct_access[]-- */

            /* change the TREE_NODE_TYPE_ARRAY_ITEM to TREE_NODE_TYPE_INCREMENT_DECREMENT */
            target_node->type = TREE_NODE_TYPE_INCREMENT_DECREMENT;

            /* ++ or -- */
            target_node->value = g_token_current->value;

            /* post */
            target_node->value_double = 1.0;

            tree_node_add_child(_get_current_open_block(), target_node);

            /* next token */
            _next_token();
          
            if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
              snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
              return print_error(g_error_message, ERROR_ERR);
            }

            /* next token */
            _next_token();
      
            return SUCCEEDED;
          }
        }
          
        if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                       g_token_current->value != SYMBOL_EQUAL_SUB &&
                                                       g_token_current->value != SYMBOL_EQUAL_ADD &&
                                                       g_token_current->value != SYMBOL_EQUAL_MUL &&
                                                       g_token_current->value != SYMBOL_EQUAL_DIV &&
                                                       g_token_current->value != SYMBOL_EQUAL_OR &&
                                                       g_token_current->value != SYMBOL_EQUAL_AND)) {
          if (target_node != NULL)
            free_tree_node(target_node);
          else
            free_tree_node(node_index);
          snprintf(g_error_message, sizeof(g_error_message), "Expected '=', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }

        symbol = g_token_current->value;
        
        /* next token */
        _next_token();
      }
      
      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED)
        return FAILED;

      /* if the assignment was of form += then we'll modify the expression... */
      if (symbol == SYMBOL_EQUAL_SUB ||
          symbol == SYMBOL_EQUAL_ADD ||
          symbol == SYMBOL_EQUAL_MUL ||
          symbol == SYMBOL_EQUAL_DIV ||
          symbol == SYMBOL_EQUAL_OR ||
          symbol == SYMBOL_EQUAL_AND) {
        int operator;

        if (target_node != NULL) {
          /* FIX! if the index expression contains increments or decrements then this
             clone will also contain them -> the statement will do them twice! */
          node = clone_tree_node(target_node);
        }
        else {
          node = allocate_tree_node_value_string(name);
          if (node == NULL)
            return FAILED;

          if (node_index != NULL) {
            struct tree_node *node_index_2;
            struct symbol_table_item *item;
          
            /* change the TREE_NODE_TYPE_VALUE_STRING to TREE_NODE_TYPE_ARRAY_ITEM */
            node->type = TREE_NODE_TYPE_ARRAY_ITEM;

            /* FIX! if the index expression contains increments or decrements then this
               clone will also contain them -> the statement will do them twice! */
            node_index_2 = clone_tree_node(node_index);
            if (node_index_2 == NULL) {
              free_tree_node(node);
              return FAILED;
            }

            /* children[0] - index */
            tree_node_add_child(node, node_index_2);

            item = symbol_table_find_symbol(node->label);
            if (item != NULL)
              node->definition = item->node;
          }
        }

        if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
          return FAILED;

        /* -, +, *, /, | or & */
        if (symbol == SYMBOL_EQUAL_SUB)
          operator = '-';
        else if (symbol == SYMBOL_EQUAL_ADD)
          operator = '+';
        else if (symbol == SYMBOL_EQUAL_MUL)
          operator = '*';
        else if (symbol == SYMBOL_EQUAL_DIV)
          operator = '/';
        else if (symbol == SYMBOL_EQUAL_OR)
          operator = '|';
        else if (symbol == SYMBOL_EQUAL_AND)
          operator = '&';
        
        node = allocate_tree_node_symbol(operator);
        if (node == NULL)
          return FAILED;

        if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
          return FAILED;

        /* '(' */
        node = allocate_tree_node_symbol('(');
        if (node == NULL)
          return FAILED;

        if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
          return FAILED;

        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED)
          return FAILED;
      }
      
      /* possibly parse a calculation */
      if (create_expression() == FAILED)
        return FAILED;

      if (symbol == SYMBOL_EQUAL_SUB ||
          symbol == SYMBOL_EQUAL_ADD ||
          symbol == SYMBOL_EQUAL_MUL ||
          symbol == SYMBOL_EQUAL_DIV ||
          symbol == SYMBOL_EQUAL_OR ||
          symbol == SYMBOL_EQUAL_AND) {
        node = _get_current_open_expression();
        
        _open_expression_pop();

        if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
          return FAILED;

        /* ')' */
        node = allocate_tree_node_symbol(')');
        if (node == NULL)
          return FAILED;

        if (tree_node_add_child(_get_current_open_expression(), node) == FAILED)
          return FAILED;
      }

      node = allocate_tree_node_with_children(TREE_NODE_TYPE_ASSIGNMENT, 3);
      if (node == NULL)
        return FAILED;

      if (target_node != NULL) {
        tree_node_add_child(node, target_node);
      }
      else {
        tree_node_add_child(node, allocate_tree_node_value_string(name));
        if (node_index != NULL)
          tree_node_add_child(node, node_index);
      }
      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();
      
      if (node->children[0] == NULL || node->children[1] == NULL || (node->added_children == 3 && node->children[2] == NULL)) {
        free_tree_node(node);
        return FAILED;
      }

      tree_node_add_child(_get_current_open_block(), node);

      item = symbol_table_find_symbol(name);
      if (item != NULL)
        node->children[0]->definition = item->node;
    }
    else {
      /* function call */
      struct symbol_table_item *item;

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

        /* possibly parse a calculation */
        if (create_expression() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
          /* next token */
          _next_token();
        }
      }

      item = symbol_table_find_symbol(name);
      if (item != NULL)
        node->definition = item->node;
      
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
      snprintf(g_error_message, sizeof(g_error_message), "Expected '}' / ',' / ';', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_SWITCH) {
    /* switch */
    
    node = allocate_tree_node_with_children(TREE_NODE_TYPE_SWITCH, 1);
    if (node == NULL)
      return FAILED;

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    /* create_expression() will put all tree_nodes it parses to g_open_expression */
    if (_open_expression_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    /* possibly parse a calculation */
    if (create_expression() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_expression());
    _open_expression_pop();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();
    
    while (1) {
      int is_case = YES;

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
        /* next token */
        _next_token();

        break;
      }

      if (g_token_current->id != TOKEN_ID_CASE && g_token_current->id != TOKEN_ID_DEFAULT) {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected 'case' or 'default', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      if (g_token_current->id == TOKEN_ID_DEFAULT)
        is_case = NO;

      /* next token */
      _next_token();

      if (is_case == YES) {
        /* case */

        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        /* possibly parse a calculation */
        if (create_expression() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();
      }
      
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ':') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected ':', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();

      /* start parsing a block */
      
      if (_open_block_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      if (create_block(NULL, NO, YES) == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      tree_node_add_child(node, _get_current_open_block());
      _open_block_pop();

      if (is_case == NO) {
        if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '}') {
          free_tree_node(node);
          snprintf(g_error_message, sizeof(g_error_message), "Expected '}', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }
      }
    }

    /* the switch ends here */
    tree_node_add_child(_get_current_open_block(), node);

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
      int is_multiline_block = NO;
      
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();

      /* create_condition() will put all tree_nodes it parses to g_open_condition */
      if (_open_condition_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      if (create_condition() == FAILED) {
        _open_condition_pop();
        free_tree_node(node);
        return FAILED;
      }

      tree_node_add_child(node, _get_current_open_condition());
      _open_condition_pop();
      
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{')
        is_multiline_block = YES;

      /* start parsing a block */
      
      if (_open_block_push() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      if (create_block(NULL, is_multiline_block, is_multiline_block) == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      tree_node_add_child(node, _get_current_open_block());
      _open_block_pop();

      if (g_token_current->id != TOKEN_ID_ELSE) {
        /* the if ends here */
        tree_node_add_child(_get_current_open_block(), node);

        return SUCCEEDED;
      }

      /* skip "else" */
      _next_token();
      
      if ((g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{') || g_token_current->id != TOKEN_ID_IF) {
        /* the final block */
        
        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{')
          is_multiline_block = YES;
        
        if (_open_block_push() == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        if (create_block(NULL, is_multiline_block, is_multiline_block) == FAILED) {
          free_tree_node(node);
          return FAILED;
        }

        tree_node_add_child(node, _get_current_open_block());
        _open_block_pop();

        /* the if ends here */
        tree_node_add_child(_get_current_open_block(), node);

        return SUCCEEDED;
      }

      if (g_token_current->id != TOKEN_ID_IF) {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected 'if', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();
    }
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && (g_token_current->value == SYMBOL_INCREMENT || g_token_current->value == SYMBOL_DECREMENT)) {
    /* increment / decrement */
    struct symbol_table_item *item;
    char name[MAX_NAME_LENGTH + 1];
    int symbol = g_token_current->value;

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a variable name, but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
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

    item = symbol_table_find_symbol(name);
    if (item != NULL)
      node->definition = item->node;
    
    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_WHILE) {
    int is_multiline_block = NO;
    
    /* while */
    
    /* next token */
    _next_token();

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_WHILE, 2);
    if (node == NULL)
      return FAILED;

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    /* create_condition() will put all tree_nodes it parses to g_open_condition */
    if (_open_condition_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    if (create_condition() == FAILED) {
      _open_condition_pop();
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_condition());
    _open_condition_pop();
      
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{')
      is_multiline_block = YES;

    /* start parsing a block */
      
    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    if (create_block(NULL, is_multiline_block, is_multiline_block) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();

    tree_node_add_child(_get_current_open_block(), node);

    return SUCCEEDED;    
  }
  else if (g_token_current->id == TOKEN_ID_DO) {
    int is_multiline_block = NO;
    
    /* do */
    
    /* next token */
    _next_token();

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_DO, 2);
    if (node == NULL)
      return FAILED;

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{')
      is_multiline_block = YES;

    /* start parsing a block */
      
    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    if (create_block(NULL, is_multiline_block, is_multiline_block) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();

    tree_node_add_child(_get_current_open_block(), node);

    if (g_token_current->id != TOKEN_ID_WHILE) {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected 'while', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* skip "while" */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    /* create_condition() will put all tree_nodes it parses to g_open_condition */
    if (_open_condition_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    if (create_condition() == FAILED) {
      _open_condition_pop();
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_condition());
    _open_condition_pop();
      
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ')') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ')', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();
    
    return SUCCEEDED;    
  }
  else if (g_token_current->id == TOKEN_ID_FOR) {
    int is_multiline_block = NO;
    
    /* for */

    /* next token */
    _next_token();

    node = allocate_tree_node_with_children(TREE_NODE_TYPE_FOR, 4);
    if (node == NULL)
      return FAILED;

    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '(') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected '(', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
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

      result = create_statement();

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

    if (create_condition() == FAILED) {
      _open_condition_pop();
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_condition());
    _open_condition_pop();
      
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
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
      
      result = create_statement();

      if (result == FAILED) {
        free_tree_node(node);
        return result;
      }
      if (result == FINISHED)
        break;
    }

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();
    
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{')
      is_multiline_block = YES;

    /* start parsing a block */
      
    if (_open_block_push() == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    if (create_block(NULL, is_multiline_block, is_multiline_block) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    tree_node_add_child(node, _get_current_open_block());
    _open_block_pop();

    tree_node_add_child(_get_current_open_block(), node);

    return SUCCEEDED;    
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
    /* the end of the block */
    return FINISHED;
  }
  else if (g_token_current->id == TOKEN_ID_CASE || g_token_current->id == TOKEN_ID_DEFAULT) {
    /* the end of the block */
    return FINISHED;
  }
  else if (g_token_current->id == TOKEN_ID_RETURN) {
    /* return */

    /* next token */
    _next_token();

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
      /* return; */
      
      node = allocate_tree_node(TREE_NODE_TYPE_RETURN);
      if (node == NULL)
        return FAILED;

      tree_node_add_child(_get_current_open_block(), node);

      /* next token */
      _next_token();

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

      /* possibly parse a calculation */
      if (create_expression() == FAILED) {
        free_tree_node(node);
        return FAILED;
      }

      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();

      tree_node_add_child(_get_current_open_block(), node);

      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
        snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* next token */
      _next_token();

      return SUCCEEDED;
    }
  }
  else if (g_token_current->id == TOKEN_ID_BREAK) {
    node = allocate_tree_node(TREE_NODE_TYPE_BREAK);
    if (node == NULL)
      return FAILED;

    tree_node_add_child(_get_current_open_block(), node);

    /* next token */
    _next_token();
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_CONTINUE) {
    node = allocate_tree_node(TREE_NODE_TYPE_CONTINUE);
    if (node == NULL)
      return FAILED;

    tree_node_add_child(_get_current_open_block(), node);

    /* next token */
    _next_token();
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();

    return SUCCEEDED;
  }
  else if (g_token_current->id == TOKEN_ID_ASM) {
    /* inline ASM */
    struct inline_asm *ia;
    struct asm_line *al;
    
    node = allocate_tree_node(TREE_NODE_TYPE_ASM);
    if (node == NULL)
      return FAILED;

    /* transfer the inline_asm id */
    node->value = g_token_current->value;
    
    tree_node_add_child(_get_current_open_block(), node);

    ia = inline_asm_find(node->value);
    if (ia == NULL)
      return FAILED;
    
    al = ia->asm_line_first;
    while (al != NULL) {
      /* update file id and line number for all TACs and error messages */
      g_current_line_number = al->line_number;
    
      /* determine if the ASM line has i/o with variables, and find the associated tree_node where the variable is created */
      if (g_backend == BACKEND_Z80) {
        if (inline_asm_z80_parse_line(al) == FAILED)
          return FAILED;
      }

      al = al->next;
    }
    
    /* next token */
    _next_token();

    return SUCCEEDED;
  }
  
  snprintf(g_error_message, sizeof(g_error_message), "Expected a statement, but got %s.\n", get_token_simple(g_token_current));
  return print_error(g_error_message, ERROR_ERR);
}


int create_block(struct tree_node *open_function_definition, int expect_curly_braces, int is_multiline_block) {

  int result, i;

  g_block_level++;

  if (open_function_definition != NULL) {
    /* clone the arguments right here */
    for (i = 2; i < open_function_definition->added_children; i += 2) {
      struct tree_node *argument = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT, 3);
      
      if (argument == NULL) {
        free_symbol_table_items(g_block_level);
        g_block_level--;
        return FAILED;
      }

      /* NOTE: we save the function argument number in tree_node's 'value_double' field */
      argument->value_double = (i / 2) - 1;

      /* NOTE: this is unlike elsewhere - the variable type node in function arguments list contains the const flags,
         but elsewhere TREE_NODE_TYPE_CREATE_VARIABLE root node has them... perhaps change this later? */
      argument->flags = open_function_definition->children[i+0]->flags;
      
      /* variable type */
      tree_node_add_child(argument, clone_tree_node(open_function_definition->children[i+0]));
      /* name */
      tree_node_add_child(argument, clone_tree_node(open_function_definition->children[i+1]));
      /* value */
      /*
      if (_open_expression_push() == FAILED) {
        free_tree_node(argument);
        free_symbol_table_items(g_block_level);
        g_block_level--;
        return FAILED;
      }
      tree_node_add_child(_get_current_open_expression(), allocate_tree_node_value_int(0));
      if (_get_current_open_expression()->children[0] == NULL) {
        free_tree_node(argument);
        free_symbol_table_items(g_block_level);
        g_block_level--;
        return FAILED;
      }
      tree_node_add_child(argument, _get_current_open_expression());
      _open_expression_pop();

      if (argument->children[0] == NULL || argument->children[1] == NULL || argument->children[2] == NULL) {
        free_tree_node(argument);
        free_symbol_table_items(g_block_level);
        g_block_level--;
        return FAILED;
      }
      */
      
      tree_node_add_child(_get_current_open_block(), argument);

      /* make the argument point to its creation */
      open_function_definition->children[i+1]->definition = argument;
      
      /* add to symbol table */
      if (symbol_table_add_symbol(argument, argument->children[1]->label, g_block_level, argument->line_number, argument->file_id) == FAILED) {
        free_symbol_table_items(g_block_level);
        g_block_level--;
        return FAILED;
      }
    }
  }

  /* a quick exit for function prototypes - there we only need the function argument variables */
  if (open_function_definition != NULL && open_function_definition->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
    free_symbol_table_items(g_block_level);
    g_block_level--;

    return SUCCEEDED;
  }
  
  if (expect_curly_braces == YES) {
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
      free_symbol_table_items(g_block_level);
      g_block_level--;
      snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }

    /* next token */
    _next_token();
  }

  while (1) {
    result = create_statement();

    if (result == FAILED) {
      free_symbol_table_items(g_block_level);
      g_block_level--;
      return result;
    }
    if (result == FINISHED)
      break;
    if (is_multiline_block == NO)
      break;
  }

  if (is_multiline_block == YES) {
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
      if (expect_curly_braces == YES) {
        /* next token */
        _next_token();
      }
    }
    else if (g_token_current->id == TOKEN_ID_CASE || g_token_current->id == TOKEN_ID_DEFAULT) {
    }
    else {
      free_symbol_table_items(g_block_level);
      g_block_level--;
      snprintf(g_error_message, sizeof(g_error_message), "Expected '}', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }
  }
  
  free_symbol_table_items(g_block_level);
  g_block_level--;

  return FINISHED;
}


struct tree_node *create_array(double pointer_depth, int variable_type, char *name, int line_number, int file_id) {

  int array_size = 0, added_items;
  struct tree_node *node;

  node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 16);
  if (node == NULL)
    return NULL;

  /* use the line where the name was given */
  node->file_id = file_id;
  node->line_number = line_number;

  node->flags |= TREE_NODE_FLAG_ARRAY;
  
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
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
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
    free_tree_node(node);
    snprintf(g_error_message, sizeof(g_error_message), "Expected array size or ']', but got %s.\n", get_token_simple(g_token_current));
    print_error(g_error_message, ERROR_ERR);
    return NULL;
  }

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
  }
  else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
  }
  else {
    int skip = NO;
    
    /* read the items */
    
    if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '=') {
      free_tree_node(node);
      snprintf(g_error_message, sizeof(g_error_message), "Expected '=', but got %s.\n", get_token_simple(g_token_current));
      print_error(g_error_message, ERROR_ERR);
      return NULL;
    }

    /* next token */
    _next_token();

    if (g_token_current->id == TOKEN_ID_BYTES) {
      /* of form "int8 var[] = "hello"; */
      tree_node_add_child(node, allocate_tree_node_bytes(g_token_current));
      
      /* next token */
      _next_token();

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
      }
      else if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ';') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected ',' or ';', but got %s.\n", get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return NULL;
      }

      skip = YES;
    }

    if (skip == NO) {
      if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != '{') {
        free_tree_node(node);
        snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got %s.\n", get_token_simple(g_token_current));
        print_error(g_error_message, ERROR_ERR);
        return NULL;
      }

      /* next token */
      _next_token();
    }
    
    while (skip == NO) {
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
      if (create_expression() == FAILED) {
        free_tree_node(_get_current_open_expression());
        _open_expression_pop();
        free_tree_node(node);
        return NULL;
      }

      tree_node_add_child(node, _get_current_open_expression());
      _open_expression_pop();

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
        /* next token */
        _next_token();
      }      
    }
  }
  
  /* some items can be TREE_NODE_TYPE_EXPRESSION with single TREE_NODE_TYPE_BYTES child -> simplify! */
  _simplify_expressions_with_tree_node_type_bytes(node);

  /* as some items in the array can be of type TREE_NODE_TYPE_BYTES, recalculate the array size here */
  added_items = tree_node_get_create_variable_data_items(node);

  if (array_size == 0 && added_items == 0) {
    free_tree_node(node);
    print_error("An array without any items doesn't make sense.\n", ERROR_ERR);
    return NULL;
  }

  if (array_size == 0)
    array_size = added_items;

  if (array_size < added_items) {
    snprintf(g_error_message, sizeof(g_error_message), "Specified array size is %d, but defined %d items in the array -> setting array size to %d.\n", array_size, added_items, added_items);
    print_error(g_error_message, ERROR_WRN);
    array_size = added_items;
  }

  /* store the size of the array */
  node->value = array_size;

  return node;
}


int create_variable_or_function(int is_extern, int is_static) {

  int variable_type, pointer_depth, is_const_1 = NO;
  char name[MAX_NAME_LENGTH + 1];
  struct tree_node *node = NULL;

  variable_type = g_token_current->value;

  if (variable_type == VARIABLE_TYPE_STRUCT || variable_type == VARIABLE_TYPE_UNION) {
    /* skip "struct/union" */
    _next_token();

    return create_struct_union(is_extern, is_static, is_const_1, variable_type, YES);
  }

  /* next token */
  _next_token();

  if (variable_type == VARIABLE_TYPE_CONST) {
    is_const_1 = YES;

    /* get the real variable type */
    if (g_token_current->id != TOKEN_ID_VARIABLE_TYPE)
      return print_error_using_token("\"const\" must be followed by a variable type.\n", ERROR_ERR, g_token_previous);

    variable_type = g_token_current->value;

    if (variable_type == VARIABLE_TYPE_CONST)
      return print_error("\"const const\" doesn't make any sense.\n", ERROR_ERR);

    /* next token */
    _next_token();
  }
  
  if (variable_type == VARIABLE_TYPE_STRUCT || variable_type == VARIABLE_TYPE_UNION)
    return create_struct_union(is_extern, is_static, is_const_1, variable_type, YES);
  
  while (1) {
    int is_const_2 = NO, file_id, line_number;
    
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
      /* next token */
      _next_token();
    }
    
    pointer_depth = 0;
    
    while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
      pointer_depth++;

      /* next token */
      _next_token();
    }

    if (pointer_depth > 0 && g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
      is_const_2 = YES;

      /* next token */
      _next_token();
    }
    
    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" must be followed by a variable or function name.\n", g_variable_types[variable_type]);
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    strncpy(name, g_token_current->label, MAX_NAME_LENGTH);

    /* remember these for symbol_table_add_symbol() */
    file_id = g_token_current->file_id;
    line_number = g_token_current->line_number;
      
    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                   g_token_current->value != ';' &&
                                                   g_token_current->value != ',' &&
                                                   g_token_current->value != '[' &&
                                                   g_token_current->value != '(')) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" must be followed by a ';', ',', '=', '[' or '('.\n", name);
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    if (g_token_current->value == '=' || g_token_current->value == ';' || g_token_current->value == '[' || g_token_current->value == ',') {
      /* a variable definition */
      int symbol = g_token_current->value;

      /* next token */
      _next_token();

      if (symbol == '=') {
        if (is_extern == YES)
          return print_error_using_token("Extern variable definitions don't take initializers.\n", ERROR_ERR, g_token_previous);
        
        /* create_expression() will put all tree_nodes it parses to g_open_expression */
        if (_open_expression_push() == FAILED)
          return FAILED;

        /* possibly parse a calculation */
        if (create_expression() == FAILED)
          return FAILED;

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
        node = create_array(pointer_depth, variable_type, name, line_number, file_id);
        if (node == NULL)
          return FAILED;
        
        /* add to global lists */
        if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, line_number, file_id) == FAILED) {
          free_tree_node(node);
          return FAILED;
        }
      
        if (tree_node_add_child(g_global_nodes, node) == FAILED)
          return FAILED;

        /* store const flags */
        if (is_const_1 == YES)
          node->flags |= TREE_NODE_FLAG_CONST_1;
        if (is_const_2 == YES)
          node->flags |= TREE_NODE_FLAG_CONST_2;
        if (is_extern == YES)
          node->flags |= TREE_NODE_FLAG_EXTERN;
        if (is_static == YES)
          node->flags |= TREE_NODE_FLAG_STATIC;

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
          /* next token */
          _next_token();

          return SUCCEEDED;
        }

        continue;
      }

      node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 3);
      if (node == NULL)
        return FAILED;

      /* use the line where the name was given */
      node->file_id = file_id;
      node->line_number = line_number;
      
      tree_node_add_child(node, allocate_tree_node_variable_type(variable_type));
      tree_node_add_child(node, allocate_tree_node_value_string(name));
      if (symbol == '=') {
        tree_node_add_child(node, _get_current_open_expression());
        _open_expression_pop();
      }

      if (node->children[0] == NULL || node->children[1] == NULL || (symbol == '=' && node->children[2] == NULL)) {
        free_tree_node(node);
        return FAILED;
      }

      /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
      node->children[0]->value_double = pointer_depth;

      /* store const flags */
      if (is_const_1 == YES)
        node->flags |= TREE_NODE_FLAG_CONST_1;
      if (is_const_2 == YES)
        node->flags |= TREE_NODE_FLAG_CONST_2;
      if (is_extern == YES)
        node->flags |= TREE_NODE_FLAG_EXTERN;
      if (is_static == YES)
        node->flags |= TREE_NODE_FLAG_STATIC;

      /* add to global lists */
      if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, line_number, file_id) == FAILED) {
        free_tree_node(node);
        return FAILED;
      }
      
      if (tree_node_add_child(g_global_nodes, node) == FAILED)
        return FAILED;

      if (symbol == ';')
        return SUCCEEDED;

      if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
        /* next token */
        _next_token();

        return SUCCEEDED;
      }
    }
    else {
      /* a function definition */
      int function_file_id = g_token_current->file_id;
      
      /* this token is '(' */
      _next_token();

      g_open_function_definition = allocate_tree_node_with_children(TREE_NODE_TYPE_FUNCTION_DEFINITION, 16);
      if (g_open_function_definition == NULL)
        return FAILED;

      tree_node_add_child(g_open_function_definition, allocate_tree_node_variable_type(variable_type));
      tree_node_add_child(g_open_function_definition, allocate_tree_node_value_string(name));

      if (g_open_function_definition->children[0] == NULL || g_open_function_definition->children[1] == NULL) {
        free_tree_node(g_open_function_definition);
        g_open_function_definition = NULL;
        return FAILED;
      }

      /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
      g_open_function_definition->children[0]->value_double = pointer_depth;

      /* NOTE: store const flags in TREE_NODE_TYPE_FUNCTION_DEFINITION */
      if (is_const_1 == YES)
        g_open_function_definition->flags |= TREE_NODE_FLAG_CONST_1;
      if (is_const_2 == YES)
        g_open_function_definition->flags |= TREE_NODE_FLAG_CONST_2;
      if (is_static == YES)
        g_open_function_definition->flags |= TREE_NODE_FLAG_STATIC;
      
      /* start collecting the argument types and names */
      while (1) {
        is_const_1 = NO;
        is_const_2 = NO;
        
        if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
          struct tree_node *node;

          variable_type = g_token_current->value;

          /* next token */
          _next_token();

          if (variable_type == VARIABLE_TYPE_CONST) {
            is_const_1 = YES;

            /* get the real variable type */
            if (g_token_current->id != TOKEN_ID_VARIABLE_TYPE)
              return print_error_using_token("\"const\" must be followed by a variable type.\n", ERROR_ERR, g_token_previous);

            variable_type = g_token_current->value;

            if (variable_type == VARIABLE_TYPE_CONST)
              return print_error("\"const const\" doesn't make any sense.\n", ERROR_ERR);

            /* next token */
            _next_token();
          }

          node = allocate_tree_node_variable_type(variable_type);
          if (node == NULL) {
            free_tree_node(g_open_function_definition);
            g_open_function_definition = NULL;
            return FAILED;
          }

          pointer_depth = 0;
        
          while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
            pointer_depth++;

            /* next token */
            _next_token();
          }

          if (pointer_depth > 0 && g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
            is_const_2 = YES;

            /* next token */
            _next_token();
          }

          /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
          node->value_double = pointer_depth;

          /* NOTE: store const flags in TREE_NODE_TYPE_VARIABLE_TYPE */
          if (is_const_1 == YES)
            node->flags |= TREE_NODE_FLAG_CONST_1;
          if (is_const_2 == YES)
            node->flags |= TREE_NODE_FLAG_CONST_2;
          
          if (node->value == VARIABLE_TYPE_VOID && pointer_depth == 0 && g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ')') {
            /* this function has a "void" argument and nothing else -> ignore "void" */
            free_tree_node(node);
            continue;
          }

          tree_node_add_child(g_open_function_definition, node);
        
          if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
            free_tree_node(g_open_function_definition);
            g_open_function_definition = NULL;
            snprintf(g_error_message, sizeof(g_error_message), "Expected an argument name, but got %s.\n", get_token_simple(g_token_current));
            return print_error(g_error_message, ERROR_ERR);
          }

          node = allocate_tree_node_value_string(g_token_current->label);
          if (node == NULL) {
            free_tree_node(g_open_function_definition);
            g_open_function_definition = NULL;
            return FAILED;
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

          if (g_token_current->id == TOKEN_ID_HINT && strcmp(g_token_current->label, "__pureasm") == 0) {
            if (g_open_function_definition->added_children > 2 ||
                g_open_function_definition->children[0]->value != VARIABLE_TYPE_VOID ||
                g_open_function_definition->children[0]->value_double > 0) {
              return print_error("A __pureasm function must be of type \"void name(void)\".\n", ERROR_ERR);
            }
            
            /* this token is '__pureasm' */
            _next_token();

            g_open_function_definition->flags |= TREE_NODE_FLAG_PUREASM;
          }

          /* is it actually a function prototype? */
          if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
            g_open_function_definition->type = TREE_NODE_TYPE_FUNCTION_PROTOTYPE;

            /* this token is ';' */
            _next_token();

            /* NOTE: even for a function prototype we will "parse" a block, which in reality means that
               we'll create a block for it that'll contain only function argument definitions. this is for
               the later pass where we calculate frames for functions and make them callable. this fake
               block is later deleted and doesn't generate any TACs... */
          }
          
          /* start parsing the function block */

          if (is_extern == YES && g_open_function_definition->type == TREE_NODE_TYPE_FUNCTION_DEFINITION)
            return print_error("Function definitions cannot be extern.\n", ERROR_ERR);

          if (_open_block_push() == FAILED) {
            free_tree_node(g_open_function_definition);
            g_open_function_definition = NULL;
            return FAILED;
          }

          /* parse the function */
          if (create_block(g_open_function_definition, YES, YES) == FAILED) {
            free_tree_node(_get_current_open_block());
            _open_block_pop();
            return FAILED;
          }

          tree_node_add_child(g_open_function_definition, _get_current_open_block());
          _open_block_pop();

          node = g_open_function_definition;
          g_open_function_definition = NULL;

          /* add to global lists */
          if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, line_number, file_id) == FAILED) {
            free_tree_node(node);
            return FAILED;
          }
      
          if (tree_node_add_child(g_global_nodes, node) == FAILED)
            return FAILED;

          /* is it actually a function prototype? */
          if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE)
            return SUCCEEDED;

          /* is it "main"? */
          if (strcmp(node->children[1]->label, "main") == 0) {
            /* yes, check that it's correctly defined */
            if (node->children[0]->value != VARIABLE_TYPE_VOID || node->added_children > 3)
              return print_error("main()'s definition must be \"void main(void)\".\n", ERROR_ERR);

            g_was_main_function_defined = YES;
          }

          /* is it "mainmain"? */
          if (strcmp(node->children[1]->label, "mainmain") == 0 && function_file_id >= 0)
            return print_error("mainmain() cannot be defined as it's a reserved function name.\n", ERROR_ERR);

          /* check that __pureasm functions contain only __asm() items */
          if ((node->flags & TREE_NODE_FLAG_PUREASM) == TREE_NODE_FLAG_PUREASM) {
            struct tree_node *block;
            int i;

            block = node->children[node->added_children - 1];
            for (i = 0; i < block->added_children; i++) {
              if (block->children[i]->type != TREE_NODE_TYPE_ASM) {
                snprintf(g_error_message, sizeof(g_error_message), "__pureasm function \"%s\" can only contain __asm() items.\n", node->children[1]->label);
                return print_error_using_tree_node(g_error_message, ERROR_ERR, block->children[i]);
              }
            }
          }
          
          return SUCCEEDED;
        }
        else {
          snprintf(g_error_message, sizeof(g_error_message), "Expected a variable type or a ')', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }
      }
    }
  }
  
  return FAILED;
}


static int _process_create_variable_struct_union_process_struct_union(struct tree_node *node, int *node_index, struct struct_item *si) {

  int index = 0, i = *node_index;
  struct tree_node *child;

  if (si == NULL) {
    si = find_struct_item(node->children[0]->children[0]->label);
    if (si == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", node->children[0]->children[0]->label);
      print_error_using_tree_node(g_error_message, ERROR_ERR, node);
      g_check_ast_failed = YES;
      return FAILED;
    }
  }

  child = node->children[i];
    
  if (child->type != TREE_NODE_TYPE_SYMBOL || child->value != '{') {
    g_check_ast_failed = YES;
    snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got something else.\n");
    return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
  }

  print_simple_tree_node(child);
  
  i++;

  for ( ; index < si->added_children; index++) {
    struct struct_item *s = si->children[index];

    if (i >= node->added_children) {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("Abrupt end of definition.\n", ERROR_ERR, node->children[node->added_children-1]);
    }
    
    child = node->children[i];

    if (s->type == STRUCT_ITEM_TYPE_STRUCT || s->type == STRUCT_ITEM_TYPE_UNION) {
      if (_process_create_variable_struct_union_process_struct_union(node, &i, s) == FAILED)
        return FAILED;

      i++;
    }
    else if (s->type == STRUCT_ITEM_TYPE_ITEM) {
      if (s->variable_type == VARIABLE_TYPE_STRUCT || s->variable_type == VARIABLE_TYPE_UNION) {
        struct struct_item *next;
        
        next = find_struct_item(s->struct_name);
        if (next == NULL) {
          g_check_ast_failed = YES;
          snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", s->struct_name);
          print_error_using_tree_node(g_error_message, ERROR_ERR, node);
          return FAILED;
        }

        if (_process_create_variable_struct_union_process_struct_union(node, &i, next) == FAILED)
          return FAILED;
      }
      else if (child->type == TREE_NODE_TYPE_EXPRESSION) {
        print_expression(child);

        /* store the struct_item pointer for later so we don't have to travere through the struct_item tree again... */
        child->struct_item = s;
      }
      else {
        g_check_ast_failed = YES;
        snprintf(g_error_message, sizeof(g_error_message), "Expected an expression, but got something else.\n");
        return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
      }

      i++;
    }
    else if (s->type == STRUCT_ITEM_TYPE_ARRAY) {
      int j;
      
      if (!(child->type == TREE_NODE_TYPE_SYMBOL && child->value == '{')) {
        g_check_ast_failed = YES;
        snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got something else.\n");
        return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
      }

      print_expression(child);
      i++;

      for (j = 0; j < s->array_items; j++) {
        if (i >= node->added_children) {
          g_check_ast_failed = YES;
          return print_error_using_tree_node("Abrupt end of definition.\n", ERROR_ERR, node->children[node->added_children-1]);
        }

        child = node->children[i];
        
        if (s->pointer_depth > 0.0) {
          if (child->type != TREE_NODE_TYPE_EXPRESSION) {
            g_check_ast_failed = YES;
            snprintf(g_error_message, sizeof(g_error_message), "Expected an expression, but got something else.\n");
            return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
          }

          /* store the struct_item pointer for later so we don't have to travere through the struct_item tree again... */
          child->struct_item = s;

          print_expression(child);
        }
        else if (s->variable_type == VARIABLE_TYPE_STRUCT || s->variable_type == VARIABLE_TYPE_UNION) {
          struct struct_item *next = s;

          if (s->struct_name[0] != 0) {
            next = find_struct_item(s->struct_name);
            if (next == NULL) {
              g_check_ast_failed = YES;
              snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", s->struct_name);
              print_error_using_tree_node(g_error_message, ERROR_ERR, node);
              return FAILED;
            }
          }
          
          if (_process_create_variable_struct_union_process_struct_union(node, &i, next) == FAILED)
            return FAILED;
        }
        else {
          if (child->type != TREE_NODE_TYPE_EXPRESSION) {
            g_check_ast_failed = YES;
            snprintf(g_error_message, sizeof(g_error_message), "Expected an expression, but got something else.\n");
            return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
          }

          /* store the struct_item pointer for later so we don't have to travere through the struct_item tree again... */
          child->struct_item = s;

          print_expression(child);
        }

        i++;

        if (i >= node->added_children) {
          g_check_ast_failed = YES;
          return print_error("Abrupt end of definition.\n", ERROR_ERR);
        }
        
        child = node->children[i];

        if (j >= s->array_items - 1) {
          if (!(child->type == TREE_NODE_TYPE_SYMBOL && child->value == '}')) {
            g_check_ast_failed = YES;
            snprintf(g_error_message, sizeof(g_error_message), "Expected '}', but got something else.\n");
            return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
          }
        }
        else {
          if (!(child->type == TREE_NODE_TYPE_SYMBOL && child->value == ',')) {
            g_check_ast_failed = YES;
            snprintf(g_error_message, sizeof(g_error_message), "Expected ',', but got something else.\n");
            return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
          }
        }

        print_simple_tree_node(child);

        i++;
      }
    }

    if (i >= node->added_children) {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("Abrupt end of definition.\n", ERROR_ERR, node->children[node->added_children-1]);
    }

    child = node->children[i];

    if (index == si->added_children - 1) {
      if (!(child->type == TREE_NODE_TYPE_SYMBOL && child->value == '}')) {
        g_check_ast_failed = YES;
        snprintf(g_error_message, sizeof(g_error_message), "Expected '}', but got something else.\n");
        return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
      }
      
      print_simple_tree_node(child);
      *node_index = i;
  
      return SUCCEEDED;
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ',') {
      print_simple_tree_node(child);

      i++;
    }
    else {
      g_check_ast_failed = YES;
      snprintf(g_error_message, sizeof(g_error_message), "Expected ',', but got something else.\n");
      return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
    }
  }

  g_check_ast_failed = YES;

  return FAILED;
}


static int _process_create_variable_struct_union(struct tree_node *node) {

  struct tree_node *child;
  int i = 2, count = 1, j;

  /* check that the struct/union initializer is correct */

  if (node->added_children <= 2)
    return SUCCEEDED;

  fprintf(stderr, "ADDED C %d (%s)\n", node->added_children, node->children[1]->label);

  child = node->children[i];

  if ((node->flags & TREE_NODE_FLAG_ARRAY) == TREE_NODE_FLAG_ARRAY) {
    if (child->type != TREE_NODE_TYPE_SYMBOL || child->value != '{') {
      g_check_ast_failed = YES;
      snprintf(g_error_message, sizeof(g_error_message), "Expected '{', but got something else.\n");
      return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
    }

    count = node->value;

    print_simple_tree_node(child);

    i++;
  }

  for (j = 0; j < count; j++) {  
    if (_process_create_variable_struct_union_process_struct_union(node, &i, NULL) == FAILED)
      return FAILED;

    i++;

    if (j < count - 1) {      
      if (i >= node->added_children) {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("Abrupt end of definition.\n", ERROR_ERR, node->children[node->added_children-1]);
      }

      child = node->children[i];

      if (!(child->type == TREE_NODE_TYPE_SYMBOL && child->value == ',')) {
        g_check_ast_failed = YES;
        snprintf(g_error_message, sizeof(g_error_message), "Expected ',', but got something else.\n");
        return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
      }

      print_simple_tree_node(child);

      i++;

      fprintf(stderr, "\n");
    }
  }

  if ((node->flags & TREE_NODE_FLAG_ARRAY) == TREE_NODE_FLAG_ARRAY) {
    if (i != node->added_children - 1) {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("Abrupt end of definition.\n", ERROR_ERR, node->children[node->added_children-1]);
    }
    
    child = node->children[i];

    if (child->type != TREE_NODE_TYPE_SYMBOL || child->value != '}') {
      g_check_ast_failed = YES;
      snprintf(g_error_message, sizeof(g_error_message), "Expected '}', but got something else.\n");
      return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
    }

    print_simple_tree_node(child);

    i++;
  }

  fprintf(stderr, "\n");
  
  return SUCCEEDED;
}


static void _check_ast_check_definition(struct tree_node *node) {

  if (node == NULL)
    return;

  g_current_filename_id = node->file_id;
  g_current_line_number = node->line_number;

  if (node->definition == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "_check_ast_check_definition(): Node is missing its definition! Please submit a bug report!\n");
    print_error(g_error_message, ERROR_ERR);
    g_check_ast_failed = YES;
    return;
  }

  if (node->type == TREE_NODE_TYPE_FUNCTION_CALL) {
    int args_caller, args_callee;
    
    if (node->definition->type != TREE_NODE_TYPE_FUNCTION_DEFINITION && node->definition->type != TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" is not a function!\n", node->children[0]->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }

    args_caller = node->added_children - 1;
    args_callee = (node->definition->added_children - 2) / 2;

    if (args_caller != args_callee) {
      snprintf(g_error_message, sizeof(g_error_message), "Calling \"%s\" with %d arguments, but it takes %d!\n", node->children[0]->label, args_caller, args_callee);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_STRING) {
    /* this will be checked later when turning the AST into IL */
  }
  else if (node->type == TREE_NODE_TYPE_GET_ADDRESS) {
    /* getting an address of a variable will always succeed */
  }
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM || node->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY) {
    int is_ok = YES;

    if (node->definition->type != TREE_NODE_TYPE_CREATE_VARIABLE && node->definition->type != TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
      is_ok = NO;
    else {
      if (node->definition->children[0]->value_double > 0)
        is_ok = YES;
      else {
        if (node->definition->value > 0)
          is_ok = YES;
        else
          is_ok = NO;
      }
    }
    
    if (is_ok == NO) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" is not a pointer / an array!\n", node->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    int is_ok = YES;

    if (node->definition->type != TREE_NODE_TYPE_CREATE_VARIABLE && node->definition->type != TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
      is_ok = NO;
    else {
      if (node->added_children > 0) {
        /* struct access */
        is_ok = YES;
      }
      else {
        if (node->definition->children[0]->value_double > 0)
          is_ok = YES;
        else {
          if (node->definition->value > 0)
            is_ok = NO;
          else
            is_ok = YES;
        }
      }
    }
    
    if (is_ok == NO) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" cannot be incremented / decremented!\n", node->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
  }
  else {
    snprintf(g_error_message, sizeof(g_error_message), "_check_ast_check_definition(): Unhandled node type %d! Please submit a bug report!\n", node->type);
    print_error(g_error_message, ERROR_ERR);
    g_check_ast_failed = YES;
  }
}


static int _check_ast_struct_access(struct tree_node *node, int is_read, int is_write) {

  struct struct_item *si;
  struct tree_node *named_node;
  int i, is_first = YES;

  si = find_struct_item(node->children[0]->definition->children[0]->children[0]->label);
  if (si == NULL) {
    g_check_ast_failed = YES;
    snprintf(g_error_message, sizeof(g_error_message), "_check_ast_struct_access(): Struct \"%s\"'s definition is missing! Please submit a bug report!\n", node->definition->children[0]->children[0]->label);
    return print_error_using_tree_node(g_error_message, ERROR_ERR, node);
  }

  node->children[0]->struct_item = si;
  named_node = node->children[0];

  if (named_node->definition == NULL) {
    struct symbol_table_item *item;
    
    item = symbol_table_find_symbol(named_node->label);
    if (item == NULL) {
      g_check_ast_failed = YES;
      snprintf(g_error_message, sizeof(g_error_message), "_check_ast_struct_access(): Cannot find variable \"%s\"'!\n", named_node->label);
      return print_error_using_tree_node(g_error_message, ERROR_ERR, named_node);
    }
    named_node->definition = item->node;
  }

  /* mark references */
  if (is_read == YES)
    named_node->definition->reads++;
  if (is_write == YES)  
    named_node->definition->writes++;
  
  i = 1;
  while (i < node->added_children) {
    int got_array_access = NO;
    
    /* array access? */
    if (node->children[i]->type == TREE_NODE_TYPE_SYMBOL && node->children[i]->value == '[') {
      if ((i == 1 && named_node->definition->value <= 0) ||
          (i > 1 && named_node->struct_item->array_items <= 0)) {
        g_check_ast_failed = YES;
        snprintf(g_error_message, sizeof(g_error_message), "_check_ast_struct_access(): \"%s\" is not an array'!\n", named_node->label);
        return print_error_using_tree_node(g_error_message, ERROR_ERR, named_node);
      }
      
      i++;

      if (i >= node->added_children) {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("_check_ast_struct_access(): Syntax error!\n", ERROR_ERR, node->children[i-1]);
      }

      if (node->children[i]->type != TREE_NODE_TYPE_VALUE_INT) {
      }
      else if (node->children[i]->type != TREE_NODE_TYPE_VALUE_STRING) {
        _check_ast_simple_tree_node(node->children[i]);
      }
      else if (node->children[i]->type != TREE_NODE_TYPE_EXPRESSION) {
        _check_ast_expression(node->children[i]);
      }
      else {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("_check_ast_struct_access(): Error in index!\n", ERROR_ERR, node->children[i]);
      }
      
      i++;

      if (i >= node->added_children) {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("_check_ast_struct_access(): Syntax error!\n", ERROR_ERR, node->children[i-1]);
      }

      if (!(node->children[i]->type == TREE_NODE_TYPE_SYMBOL && node->children[i]->value == ']')) {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("_check_ast_struct_access(): Expected ']', got something else instead!\n", ERROR_ERR, node->children[i]);
      }

      i++;

      /* the end? */
      if (i >= node->added_children)
        break;

      got_array_access = YES;
    }

    if (node->children[i]->type != TREE_NODE_TYPE_SYMBOL) {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("_check_ast_struct_access(): Pointer node is corrupted (1)! Please submit a bug report!\n", ERROR_ERR, node->children[i]);
    }

    if (si->type == STRUCT_ITEM_TYPE_STRUCT || si->type == STRUCT_ITEM_TYPE_UNION) {
    }
    else if (si->type == STRUCT_ITEM_TYPE_ITEM && (si->variable_type == VARIABLE_TYPE_STRUCT || si->variable_type == VARIABLE_TYPE_UNION)) {
      si = find_struct_item(si->struct_name);
      if (si == NULL) {
        g_check_ast_failed = YES;
        snprintf(g_error_message, sizeof(g_error_message), "_check_ast_struct_access(): Cannot find struct/union \"%s\"!\n", si->struct_name);
        return print_error_using_tree_node(g_error_message, ERROR_ERR, named_node);
      }
    }
    else if (si->type == STRUCT_ITEM_TYPE_ARRAY && got_array_access == YES && (si->variable_type == VARIABLE_TYPE_STRUCT ||
                                                                               si->variable_type == VARIABLE_TYPE_UNION)) {
    }
    else {
      g_check_ast_failed = YES;
      snprintf(g_error_message, sizeof(g_error_message), "_check_ast_struct_access(): \"%s\" is not a struct/union!\n", named_node->label);
      return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
    }
    
    if (node->children[i]->value == '.') {
      int is_ok = YES;
      
      if (is_first == YES) {
        if (named_node->definition->children[0]->value_double > 0.0)
          is_ok = NO;
      }
      else {
        if (named_node->struct_item->pointer_depth > 0)
          is_ok = NO;
      }
      
      if (is_ok == NO) {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("_check_ast_struct_access(): Wrong addressing type, use '->' instead.\n", ERROR_ERR, node->children[i]);
      }
      
      i++;
    }
    else if (node->children[i]->value == SYMBOL_POINTER) {
      int is_ok = YES;

      if (is_first == YES) {
        if (named_node->definition->children[0]->value_double <= 0.0)
          is_ok = NO;
      }
      else {
        if (named_node->struct_item->pointer_depth <= 0)
          is_ok = NO;
      }

      if (is_ok == NO) {
        g_check_ast_failed = YES;
        return print_error_using_tree_node("_check_ast_struct_access(): Wrong addressing type, use '.' instead.\n", ERROR_ERR, node->children[i]);
      }
      
      i++;
    }
    else {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("_check_ast_struct_access(): Pointer node is corrupted (2)! Please submit a bug report!\n", ERROR_ERR, node->children[i]);
    }

    if (i >= node->added_children) {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("_check_ast_struct_access(): Syntax error!\n", ERROR_ERR, node->children[i-1]);
    }

    if (node->children[i]->type != TREE_NODE_TYPE_VALUE_STRING) {
      g_check_ast_failed = YES;
      return print_error_using_tree_node("_check_ast_struct_access(): Expected a struct/union member name, got something else instead!\n", ERROR_ERR, node->children[i]);
    }

    si = find_struct_item_child(si, node->children[i]->label);
    if (si == NULL) {
      g_check_ast_failed = YES;
      snprintf(g_error_message, sizeof(g_error_message), "_check_ast_struct_access(): Cannot find the definition of \"%s\"!\n", node->children[i]->label);
      return print_error_using_tree_node(g_error_message, ERROR_ERR, node->children[i]);
    }

    node->children[i]->struct_item = si;
    named_node = node->children[i];
    is_first = NO;

    i++;
  }
  
  return SUCCEEDED;
}


static void _check_ast_simple_tree_node(struct tree_node *node) {

  struct symbol_table_item *item;
  
  if (node == NULL)
    return;

  if (node->type == TREE_NODE_TYPE_VALUE_STRING || node->type == TREE_NODE_TYPE_GET_ADDRESS) {
  }
  else if (node->type == TREE_NODE_TYPE_VALUE_INT || node->type == TREE_NODE_TYPE_VALUE_DOUBLE || node->type == TREE_NODE_TYPE_SYMBOL) {
    return;
  }
  else if (node->type == TREE_NODE_TYPE_STRUCT_ACCESS) {
    _check_ast_struct_access(node, YES, NO);
    return;
  }
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL) {
    _check_ast_function_call(node);
    return;
  }
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM || node->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY) {
    _check_ast_expression(node->children[0]);
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    /* is it accessing a struct? */
    if (node->added_children > 0) {
      int i;

      for (i = 0; i < node->added_children; i++) {
        if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
          _check_ast_expression(node->children[i]);
      }

      _check_ast_struct_access(node, YES, YES);
    }
  }
  else if (node->type == TREE_NODE_TYPE_SIZEOF) {
    if (node->value == VARIABLE_TYPE_STRUCT || node->value == VARIABLE_TYPE_UNION) {
      struct struct_item *si;

      /* sizeof(struct x) */
      si = find_struct_item(node->label);
      if (si == NULL) {
        snprintf(g_error_message, sizeof(g_error_message), "Cannot find struct/union \"%s\".\n", node->label);
        print_error_using_tree_node(g_error_message, ERROR_ERR, node);
        g_check_ast_failed = YES;
        return;
      }

      /* transform the node into TREE_NODE_TYPE_VALUE_INT */
      node->type = TREE_NODE_TYPE_VALUE_INT;
      node->value = si->size;

      return;
    }
    else {
      /* sizeof(g_x) */
      int i, size;

      for (i = 0; i < g_global_nodes->added_children; i++) {
        struct tree_node *n;

        n = g_global_nodes->children[i];
        if (n->type == TREE_NODE_TYPE_CREATE_VARIABLE && strcmp(node->label, n->children[1]->label) == 0) {
          size = get_variable_node_size_in_bytes(n);
          if (size < 0) {
            g_check_ast_failed = YES;
            return;
          }
          break;
        }
      }

      if (i == g_global_nodes->added_children) {
        snprintf(g_error_message, sizeof(g_error_message), "Cannot find variable \"%s\".\n", node->label);
        print_error_using_tree_node(g_error_message, ERROR_ERR, node);
        g_check_ast_failed = YES;
        return;
      }
      
      /* transform the node into TREE_NODE_TYPE_VALUE_INT */
      node->type = TREE_NODE_TYPE_VALUE_INT;
      node->value = size;

      return;
    }
  }
  else {
    fprintf(stderr, "XXX %d\n", node->type);
    exit(0);
    return;
  }

  if (node->definition == NULL) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;
    
    item = symbol_table_find_symbol(node->label);
    if (item == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Cannot find variable \"%s\".\n", node->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
    node->definition = item->node;
  }

  /* mark references */
  node->definition->reads++;

  if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    /* mark references */
    node->definition->writes++;
  }
  
  _check_ast_check_definition(node);
}


static void _check_ast_expression(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  if (node->type != TREE_NODE_TYPE_EXPRESSION) {
    _check_ast_simple_tree_node(node);
    return;
  }
  
  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      _check_ast_expression(node->children[i]);
    else
      _check_ast_simple_tree_node(node->children[i]);
  }
}


static void _check_ast_create_variable(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 2; i < node->added_children; i++)
    _check_ast_expression(node->children[i]);

  if (node->children[0]->value == VARIABLE_TYPE_STRUCT || node->children[0]->value == VARIABLE_TYPE_UNION)
    _process_create_variable_struct_union(node);
}


static void _check_ast_assignment(struct tree_node *node) {

  struct symbol_table_item *item;
  struct tree_node *definition;
  
  if (node == NULL)
    return;

  _check_ast_expression(node->children[1]);

  if (node->children[0]->type == TREE_NODE_TYPE_STRUCT_ACCESS)
    _check_ast_struct_access(node->children[0], NO, YES);
  
  /* array assignment? */
  if (node->added_children > 2)
    _check_ast_expression(node->children[2]);

  if (node->children[0]->definition == NULL) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;

    item = symbol_table_find_symbol(node->children[0]->label);
    if (item == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Cannot find variable \"%s\".\n", node->children[0]->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
    node->children[0]->definition = item->node;
  }

  definition = node->children[0]->definition;

  /* mark references */
  definition->writes++;
  
  if (node->added_children <= 2) {
    /* assignment to a variable */
    int is_ok = YES;

    if (definition->type != TREE_NODE_TYPE_CREATE_VARIABLE && definition->type != TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
      is_ok = NO;
    else {
      if (node->children[0]->type == TREE_NODE_TYPE_STRUCT_ACCESS)
        is_ok = YES;
      else if (definition->value > 0)
        is_ok = NO;
      else
        is_ok = YES;
    }

    if (is_ok == NO) {
      snprintf(g_error_message, sizeof(g_error_message), "Assignment to \"%s\" doesn't work!\n", node->children[0]->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
  }
  else {
    /* assignment to an array */
    int is_ok = YES;
    
    if (definition->type != TREE_NODE_TYPE_CREATE_VARIABLE && definition->type != TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
      is_ok = NO;
    else {
      if (definition->children[0]->value_double > 0)
        is_ok = YES;
      else {
        if (definition->value > 0)
          is_ok = YES;
        else
          is_ok = NO;
      }
    }

    if (is_ok == NO) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" is not a pointer / an array!\n", node->children[0]->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
  }
}


static void _check_ast_return(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->added_children > 0)
    _check_ast_expression(node->children[0]);
}


static void _check_ast_function_call(struct tree_node *node) {

  struct symbol_table_item *item;
  int i;
  
  if (node == NULL)
    return;

  for (i = 1; i < node->added_children; i++)
    _check_ast_expression(node->children[i]);

  if (node->definition == NULL) {
    g_current_filename_id = node->file_id;
    g_current_line_number = node->line_number;

    item = symbol_table_find_symbol(node->children[0]->label);
    if (item == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Cannot find function \"%s\".\n", node->children[0]->label);
      print_error(g_error_message, ERROR_ERR);
      g_check_ast_failed = YES;
      return;
    }
    node->definition = item->node;
  }

  /* mark references */
  node->definition->reads++;

  _check_ast_check_definition(node);
}


static void _check_ast_condition(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _check_ast_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      _check_ast_expression(node->children[i]);
  }
}


static void _check_ast_if(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _check_ast_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _check_ast_block(node->children[i]);
  }
}


static void _check_ast_while(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _check_ast_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _check_ast_block(node->children[i]);
  }
}


static void _check_ast_do(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _check_ast_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _check_ast_block(node->children[i]);
  }
}


static void _check_ast_for(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _check_ast_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _check_ast_block(node->children[i]);
  }
}


static void _check_ast_switch(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _check_ast_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _check_ast_block(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      _check_ast_expression(node->children[i]);
  }
}


static void _check_ast_statement(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    _check_ast_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_ASSIGNMENT)
    _check_ast_assignment(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL)
    _check_ast_function_call(node);
  else if (node->type == TREE_NODE_TYPE_RETURN)
    _check_ast_return(node);
  else if (node->type == TREE_NODE_TYPE_IF)
    _check_ast_if(node);
  else if (node->type == TREE_NODE_TYPE_WHILE)
    _check_ast_while(node);
  else if (node->type == TREE_NODE_TYPE_DO)
    _check_ast_do(node);
  else if (node->type == TREE_NODE_TYPE_FOR)
    _check_ast_for(node);
  else if (node->type == TREE_NODE_TYPE_SWITCH)
    _check_ast_switch(node);
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    if (node->added_children > 0)
      _check_ast_struct_access(node, YES, YES);
    else
      _check_ast_simple_tree_node(node);
  }
  else if (node->type == TREE_NODE_TYPE_BREAK || node->type == TREE_NODE_TYPE_CONTINUE)
    return;
  else if (node->type == TREE_NODE_TYPE_LABEL || node->type == TREE_NODE_TYPE_GOTO)
    return;
  else if (node->type == TREE_NODE_TYPE_ASM)
    return;
  else {
    fprintf(stderr, "_check_ast_statement(): Unhandled statement type %d! Please submit a bug report!\n", node->type);
    g_check_ast_failed = YES;
  }
}


static void _check_ast_block(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  if (node->type != TREE_NODE_TYPE_BLOCK) {
    fprintf(stderr, "_check_ast_block(): Was expecting TREE_NODE_TYPE_BLOCK, got %d instead! Please submit a bug report!\n", node->type);
    g_check_ast_failed = YES;
    return;
  }

  for (i = 0; i < node->added_children; i++)
    _check_ast_statement(node->children[i]);
}


static void _check_ast_function_definition(struct tree_node *node) {

  if (node == NULL)
    return;

  _check_ast_block(node->children[node->added_children-1]);
}


static void _check_ast_global_node(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE)
    _check_ast_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION)
    _check_ast_function_definition(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
    /* function prototypes are assumed to be ok */
  }
  else {
    fprintf(stderr, "_check_ast_global_node(): Unknown global node type %d! Please submit a bug report!\n", node->type);
    g_check_ast_failed = YES;
  }
}


void check_ast(void) {

  int i;

  /* mark all globals */
  for (i = 0; i < g_global_nodes->added_children; i++)
    g_global_nodes->children[i]->flags |= TREE_NODE_FLAG_GLOBAL;

  /* check ast */
  for (i = 0; i < g_global_nodes->added_children; i++)
    _check_ast_global_node(g_global_nodes->children[i]);
}


int _create_struct_union_definition(char *struct_name, int variable_type) {

  struct struct_item *stack[256], *s;
  int depth = 0, type;

  if (find_struct_item(struct_name) != NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "struct/union \"%s\" is already defined.\n", struct_name);
    return print_error(g_error_message, ERROR_ERR);
  }

  if (variable_type == VARIABLE_TYPE_STRUCT)
    type = STRUCT_ITEM_TYPE_STRUCT;
  else if (variable_type == VARIABLE_TYPE_UNION)
    type = STRUCT_ITEM_TYPE_UNION;
  else {
    snprintf(g_error_message, sizeof(g_error_message), "Unsupported variable type %d! Please submit a bug report!\n", variable_type);
    return print_error(g_error_message, ERROR_ERR);
  }
  
  s = allocate_struct_item(struct_name, type);
  if (s == NULL)
    return FAILED;

  add_struct_item(s);
  
  stack[depth] = s;
  
  /* skip "{" */
  _next_token();

  while (1) {
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
      /* skip "}" */
      _next_token();

      depth--;

      if (depth < 0)
        break;
      else {
        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
          /* the struct/union has no name... */

          /* skip ';' */
          _next_token();
        }
        else if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
          /* the struct/union has a name! */
          strcpy(s->name, g_token_current->label);

          /* skip the name */
          _next_token();

          if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '[') {
            int items;

            /* an array of structs/unions */

            /* skip '[' */
            _next_token();

            if (stack_calculate_tokens(&items) != SUCCEEDED)
              return print_error("The array size must be solved here.\n", ERROR_ERR);

            s->type = STRUCT_ITEM_TYPE_ARRAY;
            s->array_items = items;
          
            if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']')) {
              snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
              return print_error(g_error_message, ERROR_ERR);
            }

            /* skip ']' */
            _next_token();
          }
          
          if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';'))
            return print_error("struct/union name must be followed by a ';'.\n", ERROR_ERR);

          /* skip ';' */
          _next_token();
        }
      }

      s = stack[depth];
    }
    else if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE) {
      int pointer_depth = 0, is_const_1 = NO, is_const_2 = NO, got_struct_name = NO;
      char struct_name[MAX_NAME_LENGTH + 1];

      if (g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
        is_const_1 = YES;

        /* next token */
        _next_token();
      }

      if (g_token_current->id != TOKEN_ID_VARIABLE_TYPE)
        return print_error("\"const\" must be followed by a variable type.\n", ERROR_ERR);

      variable_type = g_token_current->value;
      
      /* next token */
      _next_token();

      if (variable_type == VARIABLE_TYPE_STRUCT || variable_type == VARIABLE_TYPE_UNION) {
        if (g_token_current->id == TOKEN_ID_VALUE_STRING) {
          strcpy(struct_name, g_token_current->label);
          got_struct_name = YES;

          /* next token */
          _next_token();
        }
      }
      
      while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
        pointer_depth++;

        /* next token */
        _next_token();
      }

      if (pointer_depth > 0 && g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
        is_const_2 = YES;

        /* next token */
        _next_token();
      }

      if ((variable_type == VARIABLE_TYPE_STRUCT || variable_type == VARIABLE_TYPE_UNION) && g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{' && pointer_depth == 0 && got_struct_name == NO && is_const_1 == NO && is_const_2 == NO) {
        /* local struct/union definition */
        if (variable_type == VARIABLE_TYPE_STRUCT)
          type = STRUCT_ITEM_TYPE_STRUCT;
        else if (variable_type == VARIABLE_TYPE_UNION)
          type = STRUCT_ITEM_TYPE_UNION;

        if (depth >= 255)
          return print_error("Out of struct/union stack depth! Please submit a bug report!\n", ERROR_ERR);

        s = allocate_struct_item("", type);
        if (s == NULL)
          return FAILED;

        s->file_id = g_token_current->file_id;
        s->line_number = g_token_current->line_number;
        s->variable_type = variable_type;
        s->pointer_depth = pointer_depth;
        
        struct_item_add_child(stack[depth], s);
        stack[++depth] = s;
  
        /* skip "{" */
        _next_token();
      }
      else {
        struct struct_item *child;
        int i;

        if (g_token_current->id != TOKEN_ID_VALUE_STRING)
          return print_error("union/struct member needs a name!\n", ERROR_ERR);

        /* is it already defined? */
        for (i = 0; i < s->added_children; i++) {
          if (strcmp(s->children[i]->name, g_token_current->label) == 0) {
            snprintf(g_error_message, sizeof(g_error_message), "\"%s\" is already defined in struct/union \"%s\".\n", g_token_current->label, s->name);
            return print_error(g_error_message, ERROR_ERR);
          }
        }

        /* NOTE! variable_type here might be wrong */
        child = allocate_struct_item(g_token_current->label, STRUCT_ITEM_TYPE_ITEM);
        if (child == NULL)
          return FAILED;

        child->file_id = g_token_current->file_id;
        child->line_number = g_token_current->line_number;

        struct_item_add_child(s, child);

        /* skip the name */
        _next_token();

        if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '[') {
          /* it's an array! */
          int items;

          /* skip '[' */
          _next_token();

          if (stack_calculate_tokens(&items) != SUCCEEDED)
            return print_error("The array size must be solved here.\n", ERROR_ERR);

          child->type = STRUCT_ITEM_TYPE_ARRAY;
          child->array_items = items;
          
          if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']')) {
            snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
            return print_error(g_error_message, ERROR_ERR);
          }

          /* skip ']' */
          _next_token();
        }
        else {
          child->type = STRUCT_ITEM_TYPE_ITEM;
        }

        child->variable_type = variable_type;
        child->pointer_depth = pointer_depth;

        if (variable_type == VARIABLE_TYPE_STRUCT || variable_type == VARIABLE_TYPE_UNION)
          strcpy(child->struct_name, struct_name);

        /* offset and size are calculated later */
        
        if (!(g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';')) {
          snprintf(g_error_message, sizeof(g_error_message), "Expected ';', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }

        /* skip ';' */
        _next_token();
      }
    }
    else {
      snprintf(g_error_message, sizeof(g_error_message), "Expected a variable type or a '}', but got %s.\n", get_token_simple(g_token_current));
      return print_error(g_error_message, ERROR_ERR);
    }
  }

  return SUCCEEDED;
}


static int _collect_struct_union_initializers(struct tree_node *node) {

  struct tree_node *n;
  int depth = 0;

  while (1) {
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{') {
      depth++;

      n = allocate_tree_node_symbol(g_token_current->value);
      if (n == NULL)
        return FAILED;

      /* add the symbol to the tree node - we'll parse these symbols later */
      if (tree_node_add_child(node, n) == FAILED) {
        free_tree_node(n);
        return FAILED;
      }
      
      /* skip '{' */
      _next_token();
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '}') {
      depth--;

      n = allocate_tree_node_symbol(g_token_current->value);
      if (n == NULL)
        return FAILED;

      /* add the symbol to the tree node - we'll parse these symbols later */
      if (tree_node_add_child(node, n) == FAILED) {
        free_tree_node(n);
        return FAILED;
      }

      /* skip '}' */
      _next_token();

      if (depth <= 0)
        return SUCCEEDED;
    }
    else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
      n = allocate_tree_node_symbol(g_token_current->value);
      if (n == NULL)
        return FAILED;

      /* add the symbol to the tree node - we'll parse these symbols later */
      if (tree_node_add_child(node, n) == FAILED) {
        free_tree_node(n);
        return FAILED;
      }

      /* skip ',' */
      _next_token();
    }
    else {
      /* create_expression() will put all tree_nodes it parses to g_open_expression */
      if (_open_expression_push() == FAILED)
        return FAILED;

      /* possibly parse a calculation */
      if (create_expression() == FAILED)
        return FAILED;

      if (tree_node_add_child(node, _get_current_open_expression()) == FAILED)
        return FAILED;
      
      _open_expression_pop();
    }
  }
  
  return SUCCEEDED;
}


int create_struct_union(int is_extern, int is_static, int is_const_1, int variable_type, int is_global) {

  char struct_name[MAX_NAME_LENGTH + 1];
  struct tree_node *node;
   
  if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
    snprintf(g_error_message, sizeof(g_error_message), "struct/union must be followed by a struct/union name.\n");
    return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
  }

  strncpy(struct_name, g_token_current->label, MAX_NAME_LENGTH);

  /* next token */
  _next_token();

  if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '{') {
    /* this is a struct/union definition! */
    if (_create_struct_union_definition(struct_name, variable_type) == FAILED)
      return FAILED;

    /* no instances? */
    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ';') {
      /* next token */
      _next_token();

      return SUCCEEDED;
    }
  }
  
  while (1) {
    int is_const_2 = NO, file_id, line_number, pointer_depth;
    struct tree_node *struct_node;

    if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ',') {
      /* next token */
      _next_token();
    }
    
    pointer_depth = 0;
    
    while (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == '*') {
      pointer_depth++;

      /* next token */
      _next_token();
    }

    if (pointer_depth > 0 && g_token_current->id == TOKEN_ID_VARIABLE_TYPE && g_token_current->value == VARIABLE_TYPE_CONST) {
      is_const_2 = YES;

      /* next token */
      _next_token();
    }
    
    if (g_token_current->id != TOKEN_ID_VALUE_STRING) {
      snprintf(g_error_message, sizeof(g_error_message), "struct/union \"%s\" must be followed by a variable name.\n", struct_name);
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    /* remember for symbol table */
    file_id = g_token_current->file_id;
    line_number = g_token_current->line_number;
    
    /* allocate tree_node for the variable */
    node = allocate_tree_node_with_children(TREE_NODE_TYPE_CREATE_VARIABLE, 3);
    if (node == NULL)
      return FAILED;
  
    tree_node_add_child(node, allocate_tree_node_variable_type(variable_type));
    tree_node_add_child(node, allocate_tree_node_value_string(g_token_current->label));

    if (node->children[0] == NULL || node->children[1] == NULL) {
      free_tree_node(node);
      return FAILED;
    }

    /* store const flags */
    if (is_const_1 == YES)
      node->flags |= TREE_NODE_FLAG_CONST_1;
    if (is_const_2 == YES)
      node->flags |= TREE_NODE_FLAG_CONST_2;
    if (is_extern == YES)
      node->flags |= TREE_NODE_FLAG_EXTERN;
    if (is_static == YES)
      node->flags |= TREE_NODE_FLAG_STATIC;

    /* store the pointer depth (0 - not a pointer, 1+ - is a pointer) */
    node->children[0]->value_double = pointer_depth;

    struct_node = allocate_tree_node_value_string(struct_name);
    if (struct_node == NULL) {
      free_tree_node(node);
      return FAILED;
    }

    /* add the struct name node */
    tree_node_add_child(node->children[0], struct_node);

    /* add to global lists */
    if (symbol_table_add_symbol(node, node->children[1]->label, g_block_level, line_number, file_id) == FAILED) {
      free_tree_node(node);
      return FAILED;
    }

    /* add the node to the AST */
    if (is_global == YES) {
      if (tree_node_add_child(g_global_nodes, node) == FAILED)
        return FAILED;
    }
    else {
      if (tree_node_add_child(_get_current_open_block(), node) == FAILED)
        return FAILED;
    }

    /* next token */
    _next_token();

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                   g_token_current->value != ';' &&
                                                   g_token_current->value != ',' &&
                                                   g_token_current->value != '[')) {
      snprintf(g_error_message, sizeof(g_error_message), "\"%s\" must be followed by a ';', ',', '=' or '['.\n", g_token_previous->label);
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    if (g_token_current->value == '[') {
      int array_size = 0;
      
      /* array */
      
      /* next token */
      _next_token();

      node->flags |= TREE_NODE_FLAG_ARRAY;

      if (g_token_current->id == TOKEN_ID_VALUE_INT) {
        array_size = g_token_current->value;

        /* next token */
        _next_token();

        if (g_token_current->id != TOKEN_ID_SYMBOL || g_token_current->value != ']') {
          snprintf(g_error_message, sizeof(g_error_message), "Expected ']', but got %s.\n", get_token_simple(g_token_current));
          return print_error(g_error_message, ERROR_ERR);
        }

        /* next token */
        _next_token();
      }
      else if (g_token_current->id == TOKEN_ID_SYMBOL && g_token_current->value == ']') {
        /* next token */
        _next_token();
      }
      else {
        snprintf(g_error_message, sizeof(g_error_message), "Expected array size or ']', but got %s.\n", get_token_simple(g_token_current));
        return print_error(g_error_message, ERROR_ERR);
      }

      /* store the size of the array - NOTE: this will be possbile recalculated later when we process initializers */
      node->value = array_size;
    }

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != '=' &&
                                                   g_token_current->value != ';' &&
                                                   g_token_current->value != ',')) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected '=', ';' or ',', but got %s.\n", get_token_simple(g_token_current));
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    if (g_token_current->value == ';')
      return SUCCEEDED;
    else if (g_token_current->value == ',') {
      /* skip ',' */
      _next_token();

      continue;
    }

    /* skip '=' */
    _next_token();

    if (is_extern == YES)
      return print_error_using_token("Extern variable definitions don't take initializers.\n", ERROR_ERR, g_token_previous);

    if (_collect_struct_union_initializers(node) == FAILED)
      return FAILED;

    if (g_token_current->id != TOKEN_ID_SYMBOL || (g_token_current->value != ';' &&
                                                   g_token_current->value != ',')) {
      snprintf(g_error_message, sizeof(g_error_message), "Expected ';' or ',', but got %s.\n", get_token_simple(g_token_current));
      return print_error_using_token(g_error_message, ERROR_ERR, g_token_previous);
    }

    if (g_token_current->value == ';')
      return SUCCEEDED;

    /* skip ',' */
    _next_token();
  }

  return SUCCEEDED;
}
