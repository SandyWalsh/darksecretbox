struct Action;

typedef struct {
  void* timer; 
  int index;
  int active_timer_id;
  boolean timer_done;
  Action* actions;
} ActionChain;

typedef struct {
  int num;
  int* arguments;
} Args;

typedef boolean (*ActionHandler)(ActionChain*, Args*);

typedef struct Action {
  ActionHandler action;
  Args* args;
} ActionType;

typedef struct
{
  int pin;
  int led_pin;
  boolean is_output;
  int last_value;
} Pin;


