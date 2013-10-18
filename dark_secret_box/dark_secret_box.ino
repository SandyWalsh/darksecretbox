#include <arduino.h>
#include "structs.h"

#include <Wire.h>
#include <Timer.h>

#define SLAVE_ADDRESS 0x04

Timer timer;

// ---- Pin control ----

// pin_data is only used to initialize the "pins" array ...
int pin_data[18][4] = { {49, 47, true, false}, // pin #, led #, is_output, is_logic_level
                        {48, 46, true, false},
                        {38, 45, true, false},
                        {41, 44, true, false},
                        {40, 43, true, false},
                        {37, 42, true, false},
                        
                        {23, -1, false, true},
                        {24, -1, false, true},
                        {25, -1, false, true},
                        {26, -1, false, true},
                        {27, -1, false, true},
                        {28, -1, false, true},                
        
                        {10, 39, false, false},
                        {11, 32, false, false},
                        {12, 33, false, false},
                        {13, 34, false, false},
                        {14, 35, false, false},
                        {15, 36, false, false} };                

Pin* pins[18];

void dump_pin(Pin* pin) {
  if (pin->is_output) {
    Serial.print("Output");
  } else {
    Serial.print("Input");
  }
  if (pin->is_logic_level)
    Serial.print(" (ll) ");
  else
    Serial.print(" (oc) ");    

  Serial.print("Pin: ");
  Serial.print(pin->pin);
  if (pin->led_pin > -1) {
    Serial.print(", LED: ");
    Serial.print(pin->led_pin);
  }
  Serial.println("");    
}

void init_pin(Pin* pin, int pin_num, int led, byte is_output, byte is_logic_level) {
  pin->pin = pin_num;
  pin->led_pin = led;
  pin->is_output = is_output;
  pin->is_logic_level = is_logic_level;
}

void set_pin(Pin* pin, int level) {
    if (pin->is_output) {
      digitalWrite(pin->pin, level);
    }
      
    if (pin->led_pin != -1) {
      digitalWrite(pin->led_pin, level);
    } 
}

// ---- Actions ----

void set_output(ActionChain* chain, int level) {
  int index = 0;
  Pin* pin = pins[index];
  if (pin->is_output == false)
    return;
  set_pin(pin, level);
  Serial.print("--- Setting pin ");
  Serial.print(index);
  Serial.print(" to ");
  Serial.println(level);
}

void wait_done(void* context) {
  ActionChain* chain = (ActionChain*)context;
  chain->active_timer_id = -1;
}

void wait(ActionChain* chain, int ms) {
  chain->active_timer_id = ((Timer*)chain->timer)->after(ms, wait_done, (void*)chain);
  Serial.print("--- ms delay = ");
  Serial.println(ms);
}

Action pipeline[] = {{(void*)set_output, HIGH}, 
                     {(void*)wait, 3000},
                     {(void*)set_output, LOW},
                     {(void*)0, 0}};

ActionChain this_chain;

void process_chain(ActionChain* chain) {
  // Bail if we're waiting or done ...
  if (chain->index == -1 || chain->active_timer_id > -1) {
    return;
  }
  
  Action& action = chain->actions[chain->index];

  void (*hook)(ActionChain*, int) = (void (*)(ActionChain*, int)) action.action;  
  (*hook)(chain, action.argument);
  chain->index++;
  if (chain->actions[chain->index].action == 0) {
    chain->index = -1;  // End this chain.
    Serial.println("--- Chain finished");
  }
}

// ---- Initialize ----
void setup() {
  Serial.begin(115200);
  Serial.println("Starting up DarkSecretBox. Version 0.1");

  // Convert pin_data to Pins ...
  for (int x = 0; x < 18; x++) {
    Pin* pin = (Pin*)malloc(sizeof(Pin));
    init_pin(pin, pin_data[x][0], pin_data[x][1], pin_data[x][2], pin_data[x][3]);
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
  this_chain.actions = pipeline;

  for (int x = 0; x < 18; x++) {
    set_pin(pins[x], LOW);
  }
    
  // define callbacks for i2c communication
  Wire.onReceive(receive_data);
  Wire.onRequest(send_data);

  Serial.println("Ready!");
}

void loop() {
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

