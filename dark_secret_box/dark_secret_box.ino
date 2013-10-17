#include <arduino.h>
#include "structs.h"

#include <Wire.h>
#include <Timer.h>

#define SLAVE_ADDRESS 0x04

void init_state(State* state, int i_state, int delay_ms) {
  state->state = i_state;
  state->delay_ms = delay_ms;
}

void init_pin(Pin* pin, int pin_num, int led, byte is_output, byte is_logic_level) {
  pin->pin = pin_num;
  pin->led_pin = led;
  pin->is_output = is_output;
  pin->is_logic_level = is_logic_level;
  pin->current_state = 0;
}

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

void set_pin(Pin* pin, int level) {
    if (pin->is_output) {
      digitalWrite(pin->pin, level);
    }
      
    if (pin->led_pin != -1) {
      digitalWrite(pin->led_pin, level);
    } 
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up DarkSecretBox. Version 0.1");

  // Convert pin_data to Pins ...
  for (int x = 0; x < 18; x++) {
    Pin* pin = (Pin*)malloc(sizeof(Pin));
    init_pin(pin, pin_data[x][0], pin_data[x][1], pin_data[x][2], pin_data[x][3]);
    pins[x] = pin;
    
    if (pin->is_output) {
      pinMode(pin->pin, OUTPUT);
    }
    
    if (pin->led_pin != -1) {
      pinMode(pin->led_pin, OUTPUT);     
    }
    
    set_pin(pin, LOW); // Default everything off. 
  }

  for (int x = 0; x < 18; x++) {
    set_pin(pins[x], HIGH);
  }
    
  // define callbacks for i2c communication
  Wire.onReceive(receive_data);
  Wire.onRequest(send_data);

  Serial.println("Ready!");
}

void loop() {
    delay(100);
}

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

