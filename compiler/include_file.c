
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"

#include "include_file.h"
#include "parse.h"
#include "printf.h"
#include "main.h"
#include "definition.h"
#include "source_line_manager.h"


extern int g_ind, g_source_pointer, g_extra_definitions, g_parsed_int, g_use_incdir;
extern char g_tmp[4096], g_error_message[sizeof(g_tmp) + MAX_NAME_LENGTH + 1 + 1024];
extern struct ext_include_collection g_ext_incdirs;
extern FILE *g_file_out_ptr;

struct incbin_file_data *g_incbin_file_data_first = NULL, *g_ifd_tmp;
struct active_file_info *g_active_file_info_first = NULL, *g_active_file_info_last = NULL, *g_active_file_info_tmp = NULL;
struct file_name_info *g_file_name_info_first = NULL, *g_file_name_info_last = NULL, *g_file_name_info_tmp;
char *g_include_in_tmp = NULL, *g_tmp_a = NULL;
int g_include_in_tmp_size = 0, g_tmp_a_size = 0, g_file_name_id = 1, g_open_files = 0;

char *g_full_name = NULL;
int g_full_name_size = 0;

char *g_include_dir = NULL;
int g_include_dir_size = 0;

char *g_buffer = NULL;
int g_size = 0;


int create_full_name(char *dir, char *name) {

  char *tmp;
  int i;

  
  if (dir == NULL && name == NULL)
    return SUCCEEDED;

  /* compute the length of the new string */
  i = 0;
  if (dir != NULL)
    i += (int)strlen(dir);
  if (name != NULL)
    i += (int)strlen(name);
  i++;

  if (i > g_full_name_size) {
    tmp = realloc(g_full_name, i);
    if (tmp == NULL) {
      fprintf(stderr, "CREATE_FULL_NAME: Out of memory error.\n");
      return FAILED;
    }
    g_full_name = tmp;
    g_full_name_size = i;
  }

  if (dir != NULL) {
    strcpy(g_full_name, dir);
    if (name != NULL)
      strcat(g_full_name, name);
  }
  else
    strcpy(g_full_name, name);

  return SUCCEEDED;
}


int try_open_file(char* directory, char* partial_name, FILE** out_result) {

  int error_code = create_full_name(directory, partial_name);

  if (error_code != SUCCEEDED)
    return error_code;
  
  *out_result = fopen(g_full_name, "rb");
  if (*out_result == NULL)
    return FAILED;

  return SUCCEEDED;
}


static void print_find_error(char* name) {

  int index;

  if (g_active_file_info_last)
    fprintf(stderr, "%s:%d: ", get_file_name(g_active_file_info_last->filename_id), g_active_file_info_last->line_current);

  fprintf(stderr, "FIND_FILE: Could not open \"%s\", searched in the following directories:\n", name);

  if (g_use_incdir == YES) {
    for (index = 0; index < g_ext_incdirs.count; index++) {
      fprintf(stderr, "%s\n", g_ext_incdirs.names[index]);
    }
  }

  if (g_include_dir != NULL) {
    fprintf(stderr, "%s\n", g_include_dir);
  }

  fprintf(stderr, "./ (current directory)\n");
}


static int find_file(char *name, FILE** f) {

  int index;

  if (g_use_incdir == YES) {
    /* check all external include directories first */
    for (index = 0; index < g_ext_incdirs.count; index++) {
      try_open_file(g_ext_incdirs.names[index], name, f);

      /* we succeeded and found a valid file, so escape */
      if (*f != NULL)
        return SUCCEEDED;
    }
  }

  /* fall through to g_include_dir if we either didn't check, or failed to find the file in g_ext_incdirs */
  try_open_file(g_include_dir, name, f);
  if (*f != NULL)
    return SUCCEEDED;

  /* if failed try to find the file in the current directory */
  (*f) = fopen(name, "rb");
  if (*f != NULL)
    return SUCCEEDED;

  print_find_error(name);

  return FAILED;
}


int include_file(char *name, int *include_size, char *namespace) {

  int file_size, id, change_file_buffer_size, inz;
  char *tmp_b, *n, change_file_buffer[MAX_NAME_LENGTH * 2];
  FILE *f = NULL;
  int error_code = find_file(name, &f);

  if (error_code != SUCCEEDED)
    return error_code;

  fseek(f, 0, SEEK_END);
  file_size = (int)ftell(f);
  fseek(f, 0, SEEK_SET);

  /* name */
  g_file_name_info_tmp = g_file_name_info_first;
  id = 0;

  /* NOTE: every filename, even included multiple times, is unique
     while (g_file_name_info_tmp != NULL) {
     if (strcmp(g_file_name_info_tmp->name, g_full_name) == 0) {
     id = g_file_name_info_tmp->id;
     break;
     }
     g_file_name_info_tmp = g_file_name_info_tmp->next;
     }
  */

  if (id == 0) {
    g_file_name_info_tmp = calloc(sizeof(struct file_name_info), 1);
    n = calloc(strlen(g_full_name)+1, 1);
    if (g_file_name_info_tmp == NULL || n == NULL) {
      if (g_file_name_info_tmp != NULL)
        free(g_file_name_info_tmp);
      if (n != NULL)
        free(n);
      snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying allocate info structure for file \"%s\".\n", g_full_name);
      print_error(g_error_message, ERROR_INC);
      return FAILED;
    }
    g_file_name_info_tmp->next = NULL;

    if (g_file_name_info_first == NULL) {
      g_file_name_info_first = g_file_name_info_tmp;
      g_file_name_info_last = g_file_name_info_tmp;
    }
    else {
      g_file_name_info_last->next = g_file_name_info_tmp;
      g_file_name_info_last = g_file_name_info_tmp;
    }

    strcpy(n, g_full_name);
    g_file_name_info_tmp->name = n;
    g_file_name_info_tmp->id = g_file_name_id;
    id = g_file_name_id;
    g_file_name_id++;

    /* load the source file for later copying the original lines into ASM output */
    load_source_file(g_file_name_info_tmp->name, g_file_name_info_tmp->id);
  }

  if (namespace == NULL || namespace[0] == 0)
    snprintf(change_file_buffer, sizeof(change_file_buffer), "%c.CHANGEFILE %d NONAMESPACE%c", 0xA, id, 0xA);
  else
    snprintf(change_file_buffer, sizeof(change_file_buffer), "%c.CHANGEFILE %d NAMESPACE %s%c", 0xA, id, namespace, 0xA);
  change_file_buffer_size = (int)strlen(change_file_buffer);

  /* reallocate buffer */
  if (g_include_in_tmp_size < file_size) {
    if (g_include_in_tmp != NULL)
      free(g_include_in_tmp);

    g_include_in_tmp = calloc(sizeof(char) * file_size, 1);
    if (g_include_in_tmp == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to allocate room for \"%s\".\n", g_full_name);
      print_error(g_error_message, ERROR_INC);
      return FAILED;
    }

    g_include_in_tmp_size = file_size;
  }

  /* read the whole file into a buffer */
  fread(g_include_in_tmp, 1, file_size, f);
  fclose(f);

  if (g_size == 0) {
    g_buffer = calloc(sizeof(char) * (change_file_buffer_size + (file_size + 4)), 1);
    if (g_buffer == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to allocate room for \"%s\".\n", g_full_name);
      print_error(g_error_message, ERROR_INC);
      return FAILED;
    }

    memcpy(g_buffer, change_file_buffer, change_file_buffer_size);
    
    /* preprocess */
    preprocess_file(g_include_in_tmp, g_include_in_tmp + file_size, &g_buffer[change_file_buffer_size], &g_size, g_full_name);
    g_size += change_file_buffer_size;

    g_buffer[g_size++] = 0xA;
    g_buffer[g_size++] = '.';
    g_buffer[g_size++] = 'E';
    g_buffer[g_size++] = ' ';

    *include_size = g_size;

    return SUCCEEDED;
  }

  tmp_b = calloc(sizeof(char) * (g_size + change_file_buffer_size + file_size + 4), 1);
  if (tmp_b == NULL) {
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while trying to expand the project to incorporate file \"%s\".\n", g_full_name);
    print_error(g_error_message, ERROR_INC);
    return FAILED;
  }

  /* reallocate g_tmp_a */
  if (g_tmp_a_size < change_file_buffer_size + file_size + 4) {
    if (g_tmp_a != NULL)
      free(g_tmp_a);

    g_tmp_a = calloc(sizeof(char) * (change_file_buffer_size + file_size + 4), 1);
    if (g_tmp_a == NULL) {
      snprintf(g_error_message, sizeof(g_error_message), "Out of memory while allocating new room for \"%s\".\n", g_full_name);
      print_error(g_error_message, ERROR_INC);
      free(tmp_b);
      return FAILED;
    }

    g_tmp_a_size = change_file_buffer_size + file_size + 4;
  }

  memcpy(g_tmp_a, change_file_buffer, change_file_buffer_size);
      
  /* preprocess */
  inz = 0;
  preprocess_file(g_include_in_tmp, g_include_in_tmp + file_size, &g_tmp_a[change_file_buffer_size], &inz, g_full_name);
  inz += change_file_buffer_size;

  g_tmp_a[inz++] = 0xA;
  g_tmp_a[inz++] = '.';
  g_tmp_a[inz++] = 'E';
  g_tmp_a[inz++] = ' ';

  memcpy(tmp_b, g_buffer, g_source_pointer);
  memcpy(tmp_b + g_source_pointer, g_tmp_a, inz);
  memcpy(tmp_b + g_source_pointer + inz, g_buffer + g_source_pointer, g_size - g_source_pointer);

  free(g_buffer);

  g_size += inz;
  g_buffer = tmp_b;

  *include_size = inz;
  
  return SUCCEEDED;
}


int incbin_file(char *name, int *id, int *swap, int *skip, int *read) {

  struct incbin_file_data *ifd;
  char *in_tmp, *n;
  int file_size, q;
  FILE *f = NULL;
  int error_code = find_file(name, &f);

  if (error_code != SUCCEEDED)
    return error_code;

  fseek(f, 0, SEEK_END);
  file_size = (int)ftell(f);
  fseek(f, 0, SEEK_SET);

  ifd = (struct incbin_file_data *)calloc(sizeof(struct incbin_file_data), 1);
  n = calloc(sizeof(char) * (strlen(g_full_name)+1), 1);
  in_tmp = (char *)calloc(sizeof(char) * file_size, 1);
  if (ifd == NULL || n == NULL || in_tmp == NULL) {
    if (ifd != NULL)
      free(ifd);
    if (n != NULL)
      free(n);
    if (in_tmp != NULL)
      free(in_tmp);
    snprintf(g_error_message, sizeof(g_error_message), "Out of memory while allocating data structure for \"%s\".\n", g_full_name);
    print_error(g_error_message, ERROR_INB);
    fclose(f);
    return FAILED;
  }

  /* read the whole file into a buffer */
  fread(in_tmp, 1, file_size, f);
  fclose(f);

  /* complete the structure */
  ifd->next = NULL;
  ifd->size = file_size;
  strcpy(n, g_full_name);
  ifd->name = n;
  ifd->data = in_tmp;

  /* find the index */
  q = 0;
  if (g_incbin_file_data_first != NULL) {
    g_ifd_tmp = g_incbin_file_data_first;
    while (g_ifd_tmp->next != NULL && strcmp(g_ifd_tmp->name, g_full_name) != 0) {
      g_ifd_tmp = g_ifd_tmp->next;
      q++;
    }
    if (g_ifd_tmp->next == NULL && strcmp(g_ifd_tmp->name, g_full_name) != 0) {
      g_ifd_tmp->next = ifd;
      q++;
    }
  }
  else
    g_incbin_file_data_first = ifd;

  *id = q;
  *skip = 0;
  *read = file_size - *skip;
  *swap = 0;

  return SUCCEEDED;
}


char get_file_name_error[] = "GET_FILE_NAME: Internal error.";


char *get_file_name(int id) {

  struct file_name_info *fni;
  
  fni = g_file_name_info_first;
  while (fni != NULL) {
    if (id == fni->id)
      return fni->name;
    fni = fni->next;
  }

  return get_file_name_error;
}


int print_file_names(void) {

  struct incbin_file_data *ifd;
  struct file_name_info *fni;
 
  fni = g_file_name_info_first;
  ifd = g_incbin_file_data_first;

  /* handle the main file name differently */
  if (fni != NULL) {
    if (fni->next != NULL || ifd != NULL)
      fprintf(stdout, "%s \\\n", fni->name);
    else
      fprintf(stdout, "%s\n", fni->name);
    fni = fni->next;
  }

  /* included files */
  while (fni != NULL) {
    if (fni->next != NULL || ifd != NULL)
      fprintf(stdout, "\t%s \\\n", fni->name);
    else
      fprintf(stdout, "\t%s\n", fni->name);
    fni = fni->next;
  }

  /* incbin files */
  while (ifd != NULL) {
    if (ifd->next != NULL)
      fprintf(stdout, "\t%s \\\n", ifd->name);
    else
      fprintf(stdout, "\t%s\n", ifd->name);
    ifd = ifd->next;
  }

  return SUCCEEDED;
}


/* the mystery preprocessor - touch it and prepare for trouble ;) the preprocessor
   removes as much white space as possible from the source file. this is to make
   the parsing of the file, that follows, simpler. */
int preprocess_file(char *input, char *input_end, char *out_buffer, int *out_size, char *file_name) {

  /* this is set to 1 when the parser finds a non white space symbol on the line it's parsing */
  register int got_chars_on_line = 0;

  /* values for z - z tells us the state of the preprocessor on the line it is processing
     the value of z is 0 at the beginning of a new line, and can only grow: 0 -> 1 -> 2 -> 3
     0 - new line
     1 - 1+ characters on the line
     2 - extra white space removed
     3 - again 1+ characters follow */
  register int z = 0;

  char *output = out_buffer;

  while (input < input_end) {
    switch (*input) {
    case '/':
      if (*(input + 1) == '*') {
        /* remove an ANSI C -style block comment */
        got_chars_on_line = 0;
        input += 2;
        while (got_chars_on_line == 0) {
          for ( ; input < input_end && *input != '/' && *input != 0x0A; input++)
            ;
          if (input >= input_end) {
            snprintf(g_error_message, sizeof(g_error_message), "Comment wasn't terminated properly in file \"%s\".\n", file_name);
            print_error(g_error_message, ERROR_INC);
            return FAILED;
          }
          if (*input == 0x0A) {
            *output = 0x0A;
            output++;
          }
          if (*input == '/' && *(input - 1) == '*') {
            got_chars_on_line = 1;
          }
          input++;
        }
        output--;
        for ( ; output > out_buffer && *output == ' '; output--)
          ;
        if (output < out_buffer)
          output = out_buffer;
        else if (*output != ' ')
          output++;
      }
      else if (*(input + 1) == '/') {
        /* C++ style comment -> clear a commented line */
        input += 2;
        for ( ; input < input_end && *input != 0x0A && *input != 0x0D; input++)
          ;
        output--;
        for ( ; output > out_buffer && *output == ' '; output--)
          ;
        if (output < out_buffer)
          output = out_buffer;
        else if (*output != ' ')
          output++;
      }
      else {
        input++;
        *output = '/';
        output++;
        got_chars_on_line = 1;
      }
      break;
    case ':':
      /* finding a label resets the counters */
      input++;
      *output = ':';
      output++;
      got_chars_on_line = 0;
      break;
    case 0x09:
    case ' ':
      /* remove extra white space */
      input++;
      *output = ' ';
      output++;
      for ( ; input < input_end && (*input == ' ' || *input == 0x09); input++)
        ;
      if (got_chars_on_line == 0 && (*input == '-' || *input == '+')) {
        /* special case - -/--/--- or +/++/+++, but not at the beginning of the line, but after some white space */
        output--;
        for ( ; input < input_end && (*input == '+' || *input == '-'); input++, output++)
          *output = *input;     
      }
      else if (got_chars_on_line == 0 && *input == '_') {
        /* special case - _/__, but not at the beginning of the line, but after some white space */
        output--;
        for ( ; input < input_end && *input == '_'; input++, output++)
          *output = *input;     
      }
      got_chars_on_line = 1;
      if (z == 1)
        z = 2;
      break;
    case 0x0A:
      /* take away white space from the end of the line */
      input++;
      output--;
      for ( ; output > out_buffer && *output == ' '; output--)
        ;
      output++;
      *output = 0x0A;
      output++;
      /* moving on to a new line */
      got_chars_on_line = 0;
      z = 0;
      break;
    case 0x0D:
      input++;
      break;
    case '\'':
      if (*(input + 2) == '\'') {
        *output = '\'';
        input++;
        output++;
        *output = *input;
        input++;
        output++;
        *output = '\'';
        input++;
        output++;
      }
      else {
        *output = '\'';
        input++;
        output++;
      }
      got_chars_on_line = 1;
      break;
    case '"':
      /* don't touch strings */
      *output = '"';
      input++;
      output++;
      got_chars_on_line = 1;
      while (1) {
        for ( ; input < input_end && *input != '"' && *input != 0x0A && *input != 0x0D; ) {
          *output = *input;
          input++;
          output++;
        }

        if (input >= input_end)
          break;
        else if (*input == 0x0A || *input == 0x0D) {
          /* process 0x0A/0x0D as usual, and later when we try to input a string, the parser will fail as 0x0A comes before a " */
          break;
        }
        else if (*input == '"' && *(input - 1) != '\\') {
          *output = '"';
          input++;
          output++;
          break;
        }
        else {
          *output = '"';
          input++;
          output++;
        }
      }
      break;
    case '(':
      *output = '(';
      input++;
      output++;
      for ( ; input < input_end && (*input == ' ' || *input == 0x09); input++)
        ;
      got_chars_on_line = 1;
      break;
    case ')':
      /* go back? */
      if (got_chars_on_line == 1 && *(output - 1) == ' ')
        output--;
      *output = ')';
      input++;
      output++;
      got_chars_on_line = 1;
      break;
    case ',':
    case '+':
    case '-':
      if (got_chars_on_line == 0) {
        for ( ; input < input_end && (*input == '+' || *input == '-'); input++, output++)
          *output = *input;
        got_chars_on_line = 1;
      }
      else {
        /* go back? */
        if ((z == 3 || *input == ',') && *(output - 1) == ' ')
          output--;
        *output = *input;
        input++;
        output++;
        for ( ; input < input_end && (*input == ' ' || *input == 0x09); input++)
          ;
        got_chars_on_line = 1;
      }
      break;
    default:
      *output = *input;
      input++;
      output++;
      got_chars_on_line = 1;

      /* mode changes... */
      if (z == 0)
        z = 1;
      else if (z == 2)
        z = 3;
      break;
    }
  }

  *out_size = (int)(output - out_buffer);

  return SUCCEEDED;
}
