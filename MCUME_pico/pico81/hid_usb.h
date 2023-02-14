#ifndef HID_USB
#define HID_USB
#include <stdint.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
void initialiseHid(byte* keyboard);
void readUsbKeyboard(void);
void injectKey(uint8_t code);
int16_t readUsbJoystick(int instance);
void joystickToKeyboard(int instance, byte up, byte down, byte left, byte right, byte button);
#ifdef __cplusplus
}
#endif

#endif