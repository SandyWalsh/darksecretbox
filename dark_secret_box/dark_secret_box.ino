#include <arduino.h>
#include <stdarg.h>
#include "structs.h"

#include <Wire.h>
#include <Timer.h>

#define SLAVE_ADDRESS 0x04

Timer timer;

// ---- Pin control ----

// pin_data is only used to initialize the "pins" array ...
int pin_data[18][4] = { {49, 47, true}, // pin #, led #, is_output
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

boolean wait_for_input_greater(ActionChain* chain, Args* args) {
  int index = args->arguments[0];
  int level = args->arguments[1];
  Pin* pin = pins[index];
  int value = pin->last_value;
  
  Serial.print("--- Input Pin ");
  Serial.print(pin->pin);
  Serial.print(" = ");
  Serial.println(value);
  
  return value > level;
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

// Actions + Parameters = a Pipeline
Action pipeline[] = {
                      {wait_for_input_greater, make_args(2, IN_1, 130)}, // Wait for Input 1 to go > 130
                      {set_output, make_args(2, OUT_5, HIGH)},           // Turn on Output 5
                      {wait, make_args(1, 3000)},                        // Wait 3s
                      {set_output, make_args(2, OUT_5, LOW)},            // Turn off Output 5
                      {0, 0}                                             // End this chain.
                    };

// A Pipeline lives in an ActionChain ...
ActionChain this_chain;

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

  this_chain.timer = &timer;
  this_chain.index = 0;
  this_chain.active_timer_id = -1;
  this_chain.timer_done = false;
  this_chain.actions = pipeline;

  for (int x = 0; x < 12; x++) {
    set_pin(pins[x], LOW);
  }
    
  // define callbacks for i2c communication
  Wire.onReceive(receive_data);
  Wire.onRequest(send_data);

  Serial.println("Ready!");
}

void loop() {
  delay(500); // For testing.    
  for (int x = 0; x < 12; x++) {
    if (!pins[x]->is_output) {
        read_pin(pins[x]);
    }
  }       
  timer.update();  
  process_chain(&this_chain);
}

// ---- I2C Handlers ----
int number;

// Callback for I2C received data ...
void receive_data(int byte_count){

    while(Wire.available()) {
        number = Wire.read();
        Serial.print("data received: ");
        Serial.println(number);
    }
}

// Callback for sending data on I2C ...
void send_data() {
    Wire.write(number);
}

