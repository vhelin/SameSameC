
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "defines.h"
#include "parse.h"
#include "main.h"
#include "printf.h"
#include "stack.h"
#include "include_file.h"
#include "pass_3.h"
#include "tree_node.h"
#include "inline_asm.h"
#include "struct_item.h"


/* define this for DEBUG */

#define DEBUG_PASS_3 1


extern struct tree_node *g_global_nodes, *g_label_definition;
extern int g_verbose_mode, g_input_float_mode, g_current_filename_id, g_current_line_number;
extern char *g_variable_types[9], *g_two_char_symbols[10], g_label[MAX_NAME_LENGTH + 1];
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern double g_parsed_double;

static int g_current_indentation_depth = 0, g_print_is_inside_for = NO, g_simplified_expressions = 0, g_simplify_expressions_failed = NO;
static char g_current_indentation[256];


static char *_get_current_indentation(void) {

  int i, depth;

  if (g_print_is_inside_for == YES)
    depth = 0;
  else
    depth = g_current_indentation_depth;
  
  for (i = 0; i < depth; i++)
    g_current_indentation[i] = ' ';
  g_current_indentation[depth] = 0;

  return g_current_indentation;
}


static char *_get_current_end_of_line(void) {

  if (g_print_is_inside_for == YES)
    return "";
  else
    return "\n";
}


static char *_get_current_end_of_statement(void) {

  if (g_print_is_inside_for == YES)
    return "";
  else
    return ";";
}


static void _print_simple_tree_node(struct tree_node *node);
static void _print_block(struct tree_node *node);
static void _simplify_expressions_block(struct tree_node *node);


void print_expression(struct tree_node *node) {

  int i;

  if (node == NULL) {
    fprintf(stderr, "E");
    return;
  }

  if (node->type != TREE_NODE_TYPE_EXPRESSION) {
    _print_simple_tree_node(node);
    return;
  }

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      print_expression(node->children[i]);
    else
      _print_simple_tree_node(node->children[i]);
  }
}


static void _print_struct_access(struct tree_node *node) {

  int i;

  for (i = 0; i < node->added_children; i++) {
    struct tree_node *child = node->children[i];

    if (child->type == TREE_NODE_TYPE_VALUE_STRING)
      fprintf(stderr, "%s", child->label);
    else if (child->type == TREE_NODE_TYPE_SYMBOL) {
      if (child->value <= SYMBOL_POINTER)
        fprintf(stderr, "%s", g_two_char_symbols[child->value]);
      else
        fprintf(stderr, "%c", child->value);
    }
    else if (child->type == TREE_NODE_TYPE_EXPRESSION)
      print_expression(child);
    else if (child->type == TREE_NODE_TYPE_VALUE_INT || child->type == TREE_NODE_TYPE_VALUE_STRING)
      _print_simple_tree_node(child);
    else
      fprintf(stderr, "?");
  }
}


static void _print_simple_tree_node(struct tree_node *node) {

  int i;
  
  if (node == NULL) {
    fprintf(stderr, "E");
    return;
  }

  if (node->type == TREE_NODE_TYPE_VALUE_INT)
    fprintf(stderr, "%d", node->value);
  else if (node->type == TREE_NODE_TYPE_VALUE_DOUBLE)
    fprintf(stderr, "%f", node->value_double);
  else if (node->type == TREE_NODE_TYPE_VALUE_STRING)
    fprintf(stderr, "%s", node->label);
  else if (node->type == TREE_NODE_TYPE_SYMBOL) {
    if (node->value <= SYMBOL_POINTER)
      fprintf(stderr, "%s", g_two_char_symbols[node->value]);
    else
      fprintf(stderr, "%c", node->value);
  }
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL) {
    fprintf(stderr, "%s(", node->children[0]->label);
    for (i = 1; i < node->added_children; i++) {
      if (i > 1)
        fprintf(stderr, ",");
      print_expression(node->children[i]);
    }
    fprintf(stderr, ")");
  }
  else if (node->type == TREE_NODE_TYPE_ARRAY_ITEM) {
    fprintf(stderr, "%s[", node->label);
    print_expression(node->children[0]);
    fprintf(stderr, "]");
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    if (node->value_double > 0.0) {
      /* post */
      if (node->added_children > 0)
        _print_struct_access(node);
      else
        fprintf(stderr, "%s", node->label);
      if (node->value == SYMBOL_INCREMENT)
        fprintf(stderr, "++");
      else if (node->value == SYMBOL_DECREMENT)
        fprintf(stderr, "--");
      else
        fprintf(stderr, "?");
    }
    else {
      /* pre */
      if (node->value == SYMBOL_INCREMENT)
        fprintf(stderr, "++");
      else if (node->value == SYMBOL_DECREMENT)
        fprintf(stderr, "--");
      else
        fprintf(stderr, "?");
      if (node->added_children > 0)
        _print_struct_access(node);
      else
        fprintf(stderr, "%s", node->label);
    }
  }
  else if (node->type == TREE_NODE_TYPE_GET_ADDRESS) {
    fprintf(stderr, "&%s", node->label);
  }
  else if (node->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY) {
    fprintf(stderr, "&%s[", node->label);
    print_expression(node->children[0]);
    fprintf(stderr, "]");
  }
  else if (node->type == TREE_NODE_TYPE_BYTES) {
    if (node->value_double == -1.0)
      fprintf(stderr, "\"%s\", 0", node->label);
    else
      fprintf(stderr, "[%d bytes]", node->value);
  }
  else
    fprintf(stderr, "?");
}


static char *_get_pointer_stars(struct tree_node *node) {

  if (node == NULL)
    return "";
  if (node->type != TREE_NODE_TYPE_VARIABLE_TYPE)
    return "?";
  if (node->value_double == 0)
    return "";
  if (node->value_double == 1)
    return "*";
  if (node->value_double == 2)
    return "**";
  if (node->value_double == 3)
    return "***";

  return "?";
}


static void _print_struct_union(struct tree_node *node) {

  int i;
  
  for (i = 2; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      print_expression(node->children[i]);
    else
      _print_simple_tree_node(node->children[i]);      
  }
}


static void _print_create_variable(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  fprintf(stderr, "%s", _get_current_indentation());

  if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
    fprintf(stderr, "extern ");
  if ((node->flags & TREE_NODE_FLAG_STATIC) == TREE_NODE_FLAG_STATIC)
    fprintf(stderr, "static ");
  if ((node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1)
    fprintf(stderr, "const ");
  
  fprintf(stderr, "%s ", g_variable_types[node->children[0]->value]);
  if (node->children[0]->value == VARIABLE_TYPE_STRUCT)
    fprintf(stderr, "%s ", node->children[0]->children[0]->label);
  if (node->children[0]->value_double > 0)
    fprintf(stderr, "%s ", _get_pointer_stars(node->children[0]));

  if ((node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)
    fprintf(stderr, " const ");
  
  fprintf(stderr, "%s", node->children[1]->label);

  if (node->value == 0) {
    /* not an array */
    if (node->added_children > 2) {
      fprintf(stderr, " = ");
      if (node->children[0]->value == VARIABLE_TYPE_STRUCT || node->children[0]->value == VARIABLE_TYPE_UNION)
        _print_struct_union(node);
      else
        print_expression(node->children[2]);
    }
  }
  else {
    /* an array */
    fprintf(stderr, "[%d]", node->value);
    if (node->added_children > 2) {
      fprintf(stderr, " = ");
      if (node->children[0]->value == VARIABLE_TYPE_STRUCT || node->children[0]->value == VARIABLE_TYPE_UNION)
        _print_struct_union(node);
      else {
        fprintf(stderr, " { ");
        for (i = 2; i < node->added_children; i++) {
          if (i > 2)
            fprintf(stderr, ", ");
          print_expression(node->children[i]);
        }
        fprintf(stderr, " }");
      }
    }
  }

  fprintf(stderr, ";\n");
}


static void _print_assignment(struct tree_node *node) {

  if (node == NULL)
    return;

  /* array assignment? */
  if (node->added_children > 2) {
    fprintf(stderr, "%s%s[", _get_current_indentation(), node->children[0]->label);

    print_expression(node->children[1]);
    fprintf(stderr, "] = ");
    print_expression(node->children[2]);
    
    fprintf(stderr, "%s%s", _get_current_end_of_statement(), _get_current_end_of_line());
  }
  else {
    fprintf(stderr, "%s%s = ", _get_current_indentation(), node->children[0]->label);

    print_expression(node->children[1]);

    fprintf(stderr, "%s%s", _get_current_end_of_statement(), _get_current_end_of_line());
  }
}


static void _print_return(struct tree_node *node) {

  if (node == NULL)
    return;

  fprintf(stderr, "%sreturn", _get_current_indentation());

  if (node->added_children > 0) {
    fprintf(stderr, " ");
    print_expression(node->children[0]);
  }

  fprintf(stderr, ";\n");
}


static void _print_function_call(struct tree_node *node) {

  if (node == NULL)
    return;

  fprintf(stderr, "%s", _get_current_indentation());

  _print_simple_tree_node(node);

  fprintf(stderr, "%s%s", _get_current_end_of_statement(), _get_current_end_of_line());
}


static void _print_condition(struct tree_node *node, int level) {

  int i;
  
  if (node == NULL)
    return;

  if (level > 0)
    fprintf(stderr, "(");

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _print_condition(node->children[i], level + 1);
    else if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      print_expression(node->children[i]);
    else
      _print_simple_tree_node(node->children[i]);
  }

  if (level > 0)
    fprintf(stderr, ")");
}


static void _print_if(struct tree_node *node) {

  int i, got_condition = NO;
  
  if (node == NULL)
    return;

  fprintf(stderr, "%sif (", _get_current_indentation());

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION) {
      if (i > 0)
        fprintf(stderr, "%selse if (", _get_current_indentation());
      _print_condition(node->children[i], 0);
      fprintf(stderr, ") {\n");
      got_condition = YES;
    }
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK) {
      if (i > 1 && i == node->added_children - 1 && got_condition == NO)
        fprintf(stderr, "%selse {\n", _get_current_indentation());
      _print_block(node->children[i]);
      fprintf(stderr, "%s}\n", _get_current_indentation());
      got_condition = NO;
    }
  }
}


static void _print_switch(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  fprintf(stderr, "%sswitch (", _get_current_indentation());
  print_expression(node->children[0]);
  fprintf(stderr, ") {\n");

  g_current_indentation_depth += 2;
  
  for (i = 1; i < node->added_children; i += 2) {
    if (i <= node->added_children - 2) {
      /* case */
      fprintf(stderr, "%scase ", _get_current_indentation());
      print_expression(node->children[i]);
      fprintf(stderr, ":\n");

      _print_block(node->children[i+1]);
    }
    else {
      /* default */
      fprintf(stderr, "%sdefault:\n", _get_current_indentation());
      _print_block(node->children[i]);
    }
  }

  g_current_indentation_depth -= 2;

  fprintf(stderr, "%s}\n", _get_current_indentation());
}


static void _print_while(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->added_children != 2) {
    fprintf(stderr, "We have a WHILE with added_children != 2! Please submit a bug report!\n");
    return;
  }

  fprintf(stderr, "%swhile (", _get_current_indentation());
  _print_condition(node->children[0], 0);
  fprintf(stderr, ") {\n");
  _print_block(node->children[1]);
  fprintf(stderr, "%s}\n", _get_current_indentation());
}


static void _print_do(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->added_children != 2) {
    fprintf(stderr, "We have a DO with added_children != 2! Please submit a bug report!\n");
    return;
  }

  fprintf(stderr, "%sdo {\n", _get_current_indentation());
  _print_block(node->children[0]);
  fprintf(stderr, "%s} while (", _get_current_indentation());
  _print_condition(node->children[1], 0);
  fprintf(stderr, ");\n");
}


static void _print_for(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->added_children != 4) {
    fprintf(stderr, "We have a FOR with added_children != 4! Please submit a bug report!\n");
    return;
  }
  
  fprintf(stderr, "%sfor (", _get_current_indentation());

  g_print_is_inside_for = YES;

  _print_block(node->children[0]);
  fprintf(stderr, "; ");
  _print_condition(node->children[1], 0);
  fprintf(stderr, "; ");
  _print_block(node->children[2]);
  fprintf(stderr, ") {\n");

  g_print_is_inside_for = NO;

  _print_block(node->children[3]);
  fprintf(stderr, "%s}\n", _get_current_indentation());
}


static void _print_asm(struct tree_node *node) {

  struct inline_asm *ia;
  struct asm_line *al;
  int printed = NO;
  
  if (node == NULL)
    return;

  ia = inline_asm_find(node->value);
  if (ia == NULL)
    return;

  al = ia->asm_line_first;
  while (al != NULL) {
    if (printed == NO)
      fprintf(stderr, "%s__asm(", _get_current_indentation());
    else
      fprintf(stderr, "%s      ", _get_current_indentation());
    fprintf(stderr, "\"%s\"", al->line);

    printed = YES;
    al = al->next;

    if (al != NULL)
      fprintf(stderr, ",\n");
  }

  fprintf(stderr, ");\n");
}


static void _print_statement(struct tree_node *node, int line) {

  if (node == NULL)
    return;

  if (g_print_is_inside_for == YES && line > 0)
    fprintf(stderr, ",");
  
  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    _print_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_ASSIGNMENT)
    _print_assignment(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL)
    _print_function_call(node);
  else if (node->type == TREE_NODE_TYPE_RETURN)
    _print_return(node);
  else if (node->type == TREE_NODE_TYPE_IF)
    _print_if(node);
  else if (node->type == TREE_NODE_TYPE_SWITCH)
    _print_switch(node);
  else if (node->type == TREE_NODE_TYPE_WHILE)
    _print_while(node);
  else if (node->type == TREE_NODE_TYPE_DO)
    _print_do(node);
  else if (node->type == TREE_NODE_TYPE_FOR)
    _print_for(node);
  else if (node->type == TREE_NODE_TYPE_LABEL) {
    fprintf(stderr, "%s", _get_current_indentation());
    fprintf(stderr, "%s:", node->label);
    fprintf(stderr, "%s", _get_current_end_of_line());
  }
  else if (node->type == TREE_NODE_TYPE_GOTO) {
    fprintf(stderr, "%s", _get_current_indentation());
    fprintf(stderr, "goto %s", node->label);
    fprintf(stderr, "%s%s", _get_current_end_of_statement(), _get_current_end_of_line());
  }
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
    fprintf(stderr, "%s", _get_current_indentation());
    _print_simple_tree_node(node);
    fprintf(stderr, "%s%s", _get_current_end_of_statement(), _get_current_end_of_line());
  }
  else if (node->type == TREE_NODE_TYPE_BREAK)
    fprintf(stderr, "%sbreak%s%s", _get_current_indentation(), _get_current_end_of_statement(), _get_current_end_of_line());
  else if (node->type == TREE_NODE_TYPE_CONTINUE)
    fprintf(stderr, "%scontinue%s%s", _get_current_indentation(), _get_current_end_of_statement(), _get_current_end_of_line());
  else if (node->type == TREE_NODE_TYPE_ASM)
    _print_asm(node);
  else
    fprintf(stderr, "%s?%s%s", _get_current_indentation(), _get_current_end_of_statement(), _get_current_end_of_line());
}


static void _print_block(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  if (node->type != TREE_NODE_TYPE_BLOCK) {
    fprintf(stderr, "_print_function(): Was expecting TREE_NODE_TYPE_BLOCK, got %d instead! Please submit a bug report!\n", node->type);
    return;
  }

  g_current_indentation_depth += 2;

  for (i = 0; i < node->added_children; i++)
    _print_statement(node->children[i], i);

  g_current_indentation_depth -= 2;
}


static void _print_function_definition(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  if ((node->flags & TREE_NODE_FLAG_EXTERN) == TREE_NODE_FLAG_EXTERN)
    fprintf(stderr, "extern ");
  if ((node->flags & TREE_NODE_FLAG_STATIC) == TREE_NODE_FLAG_STATIC)
    fprintf(stderr, "static ");
  if ((node->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1)
    fprintf(stderr, "const ");

  fprintf(stderr, "%s %s", g_variable_types[node->children[0]->value], _get_pointer_stars(node->children[0]));

  if ((node->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)
    fprintf(stderr, "const ");

  fprintf(stderr, "%s(", node->children[1]->label);
  
  for (i = 2; i < node->added_children-1; i += 2) {
    if (i > 2)
      fprintf(stderr, ", ");

    if ((node->children[i]->flags & TREE_NODE_FLAG_CONST_1) == TREE_NODE_FLAG_CONST_1)
      fprintf(stderr, "const ");
    
    fprintf(stderr, "%s %s", g_variable_types[node->children[i]->value], _get_pointer_stars(node->children[i]));

    if ((node->children[i]->flags & TREE_NODE_FLAG_CONST_2) == TREE_NODE_FLAG_CONST_2)
      fprintf(stderr, "const ");
    
    fprintf(stderr, "%s", node->children[i+1]->label);
  }
  
  fprintf(stderr, ")");

  if ((node->flags & TREE_NODE_FLAG_PUREASM) == TREE_NODE_FLAG_PUREASM)
    fprintf(stderr, " __pureasm");
  
  if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
    fprintf(stderr, ";\n");
    return;
  }
  
  fprintf(stderr, " {\n");

  _print_block(node->children[node->added_children-1]);

  fprintf(stderr, "}\n");
}


static void _print_global_node(struct tree_node *node) {

  if (node == NULL)
    return;
  
  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    _print_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION)
    _print_function_definition(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE)
    _print_function_definition(node);
  else
    fprintf(stderr, "_print_global_node(): Unknown global node type %d! Please submit a bug report!\n", node->type);
}


#if defined(DEBUG_PASS_3)

static void _print_global_nodes(void) {

  int i;

  fprintf(stderr, "///////////////////////////////////////////////////////////////////////\n");
  fprintf(stderr, "// TREE NODES\n");
  fprintf(stderr, "///////////////////////////////////////////////////////////////////////\n");

  print_struct_items();

  for (i = 0; i < g_global_nodes->added_children; i++)
    _print_global_node(g_global_nodes->children[i]);
}

#endif


int pass_3(void) {
  
  if (g_verbose_mode == ON)
    printf("Pass 3...\n");

#if defined(DEBUG_PASS_3)
  /* print out what we have parsed so far */
  _print_global_nodes();
#endif
  
  if (simplify_expressions() == FAILED)
    return FAILED;

#if defined(DEBUG_PASS_3)
  fprintf(stderr, ">------------------------------ SIMPLIFIED ------------------------------>\n");
#endif
  
#if defined(DEBUG_PASS_3)
  /* print out everything again to see if we were able to simplify anything */
  _print_global_nodes();
#endif

  /* keep the compiler silent */
  if (0) {
    _print_global_node(g_global_nodes->children[0]);
  }
  
  return SUCCEEDED;
}


static int _simplify_expression(struct tree_node *node) {

  int i, j, subexpressions = 0, result;
  
  if (node == NULL)
    return FAILED;

  if (node->type != TREE_NODE_TYPE_EXPRESSION)
    return FAILED;
  
  /* try to simplify a sub expression */

  for (i = 0; i < node->added_children; i++) {
    struct tree_node *child = node->children[i];
    
    if (child->type == TREE_NODE_TYPE_EXPRESSION) {
      if (_simplify_expression(child) == SUCCEEDED)
        return SUCCEEDED;
      else
        subexpressions++;
    }
    else if (child->type == TREE_NODE_TYPE_FUNCTION_CALL) {
      for (j = 1; j < child->added_children; j++) {
        if (_simplify_expression(child->children[j]) == SUCCEEDED)
          return SUCCEEDED;
        subexpressions++;
      }
    }
    else if (child->type == TREE_NODE_TYPE_GET_ADDRESS_ARRAY) {
      if (_simplify_expression(child->children[0]) == SUCCEEDED)
        return SUCCEEDED;
      subexpressions++;
    }
    else if (child->type == TREE_NODE_TYPE_VALUE_STRING) {
      /* NOTE! if the variable we are referencing here is const, then we'll transform this
         tree_node to contain its value so future passes can possibly optimize calculations
         or just use the pure value instead of an expensive variable reference */
      if (child->definition->children[0]->value_double == 0.0 && child->definition->flags & TREE_NODE_FLAG_CONST_1) {
        if (child->definition->added_children == 3 && child->definition->children[2]->type == TREE_NODE_TYPE_VALUE_INT) {
          /* transform! */
          child->type = TREE_NODE_TYPE_VALUE_INT;
          child->value = child->definition->children[2]->value;

          /* remove a read reference */
          child->definition->reads--;

          return SUCCEEDED;
        }
      }
    }
    else if (child->type == TREE_NODE_TYPE_ARRAY_ITEM) {
      /* NOTE! if the array we are referencing here is const (and index, too), then we'll transform this
         tree_node to contain its value so future passes can possibly optimize calculations
         or just use the pure value instead of an expensive variable reference */
      if (child->definition->children[0]->value_double == 0.0 && child->definition->flags & TREE_NODE_FLAG_CONST_1) {
        if (child->added_children == 1 && child->children[0]->type == TREE_NODE_TYPE_VALUE_INT) {
          int index = child->children[0]->value;
          int items = child->definition->added_children - 2;

          if (index >= items || index < 0) {
            g_simplify_expressions_failed = YES;
            snprintf(g_error_message, sizeof(g_error_message), "_simplify_expression(): Trying to access array item %d, but the array has only %d items!\n", index, items);
            return print_error_using_tree_node(g_error_message, ERROR_ERR, child);
          }

          if (child->definition->children[2 + index]->type == TREE_NODE_TYPE_VALUE_INT) {
            /* transform! */
            child->type = TREE_NODE_TYPE_VALUE_INT;
            child->value = child->definition->children[2 + index]->value;

            /* remove a read reference */
            child->definition->reads--;

            free_tree_node_children(child);

            return SUCCEEDED;
          }
        }
      }
      
      if (_simplify_expression(child->children[0]) == SUCCEEDED)
        return SUCCEEDED;
      subexpressions++;
    }
  }

  if (subexpressions == 0) {
    int value;

    /* can we simplify this expression? */
    
    g_input_float_mode = ON;
    result = stack_calculate_tree_node(node, &value);
    g_input_float_mode = OFF;
    
    if (result == INPUT_NUMBER_FLOAT) {
      /* turn the expression into a value */
      free_tree_node_children(node);

      if (ceil(g_parsed_double) == g_parsed_double) {
        node->type = TREE_NODE_TYPE_VALUE_INT;
        node->value = (int)g_parsed_double;
      }
      else {
        node->type = TREE_NODE_TYPE_VALUE_DOUBLE;
        node->value_double = g_parsed_double;
      }

      return SUCCEEDED;
    }
    else if (result == INPUT_NUMBER_STRING) {
      /* turn the expression into a string - HACK_1 */
      free_tree_node_children(node);

      node->type = TREE_NODE_TYPE_VALUE_STRING;
      node->definition = g_label_definition;

      if (tree_node_set_string(node, g_label) == FAILED)
        return FAILED;

      return SUCCEEDED;
    }
  }

  return FAILED;
}


static void _simplify_expressions(struct tree_node *node) {

  if (node == NULL)
    return;

  /* try to simplify until it cannot be simplified any more */
  while (_simplify_expression(node) == SUCCEEDED)
    g_simplified_expressions++;
}


static void _simplify_expressions_create_variable(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 2; i < node->added_children; i++)
    _simplify_expressions(node->children[i]);
}


static void _simplify_expressions_assignment(struct tree_node *node) {

  if (node == NULL)
    return;

  _simplify_expressions(node->children[1]);

  /* array assignment? */
  if (node->added_children > 2)
    _simplify_expressions(node->children[2]);
}


static void _simplify_expressions_return(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->added_children > 0)
    _simplify_expressions(node->children[0]);
}


static void _simplify_expressions_function_call(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 1; i < node->added_children; i++)
    _simplify_expressions(node->children[i]);
}


static void _simplify_expressions_condition(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _simplify_expressions_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      _simplify_expressions(node->children[i]);
  }
}


static void _simplify_expressions_if(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _simplify_expressions_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _simplify_expressions_block(node->children[i]);
  }
}


static void _simplify_expressions_switch(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      _simplify_expressions(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _simplify_expressions_block(node->children[i]);
  }
}


static void _simplify_expressions_increment_decrement(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_EXPRESSION)
      _simplify_expressions(node->children[i]);
  }
}


static void _simplify_expressions_while(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _simplify_expressions_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _simplify_expressions_block(node->children[i]);
  }
}


static void _simplify_expressions_do(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _simplify_expressions_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _simplify_expressions_block(node->children[i]);
  }
}


static void _simplify_expressions_for(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  for (i = 0; i < node->added_children; i++) {
    if (node->children[i]->type == TREE_NODE_TYPE_CONDITION)
      _simplify_expressions_condition(node->children[i]);
    else if (node->children[i]->type == TREE_NODE_TYPE_BLOCK)
      _simplify_expressions_block(node->children[i]);
  }
}


static void _simplify_expressions_statement(struct tree_node *node) {

  if (node == NULL)
    return;

  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    _simplify_expressions_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_ASSIGNMENT)
    _simplify_expressions_assignment(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_CALL)
    _simplify_expressions_function_call(node);
  else if (node->type == TREE_NODE_TYPE_RETURN)
    _simplify_expressions_return(node);
  else if (node->type == TREE_NODE_TYPE_IF)
    _simplify_expressions_if(node);
  else if (node->type == TREE_NODE_TYPE_WHILE)
    _simplify_expressions_while(node);
  else if (node->type == TREE_NODE_TYPE_DO)
    _simplify_expressions_do(node);
  else if (node->type == TREE_NODE_TYPE_FOR)
    _simplify_expressions_for(node);
  else if (node->type == TREE_NODE_TYPE_SWITCH)
    _simplify_expressions_switch(node);
  else if (node->type == TREE_NODE_TYPE_INCREMENT_DECREMENT)
    _simplify_expressions_increment_decrement(node);
}


static void _simplify_expressions_block(struct tree_node *node) {

  int i;
  
  if (node == NULL)
    return;

  if (node->type != TREE_NODE_TYPE_BLOCK) {
    g_simplify_expressions_failed = YES;
    snprintf(g_error_message, sizeof(g_error_message), "_simplify_expressions_block(): Was expecting TREE_NODE_TYPE_BLOCK, got %d instead! Please submit a bug report!\n", node->type);
    print_error_using_tree_node(g_error_message, ERROR_ERR, node);
    return;
  }

  for (i = 0; i < node->added_children; i++)
    _simplify_expressions_statement(node->children[i]);
}


static void _simplify_expressions_function_definition(struct tree_node *node) {

  if (node == NULL)
    return;

  _simplify_expressions_block(node->children[node->added_children-1]);
}


static void _simplify_expressions_global_node(struct tree_node *node) {

  if (node == NULL)
    return;
  
  if (node->type == TREE_NODE_TYPE_CREATE_VARIABLE || node->type == TREE_NODE_TYPE_CREATE_VARIABLE_FUNCTION_ARGUMENT)
    _simplify_expressions_create_variable(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_DEFINITION)
    _simplify_expressions_function_definition(node);
  else if (node->type == TREE_NODE_TYPE_FUNCTION_PROTOTYPE) {
    /* nothing to simplify here */
  }
  else {
    g_simplify_expressions_failed = YES;
    snprintf(g_error_message, sizeof(g_error_message), "_simplify_expressions_global_node(): Unknown global node type %d! Please submit a bug report!\n", node->type);
    print_error_using_tree_node(g_error_message, ERROR_ERR, node);
  }
}


int simplify_expressions(void) {

  int i, loop = 1;

  /* loop expression simplifier until all expressions are simplified as much as possible */
  do {
    g_simplified_expressions = 0;

    for (i = 0; i < g_global_nodes->added_children; i++)
      _simplify_expressions_global_node(g_global_nodes->children[i]);

    if (g_simplify_expressions_failed == NO) {
      fprintf(stderr, "simplify_expressions(): Loop %d managed to do %d optimizations.\n", loop, g_simplified_expressions);
      loop++;
    }
  } while (g_simplified_expressions > 0 && g_simplify_expressions_failed == NO);

  if (g_simplify_expressions_failed == YES)
    return FAILED;
  
  return SUCCEEDED;
}
