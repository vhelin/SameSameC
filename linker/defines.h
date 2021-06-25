
#ifndef _DEFINES_H
#define _DEFINES_H

#define TARGET_NONE 0
#define TARGET_SMS  1

#ifndef NDEBUG
  #define BILIBALI_DEBUG 1
#endif

#ifdef _CRT_SECURE_NO_WARNINGS
  #pragma warning(disable:4996) /* Just in case */
#endif

#define FAILED    0
#define SUCCEEDED 1

#define OFF 0
#define ON  1

#define NO  0
#define YES 1

#define TRUE  1
#define FALSE 0

#define MAX_NAME_LENGTH 256

struct file {
  char name[MAX_NAME_LENGTH+1];
  char name_no_extension[MAX_NAME_LENGTH+1];
  char global_variables_init_function[MAX_NAME_LENGTH+1];
  char *data;
  int  size;
  int  position_sections;
  int  position_ramsections;
  struct file *next;
};

#endif /* _DEFINES_H */
