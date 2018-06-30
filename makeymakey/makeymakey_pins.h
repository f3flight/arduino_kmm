#define NUM_PINS    24

#define INPUT_D0    0
#define INPUT_D1    1
#define INPUT_D2    2
#define INPUT_D3    3
#define INPUT_D4    4
#define INPUT_D5    5
#define INPUT_CLICK 6
#define INPUT_SPACE 7
#define INPUT_AR_DN 8
#define LED_A       9
#define LED_B       10
#define LED_C       11
#define INPUT_AR_UP 12
#define INPUT_AR_LT 13
#define OUTPUT_D14  14
#define INPUT_AR_RT 15
#define INPUT_D15   15 // same pin as Right Arrow
#define OUTPUT_D16  16
#define LED_D       17
#define INPUT_A0    18
#define INPUT_A1    19
#define INPUT_A2    20
#define INPUT_A3    21
#define INPUT_A4    22
#define INPUT_A5    23

#define NUM_INPUT_PINS 18

#define NUM_LED_PINS 4

const char INPUT_PINS[] {
  INPUT_D0, INPUT_D1, INPUT_D2, INPUT_D3, INPUT_D4, INPUT_D5,
  INPUT_CLICK, INPUT_SPACE, INPUT_AR_DN, INPUT_AR_UP, INPUT_AR_LT, INPUT_AR_RT,
  INPUT_A0, INPUT_A1, INPUT_A2, INPUT_A3, INPUT_A4, INPUT_A5
};

const char LED_PINS[] {
  LED_A, LED_B, LED_C, LED_D
};
