// Keyboard - Version: Latest 
#include <Keyboard.h>

// Mouse - Version: Latest 
#include <Mouse.h>

// MIDIUSB - Version: Latest 
#include <MIDIUSB.h>
#include <frequencyToNote.h>
#include <pitchToFrequency.h>
#include <pitchToNote.h>


#include "makeymakey_pins.h"
#include "utility.h"

bool input_state[NUM_INPUT_PINS];

const char NUM_ACTIONS = 3;

const int ACTION_NAME_LEN_MAX = 6;
const int ACTION_KB_CODES_MAX = 6;
struct keyboard_payload {
  uint8_t len;
  uint8_t codes[ACTION_KB_CODES_MAX];
};

union payload {
  keyboard_payload keyboard;
};

typedef bool(*parse_function)(char*, const char, payload*);
typedef bool(*action_function)(payload);

struct function_link {
  char action_name[ACTION_NAME_LEN_MAX];
  action_function action;
  parse_function parser;
};

bool parser_keyboard(char* input, const char delim, payload* p);
bool action_keyboard(payload input);
bool action_mouse(payload input);
bool action_midi(payload input);

const function_link function_map[NUM_ACTIONS] = {
  {"kb", &action_keyboard, &parser_keyboard},
  {"mouse", &action_mouse},
  {"midi", &action_midi},
};

struct input_action {
  action_function action;
  struct payload p;
};

input_action pin_actions[NUM_PINS][4] = {};
input_action run_action;

const char DELIM = ' ';
const char INNER_DELIM = ',';
const short READ_BUFFER_LEN = 100;
uint8_t read_buffer[READ_BUFFER_LEN] = {0};
uint8_t parse_buffer[READ_BUFFER_LEN] = {0};
short read_buffer_pos = 0;

void setup() {
  Keyboard.begin();
  for (int i = 0; i < NUM_PINS; i++) {pinMode(i, INPUT); digitalWrite(i, LOW);}
  pinMode(OUTPUT_D14, OUTPUT); digitalWrite(OUTPUT_D14, LOW);
  pinMode(OUTPUT_D16, OUTPUT); digitalWrite(OUTPUT_D16, LOW);
}

void loop() {
    for (int i = 0; i < NUM_INPUT_PINS; i++) {
      bool state = (digitalRead(INPUT_PINS[i]) == LOW);
      if (input_state[i] != state) {
        Serial.print("input "); Serial.print(INPUT_PINS[i]); Serial.print(" new state "); Serial.println(state);
        input_state[i] = state;
        // int action = (state) ? 0x90 : 0x80;
        // midiEventPacket_t noteOn = {0x09, action | 0, 48 + i, 127};
        // MidiUSB.sendMIDI(noteOn);
        // MidiUSB.flush();
      }
    }
    uint8_t input[READ_BUFFER_LEN] = {0};
    int out_len;
    if (serial_readbytes_until(read_buffer, READ_BUFFER_LEN, &read_buffer_pos, input, &out_len)) {
      Serial.print("got a line ["); Serial.print((char*)input); Serial.println("]");
      if (input[0] != '#') parse_input(input, DELIM);  // parsing input if it's not a comment
    } else {
      //Serial.println("no input this time..."); 
    }
    delay(50);
}

void parse_input(char* input, const char delim) {  // input must be a proper 0-terminated string
  int buffer_pos = 0;
  bool get = false, set = false, test = false, run = false;
  int counter = 0;
  int input_num = 0;
  action_function action;
  payload p;
  while (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    if (counter == 0) {
      if (strcmp(parse_buffer, "get") == 0) {get = true;}
      else if (strcmp(parse_buffer, "set") == 0) {set = true;}
      else if (strcmp(parse_buffer, "test") == 0) {test = true;}
      else if (strcmp(parse_buffer, "run") == 0) {run = true;}
      else {
        _pn("don't know whatcha talkin' 'bout!");
        break;  // unknown primary action, exiting
      }
    } else if (run) {
      _p("running: "); _pn(parse_buffer);
      if (parse_action(parse_buffer, INNER_DELIM, &action, &p)) {
        _pn("parsed action, executing");
        action(p);
      }
    } else if (get || (set && counter == 1) || test) {
      if (!str_to_int(parse_buffer, &input_num) || input_num < 0 || input_num > NUM_INPUT_PINS - 1) {
        _pn("Are you trying to break me? This input pin number is bad.");
        break;
      }
      //Serial.print("input pin = "); Serial.println(input_num);
      if (get) {
        _p("getting config of input "); _pn(input_num);
      } else if (set) {
        _p("clearing config for input "); _pn(input_num);
      } else if (test) {
        _p("simulating trigger of input "); _pn(input_num);
      }
    } else if (set) {
      _p("setting config for input "); _p(input_num); _p(": "); _pn(parse_buffer);
    }
    counter++;
  }
}

bool parse_action(char* input, const char delim, action_function* action, payload* p) {
  _pn("parsing action");
  bool found = false;
  parse_function parser = nullptr;
  int buffer_pos = 0;
  int len = strlen(input);
  if (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    for (int i=0; i < NUM_ACTIONS; i++) {
      if (strcmp(parse_buffer, function_map[i].action_name) == 0) {
        *action = function_map[i].action;
        parser = function_map[i].parser;
        _p("found action - "); _pn(function_map[i].action_name);
        if (parser != nullptr) {  // action has a parser, need to parse payload
          if (buffer_pos < len) {  // there is some payload for this action
            _pn("there is some payload for this action");
            if (parser(input+buffer_pos+1, delim, p)) {
              _pn("was able to parse!");
              return true;
            }
          }
        } else return true;  // action without parser = no payload parsing needed
        break;
      }
    }
  }
  return false;
}

bool parser_keyboard(char* input, const char delim, payload* p) {
  _pn("parsing kb action section");
  int counter = 0;
  int buffer_pos = 0;
  while (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    _p("found a piece: "); _pn(parse_buffer);
    int code;
    if (str_to_int(parse_buffer, &code) && code >= 0) {
      _pn("decoded int");
      if (counter < ACTION_KB_CODES_MAX) {
        p->keyboard.codes[counter] = code;
        counter++;
      } else {  // reached limit of codes array size, cannot store any more actions
        _pn("Reached limit of codes per keyboard action:"); _p(ACTION_KB_CODES_MAX); _pn("; Add new action instead.");
        p->keyboard.len = ACTION_KB_CODES_MAX;
        return true;
      }
    } else {
      _pn("wrong code for keyboard input, ignoring action payload");
      break;
    }
  }
  p->keyboard.len = counter;
  _p("p->keyboard.len = "); _pn(p->keyboard.len);
  _p("counter > 0"); _pn(counter > 0);
  return counter > 0;
}

bool action_keyboard(payload p) {
  for (int i=0; i < p.keyboard.len; i++) {
    Keyboard.press(p.keyboard.codes[i]);
    //Keyboard.release(p.keyboard.codes[i]);
  }
  Keyboard.releaseAll();
}

bool action_mouse(payload p) {
  Serial.println("MO "); //Serial.println(input.var_a);
}

bool action_midi(payload p) {
  Serial.println("MI "); //Serial.println(p.midi...);
}
