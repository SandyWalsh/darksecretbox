typedef struct State
{
  int state;
  int delay_ms;
  State *next_state;
};

typedef struct Pin
{
  int pin;
  int led_pin;
  boolean is_output;
  boolean is_logic_level;
  State *current_state;
};
