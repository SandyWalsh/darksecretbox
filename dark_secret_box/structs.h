struct Action;

typedef struct {
  void* timer; 
  int index;
  int active_timer_id;
  boolean timer_done;
  Action* actions;
} ActionChain;

typedef boolean (*ActionHandler)(ActionChain*, int, int*);

typedef struct Action {
  ActionHandler action;
  int num_args;
  int* arguments;
} ActionType;

typedef struct
{
  int pin;
  int led_pin;
  boolean is_output;
  boolean is_logic_level;
} Pin;


