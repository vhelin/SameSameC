#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

struct next_use_test_context {
  char active[6];
  char reads[6];
  char writes[6];
};

static int is_active(void *context, int instruction_index, int temp_index) {

  struct next_use_test_context *test_context;

  (void)temp_index;
  test_context = (struct next_use_test_context *)context;
  return test_context->active[instruction_index];
}

static int reads_temp(void *context, int instruction_index, int temp_index) {

  struct next_use_test_context *test_context;

  (void)temp_index;
  test_context = (struct next_use_test_context *)context;
  return test_context->reads[instruction_index];
}

static int writes_temp(void *context, int instruction_index, int temp_index) {

  struct next_use_test_context *test_context;

  (void)temp_index;
  test_context = (struct next_use_test_context *)context;
  return test_context->writes[instruction_index];
}

int main(void) {

  struct next_use_test_context context = {
    { YES, NO, YES, YES, YES, YES },
    { NO, YES, NO, YES, NO, YES },
    { NO, NO, NO, YES, YES, NO }
  };

  if (register_allocator_find_next_use("coreFound", 2, 0, 5, 6, &context, is_active, reads_temp, writes_temp) != 3)
    return 1;
  context.writes[2] = YES;
  if (register_allocator_find_next_use("coreRedefined", 2, 0, 2, 6, &context, is_active, reads_temp, writes_temp) != -1)
    return 2;
  if (register_allocator_find_next_use("coreBlockEnd", 2, 0, 0, 6, &context, is_active, reads_temp, writes_temp) != -1)
    return 3;
  if (register_allocator_find_next_use("coreEmpty", 2, 6, 5, 6, &context, is_active, reads_temp, writes_temp) != -1)
    return 4;
  if (register_allocator_find_next_use("coreInvalid", 2, 7, 7, 6, &context, is_active, reads_temp, writes_temp) != -1)
    return 5;

  return 0;
}
