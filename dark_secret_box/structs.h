typedef struct {
  void *action;
  int num_args;
  int* arguments;
} Action;

typedef struct {
  void* timer; 
  int index;
  int active_timer_id;
  Action* actions;
} ActionChain;

typedef struct
{
  int pin;
  int led_pin;
  boolean is_output;
  boolean is_logic_level;
} Pin;
