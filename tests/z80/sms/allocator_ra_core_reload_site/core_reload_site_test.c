#include <stddef.h>
#include <stdio.h>

#include "../../../../compiler/defines.h"
#include "../../../../compiler/register_allocator.h"

#define TEST_INSTRUCTION_COUNT 8

struct reload_test_context {
  char active[TEST_INSTRUCTION_COUNT];
  char reads[TEST_INSTRUCTION_COUNT];
  char writes[TEST_INSTRUCTION_COUNT];
  char eligible[TEST_INSTRUCTION_COUNT];
};

static int is_active(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->active[instruction_index];
}

static int reads_temp(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->reads[instruction_index];
}

static int writes_temp(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->writes[instruction_index];
}

static int is_reload_eligible(void *context, int instruction_index, int temp_index) {

  struct reload_test_context *test_context;

  (void)temp_index;
  test_context = (struct reload_test_context *)context;
  return test_context->eligible[instruction_index];
}

static void reset_context(struct reload_test_context *context) {

  int instruction_index;

  for (instruction_index = 0; instruction_index < TEST_INSTRUCTION_COUNT; instruction_index++) {
    context->active[instruction_index] = YES;
    context->reads[instruction_index] = NO;
    context->writes[instruction_index] = NO;
    context->eligible[instruction_index] = NO;
  }
}

static int reload_site_is_empty(struct register_allocator_reload_site *reload_site) {

  return reload_site->found == NO && reload_site->instruction == -1;
}

int main(void) {

  struct reload_test_context context;
  struct register_allocator_reload_site reload_site;

  reset_context(&context);
  context.reads[2] = YES;
  context.eligible[2] = YES;
  if (register_allocator_discover_reload_site("coreReloadFound", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 1;
  if (reload_site.found != YES || reload_site.instruction != 2)
    return 2;

  context.active[2] = NO;
  context.reads[4] = YES;
  context.eligible[4] = YES;
  if (register_allocator_discover_reload_site("coreReloadInactive", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 3;
  if (reload_site.found != YES || reload_site.instruction != 4)
    return 4;

  context.writes[3] = YES;
  reload_site.found = YES;
  reload_site.instruction = 99;
  if (register_allocator_discover_reload_site("coreReloadRedefined", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 5;
  if (reload_site_is_empty(&reload_site) == NO)
    return 6;

  reset_context(&context);
  context.reads[2] = YES;
  context.reads[4] = YES;
  context.eligible[4] = YES;
  if (register_allocator_discover_reload_site("coreReloadFirstIneligible", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 7;
  if (reload_site_is_empty(&reload_site) == NO)
    return 8;

  context.eligible[2] = YES;
  context.writes[2] = YES;
  if (register_allocator_discover_reload_site("coreReloadReadWrite", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 9;
  if (reload_site.found != YES || reload_site.instruction != 2)
    return 10;

  reset_context(&context);
  if (register_allocator_discover_reload_site("coreReloadNoRead", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 11;
  if (reload_site_is_empty(&reload_site) == NO)
    return 12;

  if (register_allocator_discover_reload_site("coreReloadEmpty", 1, 7, 8, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) == FAILED)
    return 13;
  if (reload_site_is_empty(&reload_site) == NO)
    return 14;

  reload_site.found = YES;
  reload_site.instruction = 99;
  if (register_allocator_discover_reload_site(NULL, 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 15;
  if (reload_site_is_empty(&reload_site) == NO)
    return 16;
  if (register_allocator_discover_reload_site("coreReloadBadBlock", -1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 17;
  if (register_allocator_discover_reload_site("coreReloadBadTemp", 1, -1, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 18;
  if (register_allocator_discover_reload_site("coreReloadBadStart", 1, 7, 9, 7,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 19;
  if (register_allocator_discover_reload_site("coreReloadBadEnd", 1, 7, 1, 8,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 20;
  if (register_allocator_discover_reload_site("coreReloadZeroCount", 1, 7, 0, -1,
      0, &context, is_active, reads_temp, writes_temp, is_reload_eligible,
      &reload_site) != FAILED)
    return 21;
  if (register_allocator_discover_reload_site("coreReloadNullActive", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, NULL, reads_temp, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 22;
  if (register_allocator_discover_reload_site("coreReloadNullReads", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, NULL, writes_temp,
      is_reload_eligible, &reload_site) != FAILED)
    return 23;
  if (register_allocator_discover_reload_site("coreReloadNullWrites", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, NULL,
      is_reload_eligible, &reload_site) != FAILED)
    return 24;
  if (register_allocator_discover_reload_site("coreReloadNullEligible", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      NULL, &reload_site) != FAILED)
    return 25;
  if (register_allocator_discover_reload_site("coreReloadNullOutput", 1, 7, 1, 6,
      TEST_INSTRUCTION_COUNT, &context, is_active, reads_temp, writes_temp,
      is_reload_eligible, NULL) != FAILED)
    return 26;

  return 0;
}
