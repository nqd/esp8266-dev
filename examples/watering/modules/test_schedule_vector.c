#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "schedule_vector.h"

struct schedule_tag {
  valve_schedule *v1;
  valve_schedule *v2;
  valve_schedule *v3;
  valve_schedule *v4;
};

typedef struct schedule_tag schedule;

static schedule valves_schedule;


int main() {
  // declare and initialize a new schedule_vector
  ScheduleVector schedule_vector1;
  schedule_vector_init(&schedule_vector1, 2);
  schedule_vector_append(&schedule_vector1, 3600, 1, 12,13);
  schedule_vector_append(&schedule_vector1, 1800, 2, 20,12);
  valve_schedule slot1 = schedule_vector_get(&schedule_vector1, 0);
  valve_schedule slot2 = schedule_vector_get(&schedule_vector1, 1);
  assert(slot1.duration == 3600);
  assert(slot1.when.d == 1);
  assert(slot1.when.h == 12);
  assert(slot1.when.m == 13); 
  assert(slot2.duration == 1800);
  assert(slot2.when.d == 2);
  assert(slot2.when.h == 20);
  assert(slot2.when.m == 12);

  printf("OK\n");
  // int i;
  // for (i = 200; i > -50; i--) {
  //   schedule_vector_append(&schedule_vector, i);
  // }

  // // set a value at an arbitrary index
  // // this will expand and zero-fill the schedule_vector to fit
  // schedule_vector_set(&schedule_vector, 4452, 21312984);

  // // print out an arbitrary value in the schedule_vector
  // printf("Heres the value at 27: %d\n", schedule_vector_get(&schedule_vector, 27));

  // // we're all done playing with our schedule_vector, 
  // // so free its underlying data array
  // schedule_vector_free(&schedule_vector);
}