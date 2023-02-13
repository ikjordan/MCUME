#ifndef HID_USB
#define HID_USB
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
void initialiseHid(byte* keyboard);
void readUsbKeyboard(void);
void menu_joystick(void);
#ifdef __cplusplus
}
#endif

#endif