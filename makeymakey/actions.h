const char NUM_ACTIONS = 3;

const int ACTION_NAME_LEN_MAX = 6;
const int ACTION_KB_CODES_MAX = 3;
const int PIN_ACTIONS_MAX = 3;

const int MAX_ACTION_STRING_LENGTH = 50; // max length of a single action definition string, i.e. "midi,0,0,0,0,7f" or "kb,40,41,42,43,44", etc.

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

struct mouse_buttons {
  uint8_t left  : 1;
  uint8_t right : 1;
};

struct mouse_payload {
  uint16_t x;
  uint16_t y;
  mouse_buttons buttons;
  int8_t wheel;
};

union payload {
  keyboard_payload keyboard;
  midi_payload midi;
  mouse_payload mouse;
  uint8_t bytes[6];
};

typedef bool(*parse_function)(char*, const char, payload*);
typedef bool(*action_function)(payload, int, char);

bool parser_keyboard(char* input, const char* delims, payload* p);
bool parser_mouse(char* input, const char* delims, payload* p);
bool parser_midi(char* input, const char* delims, payload* p);
bool action_keyboard(payload input, int state, char event);
bool action_mouse(payload input, int state, char event);
bool action_midi(payload input, int state, char event);

struct function_link {
  char action_name[ACTION_NAME_LEN_MAX];
  action_function action;
  parse_function parser;
};

const function_link function_map[NUM_ACTIONS] = {
  {"kb", &action_keyboard, &parser_keyboard},
  {"mouse", &action_mouse, &parser_mouse},
  {"midi", &action_midi, &parser_midi},
};

const int NUM_EVENTS = 5;
const char EVENT_PRESS_AUTO_RELEASE = 'a';
const char EVENT_PRESS = 'p';
const char EVENT_RELEASE = 'r';
const char EVENT_LONG_PRESS = 'l';
const char EVENT_LONG_PRESS_AUTO_RELEASE = 'b';
const char EVENTS[NUM_EVENTS] = {EVENT_PRESS_AUTO_RELEASE, EVENT_PRESS, EVENT_RELEASE, EVENT_LONG_PRESS, EVENT_LONG_PRESS_AUTO_RELEASE};
/*
 *  EVENT_PRESS_AUTO_RELEASE is default and can be omitted. This allows setting press event action, and release event will be called with opposite action.
 *  i.e. if press is midi note on -> release will be midi note off
 *  Supported auto-release actions (payload transform in brackets): kb (keys released on input release); midi type NOTE_ON 0x9 (->0x8), NOTE_OFF 0x8 (->0x9), CC 0xb (data -> 0)
 *  Example:
 *  set 0 midi,b,b,0,0,7f
 *  Here, event will be set to EVENT_PRESS_AUTO_RELEASE.
 *  When input 0 is pressed, midi CC 0 0 7f will be generated.
 *  When input 0 is released, midi CC 0 0 0 will be generated, i.e. "data" is changed to "0".
 */


struct input_action {
  int action_id = -1;
  char event = EVENT_PRESS_AUTO_RELEASE;
  payload p;
  bool execute(bool state, bool long_event) {
    if (action_id < 0) {return false;} // action not defined, incorrect invocation
    _p(F("consider executing: action = ")); _p(function_map[action_id].action_name); _p(F(", event = ")); _p(event); _p(F(", state = ")); _p(state); _p(F(", long_event = ")); _pn(long_event);
    if (long_event && event != EVENT_LONG_PRESS && event != EVENT_LONG_PRESS_AUTO_RELEASE) {return false;} // got long press/release event, skipping non-long-press events
    if (!long_event && (event == EVENT_LONG_PRESS || event == EVENT_LONG_PRESS_AUTO_RELEASE)) {return false;} // and vice versa
    if (state && (event == EVENT_RELEASE)) {return false;} // ignoring press for release events
    if (!state && (event == EVENT_PRESS || event == EVENT_LONG_PRESS)) {return false;} // and vice versa
    _pn(F("checks passed, executing..."));
    return function_map[action_id].action(p, state, event);
  }
};

input_action pin_actions[NUM_PINS][PIN_ACTIONS_MAX] = {};
input_action parsed_action;

bool parse_action(char* input, const char* delims, input_action* action) {
  _pn(F("parsing action"));
  bool found = false;
  parse_function parser = nullptr;
  int len = strlen(input);
  char event = EVENT_PRESS_AUTO_RELEASE; // default to "autorelease" mode where we fire release events based on press event's payload (or just fire the same event)
  char input_copy[MAX_ACTION_STRING_LENGTH] = {0}; // need to copy input because this is a nested call to strtok, with a different delim
  if (strlen(input) > MAX_ACTION_STRING_LENGTH) {
    _p(F("Action string |")); _p(input); _p(F("| is too long, ignoring, sorry! Max length = ")); _pn(MAX_ACTION_STRING_LENGTH);
  }
  strcpy(input_copy, input);
  char* saveptr;
  char* token = strtok_r(input_copy, delims, &saveptr);
  int input_offset = strlen(token) + 1; // we will need this to pass pointer to a nested function to allow further strtok in there
  if (token != NULL) {
    _p(F("Found item = ")); _pn(token);
    for (int j=0; j < NUM_EVENTS; j++) {
      _pn(EVENTS[j]);
      _pn(char(*token));
      if (char(*token) == EVENTS[j]) {
        event = EVENTS[j];
        token = strtok_r(NULL, delims, &saveptr);
        if (token == NULL) { // moving to next item, if fails = input incomplete
          _p(F("Found event '")); _p(event); _pn(F("' but no action after it, ignoring"));
          return false;
        } else {
          _p(F("Found item = ")); _pn(token);
          input_offset += strlen(token) + 1;
        }
        break;
      }
    }
    for (int i=0; i < NUM_ACTIONS; i++) {
      if (strcmp(token, function_map[i].action_name) == 0) {
        action->action_id = i;
        action->event = event;
        parser = function_map[i].parser;
        _p(F("found action - ")); _pn(function_map[i].action_name);
        if (parser != nullptr) {  // action has a parser, need to parse payload
          if (len >= input_offset) {  // there is some payload for this action
            _pn(F("there is some payload for this action"));
            if (parser(input_copy + input_offset, delims, &(action->p))) {
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

bool parser_keyboard(char* input, const char* delims, payload* p) {
  _pn(F("parsing kb action section"));
  int counter = 0;
  char* saveptr;
  char* token = strtok_r(input, delims, &saveptr);
  while (token != NULL) {
    _p(F("delims: ")); _pn(delims);
    _p(F("found a piece: ")); _pn(token);
    int code;
    if (str_to_int(token, &code, 10) && code >= 0) {
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
    token = strtok_r(NULL, delims, &saveptr);
  }
  p->keyboard.len = counter;
  _p(F("p->keyboard.len = ")); _pn(p->keyboard.len);
  _p(F("counter > 0")); _pn(counter > 0);
  return counter > 0;
}

bool action_keyboard(payload p, int state, char event) {
  // does not support "release" events for now, will do nothing (will release button without pressing)
  _pn(F("KEYBOARD"));
  for (int i=0; i < p.keyboard.len; i++) {
    if (state) {Keyboard.press(p.keyboard.codes[i]);}
    else Keyboard.release(p.keyboard.codes[i]);
  }
}

bool parser_mouse(char* input, const char* delims, payload* p) {
  _pn(F("parsing mouse action section"));
  int counter = 0;
  char* saveptr;
  char* token = strtok_r(input, delims, &saveptr);
  p->mouse.wheel = 0;
  p->mouse.buttons.left = 0;
  p->mouse.buttons.right = 0;
  bool bad_input = false;
  while (token != NULL) {
    _p(F("found a piece: ")); _pn(token);
    int code;
    if (str_to_int(token, &code, 10)) {
      _pn(F("decoded dec int"));
      switch (counter) {
        case 0: if (code >= -1 && code <= 1023) {p->mouse.x = code;}
                else {bad_input = true;}
                break;
        case 1: if (code >= -1 && code <= 1023) {p->mouse.y = code;}
                else {bad_input = true;}
                break;
        case 2: if (code >= -127 && code <= 127) {p->mouse.wheel = code;}
                else {bad_input = true;}
                break;
        case 3: p->mouse.buttons.left = (code > 0) ? 1 : 0; break;
        case 4: p->mouse.buttons.right = (code > 0) ? 1 : 0; return true;
      }
    } else {
      bad_input = true;
    }
    if (bad_input) {
      _p(F("wrong value for mouse input at position ")); _p(counter); _p(F(": ")); _p(token); _pn(F("; ignoring action payload"));
      return false;
    }
    counter++;
    token = strtok_r(NULL, delims, &saveptr);
  }
  if (counter >= 2) {
    return true;
  } else {
    return false;
  }
}

bool action_mouse(payload p, int state, char event) {
  // does not support "release" events for now, will do nothing
  _pn(F("MOUSE"));
  if (!state) {return false;}
  Mouse.move(p.mouse.x, p.mouse.y, p.mouse.wheel);
  if (p.mouse.buttons.left > 0) {Mouse.click();}
}

bool parser_midi(char* input, const char* delims, payload* p) {
  _pn(F("parsing midi action section"));
  int counter = 0;
  char* saveptr;
  char* token = strtok_r(input, delims, &saveptr);
  while (token != NULL) {
    _p(F("found a piece: ")); _pn(token);
    int code;
    if (str_to_int(token, &code, 16) && code >= 0 && code <= 127) {
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
      _pn(F("wrong code for midi input: ")); _p(token); _pn(F("; ignoring action payload"));
      break;
    }
    counter++;
    token = strtok_r(NULL, delims, &saveptr);
  }
  return false;
}

bool action_midi(payload p, int state, char event) {
  // does not support "release" events for now, will work as if it was a release of a "press" event
  _pn(F("MIDI"));
  uint8_t type = p.midi.type;
  uint8_t cin = p.midi.cin;
  uint8_t data2 = p.midi.data2;
  if (event == EVENT_PRESS_AUTO_RELEASE || event == EVENT_LONG_PRESS_AUTO_RELEASE) {
    if (p.midi.type == 0x9) type = cin = (state) ? 0x9 : 0x8;
    if (p.midi.type == 0x8) type = cin = (state) ? 0x8 : 0x9;
    if (p.midi.type == 0xb) data2 = (state) ? data2 : 0;
  }
  midiEventPacket_t midi_event = {cin, type << 4 | p.midi.channel, p.midi.data1, data2};
  MidiUSB.sendMIDI(midi_event);
  MidiUSB.flush();
}
