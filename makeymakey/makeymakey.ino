// Keyboard - Version: Latest 
#include <Keyboard.h>

// Mouse - Version: Latest 
#include <EEPROM.h>

const int EEPROM_SIZE = 1024;

#include "MouseAbs.h"

// MIDIUSB - Version: Latest 
#include <MIDIUSB.h>
#include <frequencyToNote.h>
#include <pitchToFrequency.h>
#include <pitchToNote.h>


#include "makeymakey_pins.h"
#include "utility.h"
#include "actions.h"

bool input_state[NUM_INPUT_PINS] = {false};
bool input_state_long[NUM_INPUT_PINS] = {false};
bool input_state_m_to_p_processed[NUM_INPUT_PINS] = {false};
bool input_state_m_to_p_long_processed[NUM_INPUT_PINS] = {false};
bool input_state_delayed[NUM_INPUT_PINS] = {false};
unsigned long input_long_press_timer[NUM_INPUT_PINS] = {0};
bool input_has_long_press_events[NUM_INPUT_PINS]  = {0};

const int LONG_PRESS_MS = 500; // 500ms will trigger long press event(s)

const uint8_t FLAG_M_TO_P = 1; // convert momentary button into persistent button
const uint8_t FLAG_DONT_WAIT = 2; // this flag allows events of type "EVENT_PRESS*" fire even if there is an event of type "EVENT_LONG_PRESS*" and LONG_PRESS_MS time has not passed;

uint8_t input_flags[NUM_INPUT_PINS] = {0};

const char* DELIM = " ";
const char* INNER_DELIM = ",";
const short READ_BUFFER_LEN = 100;
uint8_t read_buffer[READ_BUFFER_LEN] = {0};
short read_buffer_pos = 0;

void execute_actions(int pin, bool state, bool long_event) {
  _p(F("Executing actions for pin: ")); _pn(pin);
  for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
    if (pin_actions[pin][i].action_id == -1) break;
      pin_actions[pin][i].execute(state, long_event);
  }
}

void debug_info() {
   _p(F("size of pin config: ")); _pn(sizeof(pin_actions));
   _p(F("size of keyboard payload: ")); _pn(sizeof(keyboard_payload));
   _p(F("size of midi payload: ")); _pn(sizeof(midi_payload));
   _p(F("size of mouse payload: ")); _pn(sizeof(mouse_payload));
   _p(F("size of payload: ")); _pn(sizeof(payload));
   for (int i = 0; i < NUM_INPUT_PINS; i++) {
     _p(i);
     _p(F(" input_state = ")); _p(input_state[i]);
     _p(F(" input_state_long = ")); _p(input_state_long[i]);
     _p(F(" input_state_m_to_p_processed = ")); _p(input_state_m_to_p_processed[i]);
     _p(F(" input_state_m_to_p_long_processed = ")); _p(input_state_m_to_p_long_processed[i]);
     _p(F(" input_long_press_timer = ")); Serial.print(input_long_press_timer[i]);
     _p(F(" input_has_long_press_events = ")); _p(input_has_long_press_events[i]);
     _pn("");
     for (int j = 0; j < PIN_ACTIONS_MAX; j++) {
       if (pin_actions[i][j].action_id == -1) break;
       _p(F("  action = ")); _p(function_map[pin_actions[i][j].action_id].action_name); _p(F(", event = ")); _pn(pin_actions[i][j].event);
     }
   }
}

void update_input_has_long_press_events(int input_num) {
  input_has_long_press_events[input_num] = false;
  for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
    if (pin_actions[input_num][i].action_id == -1) break;
    if (pin_actions[input_num][i].event == EVENT_LONG_PRESS || pin_actions[input_num][i].event == EVENT_LONG_PRESS_AUTO_RELEASE) {
      input_has_long_press_events[input_num] = true;
    }
  }
}

void parse_input(char* input, const char* delims) {  // input must be a proper 0-terminated string
  int buffer_pos = 0;
  bool get = false, set = false, test = false, flags = false, run = false, save = false, load = false;
  int counter = 0;
  int input_num = 0;
  int parsed_input_flags = 0;
  char* saveptr;
  char* token = strtok_r(input, delims, &saveptr);
  while (token != NULL) {
    _p(F("token = ")); _pn(token);
    if (counter == 0) {
      if (strcmp(token, "get") == 0) {get = true;}
      else if (strcmp(token, "set") == 0) {set = true;}
      else if (strcmp(token, "test") == 0) {test = true;}
      else if (strcmp(token, "flags") == 0) {flags = true;}
      else if (strcmp(token, "run") == 0) {run = true;}
      else if (strcmp(token, "reset") == 0) {
        for (int i = 0; i < NUM_INPUT_PINS; i++) {
          for (int j = 0; j < PIN_ACTIONS_MAX; j++) {
            pin_actions[i][j].action_id = -1;
          }
        }
      }
      else if (strcmp(token, "save") == 0) {
        save = true;
        if (sizeof(pin_actions) + sizeof(input_flags) > EEPROM_SIZE) {
          _pn("Data does not fit into EEPROM, cannot save!");
          break;
        }
        EEPROM.put(0, pin_actions);
        for (int i = 0; i < sizeof(input_flags); i++) {
          EEPROM.put(EEPROM_SIZE - i - 1, input_flags[i]);
        }
      }
      else if (strcmp(token, "load") == 0) {
        load = true;
        EEPROM.get(0, pin_actions);
        for (int i = 0; i < sizeof(input_flags); i++) {
          EEPROM.get(EEPROM_SIZE - i - 1, input_flags[i]);
        }
      }
      else if (strcmp(token, "info") == 0) {debug_info(); break;}
      else {
        _pn(F("don't know whatcha talkin' 'bout!"));
        break;  // unknown primary action, exiting
      }
    } else if (run) {
      _p(F("running: ")); _p(token);  _p("; delims: "); _pn(INNER_DELIM);
      if (parse_action(token, INNER_DELIM, &parsed_action)) {
        _pn(F("parsed action, executing"));
        parsed_action.execute(true, false);
        parsed_action.execute(false, false);
      }
    } else if (get || (set && counter == 1) || test || (flags && counter == 1)) {
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
      } else if (flags) {
        _p(F("input ")); _p(input_num); _p(F(" type = ")); _pn(input_flags[input_num]);
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
    } else if (flags) {
      if (!str_to_int(token, &parsed_input_flags, 10) || parsed_input_flags < 0 || parsed_input_flags > 2) {
        _pn(F("Are you trying to break me? This input type is bad."));
        break;
      }
      input_flags[input_num] = parsed_input_flags;
      _p(F("Set input type to: ")); _pn(parsed_input_flags);
    }
    counter++;
    token = strtok_r(NULL, delims, &saveptr);
  }
  if (set || load) {
    update_input_has_long_press_events(input_num);
  }
}

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
    if (input_has_long_press_events[pin] && state && input_long_press_timer[pin] == 0) {
      input_long_press_timer[pin] = millis();
    }
    if (input_flags[pin] & FLAG_M_TO_P) {
      if (state) {
        if (!input_state_m_to_p_processed[pin] || !input_state_m_to_p_long_processed[pin]) {
          if (input_has_long_press_events[pin] && !input_state_m_to_p_long_processed[pin] && millis() - input_long_press_timer[pin] >= LONG_PRESS_MS) {
            input_state_long[pin] = !input_state_long[pin];
            execute_actions(pin, input_state_long[pin], true);
            input_state_m_to_p_long_processed[pin] = true;
            input_state_delayed[pin] = false;
          }
          if (input_flags[pin] & FLAG_DONT_WAIT || !input_has_long_press_events[pin]) {
            if (!input_state_m_to_p_processed[pin]) {
              input_state[pin] = !input_state[pin];
              execute_actions(pin, input_state[pin], false);
              input_state_m_to_p_processed[pin] = true;
            }
          } else if (!input_state_m_to_p_long_processed[pin]) {
            input_state_delayed[pin] = true;
          }
        }
      } else {
        if (input_state_delayed[pin]) {
          input_state[pin] = !input_state[pin];
          execute_actions(pin, input_state[pin], false);
          input_state_delayed[pin] = false;
        }
        input_state_m_to_p_processed[pin] = false;
        input_state_m_to_p_long_processed[pin] = false;
        input_long_press_timer[pin] = 0;
      }
    } else if (input_state[pin] != state || input_state_long[pin] != state || input_state_delayed[pin]) {
      if (state) {
        if (input_has_long_press_events[pin] && !input_state_long[pin] && millis() - input_long_press_timer[pin] >= LONG_PRESS_MS) {
          execute_actions(pin, true, true);
          input_state_long[pin] = 1;
          input_state_delayed[pin] = false;
        }
        if (input_flags[pin] & FLAG_DONT_WAIT || !input_has_long_press_events[pin]) {
          if (!input_state[pin]) {
            execute_actions(pin, true, false);
            input_state[pin] = 1;
          }
        } else if (!input_state_long[pin]) {
          input_state_delayed[pin] = true;
        }
      } else {
        if (input_state_long[pin]) {
          execute_actions(pin, false, true);
          input_state_long[pin] = 0;
        }
        if (input_state_delayed[pin]) {
          execute_actions(pin, true, false);  // delayed execution of non-longpress actions
          input_state[pin] = 1;
          input_state_delayed[pin] = false;
        }
        if (input_state[pin]) {
          execute_actions(pin, false, false);
          input_state[pin] = 0;
        }
        input_long_press_timer[pin] = 0;
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
