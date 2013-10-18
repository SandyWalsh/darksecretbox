typedef struct Action
{
  void *action;
  int argument;
};

typedef struct ActionChain {
  void* timer; 
  int index;
  int active_timer_id;
  Action* actions;
};

typedef struct Pin
{
  int pin;
  int led_pin;
  boolean is_output;
  boolean is_logic_level;
};
