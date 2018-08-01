#include "MouseAbs.h"

#if defined(_USING_HID)

static const uint8_t _hidReportDescriptor[] PROGMEM = {
  
  //  Mouse
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)  // 54
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x85, 0x01,                    //     REPORT_ID (1)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
// section which differs starts here
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
/*
  For some reason:
  - 0,0 doesn't work, so minimum of -1 allows moving to X,Y=0,0 via X,Y=-1,-1
  - negative values always move to 0,0 so there's no benefit in
    having -127 as the mininum.
  - finer granulariry may be implemented but requires
    non-backward compatible changes in the code to switch to 16-bit values
*/
    0x16, 0xff, 0xff,              //     LOGICAL_MINIMUM (-1) - allows moving to 0,0
    0x26, 0xff, 0x03,              //     LOGICAL_MAXIMUM (1023)
    0x36, 0xff, 0xff,              //     PHYSICAL_MINIMUM (-1) - allows moving to 0,0 
    0x46, 0xff, 0x03,              //     PHYSICAL_MAXIMUM (1023)
    0x75, 0x10,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x09, 0x38,                    //     USAGE (Wheel)  
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x35, 0x81,                    //     PHYSICAL_MINIMUM (-127)
    0x45, 0x7f,                    //     PHYSICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
// ends here
    0xc0,                          //   END_COLLECTION
    0xc0,                          // END_COLLECTION
};

//================================================================================
//================================================================================
//  Mouse

Mouse_::Mouse_(void) : _buttons(0)
{
    static HIDSubDescriptor node(_hidReportDescriptor, sizeof(_hidReportDescriptor));
    HID().AppendDescriptor(&node);
}

void Mouse_::begin(void) 
{
}

void Mouse_::end(void) 
{
}

void Mouse_::click(uint8_t b)
{
  _buttons = b;
  move(0,0,0);
  _buttons = 0;
  move(0,0,0);
}

void Mouse_::move(uint16_t x, uint16_t y, signed char wheel)
{
  uint8_t m[6];
  m[0] = _buttons;
  m[1] = x & 0xff;
  m[2] = x >> 8;
  m[3] = y & 0xff;
  m[4] = y >> 8;
  char p[100] = {0};
  sprintf(p,"_buttons = 0x%2x, x = %d, y = %d, wheel = %d", _buttons, x, y, wheel);
  Serial.println(p);
  m[5] = wheel;
  HID().SendReport(1,m,6);
}

void Mouse_::buttons(uint8_t b)
{
  if (b != _buttons)
  {
    _buttons = b;
    move(0,0,0);
  }
}

void Mouse_::press(uint8_t b) 
{
  buttons(_buttons | b);
}

void Mouse_::release(uint8_t b)
{
  buttons(_buttons & ~b);
}

bool Mouse_::isPressed(uint8_t b)
{
  if ((b & _buttons) > 0) 
    return true;
  return false;
}

Mouse_ Mouse;

#endif
