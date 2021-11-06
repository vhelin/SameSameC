
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"

#include "parse.h"
#include "stack.h"
#include "include_file.h"
#include "printf.h"
#include "main.h"
#include "definition.h"



int g_input_number_error_msg = YES, g_ss, g_string_size, g_input_float_mode = OFF, g_parse_floats = YES, g_expect_calculations = YES;
int g_newline_beginning = ON, g_parsed_double_decimal_numbers = 0, g_operand_hint, g_operand_hint_type, g_parsed_int, g_get_next_token_return_linefeed = NO;
char g_label[MAX_NAME_LENGTH + 1], g_xyz[512];
double g_parsed_double;
int g_source_pointer;

extern int g_size;
extern char *g_buffer, g_tmp[4096];



int is_string_ending_with(char *s, char *e) {

  int length_s, length_e, k;
  
  if (s == NULL || e == NULL)
    return -1;

  length_s = (int)strlen(s);
  length_e = (int)strlen(e);

  if (length_e > length_s)
    return -1;

  for (k = 0; k < length_e; k++) {
    if (s[length_s - length_e + k] != e[k])
      return -1;
  }

  return 1;
}


int compare_next_token(char *token) {

  int ii, t, length;
  char e;
  
  length = (int)strlen(token);

  /* skip white space */
  ii = g_source_pointer;
  for (e = g_buffer[ii]; e == ' ' || e == 0x0A; e = g_buffer[++ii])
    ;

  for (t = 0; t < length && e != ' ' && e != 0x0A; ) {
    if (toupper((int)token[t]) != toupper((int)e))
      return FAILED;
    t++;
    e = g_buffer[++ii];
  }

  if (t == length)
    return SUCCEEDED;

  return FAILED;
}


void skip_whitespace(void) {

  while (1) {
    if (g_source_pointer == g_size)
      break;
    if (g_buffer[g_source_pointer] == '\\' && g_buffer[g_source_pointer+1] == 0xA) {
      g_source_pointer += 2;
      next_line();
      continue;
    }
    if (g_buffer[g_source_pointer] == ' ') {
      g_source_pointer++;
      g_newline_beginning = OFF;
      continue;
    }
    if (g_buffer[g_source_pointer] == 0xA) {
      if (g_get_next_token_return_linefeed == YES)
        break;
      g_source_pointer++;
      next_line();
      continue;
    }
    break;
  }
}


int is_char_a_symbol(char c) {

  if (c == '=' || c == '>' || c == '<' || c == '!' || c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')' || c == '&' || c == '|' || c == ';' || c == ':' || c == '~' || c == '^' || c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == '.')
    return YES;
  else
    return NO;
}


int is_char_a_letter(char c) {

  if (c == '_' || c == '@')
    return YES;
  if (c >= 'a' && c <= 'z')
    return YES;
  if (c >= 'A' && c <= 'Z')
    return YES;

  return NO;
}


int is_char_a_number(char c) {

  if (c >= '0' && c <= '9')
    return YES;

  return NO;
}


int get_next_token(void) {

  double decimal_mul;
  int k, q;
  char c;
  
  skip_whitespace();

  if (g_buffer[g_source_pointer] == 0xA) {
    if (g_get_next_token_return_linefeed == YES) {
      g_source_pointer++;
      next_line();

      return GET_NEXT_TOKEN_LINEFEED;
    }
  }

  c = g_buffer[g_source_pointer];
  
  /* "string"? */
  if (c == '"') {
    for (g_ss = 0, g_source_pointer++; g_buffer[g_source_pointer] != 0xA && g_buffer[g_source_pointer] != '"'; ) {
      if (g_buffer[g_source_pointer] == '\\' && g_buffer[g_source_pointer + 1] == '"') {
        g_tmp[g_ss++] = '"';
        g_source_pointer += 2;
      }
      else
        g_tmp[g_ss++] = g_buffer[g_source_pointer++];
    }

    if (g_buffer[g_source_pointer] == 0xA)
      return print_error("GET_NEXT_TOKEN: String wasn't terminated properly.\n", ERROR_NONE);

    g_tmp[g_ss] = 0;
    g_source_pointer++;

    return GET_NEXT_TOKEN_STRING_DATA;
  }

  /* a symbol? */
  if (is_char_a_symbol(c) == YES) {
    char c2 = g_buffer[g_source_pointer + 1];
    char c3 = g_buffer[g_source_pointer + 2];

    g_source_pointer++;

    /* a three char symbol? */
    if (is_char_a_symbol(c2) && is_char_a_symbol(c3)) {
      /* possibly -> check all combinations */
      if ((c == '<' && c2 == '<' && c3 == '=') ||
          (c == '>' && c2 == '>' && c3 == '=')) {
        g_ss = 3;
        g_tmp[0] = c;
        g_tmp[1] = c2;
        g_tmp[2] = c3;
        g_tmp[3] = 0;

        g_source_pointer += 2;
        
        return GET_NEXT_TOKEN_SYMBOL;
      }
    }
    /* a two char symbol? */
    if (is_char_a_symbol(c2)) {
      /* possibly -> check all combinations */
      if ((c == '&' && c2 == '&') ||
          (c == '|' && c2 == '|') ||
          (c == '<' && c2 == '=') ||
          (c == '>' && c2 == '=') ||
          (c == '=' && c2 == '=') ||
          (c == '!' && c2 == '=') ||
          (c == '+' && c2 == '+') ||
          (c == '-' && c2 == '-') ||
          (c == '<' && c2 == '<') ||
          (c == '>' && c2 == '>') ||
          (c == '+' && c2 == '=') ||
          (c == '-' && c2 == '=') ||
          (c == '*' && c2 == '=') ||
          (c == '/' && c2 == '=') ||
          (c == '|' && c2 == '=') ||
          (c == '&' && c2 == '=') ||
          (c == '-' && c2 == '>')) {
        g_ss = 2;
        g_tmp[0] = c;
        g_tmp[1] = c2;
        g_tmp[2] = 0;

        g_source_pointer++;
        
        return GET_NEXT_TOKEN_SYMBOL;
      }
    }
    
    g_ss = 1;
    g_tmp[0] = c;
    g_tmp[1] = 0;

    return GET_NEXT_TOKEN_SYMBOL;
  }

  /* a string? */
  if (is_char_a_letter(c) == YES || c == '#') {
    g_tmp[0] = c;
    g_source_pointer++;
    for (g_ss = 1; is_char_a_letter(g_buffer[g_source_pointer]) == YES || is_char_a_number(g_buffer[g_source_pointer]) == YES; g_source_pointer++, g_ss++)
      g_tmp[g_ss] = g_buffer[g_source_pointer];
    g_tmp[g_ss] = 0;

    if (g_ss >= MAX_NAME_LENGTH)
      return print_error("GET_NEXT_TOKEN: The string is too long...\n", ERROR_NONE);

    return GET_NEXT_TOKEN_STRING;
  }

  /* is it a hexadecimal value? */
  g_parsed_int = 0;
  if (c >= '0' && c <= '9') {    
    for (k = 0; 1; k++) {
      if (g_buffer[g_source_pointer+k] >= '0' && g_buffer[g_source_pointer+k] <= '9')
        continue;
      if (g_buffer[g_source_pointer+k] >= 'a' && g_buffer[g_source_pointer+k] <= 'f')
        continue;
      if (g_buffer[g_source_pointer+k] >= 'A' && g_buffer[g_source_pointer+k] <= 'F')
        continue;
      if (g_buffer[g_source_pointer+k] == 'h' || g_buffer[g_source_pointer+k] == 'H') {
        g_parsed_int = 1;
        break;
      }
      break;
    }
  }

  if (c == '0' && g_buffer[g_source_pointer+1] == 'x') {
    g_source_pointer += 2;
    g_parsed_int = 1;
  }

  if (c == '$' || g_parsed_int == 1) {
    g_source_pointer++;
    if (g_parsed_int == 1)
      g_source_pointer--;
    for (g_parsed_int = 0, k = 0; k < 8; k++, g_source_pointer++) {
      c = g_buffer[g_source_pointer];
      if (c >= '0' && c <= '9')
        g_parsed_int = (g_parsed_int << 4) + c - '0';
      else if (c >= 'A' && c <= 'F')
        g_parsed_int = (g_parsed_int << 4) + c - 'A' + 10;
      else if (c >= 'a' && c <= 'f')
        g_parsed_int = (g_parsed_int << 4) + c - 'a' + 10;
      else if (c == 'h' || c == 'H') {
        g_source_pointer++;
        c = g_buffer[g_source_pointer];
        break;
      }
      else
        break;
    }

    g_parsed_double = (double)g_parsed_int;
    
    return GET_NEXT_TOKEN_INT;
  }

  if (c == '%' || (c == '0' && g_buffer[g_source_pointer+1] == 'b')) {
    if (c == '%')
      g_source_pointer++;
    else
      g_source_pointer += 2;
    for (g_parsed_int = 0, k = 0; k < 32; k++, g_source_pointer++) {
      c = g_buffer[g_source_pointer];
      if (c == '0' || c == '1')
        g_parsed_int = (g_parsed_int << 1) + c - '0';
      else
        break;
    }

    g_parsed_double = (double)g_parsed_int;

    return GET_NEXT_TOKEN_INT;
  }

  if (c >= '0' && c <= '9') {
    int max_digits = 10;
    
    /* we are parsing decimals when q == 1 */
    q = 0;
    g_parsed_double = 0;
    g_parsed_double_decimal_numbers = 0;
    decimal_mul = 0.1;
    for (k = 0; k < max_digits; k++, g_source_pointer++) {
      c = g_buffer[g_source_pointer];
      if (c >= '0' && c <= '9') {
        if (k == max_digits - 1) {
          if (q == 0)
            print_error("Too many digits in the integer value. Max 10 is supported.\n", ERROR_NUM);
          else {
            snprintf(g_xyz, sizeof(g_xyz), "Too many digits in the floating point value. Max %d is supported.\n", MAX_FLOAT_DIGITS);
            print_error(g_xyz, ERROR_NUM);
          }
          return FAILED;
        }
        
        if (q == 0) {
          /* still parsing an integer */
          g_parsed_double = g_parsed_double*10 + c-'0';
        }
        else {
          g_parsed_double = g_parsed_double + decimal_mul*(c-'0');
          decimal_mul /= 10.0;
          g_parsed_double_decimal_numbers = g_parsed_double_decimal_numbers*10 + (c-'0');
        }
      }
      else if (c == '.') {
        if (q == 1)
          return print_error("Syntax error.\n", ERROR_NUM);

        c = g_buffer[g_source_pointer+1];
        if (c >= '0' && c <= '9') {
          /* float mode, read decimals */
          if (g_parse_floats == NO)
            break;
          q = 1;
          max_digits = MAX_FLOAT_DIGITS+1;
        }
      }
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        /* a number directly followed by a letter when parsing a integer/float -> syntax error */
        return print_error("Syntax error.\n", ERROR_NUM);
      }
      else
        break;
    }

    /* drop the decimals */
    g_parsed_int = (int)g_parsed_double;

    if (q == 1)
      return GET_NEXT_TOKEN_DOUBLE;

    return GET_NEXT_TOKEN_INT;
  }

  if (c == '\'') {
    g_source_pointer++;
    g_parsed_int = g_buffer[g_source_pointer++];
    c = g_buffer[g_source_pointer];
    if (c != '\'') {
      if (g_input_number_error_msg == YES) {
        snprintf(g_xyz, sizeof(g_xyz), "Got '%c' (%d) when expected \"'\".\n", c, c);
        print_error(g_xyz, ERROR_NUM);
      }
      return FAILED;
    }
    g_source_pointer++;

    g_parsed_double = (double)g_parsed_int;
    
    return GET_NEXT_TOKEN_INT;
  }  

  return FAILED;
}
