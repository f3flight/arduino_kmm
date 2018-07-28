// Keyboard - Version: Latest 
#include <Keyboard.h>

// Mouse - Version: Latest 
#include <EEPROM.h>

const int EEPROM_SIZE = 1024;

#include <Mouse.h>

// MIDIUSB - Version: Latest 
#include <MIDIUSB.h>
#include <frequencyToNote.h>
#include <pitchToFrequency.h>
#include <pitchToNote.h>


#include "makeymakey_pins.h"
#include "utility.h"
#include "actions.h"

const uint8_t TYPE_MOMENTARY = 0;
const uint8_t TYPE_PERSISTENT = 1;
const uint8_t TYPE_M_TO_P = 2;

bool input_state[NUM_INPUT_PINS] = {false};
uint8_t input_type[NUM_INPUT_PINS] = {TYPE_MOMENTARY};
bool input_state_m_to_p[NUM_INPUT_PINS] = {false};
unsigned long input_long_press_timer[NUM_INPUT_PINS] = {0};
bool input_long_press_fired[NUM_INPUT_PINS] = {0};

const int LONG_PRESS_MS = 500; // 500ms will trigger long press event(s)

const char* DELIM = " ";
const char* INNER_DELIM = ",";
const short READ_BUFFER_LEN = 100;
uint8_t read_buffer[READ_BUFFER_LEN] = {0};
short read_buffer_pos = 0;

void setup() {
  Keyboard.begin();
  Mouse.begin();
  Serial.begin(9600);
  for (int i = 0; i < NUM_PINS; i++) {pinMode(i, INPUT); digitalWrite(i, LOW);}
  pinMode(OUTPUT_D14, OUTPUT); digitalWrite(OUTPUT_D14, LOW);
  pinMode(OUTPUT_D16, OUTPUT); digitalWrite(OUTPUT_D16, LOW);
}

void loop() {
    for (int pin = 0; pin < NUM_INPUT_PINS; pin++) {
      int state = (digitalRead(INPUT_PINS[pin]) == LOW);
      if (state == STATE_PRESSED && input_long_press_timer[pin] == 0) {
        input_long_press_timer[pin] = millis();
      }
      if (state == STATE_RELEASED) {
        input_long_press_timer[pin] = 0;
        input_long_press_fired[pin] = 0;
      }
      if (input_type[pin] == TYPE_M_TO_P) {
        if (state == STATE_PRESSED && !input_state_m_to_p[pin]) {
          input_state_m_to_p[pin] = true;
          state = !input_state[pin];
        } else {
          if (state == STATE_RELEASED) input_state_m_to_p[pin] = false;
          state = input_state[pin];
        }
      }
      if (input_state[pin] != state) {
        _p(F("input ")); _p(pin); _p(F(" new state ")); _pn(state);
        input_state[pin] = state;
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          if (pin_actions[pin][i].action_id == -1) break;
          pin_actions[pin][i].execute(state);
        }
      } else if (state == STATE_PRESSED && input_long_press_fired[pin] == 0 && millis() - input_long_press_timer[pin] >= LONG_PRESS_MS) { // time to fire log press events
        input_long_press_fired[pin] = 1;
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          if (pin_actions[pin][i].action_id == -1) break;
          pin_actions[pin][i].execute(STATE_LONG_PRESSED);
        }
      }
    }
    uint8_t input[READ_BUFFER_LEN] = {0};
    int out_len;
    if (serial_readbytes_until(read_buffer, READ_BUFFER_LEN, &read_buffer_pos, input, &out_len)) {
      _p(F("got a line [")); _p((char*)input); _pn(F("]"));
      if (input[0] != '#') parse_input(input, DELIM);  // parsing input if it's not a comment
    } else {
      //_pn(F("no input this time..."));
    }
    delay(50);
}

void parse_input(char* input, const char* delims) {  // input must be a proper 0-terminated string
  int buffer_pos = 0;
  bool get = false, set = false, test = false, type = false, run = false;
  int counter = 0;
  int input_num = 0;
  int parsed_input_type = 0;
  //char* input_copy[READ_BUFFER_LEN] = {0};
  //strcpy(input_copy, input);
  //char* token = strtok(input_copy, delims);
  char* saveptr;
  char* token = strtok_r(input, delims, &saveptr);
  while (token != NULL) {
    _p(F("token = ")); _pn(token);
    if (counter == 0) {
      if (strcmp(token, "get") == 0) {get = true;}
      else if (strcmp(token, "set") == 0) {set = true;}
      else if (strcmp(token, "test") == 0) {test = true;}
      else if (strcmp(token, "type") == 0) {type = true;}
      else if (strcmp(token, "run") == 0) {run = true;}
      else if (strcmp(token, "reset") == 0) {
        for (int i = 0; i < NUM_PINS; i++) {
          for (int j = 0; j < PIN_ACTIONS_MAX; j++) {
            pin_actions[i][j].action_id = -1;
          }
        }
      }
      else if (strcmp(token, "save") == 0) {
        if (sizeof(pin_actions) + sizeof(input_type) > EEPROM_SIZE) {
          _pn("Data does not fit into EEPROM, cannot save!");
          break;
        }
        EEPROM.put(0, pin_actions);
        for (int i = 0; i < sizeof(input_type); i++) {
          EEPROM.put(EEPROM_SIZE - i - 1, input_type[i]);
        }
      }
      else if (strcmp(token, "load") == 0) {
        EEPROM.get(0, pin_actions);
        for (int i = 0; i < sizeof(input_type); i++) {
          EEPROM.get(EEPROM_SIZE - i - 1, input_type[i]);
        }
      }
      else if (strcmp(token, "info") == 0) {debug_info(); break;}
      else {
        _pn(F("don't know whatcha talkin' 'bout!"));
        break;  // unknown primary action, exiting
      }
    } else if (run) {
      _p(F("running: ")); _pn(token);
      if (parse_action(token, INNER_DELIM, &parsed_action)) {
        _pn(F("parsed action, executing"));
        parsed_action.execute(true);
        parsed_action.execute(false);
      }
    } else if (get || (set && counter == 1) || test || (type && counter == 1)) {
      if (!str_to_int(token, &input_num, 10) || input_num < 0 || input_num > NUM_INPUT_PINS - 1) {
        _pn(F("Are you trying to break me? This input pin number is bad."));
        break;
      }
      //_p(F("input pin = ")); _pn(input_num);
      if (get) {
        _p(F("getting config of input ")); _pn(input_num);
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          if (pin_actions[input_num][i].action_id == -1) break;
          _p(F("has ")); _pn(function_map[pin_actions[input_num][i].action_id].action_name);
        }
      } else if (set) {
        _p(F("clearing config for input ")); _pn(input_num);
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          pin_actions[input_num][i].action_id = -1;
        }
      } else if (test) {
        _p(F("simulating trigger of input ")); _pn(input_num);
      } else if (type) {
        _p(F("input ")); _p(input_num); _p(F(" type = ")); _pn(input_type[input_num]);
      }
    } else if (set) {
      if (counter - 2 >= PIN_ACTIONS_MAX) {
        _p(F("Too many actions, max allowed: ")); _p(PIN_ACTIONS_MAX); _pn(F(", ignoring rest..."));
        break;
      }
      _p(F("setting config for input ")); _p(input_num); _p(F(": ")); _pn(token);
      if (parse_action(token, INNER_DELIM, &parsed_action)) {
        _pn(F("parsed action, setting"));
        pin_actions[input_num][counter-2] = parsed_action;
      }
    } else if (type) {
      if (!str_to_int(token, &parsed_input_type, 10) || parsed_input_type < 0 || parsed_input_type > 2) {
        _pn(F("Are you trying to break me? This input type is bad."));
        break;
      }
      input_type[input_num] = parsed_input_type;
      _p(F("Set input type to: ")); _pn(parsed_input_type);
    }
    counter++;
    token = strtok_r(NULL, delims, &saveptr);
  }
}

void debug_info() {
   _p(F("size of pin config: ")); _pn(sizeof(pin_actions));
   _p(F("size of keyboard payload: ")); _pn(sizeof(keyboard_payload));
   _p(F("size of midi payload: ")); _pn(sizeof(midi_payload));
   _p(F("size of mouse payload: ")); _pn(sizeof(mouse_payload));
   _p(F("size of payload: ")); _pn(sizeof(payload));
}
