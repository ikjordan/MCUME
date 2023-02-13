#ifndef HID_USB
#define HID_USB
#include <stdint.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
void initialiseHid(byte* keyboard);
void readUsbKeyboard(void);
int16_t readUsbJoystick(int instance);
#ifdef __cplusplus
}
#endif

#endif