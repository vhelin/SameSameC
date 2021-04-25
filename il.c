
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "defines.h"

#include "printf.h"
#include "include_file.h"
#include "main.h"
#include "il.h"
#include "tac.h"
#include "pass_4.h"
#include "tree_node.h"
#include "stack.h"


extern int g_current_line_number, g_current_filename_id;
extern int g_temp_r, g_temp_label_id;


int il_stack_calculate_expression(struct tree_node *node) {

  struct stack_item si[256];
  struct tree_node *child;
  int q, b = 0, z, rresult = g_temp_r++;

  /* do we need to flatten the expression? (i.e., does it contain other expressions? */
  if (tree_node_does_contain_expressions(node) == YES)
    tree_node_flatten(node);
  
  /* slice the data into infix format */
  for (q = 0, z = 0; q < node->added_children; q++, z++) {
    /* init the stack item */
    si[z].type = 0x123456;
    si[z].sign = 0x123456;
    si[z].value = 0x123456;
    si[z].string[0] = 0;

    child = node->children[q];

    if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '-') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_MINUS;
      fprintf(stderr, "GOT STACK ITEM -\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '+') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_PLUS;
      fprintf(stderr, "GOT STACK ITEM +\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '*') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_MULTIPLY;
      fprintf(stderr, "GOT STACK ITEM *\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '/') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_DIVIDE;
      fprintf(stderr, "GOT STACK ITEM /\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '|') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_OR;
      fprintf(stderr, "GOT STACK ITEM |\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '&') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_AND;
      fprintf(stderr, "GOT STACK ITEM &\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '^') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_POWER;
      fprintf(stderr, "GOT STACK ITEM ^\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '%') {
      if (q == 0) {
        print_error("Syntax error. Invalid use of modulo.\n", ERROR_STC);
        return FAILED;
      }
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_MODULO;
      fprintf(stderr, "GOT STACK ITEM %%\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_LOGICAL_OR) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_LOGICAL_OR;
      fprintf(stderr, "GOT STACK ITEM ||\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_LOGICAL_AND) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_LOGICAL_AND;
      fprintf(stderr, "GOT STACK ITEM &&\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_SHIFT_LEFT) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_SHIFT_LEFT;
      fprintf(stderr, "GOT STACK ITEM <<\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_SHIFT_RIGHT) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_SHIFT_RIGHT;
      fprintf(stderr, "GOT STACK ITEM >>\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '~') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_XOR;
      fprintf(stderr, "GOT STACK ITEM ~\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '!') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_NOT;
      fprintf(stderr, "GOT STACK ITEM !\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ':') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_BANK;
      fprintf(stderr, "GOT STACK ITEM :\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '(') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_LEFT;
      b++;
      fprintf(stderr, "GOT STACK ITEM (\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ')') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_RIGHT;
      /* end of expression? */
      if (b == 0)
        break;
      b--;
      fprintf(stderr, "GOT STACK ITEM )\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '<') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_COMPARE_LT;
      fprintf(stderr, "GOT STACK ITEM <\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == '>') {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_COMPARE_GT;
      fprintf(stderr, "GOT STACK ITEM >\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_EQUAL) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_COMPARE_EQ;
      fprintf(stderr, "GOT STACK ITEM ==\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_UNEQUAL) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_COMPARE_NEQ;
      fprintf(stderr, "GOT STACK ITEM !=\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_LTE) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_COMPARE_LTE;
      fprintf(stderr, "GOT STACK ITEM <=\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == SYMBOL_GTE) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_COMPARE_GTE;
      fprintf(stderr, "GOT STACK ITEM >=\n");
    }
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ',')
      break;
    else if (child->type == TREE_NODE_TYPE_SYMBOL && child->value == ']')
      break;
    else if (child->type == TREE_NODE_TYPE_VALUE_INT) {
      si[z].type = STACK_ITEM_TYPE_VALUE;
      si[z].value = child->value;
      si[z].sign = SI_SIGN_POSITIVE;
      fprintf(stderr, "GOT STACK ITEM %d\n", (int)si[z].value);
    }
    else if (child->type == TREE_NODE_TYPE_VALUE_DOUBLE) {
      si[z].type = STACK_ITEM_TYPE_VALUE;
      si[z].value = child->value_double;
      si[z].sign = SI_SIGN_POSITIVE;
      fprintf(stderr, "GOT STACK ITEM %f\n", si[z].value);
    }
    else if (child->type == TREE_NODE_TYPE_VALUE_STRING) {
      si[z].sign = SI_SIGN_POSITIVE;
      strncpy(si[z].string, child->label, MAX_NAME_LENGTH);
      si[z].type = STACK_ITEM_TYPE_STRING;
      fprintf(stderr, "GOT STACK ITEM %s\n", si[z].string);
    }
    else if (child->type == TREE_NODE_TYPE_INCREMENT_DECREMENT) {
      si[z].type = STACK_ITEM_TYPE_OPERATOR;

      /* inc or dec? */
      if ((int)child->value == SYMBOL_INCREMENT) {
        /* post or pre? */
        if (child->value_double > 0.0) {
          si[z].value = SI_OP_POST_INC;
          fprintf(stderr, "GOT STACK ITEM %s++\n", child->label);
        }
        else {
          si[z].value = SI_OP_PRE_INC;
          fprintf(stderr, "GOT STACK ITEM ++%s\n", child->label);
        }
      }
      else {
        /* post or pre? */
        if (child->value_double > 0.0) {
          si[z].value = SI_OP_POST_DEC;
          fprintf(stderr, "GOT STACK ITEM %s--\n", child->label);
        }
        else {
          si[z].value = SI_OP_PRE_DEC;
          fprintf(stderr, "GOT STACK ITEM --%s\n", child->label);
        }
      }

      z++;

      si[z].sign = SI_SIGN_POSITIVE;
      strncpy(si[z].string, child->label, MAX_NAME_LENGTH);
      si[z].type = STACK_ITEM_TYPE_STRING;
      si[z].value = 0;
    }
    else if (child->type == TREE_NODE_TYPE_FUNCTION_CALL) {
      struct tac *t = generate_il_create_function_call(child);

      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_FUNCTION_CALL_USE_RETURN_VALUE;

      tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);

      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_USE_REGISTER;

      z++;

      si[z].type = STACK_ITEM_TYPE_VALUE;
      si[z].value = g_temp_r - 1;
      si[z].sign = SI_SIGN_POSITIVE;
    }
    else if (child->type == TREE_NODE_TYPE_ARRAY_ITEM) {
      int rindex = g_temp_r;
      struct tac *t;
      
      /* calculate the index */
      if (il_stack_calculate_expression(child->children[0]) == FAILED)
        return FAILED;

      t = add_tac();
      if (t == NULL)
        return FAILED;

      t->op = TAC_OP_ARRAY_READ;

      tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, child->label);
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, rindex, NULL);

      si[z].type = STACK_ITEM_TYPE_OPERATOR;
      si[z].value = SI_OP_USE_REGISTER;

      z++;

      si[z].type = STACK_ITEM_TYPE_VALUE;
      si[z].value = g_temp_r - 1;
      si[z].sign = SI_SIGN_POSITIVE;      
    }
    else {
      fprintf(stderr, "_il_stack_calculate_expression(): Got an unhandled tree_node of type %d! Please submit a bug report!\n", child->type);
      return FAILED;
    }

    if (z >= 255) {
      print_error("Out of stack space.\n", ERROR_STC);
      return FAILED;
    }
  }

  if (b != 0) {
    print_error("Unbalanced parentheses.\n", ERROR_STC);
    return FAILED;
  }
  
  return il_stack_calculate(si, z, rresult);
}


int il_stack_calculate(struct stack_item *si, int q, int rresult) {

  int b, d, k, op[256], o, l;
  struct stack_item ta[256];
  struct stack s;

  /* only one item found -> let the item parser handle it */
  /*
  if (q == 1)
    return STACK_CALCULATE_DELAY;
  */

  /* check if there was data before the computation */
  if (q > 1 && (si[0].type == STACK_ITEM_TYPE_STRING || si[0].type == STACK_ITEM_TYPE_VALUE)) {
    if (si[1].type == STACK_ITEM_TYPE_STRING || si[1].type == STACK_ITEM_TYPE_VALUE)
      return STACK_CALCULATE_DELAY;
    if (si[1].type == STACK_ITEM_TYPE_OPERATOR) {
      if (si[1].value == SI_OP_LEFT)
        return STACK_CALCULATE_DELAY;
    }
  }

  /* check if the computation is of the form "+-..." and remove that leading "+" */
  if (q > 2 && si[0].type == STACK_ITEM_TYPE_OPERATOR && si[0].value == SI_OP_PLUS &&
      si[1].type == STACK_ITEM_TYPE_OPERATOR && si[1].value == SI_OP_MINUS) {
    si[0].type = STACK_ITEM_TYPE_DELETED;
  }

  /* fix the sign in every operand */
  for (b = 1, k = 0; k < q; k++) {
    /* NOT VERY USEFUL IN BILIBALI
    if ((q - k) != 1 && si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].value != SI_OP_BANK
        && si[k + 1].value != SI_OP_HIGH_BYTE && si[k + 1].value != SI_OP_LOW_BYTE) {
      if (si[k].value != SI_OP_LEFT && si[k].value != SI_OP_RIGHT && si[k + 1].value != SI_OP_LEFT && si[k + 1].value != SI_OP_RIGHT) {
        print_error("Error in computation syntax.\n", ERROR_STC);
        return FAILED;
      }
    }
    */
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_MINUS && b == 1) {
      if (si[k + 1].type == STACK_ITEM_TYPE_VALUE || si[k + 1].type == STACK_ITEM_TYPE_STRING) {
        if (si[k + 1].sign == SI_SIGN_POSITIVE)
          si[k + 1].sign = SI_SIGN_NEGATIVE;
        else
          si[k + 1].sign = SI_SIGN_POSITIVE;
        /* it wasn't a minus operator, it was a sign */
        si[k].type = STACK_ITEM_TYPE_DELETED;
      }
      else if (si[k + 1].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].value == SI_OP_LEFT) {
        o = 1;
        l = k + 2;
        while (o > 0 && l < q) {
          if (si[l].type == STACK_ITEM_TYPE_VALUE || si[l].type == STACK_ITEM_TYPE_STRING) {
            if (si[l].sign == SI_SIGN_POSITIVE)
              si[l].sign = SI_SIGN_NEGATIVE;
            else
              si[l].sign = SI_SIGN_POSITIVE;
          }
          else if (si[l].type == STACK_ITEM_TYPE_OPERATOR) {
            if (si[l].value == SI_OP_LEFT)
              o++;
            else if (si[l].value == SI_OP_RIGHT)
              o--;
          }
          l++;
        }

        if (o != 0) {
          print_error("Unbalanced parentheses.\n", ERROR_STC);
          return FAILED;
        }

        si[k].type = STACK_ITEM_TYPE_DELETED;
      }
    }
    /* remove unnecessary + */
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_PLUS && b == 1) {
      if (si[k + 1].type == STACK_ITEM_TYPE_VALUE || si[k + 1].type == STACK_ITEM_TYPE_STRING)
        si[k].type = STACK_ITEM_TYPE_DELETED;
      else if (si[k + 1].type == STACK_ITEM_TYPE_OPERATOR && si[k + 1].value == SI_OP_LEFT)
        si[k].type = STACK_ITEM_TYPE_DELETED;
    }
    else if (si[k].type == STACK_ITEM_TYPE_VALUE || si[k].type == STACK_ITEM_TYPE_STRING)
      b = 0;
    else if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_LEFT)
      b = 1;
  }

  /* turn unary XORs into NOTs */
  for (b = 1, k = 0; k < q; k++) {
    if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_XOR && b == 1)
      si[k].value = SI_OP_NOT;
    else if (si[k].type == STACK_ITEM_TYPE_VALUE || si[k].type == STACK_ITEM_TYPE_STRING)
      b = 0;
    else if (si[k].type == STACK_ITEM_TYPE_OPERATOR && si[k].value == SI_OP_LEFT)
      b = 1;
  }

  /* convert infix stack into postfix stack */
  for (b = 0, k = 0, d = 0; k < q; k++) {
    /* operands pass through */
    if (si[k].type == STACK_ITEM_TYPE_VALUE) {
      ta[d].type = si[k].type;
      ta[d].value = si[k].value;
      ta[d].sign = si[k].sign;
      d++;
    }
    else if (si[k].type == STACK_ITEM_TYPE_STRING) {
      ta[d].type = si[k].type;
      strcpy(ta[d].string, si[k].string);
      ta[d].sign = si[k].sign;
      d++;
    }
    /* operators get inspected */
    else if (si[k].type == STACK_ITEM_TYPE_OPERATOR) {
      if (b == 0) {
        op[0] = (int)si[k].value;
        b++;
      }
      else {
        if (si[k].value == SI_OP_LEFT) {
          op[b] = SI_OP_LEFT;
          b++;
        }
        else if (si[k].value == SI_OP_RIGHT) {
          b--;
          while (op[b] != SI_OP_LEFT) {
            ta[d].type = STACK_ITEM_TYPE_OPERATOR;
            ta[d].value = op[b];
            b--;
            d++;
          }
        }
        else if (si[k].value == SI_OP_USE_REGISTER) {
          /* use register, priority over everything else */
          op[b] = (int)si[k].value;
          b++;
        }
        else {
          int priority = get_op_priority(si[k].value);
          
          b--;
          while (b != -1 && op[b] != SI_OP_LEFT && get_op_priority(op[b]) >= priority) {
            ta[d].type = STACK_ITEM_TYPE_OPERATOR;
            ta[d].value = op[b];
            b--;
            d++;
          }
          b++;
          op[b] = (int)si[k].value;
          b++;
        }
      }
    }
  }

  /* empty the operator stack */
  while (b > 0) {
    b--;
    ta[d].type = STACK_ITEM_TYPE_OPERATOR;
    ta[d].value = op[b];
    d++;
  }

  s.stack = ta;
  s.linenumber = g_current_line_number;
  s.filename_id = g_current_filename_id;

  if (il_compute_stack(&s, d, rresult) == FAILED)
    return FAILED;
    
  return SUCCEEDED;
}


static void _increment_decrement(char *label, int increment) {

  struct tac *t = add_tac();

  if (t == NULL)
    return;

  if (increment == YES)
    t->op = TAC_OP_ADD;
  else
    t->op = TAC_OP_SUB;

  tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, label);
  tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, 1, NULL);
  tac_set_result(t, TAC_ARG_TYPE_LABEL, 0, label);
}


static struct tac *_add_tac_calculation(int op, int r1, int r2, int rresult, struct stack_item *si[256], double v[256]) {

  struct tac *t = add_tac();

  if (t == NULL)
    return NULL;

  t->op = op;

  tac_set_result(t, TAC_ARG_TYPE_TEMP, rresult, NULL);

  if (si[r1] != NULL) {
    if (si[r1]->type == STACK_ITEM_TYPE_REGISTER)
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (int)si[r1]->value, NULL);
    else
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, si[r1]->string);
  }
  else
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, (int)v[r1], NULL);

  if (si[r2] != NULL) {
    if (si[r2]->type == STACK_ITEM_TYPE_REGISTER)
      tac_set_arg2(t, TAC_ARG_TYPE_TEMP, (int)si[r2]->value, NULL);
    else
      tac_set_arg2(t, TAC_ARG_TYPE_LABEL, 0, si[r2]->string);
  }
  else
    tac_set_arg2(t, TAC_ARG_TYPE_CONSTANT, (int)v[r2], NULL);

  return t;
}


static int _load_to_register(struct stack_item *si[256], double v[256], int index) {

  struct tac *t = add_tac();

  if (t == NULL)
    return -1;

  t->op = TAC_OP_ASSIGNMENT;

  tac_set_result(t, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
  if (si[index] != NULL) {
    if (si[index]->type == STACK_ITEM_TYPE_REGISTER)
      tac_set_arg1(t, TAC_ARG_TYPE_TEMP, (int)si[index]->value, NULL);
    else
      tac_set_arg1(t, TAC_ARG_TYPE_LABEL, 0, si[index]->string);
  }
  else
    tac_set_arg1(t, TAC_ARG_TYPE_CONSTANT, (int)v[index], NULL);

  return g_temp_r - 1;
}


static void _turn_stack_item_into_a_register(struct stack_item *si[256], struct stack_item sit[256], int index, int reg) {

  struct stack_item *s;

  s = &sit[index];
  si[index] = s;

  s->type = STACK_ITEM_TYPE_REGISTER;
  s->value = reg;
}


static int _add_comparison(struct stack_item *si[256], struct stack_item sit[256], double v[256], int index, int op) {

  int label_true = ++g_temp_label_id, label_false = ++g_temp_label_id;
  int tresult = g_temp_r++, r1, r2;
  struct tac *ta;
          
  ta = add_tac();
  if (ta == NULL)
    return FAILED;

  /* load 0 - false */
  ta->op = TAC_OP_ASSIGNMENT;
  tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
  tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 0, NULL);

  /* comparison + jump if true, to label true */
  r1 = _load_to_register(si, v, index-2);
  r2 = _load_to_register(si, v, index-1);

  ta = add_tac();
  if (ta == NULL)
    return FAILED;

  /* TAC_OP_JUMP_* */
  ta->op = op;
  tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, r1, NULL);
  tac_set_arg2(ta, TAC_ARG_TYPE_TEMP, r2, NULL);
  tac_set_result(ta, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(label_true));

  /* jump to label false */
  ta = add_tac();
  if (ta == NULL)
    return FAILED;

  add_tac_jump(generate_temp_label(label_false));

  /* label true */
  add_tac_label(generate_temp_label(label_true));

  /* load 1 - true */
  ta = add_tac();
  if (ta == NULL)
    return FAILED;

  ta->op = TAC_OP_ASSIGNMENT;
  tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
  tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 1, NULL);

  /* label false */
  add_tac_label(generate_temp_label(label_false));

  /* use the result */
  ta = add_tac();
  if (ta == NULL)
    return FAILED;
        
  ta->op = TAC_OP_ASSIGNMENT;
  tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
  tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
          
  _turn_stack_item_into_a_register(si, sit, index-2, (int)ta->result_d);

  return SUCCEEDED;
}


int il_compute_stack(struct stack *sta, int count, int rresult) {

  struct stack_item *s, *si[256], sit[256];
  struct tac *ta;
  double v[256];
  int r, t, r1, r2, tresult;


  v[0] = 0.0;

  s = sta->stack;
  for (r = 0, t = 0; r < count; r++, s++) {
    if (s->type == STACK_ITEM_TYPE_VALUE) {
      if (s->sign == SI_SIGN_NEGATIVE)
        v[t] = -s->value;
      else
        v[t] = s->value;
      si[t] = NULL;
      t++;
    }
    else if (s->type == STACK_ITEM_TYPE_STRING) {
      v[t] = 0.0;
      si[t] = s;
      t++;
    }
    else {
      switch ((int)s->value) {
      case SI_OP_PLUS:
        ta = _add_tac_calculation(TAC_OP_ADD, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_MINUS:
        ta = _add_tac_calculation(TAC_OP_SUB, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_PRE_INC:
        /* increment */
        _increment_decrement(si[t-1]->string, YES);

        /* have the result in the latest register */
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_LABEL, 0, si[t-1]->string);

        _turn_stack_item_into_a_register(si, sit, t-1, g_temp_r-1);
        t--;
        
        break;
      case SI_OP_PRE_DEC:
        /* decrement */
        _increment_decrement(si[t-1]->string, NO);

        /* have the result in the latest register */
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_LABEL, 0, si[t-1]->string);

        _turn_stack_item_into_a_register(si, sit, t-1, g_temp_r-1);
        t--;
        
        break;
      case SI_OP_POST_INC:
        /* get the value before increment */
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        tresult = g_temp_r++;
        
        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_LABEL, 0, si[t-1]->string);

        /* increment */
        _increment_decrement(si[t-1]->string, YES);

        /* have the result in the latest register */
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, tresult, 0);
        
        _turn_stack_item_into_a_register(si, sit, t-1, g_temp_r-1);
        t--;
        
        break;
      case SI_OP_POST_DEC:
        /* get the value before decrement */
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        tresult = g_temp_r++;
        
        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_LABEL, 0, si[t-1]->string);

        /* decrement */
        _increment_decrement(si[t-1]->string, NO);

        /* have the result in the latest register */
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
        
        _turn_stack_item_into_a_register(si, sit, t-1, g_temp_r-1);
        t--;

        break;
      case SI_OP_USE_REGISTER:
        ta = add_tac();
        if (ta == NULL)
          return FAILED;

        ta->op = TAC_OP_ASSIGNMENT;
        tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
        tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, (int)v[t-1], NULL);
        
        _turn_stack_item_into_a_register(si, sit, t-1, g_temp_r-1);
        t--;
        break;
      case SI_OP_MULTIPLY:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Multiply is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_MUL, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_COMPARE_LT:
        if (_add_comparison(si, sit, v, t, TAC_OP_JUMP_LT) == FAILED)
          return FAILED;
        t--;
        break;
      case SI_OP_COMPARE_GT:
        if (_add_comparison(si, sit, v, t, TAC_OP_JUMP_GT) == FAILED)
          return FAILED;
        t--;
        break;
      case SI_OP_COMPARE_LTE:
        if (_add_comparison(si, sit, v, t, TAC_OP_JUMP_LTE) == FAILED)
          return FAILED;
        t--;
        break;
      case SI_OP_COMPARE_GTE:
        if (_add_comparison(si, sit, v, t, TAC_OP_JUMP_GTE) == FAILED)
          return FAILED;
        t--;
        break;
      case SI_OP_COMPARE_EQ:
        if (_add_comparison(si, sit, v, t, TAC_OP_JUMP_EQ) == FAILED)
          return FAILED;
        t--;
        break;
      case SI_OP_COMPARE_NEQ:
        if (_add_comparison(si, sit, v, t, TAC_OP_JUMP_NEQ) == FAILED)
          return FAILED;
        t--;
        break;
      case SI_OP_LOGICAL_OR:
        {
          int label_true = ++g_temp_label_id, label_exit = ++g_temp_label_id;
          int tresult;

          /* load r1 */
          r1 = _load_to_register(si, v, t-1);

          tresult = g_temp_r++;
          
          /* load 0 */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 0, NULL);

          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_JUMP_NEQ;
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, r1, NULL);
          tac_set_arg2(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_result(ta, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(label_true));

          /* load r2 */
          r2 = _load_to_register(si, v, t-2);

          ta = add_tac();
          if (ta == NULL)
            return FAILED;
          
          ta->op = TAC_OP_JUMP_NEQ;
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, r2, NULL);
          tac_set_arg2(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_result(ta, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(label_true));

          /* jump to exit */
          add_tac_jump(generate_temp_label(label_exit));

          /* label true */
          add_tac_label(generate_temp_label(label_true));
          
          /* load 1 */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 1, NULL);
          
          /* label exit */
          add_tac_label(generate_temp_label(label_exit));

          /* load tresult to a new register */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          
          _turn_stack_item_into_a_register(si, sit, t-1, (int)ta->result_d);

          t--;
        }
        break;
      case SI_OP_LOGICAL_AND:
        {
          int label_false = ++g_temp_label_id;
          int tresult;

          /* load r1 */
          r1 = _load_to_register(si, v, t-1);

          tresult = g_temp_r++;
          
          /* load 0 */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 0, NULL);

          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_JUMP_EQ;
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, r1, NULL);
          tac_set_arg2(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_result(ta, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(label_false));

          /* load r2 */
          r2 = _load_to_register(si, v, t-2);

          ta = add_tac();
          if (ta == NULL)
            return FAILED;
          
          ta->op = TAC_OP_JUMP_EQ;
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, r2, NULL);
          tac_set_arg2(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_result(ta, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(label_false));

          /* load 1 */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 1, NULL);
          
          /* label false */
          add_tac_label(generate_temp_label(label_false));

          /* load tresult to a new register */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, g_temp_r++, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          
          _turn_stack_item_into_a_register(si, sit, t-1, (int)ta->result_d);

          t--;
        }
        break;
      case SI_OP_LOW_BYTE:
        fprintf(stderr, "IMPLEMENT ME!\n");
        /*
        z = (int)v[t - 1];
        v[t - 1] = z & 0xFF;
        */
        break;
      case SI_OP_HIGH_BYTE:
        fprintf(stderr, "IMPLEMENT ME!\n");
        /*
        z = (int)v[t - 1];
        v[t - 1] = (z>>8) & 0xFF;
        */
        break;
      case SI_OP_NOT:
        {
          int label_zero = ++g_temp_label_id, label_non_zero = ++g_temp_label_id;
          int tresult;
          
          r1 = _load_to_register(si, v, t-1);

          tresult = g_temp_r++;
          
          /* load 0 */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 0, NULL);

          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_JUMP_NEQ;
          tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, r1, NULL);
          tac_set_arg2(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_result(ta, TAC_ARG_TYPE_LABEL, 0, generate_temp_label(label_non_zero));

          /* label zero */
          add_tac_label(generate_temp_label(label_zero));

          /* load 1 */
          ta = add_tac();
          if (ta == NULL)
            return FAILED;

          ta->op = TAC_OP_ASSIGNMENT;
          tac_set_result(ta, TAC_ARG_TYPE_TEMP, tresult, NULL);
          tac_set_arg1(ta, TAC_ARG_TYPE_CONSTANT, 1, NULL);

          /* label non-zero */
          add_tac_label(generate_temp_label(label_non_zero));

          _turn_stack_item_into_a_register(si, sit, t-1, tresult);
        }
        break;
      case SI_OP_XOR:
        fprintf(stderr, "IMPLEMENT ME!\n");
        /*
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: XOR is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = (int)v[t - 1] ^ (int)v[t - 2];
        t--;
        */
        break;
      case SI_OP_OR:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: OR is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_OR, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_AND:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: AND is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_AND, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_MODULO:
        if (((int)v[t - 1]) == 0) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Modulo by zero.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Modulo is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_MOD, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_DIVIDE:
        if (v[t - 1] == 0.0) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Division by zero.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Division is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_DIV, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_POWER:
        fprintf(stderr, "IMPLEMENT ME!\n");
        /*
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Power is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }
        v[t - 2] = pow(v[t - 2], v[t - 1]);
        t--;
        */
        break;
      case SI_OP_SHIFT_LEFT:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Shift left is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_SHIFT_LEFT, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      case SI_OP_SHIFT_RIGHT:
        if (t <= 1) {
          fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Shift right is missing an operand.\n", get_file_name(sta->filename_id), sta->linenumber);
          return FAILED;
        }

        ta = _add_tac_calculation(TAC_OP_SHIFT_RIGHT, t-2, t-1, g_temp_r++, si, v);
        _turn_stack_item_into_a_register(si, sit, t-2, (int)ta->result_d);
        t--;
        break;
      default:
        fprintf(stderr, "%s:%d: IL_COMPUTE_STACK: Unhandled internal operation %d! Please submit a bug report!\n", get_file_name(sta->filename_id), sta->linenumber, (int)s->value);
        break;
      }
    }
  }

  /* no operations were made and we have a value in the stack? */
  /*
  if (rresult == g_temp_r - 1 && t == 1)
    r1 = _load_to_register(si, v, t-1);
  */

  /* move the result to the result register */
  ta = add_tac();

  if (ta == NULL)
    return FAILED;

  ta->op = TAC_OP_ASSIGNMENT;

  tac_set_result(ta, TAC_ARG_TYPE_TEMP, rresult, NULL);
  tac_set_arg1(ta, TAC_ARG_TYPE_TEMP, g_temp_r - 1, NULL);

  return SUCCEEDED;
}
