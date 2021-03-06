
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
#include "definitions.h"


int parse_string_length(char *end);

int g_input_number_error_msg = YES, g_ss, g_string_size, g_input_float_mode = OFF, g_parse_floats = YES, g_expect_calculations = YES;
int g_newline_beginning = ON, g_parsed_double_decimal_numbers = 0, g_operand_hint, g_operand_hint_type, g_parsed_int;
char g_label[MAX_NAME_LENGTH + 1], g_xyz[512];
char g_unevaluated_expression[256];
double g_parsed_double;
int g_source_pointer;

extern int g_size;
extern char *g_buffer, g_tmp[4096], g_current_directive[256];
extern struct active_file_info *g_active_file_info_first, *g_active_file_info_last, *g_active_file_info_tmp;
extern struct definition *g_tmp_def;
extern struct map_t *g_defines_map;
extern int g_latest_stack;



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
  for (e = g_buffer[ii]; e == ' ' || e == ',' || e == 0x0A; e = g_buffer[++ii])
    ;

  for (t = 0; t < length && e != ' ' && e != ',' && e != 0x0A; ) {
    if (toupper((int)token[t]) != toupper((int)e))
      return FAILED;
    t++;
    e = g_buffer[++ii];
  }

  if (t == length)
    return SUCCEEDED;

  return FAILED;
}


int input_next_string(void) {

  char e;
  int k;

  
  /* skip white space */
  for (e = g_buffer[g_source_pointer++]; e == ' ' || e == ','; e = g_buffer[g_source_pointer++])
    ;

  if (e == 0x0A)
    return INPUT_NUMBER_EOL;

  /* last choice is a label */
  g_tmp[0] = e;
  for (k = 1; k < MAX_NAME_LENGTH; k++) {
    e = g_buffer[g_source_pointer++];
    if (e == 0x0A || e == ',') {
      g_source_pointer--;
      break;
    }
    else if (e == ' ')
      break;
    g_tmp[k] = e;
  }

  if (k == MAX_NAME_LENGTH) {
    if (g_input_number_error_msg == YES) {
      snprintf(g_xyz, sizeof(g_xyz), "The string is too long (max %d characters allowed).\n", MAX_NAME_LENGTH);
      print_error(g_xyz, ERROR_NUM);
    }
    return FAILED;
  }

  g_tmp[k] = 0;

  return SUCCEEDED;
}


int input_number(void) {

  char label_tmp[MAX_NAME_LENGTH + 1];
  unsigned char e;
  int k, q;
  double decimal_mul;


  /* skip white space */
  for (e = g_buffer[g_source_pointer++]; e == ' ' || e == ','; e = g_buffer[g_source_pointer++])
    ;

  if (e == 0x0A)
    return INPUT_NUMBER_EOL;

#ifdef XXX
  if (g_expect_calculations == YES) {
    /* check the type of the expression */
    p = g_source_pointer;
    ee = e;
    while (ee != 0x0A) {
      if (ee == '{')
        curly_braces++;
      else if (ee == '}')
        curly_braces--;
      /* string / symbol -> no calculating */
      else if (ee == '"' || ee == ',' || (ee == '=' && g_buffer[p] == '=') || (ee == '!' && g_buffer[p] == '='))
        break;
      else if (ee == ' ')
        spaces++;
      else if (curly_braces <= 0 && (ee == '-' || ee == '+' || ee == '*' || ee == '/' || ee == '&' || ee == '|' || ee == '^' ||
                                     ee == '<' || ee == '>' || ee == '#' || ee == '~' || ee == ':')) {
        if (ee == ':' && spaces > 0)
          break;
        
        /* launch stack calculator */
        p = stack_calculate(&g_buffer[g_source_pointer - 1], &g_parsed_int);

        if (p == STACK_CALCULATE_DELAY)
          break;
        else if (p == STACK_RETURN_LABEL)
          return INPUT_NUMBER_ADDRESS_LABEL;
        else
          return p;
      }
      ee = g_buffer[p];
      p++;
    }
  }
#endif
  
  /* is it a hexadecimal value? */
  g_parsed_int = 0;
  if (e >= '0' && e <= '9') {
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

  if (e == '$' || g_parsed_int == 1) {
    if (g_parsed_int == 1)
      g_source_pointer--;
    for (g_parsed_int = 0, k = 0; k < 8; k++, g_source_pointer++) {
      e = g_buffer[g_source_pointer];
      if (e >= '0' && e <= '9')
        g_parsed_int = (g_parsed_int << 4) + e - '0';
      else if (e >= 'A' && e <= 'F')
        g_parsed_int = (g_parsed_int << 4) + e - 'A' + 10;
      else if (e >= 'a' && e <= 'f')
        g_parsed_int = (g_parsed_int << 4) + e - 'a' + 10;
      else if (e == 'h' || e == 'H') {
        g_source_pointer++;
        e = g_buffer[g_source_pointer];
        break;
      }
      else
        break;
    }

    g_parsed_double = (double)g_parsed_int;
    
    return SUCCEEDED;
  }

  if (e >= '0' && e <= '9') {
    int max_digits = 10;
    
    /* we are parsing decimals when q == 1 */
    q = 0;
    g_parsed_double = e-'0';
    g_parsed_double_decimal_numbers = 0;
    decimal_mul = 0.1;
    for (k = 0; k < max_digits; k++, g_source_pointer++) {
      e = g_buffer[g_source_pointer];
      if (e >= '0' && e <= '9') {
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
          g_parsed_double = g_parsed_double*10 + e-'0';
        }
        else {
          g_parsed_double = g_parsed_double + decimal_mul*(e-'0');
          decimal_mul /= 10.0;
          g_parsed_double_decimal_numbers = g_parsed_double_decimal_numbers*10 + (e-'0');
        }
      }
      else if (e == '.') {
        if (q == 1) {
          print_error("Syntax error.\n", ERROR_NUM);
          return FAILED;
        }
        e = g_buffer[g_source_pointer+1];
        if (e >= '0' && e <= '9') {
          /* float mode, read decimals */
          if (g_parse_floats == NO)
            break;
          q = 1;
          max_digits = MAX_FLOAT_DIGITS+1;
        }
      }
      else if ((e >= 'a' && e <= 'z') || (e >= 'A' && e <= 'Z')) {
        /* a number directly followed by a letter when parsing a integer/float -> syntax error */
        print_error("Syntax error.\n", ERROR_NUM);
        return FAILED;
      }
      else
        break;
    }

    /* drop the decimals */
    g_parsed_int = (int)g_parsed_double;

    if (q == 1 && g_input_float_mode == ON)
      return INPUT_NUMBER_FLOAT;

    return SUCCEEDED;
  }

  if (e == '%') {
    for (g_parsed_int = 0, k = 0; k < 32; k++, g_source_pointer++) {
      e = g_buffer[g_source_pointer];
      if (e == '0' || e == '1')
        g_parsed_int = (g_parsed_int << 1) + e - '0';
      else
        break;
    }

    g_parsed_double = (double)g_parsed_int;

    return SUCCEEDED;
  }

  if (e == '\'') {
    g_parsed_int = g_buffer[g_source_pointer++];
    e = g_buffer[g_source_pointer];
    if (e != '\'') {
      if (g_input_number_error_msg == YES) {
        snprintf(g_xyz, sizeof(g_xyz), "Got '%c' (%d) when expected \"'\".\n", e, e);
        print_error(g_xyz, ERROR_NUM);
      }
      return FAILED;
    }
    g_source_pointer++;

    g_parsed_double = (double)g_parsed_int;
    
    return SUCCEEDED;
  }

  if (e == '"' || e == '{') {
    int curly_braces = 0;

    if (e == '{')
      curly_braces++;
    
    for (k = 0; k < MAX_NAME_LENGTH; ) {
      e = g_buffer[g_source_pointer++];

      if (e == '\\' && g_buffer[g_source_pointer] == '"') {
        g_label[k++] = '"';
        g_source_pointer++;
        continue;
      }

      if (e == '{') {
        curly_braces++;
        continue;
      }
      
      if (e == '"' || e == '}') {
        if (e == '}')
          curly_braces--;

        /* check for "string".length */
        if (g_buffer[g_source_pointer+0] == '.' &&
            (g_buffer[g_source_pointer+1] == 'l' || g_buffer[g_source_pointer+1] == 'L') &&
            (g_buffer[g_source_pointer+2] == 'e' || g_buffer[g_source_pointer+2] == 'E') &&
            (g_buffer[g_source_pointer+3] == 'n' || g_buffer[g_source_pointer+3] == 'N') &&
            (g_buffer[g_source_pointer+4] == 'g' || g_buffer[g_source_pointer+4] == 'G') &&
            (g_buffer[g_source_pointer+5] == 't' || g_buffer[g_source_pointer+5] == 'T') &&
            (g_buffer[g_source_pointer+6] == 'h' || g_buffer[g_source_pointer+6] == 'H')) {
          /* yes, we've got it! calculate the length and return the integer */
          g_source_pointer += 7;
          g_label[k] = 0;
          g_parsed_int = (int)get_label_length(g_label);
          g_parsed_double = (double)g_parsed_int;

          return SUCCEEDED;
        }

        if (e == '"')
          break;
        if (e == '}' && curly_braces <= 0)
          break;
      }
      
      if (e == 0 || e == 0x0A) {
        print_error("String wasn't terminated properly.\n", ERROR_NUM);
        return FAILED;
      }

      g_label[k++] = e;
    }

    g_label[k] = 0;

    if (k == MAX_NAME_LENGTH) {
      if (g_input_number_error_msg == YES) {
        snprintf(g_xyz, sizeof(g_xyz), "The string is too long (max %d characters allowed).\n", MAX_NAME_LENGTH);
        print_error(g_xyz, ERROR_NUM);
      }
      return FAILED;
    }

    g_label[k] = 0;
    g_string_size = k;
    
    return INPUT_NUMBER_STRING;
  }

  /* the last choice is a label */
  g_label[0] = e;
  for (k = 1; k < MAX_NAME_LENGTH; k++) {
    e = g_buffer[g_source_pointer++];
    if (e == 0x0A || e == '(' || e == '[' || e == ')' || e == ',' || e == ']') {
      g_source_pointer--;
      break;
    }
    else if (e == ' ')
      break;
    g_label[k] = e;
  }

  if (k == MAX_NAME_LENGTH) {
    if (g_input_number_error_msg == YES) {
      snprintf(g_xyz, sizeof(g_xyz), "The label is too long (max %d characters allowed).\n", MAX_NAME_LENGTH);
      print_error(g_xyz, ERROR_NUM);
    }
    return FAILED;
  }

  g_label[k] = 0;

  /* label_tmp contains the label without possible prefix ':' */
  if (strlen(g_label) > 1 && g_label[0] == ':')
    strcpy(label_tmp, &g_label[1]);
  else
    strcpy(label_tmp, g_label);

  /* check for "string".length */
  if (strstr(g_label, ".length") != NULL) {
    parse_string_length(strstr(g_label, ".length"));
    return SUCCEEDED;
  }
  else if (strstr(g_label, ".LENGTH") != NULL) {
    parse_string_length(strstr(g_label, ".LENGTH"));
    return SUCCEEDED;
  }

  /* check if the label is actually a definition */
  definition_get(g_label, &g_tmp_def);
  if (g_tmp_def != NULL) {
    if (g_tmp_def->type == DEFINITION_TYPE_VALUE) {
      g_parsed_int = (int)g_tmp_def->value;
      g_parsed_double = (double)g_parsed_int;

      return SUCCEEDED;
    }
    else if (g_tmp_def->type == DEFINITION_TYPE_STACK) {
      /* wrap the referenced, existing stack calculation inside a new stack calculation as stack
         calculation contains a write. the 2nd, 3rd etc. reference don't do anything by themselves.
         but wrapping creates a new stack calculation that also makes a write */
      stack_create_stack_stack((int)g_tmp_def->value);
      return INPUT_NUMBER_STACK;
    }
    else if (g_tmp_def->type == DEFINITION_TYPE_ADDRESS_LABEL) {
      if (g_label[0] == ':') {
        /* we need to keep the ':' prefix */
        if (strlen(g_tmp_def->string) >= MAX_NAME_LENGTH-1) {
          if (g_input_number_error_msg == YES) {
            snprintf(g_xyz, sizeof(g_xyz), "The label is too long (max %d characters allowed).\n", MAX_NAME_LENGTH);
            print_error(g_xyz, ERROR_NUM);
          }
          return FAILED;          
        }
        snprintf(g_label, sizeof(g_label), ":%.254s", g_tmp_def->string);
        g_string_size = g_tmp_def->size + 1;
      }
      else {
        g_string_size = g_tmp_def->size;
        memcpy(g_label, g_tmp_def->string, g_string_size);
        g_label[g_string_size] = 0;
      }
      return INPUT_NUMBER_ADDRESS_LABEL;
    }
    else {
      g_string_size = g_tmp_def->size;
      memcpy(g_label, g_tmp_def->string, g_string_size);
      g_label[g_string_size] = 0;
      
      return INPUT_NUMBER_STRING;
    }
  }

  return INPUT_NUMBER_ADDRESS_LABEL;
}


int parse_string_length(char *end) {

  /* remove ".length" from the end of label (end points to inside of label) */
  end[0] = 0;

  /* check if the label is actually a definition - it should be or else we'll give an error */
  definition_get(g_label, &g_tmp_def);
  
  if (g_tmp_def != NULL) {
    if (g_tmp_def->type == DEFINITION_TYPE_VALUE) {
      if (g_input_number_error_msg == YES) {
        print_error(".length of a value does not make any sense.\n", ERROR_NUM);
      }
      return FAILED;
    }
    else if (g_tmp_def->type == DEFINITION_TYPE_STACK) {
      if (g_input_number_error_msg == YES) {
        print_error(".length of a pending computation does not make any sense.\n", ERROR_NUM);
      }
      return FAILED;
    }
    else if (g_tmp_def->type == DEFINITION_TYPE_ADDRESS_LABEL) {
      if (g_input_number_error_msg == YES) {
        print_error(".length of an address label does not make any sense.\n", ERROR_NUM);
      }
      return FAILED;
    }
    else {
      g_string_size = g_tmp_def->size;
      memcpy(g_label, g_tmp_def->string, g_string_size);
      g_label[g_string_size] = 0;

      g_parsed_int = (int)strlen(g_label);
      g_parsed_double = (double)g_parsed_int;
          
      return SUCCEEDED;
    }
  }

  return FAILED;
}


void skip_whitespace(void) {

  while (1) {
    if (g_source_pointer == g_size)
      break;
    if (g_buffer[g_source_pointer] == ' ' || g_buffer[g_source_pointer] == ',') {
      g_source_pointer++;
      g_newline_beginning = OFF;
      continue;
    }
    if (g_buffer[g_source_pointer] == 0xA) {
      g_source_pointer++;
      next_line();
      continue;
    }
    break;
  }
}


int get_next_plain_string(void) {

  char c;
  
  skip_whitespace();

  g_ss = 0;
  while (1) {
    if (g_ss >= MAX_NAME_LENGTH) {
      print_error("GET_NEXT_PLAIN_STRING: Too long for a string.\n", ERROR_NONE);
      return FAILED;
    }

    c = g_buffer[g_source_pointer];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '\\' || c == '@' || c == ':') {
      g_tmp[g_ss] = c;
      g_ss++;
      g_source_pointer++;
    }
    else
      break;
  }

  g_tmp[g_ss] = 0;

  return SUCCEEDED;
}


int is_char_a_symbol(char c) {

  if (c == '=' || c == '>' || c == '<' || c == '!' || c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')' || c == '&' || c == '|' || c == ';' || c == '~' || c == '#' || c == '^' || c == '{' || c == '}' || c == '[' || c == ']')
    return YES;
  else
    return NO;
}


int is_char_a_letter(char c) {

  if (c == '_' || c == '.')
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
  
  g_current_directive[0] = 0;
  
  skip_whitespace();

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

    if (g_buffer[g_source_pointer] == 0xA) {
      print_error("GET_NEXT_TOKEN: String wasn't terminated properly.\n", ERROR_NONE);
      return FAILED;
    }
    g_tmp[g_ss] = 0;
    g_source_pointer++;

    return GET_NEXT_TOKEN_STRING;
  }

  /* a symbol? */
  if (is_char_a_symbol(c) == YES) {
    char c2 = g_buffer[g_source_pointer + 1];

    g_source_pointer++;

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
          (c == '>' && c2 == '>')) {
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
  if (is_char_a_letter(c) == YES) {
    for (g_ss = 0; is_char_a_letter(g_buffer[g_source_pointer]) == YES || is_char_a_number(g_buffer[g_source_pointer]) == YES; g_source_pointer++, g_ss++)
      g_tmp[g_ss] = g_buffer[g_source_pointer];
    g_tmp[g_ss] = 0;

    if (g_ss >= MAX_NAME_LENGTH) {
      print_error("GET_NEXT_TOKEN: The string is too long...\n", ERROR_NONE);
      return FAILED;
    }

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
        if (q == 1) {
          print_error("Syntax error.\n", ERROR_NUM);
          return FAILED;
        }
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
        print_error("Syntax error.\n", ERROR_NUM);
        return FAILED;
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

  if (c == '%') {
    g_source_pointer++;
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
