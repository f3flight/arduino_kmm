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

bool input_state[NUM_INPUT_PINS] = {false};

const char NUM_ACTIONS = 3;

const int ACTION_NAME_LEN_MAX = 6;
const int ACTION_KB_CODES_MAX = 6;
const int PIN_ACTIONS_MAX = 3;

struct keyboard_payload {
  uint8_t len;
  uint8_t codes[ACTION_KB_CODES_MAX];
};

struct midi_payload {
  uint8_t cin;  // "Code Index Number", 4 bits - used specifically in USB MIDI implementation and in most cases duplicates type value.
  uint8_t type: 4;
  uint8_t channel: 4;
  uint8_t data1;
  uint8_t data2;
};

union payload {
  keyboard_payload keyboard;
  midi_payload midi;
};

typedef bool(*parse_function)(char*, const char, payload*);
typedef bool(*action_function)(payload, bool);

struct function_link {
  char action_name[ACTION_NAME_LEN_MAX];
  action_function action;
  parse_function parser;
};

bool parser_keyboard(char* input, const char delim, payload* p);
bool parser_midi(char* input, const char delim, payload* p);
bool action_keyboard(payload input, bool state);
bool action_mouse(payload input, bool state);
bool action_midi(payload input, bool state);

const function_link function_map[NUM_ACTIONS] = {
  {"kb", &action_keyboard, &parser_keyboard},
  {"mouse", &action_mouse},
  {"midi", &action_midi, &parser_midi},
};

struct input_action {
  action_function action;
  payload p;
  bool execute(bool state) {
    return action(p, state);
  }
};

input_action pin_actions[NUM_PINS][PIN_ACTIONS_MAX] = {};
input_action parsed_action;

const char DELIM = ' ';
const char INNER_DELIM = ',';
const short READ_BUFFER_LEN = 100;
uint8_t read_buffer[READ_BUFFER_LEN] = {0};
uint8_t parse_buffer[READ_BUFFER_LEN] = {0};
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
      bool state = (digitalRead(INPUT_PINS[pin]) == LOW);
      if (input_state[pin] != state) {
        _p(F("input ")); _p(pin); _p(F(" new state ")); _pn(state);
        input_state[pin] = state;
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          if (pin_actions[pin][i].action == nullptr) break;
          pin_actions[pin][i].execute(state);
        }
        // int action = (state) ? 0x90 : 0x80;
        // midiEventPacket_t noteOn = {0x09, action | 0, 48 + pin, 127};
        // MidiUSB.sendMIDI(noteOn);
        // MidiUSB.flush();
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

void parse_input(char* input, const char delim) {  // input must be a proper 0-terminated string
  int buffer_pos = 0;
  bool get = false, set = false, test = false, run = false;
  int counter = 0;
  int input_num = 0;
  while (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    if (counter == 0) {
      if (strcmp(parse_buffer, "get") == 0) {get = true;}
      else if (strcmp(parse_buffer, "set") == 0) {set = true;}
      else if (strcmp(parse_buffer, "test") == 0) {test = true;}
      else if (strcmp(parse_buffer, "run") == 0) {run = true;}
      else if (strcmp(parse_buffer, "info") == 0) {debug_info(); break;}
      else {
        _pn(F("don't know whatcha talkin' 'bout!"));
        break;  // unknown primary action, exiting
      }
    } else if (run) {
      _p(F("running: ")); _pn(parse_buffer);
      if (parse_action(parse_buffer, INNER_DELIM, &(parsed_action.action), &(parsed_action.p))) {
        _pn(F("parsed action, executing"));
        parsed_action.execute(true);
        parsed_action.execute(false);
      }
    } else if (get || (set && counter == 1) || test) {
      if (!str_to_int(parse_buffer, &input_num, 10) || input_num < 0 || input_num > NUM_INPUT_PINS - 1) {
        _pn(F("Are you trying to break me? This input pin number is bad."));
        break;
      }
      //_p(F("input pin = ")); _pn(input_num);
      if (get) {
        _p(F("getting config of input ")); _pn(input_num);
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          if (pin_actions[input_num][i].action == nullptr) break;
          _p(F("has ")); _p(pin_actions[input_num][i].p.keyboard.len); _pn(F(" keyboard codes saved"));
        }
      } else if (set) {
        _p(F("clearing config for input ")); _pn(input_num);
        for (int i = 0; i < PIN_ACTIONS_MAX; i++) {
          pin_actions[input_num][i].action = nullptr;
        }
      } else if (test) {
        _p(F("simulating trigger of input ")); _pn(input_num);
      }
    } else if (set) {
      if (counter - 2 >= PIN_ACTIONS_MAX) {
        _p(F("Too many actions, max allowed: ")); _p(PIN_ACTIONS_MAX); _pn(F(", ignoring rest..."));
        break;
      }
      _p(F("setting config for input ")); _p(input_num); _p(F(": ")); _pn(parse_buffer);
      if (parse_action(parse_buffer, INNER_DELIM, &(parsed_action.action), &(parsed_action.p))) {
        _pn(F("parsed action, setting"));
        pin_actions[input_num][counter-2] = parsed_action;
      }
    }
    counter++;
  }
}

bool parse_action(char* input, const char delim, action_function* action, payload* p) {
  _pn(F("parsing action"));
  bool found = false;
  parse_function parser = nullptr;
  int buffer_pos = 0;
  int len = strlen(input);
  if (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    for (int i=0; i < NUM_ACTIONS; i++) {
      if (strcmp(parse_buffer, function_map[i].action_name) == 0) {
        *action = function_map[i].action;
        parser = function_map[i].parser;
        _p(F("found action - ")); _pn(function_map[i].action_name);
        if (parser != nullptr) {  // action has a parser, need to parse payload
          if (buffer_pos < len) {  // there is some payload for this action
            _pn(F("there is some payload for this action"));
            if (parser(input+buffer_pos+1, delim, p)) {
              _pn(F("was able to parse!"));
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
  _pn(F("parsing kb action section"));
  int counter = 0;
  int buffer_pos = 0;
  while (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    _p(F("found a piece: ")); _pn(parse_buffer);
    int code;
    if (str_to_int(parse_buffer, &code, 10) && code >= 0) {
      _pn(F("decoded int"));
      if (counter < ACTION_KB_CODES_MAX) {
        p->keyboard.codes[counter] = code;
        counter++;
      } else {  // reached limit of codes array size, cannot store any more actions
        _pn(F("Reached limit of codes per keyboard action:")); _p(ACTION_KB_CODES_MAX); _pn(F("; Add new action instead."));
        p->keyboard.len = ACTION_KB_CODES_MAX;
        return true;
      }
    } else {
      _pn(F("wrong code for keyboard input, ignoring action payload"));
      break;
    }
  }
  p->keyboard.len = counter;
  _p(F("p->keyboard.len = ")); _pn(p->keyboard.len);
  _p(F("counter > 0")); _pn(counter > 0);
  return counter > 0;
}

bool action_keyboard(payload p, bool state) {
  _pn(F("KEYBOARD"));
  for (int i=0; i < p.keyboard.len; i++) {
    if (state) {Keyboard.press(p.keyboard.codes[i]);}
    else Keyboard.release(p.keyboard.codes[i]);
  }
}

bool action_mouse(payload p, bool state) {
  _pn(F("MOUSE"));
  for (int i = 0; i < 20; i++) {
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
    Mouse.move(-127, -127, 0);
  }
  Mouse.move(127,127);
}

bool parser_midi(char* input, const char delim, payload* p) {
  _pn(F("parsing midi action section"));
  int counter = 0;
  int buffer_pos = 0;
  while (strtok_better(parse_buffer, READ_BUFFER_LEN, &buffer_pos, input, delim)) {
    _p(F("found a piece: ")); _pn(parse_buffer);
    int code;
    if (str_to_int(parse_buffer, &code, 16) && code >= 0 && code <= 127) {
      _pn(F("decoded hex int"));
      if (counter <= 2 && code > 15) {_pn(F("first 3 midi codes shoud be 'f' or less! Wrong data, ignoring payload.")); return false;}
      switch (counter) {
        case 0: p->midi.cin = code; break;
        case 1: p->midi.type = code; break;
        case 2: p->midi.channel = code; break;
        case 3: p->midi.data1 = code; break;
        case 4: p->midi.data2 = code; return true;  // already collected all values, ignoring rest of data
      }
    } else {
      _pn(F("wrong code for midi input: ")); _p(parse_buffer); _pn(F("; ignoring action payload"));
      break;
    }
    counter++;
  }
  return false;
}

bool action_midi(payload p, bool state) {
  _pn(F("MIDI"));
  uint8_t type = p.midi.type;
  uint8_t cin = p.midi.cin;
  if (p.midi.type == 0x9) type = cin = (state) ? 0x9 : 0x8;
  if (p.midi.type == 0x8) type = cin = (state) ? 0x8 : 0x9;
  midiEventPacket_t event = {cin, type << 4 | p.midi.channel, p.midi.data1, p.midi.data2};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}

void debug_info() {
   _p(F("size of pin config: ")); _pn(sizeof(pin_actions));
}
