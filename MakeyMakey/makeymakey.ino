// Mouse - Version: Latest 
#include <Mouse.h>

// Keyboard - Version: Latest 
#include <Keyboard.h>

// MIDIUSB - Version: Latest 
#include <MIDIUSB.h>
#include <frequencyToNote.h>
#include <pitchToFrequency.h>
#include <pitchToNote.h>


#include "makeymakey_pins.h"
#include "utility.h"


const char input_pins[] {
  INPUT_D0, INPUT_D1, INPUT_D2, INPUT_D3, INPUT_D4, INPUT_D5, 
  INPUT_CLICK, INPUT_SPACE, INPUT_AR_DN, INPUT_AR_UP, INPUT_AR_LT, INPUT_AR_RT,
  INPUT_A0, INPUT_A1, INPUT_A2, INPUT_A3, INPUT_A4, INPUT_A5
};

const char led_pins[] {
  LED_A, LED_B, LED_C, LED_D
};

bool input_state[NUM_INPUT_PINS];

const char NUM_ACTIONS = 3;

struct payload {
  char txt[5];
  int var_a;
  int var_b;
  bool flag_c;
};

typedef bool(*command_function)(payload);

struct function_link {
  char function_name[20];
  command_function function;
};

bool action_keyboard(payload input);
bool action_mouse(payload input);
bool action_midi(payload input);

const function_link function_map[] = {
  {"keyboard", &action_keyboard},
  {"mouse", &action_mouse},
  {"midi", &action_midi},
};

struct input_action {
  //command_function action;
  struct payload p;
};

input_action test_action;
payload test_payload;

const char delim_str[] = " ";
const char inner_delim_str[] = "";
const short read_buffer_len = 100;
uint8_t read_buffer[read_buffer_len] = {0};
short read_buffer_pos = 0;

void setup() {
  Keyboard.begin();
  for (int i=0; i < NUM_PINS; i++) {pinMode(i, INPUT); digitalWrite(i, LOW);}
  pinMode(OUTPUT_D14, OUTPUT); digitalWrite(OUTPUT_D14, LOW);
  pinMode(OUTPUT_D16, OUTPUT); digitalWrite(OUTPUT_D16, LOW);
}

void loop() {
    for (int i = 0; i < NUM_INPUT_PINS; i++)
    {
      bool state = (digitalRead(input_pins[i]) == LOW);
      if (input_state[i] != state)
      {
        Serial.print("input "); Serial.print(input_pins[i]); Serial.print(" new state "); Serial.println(state);
        input_state[i] = state;
        // int action = (state) ? 0x90 : 0x80;
        // midiEventPacket_t noteOn = {0x09, action | 0, 48 + i, 127};
        // MidiUSB.sendMIDI(noteOn);
        // MidiUSB.flush();
      }
    }
    uint8_t input[read_buffer_len] = {0};
    int out_len;
    if (serial_readbytes_until(read_buffer, read_buffer_len, &read_buffer_pos, input, &out_len)) {
      Serial.print("got a line ["); Serial.print((char*)input); Serial.println("]");
      if (input[0] != '#') parse_input(input, delim_str);  // parsing input if it's not a comment
    } else {
      //Serial.println("no input this time..."); 
    }
    delay(50);
}

void parse_input(char* input, char* delims) {  // input must be a proper 0-terminated string
  char* piece = strtok(input, delims);
  int counter = 0;
  bool get = false, set = false, test = false, run = false;
  int input_num = 0;
  while (piece != NULL) {
    counter++;
    if (counter == 1) {
      if (strcmp(piece, "get") == 0) {get = true;}
      else if (strcmp(piece, "set") == 0) {set = true;}
      else if (strcmp(piece, "test") == 0) {test = true;}
      else if (strcmp(piece, "run") == 0) {run = true;}
      else {Serial.println("don't know whatcha talkin' 'bout!"); break;}}  // unknown primary action, exiting
    else if (run) {
      Serial.print("running: "); Serial.println(piece);
      //test_action.command_function = &action_keyboard;
      //test_action.payload.var_a = 123;
      //test_action.command_function(test_action.payload);
      //test_payload.var_a = 123;
      Serial.println(function_map[0].function_name);
    }
    else if (counter == 2) {
      if (str_to_int(piece, &input_num) && input_num >= 0 && input_num < NUM_INPUT_PINS-1) {
        //Serial.print("input pin = "); Serial.println(input_num);
        if (get) {
          Serial.print("getting config of input "); Serial.println(input_num);
          break;}  // dumped config, ignoring rest of input
        else if (set) {
          Serial.print("clearing config for input "); Serial.println(input_num);}
        else if (test) {
          Serial.print("simulating trigger of input "); Serial.println(input_num);
          break;}}  // done simulation, ignoring rest of input
      else {
        Serial.println("Are you trying to break me? This input pin number is bad.");
        break;}}  // exiting due to incorrect input
    else if (counter >= 3) {
      if (set) {
        Serial.print("setting for input "); Serial.print(input_num); Serial.print(": "); Serial.println(piece);}}  
    piece = strtok(NULL, delims);
  }
  if (get && counter == 1) {
    Serial.println("getting config of all inputs...");
  } 
}

bool action_keyboard(payload input) {
  Serial.print("KB "); Serial.println(input.var_a);
}

bool action_mouse(payload input) {
  Serial.print("MO "); Serial.println(input.var_a);
}

bool action_midi(payload input) {
  Serial.print("MI "); Serial.println(input.var_a);
}
