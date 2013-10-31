#include <arduino.h>
#include <stdarg.h>
#include "structs.h"

#include <Wire.h>
#include <Timer.h>

#define SLAVE_ADDRESS 0x04

Timer timer;
Sound sound;

// ---- Pin control ----

// pin_data is only used to initialize the "pins" array ...
int pin_data[12][3] = { {49, 47, true}, // pin #, led #, is_output
                        {48, 46, true},
                        {38, 45, true},
                        {41, 44, true},
                        {40, 43, true},
                        {37, 42, true},
                                
                        {10, 39, false},
                        {11, 32, false},
                        {12, 33, false},
                        {13, 34, false},
                        {14, 35, false},
                        {15, 36, false} };                

// Some consts for internal use ...
int OUT_1 = 0;
int OUT_2 = 1;
int OUT_3 = 2;
int OUT_4 = 3;
int OUT_5 = 4;
int OUT_6 = 5;

int IN_1 = 6;
int IN_2 = 7;
int IN_3 = 8;
int IN_4 = 9;
int IN_5 = 10;
int IN_6 = 11;

Pin* pins[12];

void dump_pin(Pin* pin) {
  if (pin->is_output) {
    Serial.print("Output");
  } else {
    Serial.print("Input");
  }
  Serial.print("Pin: ");
  Serial.print(pin->pin);
  if (pin->led_pin > -1) {
    Serial.print(", LED: ");
    Serial.print(pin->led_pin);
  }
  Serial.println("");    
}

void init_pin(Pin* pin, int pin_num, int led, byte is_output) {
  pin->pin = pin_num;
  pin->led_pin = led;
  pin->is_output = is_output;
  pin->last_value = 0;
}

void set_pin(Pin* pin, int level) {
    if (pin->is_output) {
      digitalWrite(pin->pin, level);
    }
      
    if (pin->led_pin != -1) {
      digitalWrite(pin->led_pin, level);
    } 
}

void read_pin(Pin* pin) {
    if (pin->is_output) {
      return;
    }
    
    int value = analogRead(pin->pin);
    digitalWrite(pin->led_pin, value > 0);
    pin->last_value = value;
}

// ---- Actions ----

int num_chains = 3;
ActionChain chains[10];

boolean set_output(ActionChain* chain, Args* args) {
  int index = args->arguments[0];
  int level = args->arguments[1];
  Pin* pin = pins[index];
  if (pin->is_output == false)
    return true;
  set_pin(pin, level);
  Serial.print("--- Setting pin ");
  Serial.print(index);
  Serial.print(" to ");
  Serial.println(level);
  return true; 
}

void wait_done(void* context) {
  ActionChain* chain = (ActionChain*)context;
  chain->active_timer_id = -1;
  chain->timer_done = true;
}

boolean wait(ActionChain* chain, Args* args) {
  if (!chain->timer_done) {
    // First time through, initialize the timer ...
    if (chain->active_timer_id == -1) {
      int ms = args->arguments[0];     
      chain->timer_done = false;
      chain->active_timer_id = ((Timer*)chain->timer)->after(ms, wait_done, (void*)chain);
      Serial.print("--- ms delay = ");
      Serial.println(ms);
    }
    return false;
  }
  chain->timer_done = false; // So we can redo this action if needed.
  return true; 
}

int _get_level(ActionChain* chain, Args* args) {
  int index = args->arguments[0];
  Pin* pin = pins[index];
  int value = pin->last_value;
  
  Serial.print("--- Input Pin ");
  Serial.print(pin->pin);
  Serial.print(" = ");
  Serial.println(value);
  return value;
}  

boolean wait_for_input_greater(ActionChain* chain, Args* args) {
  int level = args->arguments[1];
  int value = _get_level(chain, args);  
  return value > level;
}

boolean wait_for_input_lessthan(ActionChain* chain, Args* args) {
  int level = args->arguments[1];
  int value = _get_level(chain, args);  
  return value < level;
}

boolean restart(ActionChain* chain, Args* args) {
    chain->index = 0;
    return false;
}

boolean play_sound(ActionChain* chain, Args* args) {
  sound.sound_to_play_next = args->arguments[0];
  sound.duration_ms = args->arguments[1];
  return true;
}

boolean reset_all(ActionChain* chain, Args* args) {
  for (int x = 0; x < num_chains; x++) {
    chains[x].index = 0;
    if (chains[x].active_timer_id != -1) {
      ((Timer*)(chains[x].timer))->stop(chains[x].active_timer_id);
      chains[x].active_timer_id = -1;   
    }
    chains[x].timer_done = false;
  }
    
  // Turn off all outputs
  for (int x = 0; x < 12; x++) {
    set_pin(pins[x], LOW);
  }    
  return false;
}

// Helper function for argument creation for hardcoded chains. 
// When we drive this from the RPi, this won't be necessary.
Args *make_args(int num, ...)
{
  va_list args;
  va_start(args, num);
  int* list = (int*)(malloc(sizeof(int) * (num + 1)));
  for (int x = 0; x < num; x++) {
    list[x] = va_arg(args, int);
  }
  va_end(args);
  
  Args* a = (Args*)(malloc(sizeof(Args)));
  a->num = num;
  a->arguments = list;
  return a;
}

void process_chain(ActionChain* chain) {
  // Bail if we're waiting or done ...
  if (chain->index == -1) {
    return;
  }
  
  Action& action = chain->actions[chain->index];

  // Call the action handler ...
  boolean bump = (*action.action)(chain, action.args);
  if (bump) {
    chain->index++;
    if (chain->actions[chain->index].action == 0) {
      chain->index = -1;  // End this chain.
      Serial.println("--- Chain finished");
    }
  }
}

// ---- Initialize ----

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up DarkSecretBox. Version 0.1");

  // Convert pin_data to Pins ...
  for (int x = 0; x < 12; x++) {
    Pin* pin = (Pin*)malloc(sizeof(Pin));
    init_pin(pin, pin_data[x][0], pin_data[x][1], pin_data[x][2]);
    dump_pin(pin);
    pins[x] = pin;
    
    if (pin->is_output) {
      pinMode(pin->pin, OUTPUT);
    }
    
    if (pin->led_pin != -1) {
      pinMode(pin->led_pin, OUTPUT);     
    }
    
    set_pin(pin, LOW); // Default everything off. 
  }

  // Testing pipeline (this will come from RPi soon) ...
  static Action pipeline_0[] = {
                      {wait_for_input_greater, make_args(2, IN_2, 100)},  // Furthest motion sensor (input 2)
                      {set_output, make_args(2, OUT_4, HIGH)},            // Turn on left eye (output 4)
                      {wait, make_args(1, 250)},                          // Wait .25s          
                      {set_output, make_args(2, OUT_5, HIGH)},            // Turn on right eye (output 5)
                      {wait, make_args(1, 500)},                          // Wait .5s                               
                      {set_output, make_args(2, OUT_2, HIGH)},            // Turn on growl (output 2)
                      {wait, make_args(1, 1000)},                         // Wait 1s                               
                      {set_output, make_args(2, OUT_2, LOW)},             // Go back to ambient sound.
                      {0, 0}                                              // End the chain
                    };

  static Action pipeline_1[] = {
                      {wait_for_input_greater, make_args(2, IN_4, 100)},  // Closest motion sensor (input 4)
                      {set_output, make_args(2, OUT_1, HIGH)},            // Turn on clanger (output 1)                     
                      {wait, make_args(1, 3000)},                         // Wait 3s
                      {set_output, make_args(2, OUT_1, LOW)},             // Turn off clanger (output 1)                         
                      {set_output, make_args(2, OUT_3, HIGH)},            // Turn on blower (output 3)                         
                      {0, 0}                                              // End the chain 
                    };
                    
  static Action pipeline_2[] = {
                      {wait_for_input_greater, make_args(2, IN_3, 100)},  // Wait for reset-all button (input 3)
                      {reset_all, make_args(1, 0)},                       // Reset all chains
                      {0, 0}                                              // End the chain (not reached)
                    };

  chains[0].timer = &timer;
  chains[0].index = 0;
  chains[0].active_timer_id = -1;
  chains[0].timer_done = false;
  chains[0].actions = pipeline_0;

  chains[1].timer = &timer;
  chains[1].index = 0;
  chains[1].active_timer_id = -1;
  chains[1].timer_done = false;
  chains[1].actions = pipeline_1;

  chains[2].timer = &timer;
  chains[2].index = 0;
  chains[2].active_timer_id = -1;
  chains[2].timer_done = false;
  chains[2].actions = pipeline_2;

  for (int x = 0; x < 12; x++) {
    set_pin(pins[x], LOW);
  }

  sound.sound_to_play_next = -1;
  sound.duration_ms = 0;
    
  // define callbacks for i2c communication
  Wire.onReceive(receive_data);
  Wire.onRequest(send_data);

  Serial.println("Ready!");
}

void loop() {
  delay(50); // For testing.    
  for (int x = 0; x < 12; x++) {
    if (!pins[x]->is_output) {
        read_pin(pins[x]);
    }
  }       
  timer.update();  
  for (int x = 0; x < num_chains; x++) {
    process_chain(&chains[x]);
  }
}

// ---- I2C Handlers ----

int cmd_buffer[64];
int cmd_index = 0;
int cmd_expected = 0;
int is_waiting_for_start = true;
int is_waiting_for_end = true;

int CMD_SET_OUTPUT = 1;
int CMD_WAIT = 2;
int CMD_GREATER = 3;
int CMD_LESSTHAN = 4;
int CMD_RESTART = 5;
int CMD_SOUND = 6;

// Wire format ...
// <STX><Chain#><Action#><Command><arg><arg>...<ETX>
int STX = 0x7E;
int ETX = 0x7F;

I2Command cmd_lengths[] = {
                            {CMD_SET_OUTPUT, 2, set_output},
                            {CMD_WAIT, 1, wait},
                            {CMD_GREATER, 2, wait_for_input_greater},
                            {CMD_LESSTHAN, 2, wait_for_input_lessthan},                           
                            {CMD_RESTART, 1, restart},
                            {CMD_SOUND, 2, play_sound},
                          };

// Callback for I2C received data ...
void receive_data(int byte_count){
/*         
  while(Wire.available()) {
      int bite = Wire.read();
      if (is_waiting_for_start && bite != STX) {
        continue;
      }
      
      if (is_waiting_for_end && bite != ETX) {
        continue;
      }
    
      if (bite == STX) {
        is_waiting_for_start = false;
        cmd_expected = 3; // <Chain#><Action#><Command>       
        continue;
      }
      
      if (bite == ETX) {
          is_waiting_for_end = false;
          is_waiting_for_start = true;
          cmd_expected = 0;
          process_command();
      }
      
      if (cmd_index < cmd_expected && cmd_index < 64) {
        cmd_buffer[cmd_index++] = num;
      }
      if (cmd_index => cmd_expected) {
        cmd_expected = 1;
      }
        
      Serial.print("data received: ");
      Serial.println(number);
  }
  */
}

// Callback for sending data on I2C ...
void send_data() {
  Wire.write(sound.sound_to_play_next);
  Wire.write(sound.duration_ms);
}

