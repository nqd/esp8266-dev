

// Definition for date element
typedef struct {
  uint8_t d;
  uint8_t h;
  uint8_t m;
} date;


// Definition for valve schedule element
struct valve_schedule_tag {
  uint32_t duration;
  date when;
};
typedef struct valve_schedule_tag valve_schedule;

// Define a vector type
typedef struct {
  int size;    
  int capacity;
  valve_schedule *data;
} ScheduleVector;

void schedule_vector_init(ScheduleVector *schedule_vectorn, uint8_t capacity);

void schedule_vector_append(ScheduleVector *schedule_vector, uint32_t duration, uint8_t day, uint8_t hour, uint8_t min);

valve_schedule schedule_vector_get(ScheduleVector *schedule_vector, uint8_t index);

void schedule_vector_set(ScheduleVector *schedule_vector, uint8_t index, uint32_t duration, uint8_t day, uint8_t hour, uint8_t min);

void schedule_vector_remove(ScheduleVector *schedule_vector, uint8_t index);

void schedule_vector_double_capacity_if_full(ScheduleVector *schedule_vector);

void schedule_vector_free(ScheduleVector *schedule_vector);