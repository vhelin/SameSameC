
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"

#include "printf.h"
#include "main.h"
#include "inline_asm_z80.h"
#include "symbol_table.h"


extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern int g_current_filename_id, g_current_line_number;


int inline_asm_z80_parse_line(struct asm_line *al) {

  char *line, variable[MAX_NAME_LENGTH+1], c1, c2, c3;
  struct symbol_table_item *sti;
  int i, j, length, got_parenthesis;

  line = al->line;
  length = strlen(line);

  /* skip whitespace */
  for (i = 0; i < length && (line[i] == ' ' || line[i] == '\t'); i++)
    ;

  /* a read or a write? */
  if (i > length - 3)
    return SUCCEEDED;

  if (toupper(line[i]) == 'L' && toupper(line[i+1] == 'D'))
    i += 2;
  else
    return SUCCEEDED;
  
  /* skip whitespace */
  for (; i < length && (line[i] == ' ' || line[i] == '\t'); i++)
    ;

  /* write? */
  if (i > length - 4)
    return SUCCEEDED;

  c1 = toupper(line[i]);
  c2 = toupper(line[i+1]);
  c3 = toupper(line[i+2]);

  if (c1 == '(' && c2 == '@') {
    /* it's a write! */
    i += 2;

    al->flags |= ASM_LINE_FLAG_WRITE;
  }
  else if ((c1 == 'A' && c2 == ',') ||
           (c1 == 'B' && c2 == ',') ||
           (c1 == 'C' && c2 == ',') ||
           (c1 == 'D' && c2 == ',') ||
           (c1 == 'E' && c2 == ',') ||
           (c1 == 'H' && c2 == ',') ||
           (c1 == 'L' && c2 == ',')) {
    al->cpu_register[0] = c1;
    al->cpu_register[1] = 0;

    i += 2;

    /* skip whitespace */
    for ( ; i < length && (line[i] == ' ' || line[i] == '\t'); i++)
      ;

    if (i > length - 3)
      return SUCCEEDED;

    c1 = toupper(line[i]);
    c2 = toupper(line[i+1]);

    if (c1 == '(' && c2 == '@') {
      /* it's a read! */
      i += 2;
      
      al->flags |= ASM_LINE_FLAG_READ;
    }
    else
      return SUCCEEDED;
  }
  else if ((c1 == 'B' && c2 == 'C' && c3 == ',') ||
           (c1 == 'D' && c2 == 'E' && c3 == ',') ||
           (c1 == 'H' && c2 == 'L' && c3 == ',') ||
           (c1 == 'I' && c2 == 'X' && c3 == ',') ||
           (c1 == 'I' && c2 == 'Y' && c3 == ',')) {
    al->cpu_register[0] = c1;
    al->cpu_register[1] = c2;
    al->cpu_register[2] = 0;

    i += 3;

    /* skip whitespace */
    for ( ; i < length && (line[i] == ' ' || line[i] == '\t'); i++)
      ;

    if (i > length - 3)
      return SUCCEEDED;

    c1 = toupper(line[i]);
    c2 = toupper(line[i+1]);

    if (c1 == '(' && c2 == '@') {
      /* it's a read! */
      i += 2;
      
      al->flags |= ASM_LINE_FLAG_READ;
    }
    else
      return SUCCEEDED;
  }
  
  /* parse the variable name */
  got_parenthesis = NO;
  for (j = 0; j < MAX_NAME_LENGTH && i < length; j++, i++) {
    if (line[i] == ')') {
      i++;
      got_parenthesis = YES;
      break;
    }
    variable[j] = line[i];
  }
  variable[j] = 0;

  if (got_parenthesis == NO) {
    g_current_line_number = al->line_number;
    print_error("inline_asm_z80_parse_line(): Missing closing parenthesis.\n", ERROR_ERR);
    return FAILED;
  }

  if ((al->flags & ASM_LINE_FLAG_WRITE) == ASM_LINE_FLAG_WRITE) {
    /* parse the source register for write */

    /* skip whitespace */
    for ( ; i < length && (line[i] == ' ' || line[i] == '\t'); i++)
      ;

    if (i < length && line[i] != ',') {
      g_current_line_number = al->line_number;
      print_error("inline_asm_z80_parse_line(): Missing comma!\n", ERROR_ERR);
      return FAILED;
    }

    /* skip comma */
    i++;
    
    /* skip whitespace */
    for ( ; i < length && (line[i] == ' ' || line[i] == '\t'); i++)
      ;

    if (i == length) {
      g_current_line_number = al->line_number;
      print_error("inline_asm_z80_parse_line(): Missing source register name!\n", ERROR_ERR);
      return FAILED;
    }

    c1 = toupper(line[i]);
    if (i == length-2)
      c2 = toupper(line[i+1]);

    if (i == length-1 && (c1 == 'A' ||
                          c1 == 'B' ||
                          c1 == 'C' ||
                          c1 == 'D' ||
                          c1 == 'E' ||
                          c1 == 'H' ||
                          c1 == 'L')) {
      al->cpu_register[0] = c1;
      al->cpu_register[1] = 0;
    }
    else if (i == length-2 && ((c1 == 'B' && c2 == 'C') ||
                               (c1 == 'D' && c2 == 'E') ||
                               (c1 == 'H' && c2 == 'L') ||
                               (c1 == 'I' && c2 == 'X') ||
                               (c1 == 'I' && c2 == 'Y'))) {
      al->cpu_register[0] = c1;
      al->cpu_register[1] = c2;
      al->cpu_register[2] = 0;
    }
    else {
      g_current_line_number = al->line_number;
      print_error("inline_asm_z80_parse_line(): Cannot parse source register name!\n", ERROR_ERR);
      return FAILED;
    }
  }
  
  /* find the variable */
  sti = symbol_table_find_symbol(variable);
  if (sti == NULL) {
    g_current_line_number = al->line_number;
    snprintf(g_error_message, sizeof(g_error_message), "inline_asm_z80_parse_line(): Cannot find variable \"%s\"!\n", variable);
    print_error(g_error_message, ERROR_ERR);
    return FAILED;
  }

  al->variable = sti->node;

  /* count reads and writes */
  if ((al->flags & ASM_LINE_FLAG_READ) == ASM_LINE_FLAG_READ)
    al->variable->reads++;
  if ((al->flags & ASM_LINE_FLAG_WRITE) == ASM_LINE_FLAG_WRITE)
    al->variable->writes++;
  
  return SUCCEEDED;
}
