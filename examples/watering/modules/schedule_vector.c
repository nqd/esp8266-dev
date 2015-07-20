// schedule_vector.c

#include "ets_sys.h"
#include "os_type.h"
#include "debug.h"
#include "user_interface.h"
#include "mem.h"
#include "osapi.h"
#include "util.h"
#include "schedule_vector.h"


void ICACHE_FLASH_ATTR schedule_vector_init(ScheduleVector *schedule_vector, uint8_t capacity) {
  schedule_vector->size = 0;
  schedule_vector->capacity = capacity;

  // allocate memory for schedule_vector->data
  schedule_vector->data = util_zalloc(sizeof(valve_schedule) * schedule_vector->capacity);
  INFO("Vector %p init with capacity: %d\n",schedule_vector, capacity);
}

void ICACHE_FLASH_ATTR schedule_vector_append(ScheduleVector *schedule_vector, uint32_t duration, uint8_t day, uint8_t hour, uint8_t min) {
  INFO("Vector append, size: %d, capacity: %d, duration: %d, day:%d\n", schedule_vector->size, schedule_vector->capacity, duration, day);
  schedule_vector_double_capacity_if_full(schedule_vector);
  schedule_vector->data[schedule_vector->size].duration = duration;
  schedule_vector->data[schedule_vector->size].when.d = day;
  schedule_vector->data[schedule_vector->size].when.h = hour;
  schedule_vector->data[schedule_vector->size].when.m = min;
  schedule_vector->size++;
}

valve_schedule ICACHE_FLASH_ATTR schedule_vector_get(ScheduleVector *schedule_vector, uint8_t index) {
  if ((index >= schedule_vector->size) || (index < 0)) {
    util_assert(FALSE, "Index %d out of bounds for schedule_vector of size %d\n", index, schedule_vector->size);
  }
  return schedule_vector->data[index];
}

void ICACHE_FLASH_ATTR schedule_vector_set(ScheduleVector *schedule_vector, uint8_t index, uint32_t duration, uint8_t day, uint8_t hour, uint8_t min) {
  if ((index >= schedule_vector->size) || (index < 0)) {
    INFO("Index %d out of bounds for schedule_vector of capacity %d\n", index, schedule_vector->capacity);
    return;
  }
  schedule_vector->data[index].duration = duration;
  schedule_vector->data[index].when.d = day;
  schedule_vector->data[index].when.h = hour;
  schedule_vector->data[index].when.m = min;
}

void ICACHE_FLASH_ATTR schedule_vector_remove(ScheduleVector *schedule_vector, uint8_t index) {
  if ((index >= schedule_vector->size) || (index < 0)) {
    INFO("Index %d out of bounds for schedule_vector of capacity %d\n", index, schedule_vector->capacity);
    return;
  }
  if (schedule_vector->size == 1) {
    schedule_vector->data[index].duration = 0;
    schedule_vector->data[index].when.d = 0;
    schedule_vector->data[index].when.h = 0;
    schedule_vector->data[index].when.m = 0;
  } else {
    schedule_vector->data[index].duration = schedule_vector->data[schedule_vector->size - 1].duration;
    schedule_vector->data[index].when.d = schedule_vector->data[schedule_vector->size - 1].when.d;
    schedule_vector->data[index].when.h = schedule_vector->data[schedule_vector->size - 1].when.h;
    schedule_vector->data[index].when.m = schedule_vector->data[schedule_vector->size - 1].when.m;
  }
  schedule_vector->size--;
}

void ICACHE_FLASH_ATTR schedule_vector_double_capacity_if_full(ScheduleVector *schedule_vector) {
  if (schedule_vector->size >= schedule_vector->capacity) {
    // double schedule_vector->capacity and resize the allocated memory accordingly
    schedule_vector->capacity *= 2;
    os_realloc(schedule_vector->data, sizeof(valve_schedule) * schedule_vector->capacity);
  }
}

void ICACHE_FLASH_ATTR schedule_vector_free(ScheduleVector *schedule_vector) {
  os_free(schedule_vector->data);
}