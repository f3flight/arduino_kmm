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

const int ACTION_NAME_LEN_MAX = 6;
const int ACTION_KB_CODES_MAX = 7;
struct keyboard_payload {
  uint8_t len;
  uint8_t codes[ACTION_KB_CODES_MAX];
};

union payload {
  keyboard_payload keyboard;
};

typedef bool(*action_function)(payload);
typedef bool(*parse_function)(char*, char*, payload*);

struct function_link {
  char action_name[ACTION_NAME_LEN_MAX];
  action_function action;
  parse_function parser;
};

bool action_keyboard(payload input);
bool parser_keyboard(char* input, char* delims, payload* p);
bool action_mouse(payload input);
bool action_midi(payload input);

const function_link function_map[] = {
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

const char delim_str[] = " ";
const char inner_delim = ',';
const char inner_delim_str[] = ",";
const short read_buffer_len = 100;
uint8_t read_buffer[read_buffer_len] = {0};
uint8_t parse_buffer[read_buffer_len] = {0};
short read_buffer_pos = 0;

void setup() {
  Keyboard.begin();
  for (int i = 0; i < NUM_PINS; i++) {pinMode(i, INPUT); digitalWrite(i, LOW);}
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

void parse_input(const char* input, const char* delims) {  // input must be a proper 0-terminated string
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
      // TODO: parse_action corrupts piece somehow, when it doesn't parse it till the end, need to investigate
      // repro: run kb,47,48,49,50,51,52,53,54,55,56,57,58 --> will pring "running:" 2 times
      Serial.print("running: "); Serial.println(piece);
      action_function action;
      payload p;
      if (parse_action(piece, inner_delim, &action, &p)) {
        Serial.println("action parsed, executing!");
        action(p);
      }
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

bool parse_action(char* input, const char delim, action_function* action, payload* p) {
  memset(parse_buffer, 0, read_buffer_len);
  memcpy(parse_buffer, input, strlen(input));
  int input_len = strlen(parse_buffer);  // length of current (single) action piece
  char* name_len = strchr(parse_buffer, delim);
  parse_function parser = nullptr;
  bool action_found = false;
  if (name_len != NULL) {
    *name_len = 0;  //zero out first found delimiter to convert action name into 0-terminated string
  }
  for (int i=0; i < NUM_ACTIONS; i++) {
    if (strcmp(parse_buffer, function_map[i].action_name) == 0) {
      *action = function_map[i].action;
      parser = function_map[i].parser;
      action_found = true;
      Serial.print("found action - "); Serial.println(function_map[i].action_name); 
      break;
    }
  }
  if (action_found) {
    if (parser != nullptr) {  // action has a parser, should have payload
      Serial.println("action has a parser");
      if (input_len > strlen(parse_buffer) + 1) {  // there is some payload for this action
        Serial.println("there is some payload for this action");
        if (parser(parse_buffer+strlen(parse_buffer)+1, inner_delim_str, p)) {
          Serial.println("was able to parse!");
          return true;
        }
      }
    } else return true;
  }
  return false;
}

bool action_keyboard(payload p) {
  for (int i=0; i < p.keyboard.len; i++) {
    // TODO: press all won't ahndle more than 6 keys, looks like a general limitation, need to decide what to do
    // decrease codes array or add extra handling for modifier keys
    Keyboard.press(p.keyboard.codes[i]);
    Keyboard.release(p.keyboard.codes[i]);
  }
  //Keyboard.releaseAll();
}

bool parser_keyboard(char* input, char* delims, payload* p) {
  Serial.println("parsing kb action section");
  char* piece = strtok(input, delims);
  int counter = 0;
  while (piece != NULL) {
    Serial.println("found a piece");
    int code;
    if (str_to_int(piece, &code) && code >= 0) {
      Serial.println("decoded int");
      if (counter < ACTION_KB_CODES_MAX) {
        p->keyboard.codes[counter] = code;
        counter++;
      } else {  // reached limit of codes array size, cannot store any more actions
        Serial.println("Reached limit of codes per keyboard action, add new action instead.");
        p->keyboard.len = ACTION_KB_CODES_MAX;
        return true;
      }
    } else {
      Serial.println("wrong code for keyboard input, ignoring action payload");
      break;
    }
    piece = strtok(NULL, delims);
  }
  p->keyboard.len = counter;
  Serial.print("p->keyboard.len = "); Serial.println(p->keyboard.len);
  Serial.print("counter > 0"); Serial.println(counter > 0);
  return counter > 0;
}

bool action_mouse(payload p) {
  Serial.println("MO "); //Serial.println(input.var_a);
}

bool action_midi(payload p) {
  Serial.println("MI "); //Serial.println(p.midi...);
}
