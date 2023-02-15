#include <stdlib.h>
#include "tusb.h"
#include "hid_usb.h"
#include "hid_app.h"

typedef struct  {
    unsigned char line;
    unsigned char key;
} Contact_t;

typedef struct {
    uint8_t keycode;
    uint8_t contacts;
    Contact_t contact[2];
} HidKey_t;

HidKey_t keys[] = {
    { HID_KEY_SHIFT_LEFT,  1, { {0, 0} }},
    { HID_KEY_SHIFT_RIGHT, 1, { {0, 0} }},
    { HID_KEY_Z,           1, { {0, 1} }},
    { HID_KEY_X,           1, { {0, 2} }},
    { HID_KEY_C,           1, { {0, 3} }},
    { HID_KEY_V,           1, { {0, 4} }},

    { HID_KEY_A,           1, { {1, 0} }},
    { HID_KEY_S,           1, { {1, 1} }},
    { HID_KEY_D,           1, { {1, 2} }},
    { HID_KEY_F,           1, { {1, 3} }},
    { HID_KEY_G,           1, { {1, 4} }},

    { HID_KEY_Q,           1, { {2, 0} }},
    { HID_KEY_W,           1, { {2, 1} }},
    { HID_KEY_E,           1, { {2, 2} }},
    { HID_KEY_R,           1, { {2, 3} }},
    { HID_KEY_T,           1, { {2, 4} }},
  
    { HID_KEY_1,           1, { {3, 0} }},
    { HID_KEY_2,           1, { {3, 1} }},
    { HID_KEY_3,           1, { {3, 2} }},
    { HID_KEY_4,           1, { {3, 3} }},
    { HID_KEY_5,           1, { {3, 4} }},

    { HID_KEY_0,           1, { {4, 0} }},
    { HID_KEY_9,           1, { {4, 1} }},
    { HID_KEY_8,           1, { {4, 2} }},
    { HID_KEY_7,           1, { {4, 3} }},
    { HID_KEY_6,           1, { {4, 4} }},

    { HID_KEY_P,           1, { {5, 0} }},
    { HID_KEY_O,           1, { {5, 1} }},
    { HID_KEY_I,           1, { {5, 2} }},
    { HID_KEY_U,           1, { {5, 3} }},
    { HID_KEY_Y,           1, { {5, 4} }},

    { HID_KEY_ENTER,       1, { {6, 0} }},
    { HID_KEY_L,           1, { {6, 1} }},
    { HID_KEY_K,           1, { {6, 2} }},
    { HID_KEY_J,           1, { {6, 3} }},
    { HID_KEY_H,           1, { {6, 4} }},

    { HID_KEY_SPACE,       1, { {7, 0} }},
    { HID_KEY_ALT_RIGHT,   1, { {7, 1} }},
    { HID_KEY_M,           1, { {7, 2} }},
    { HID_KEY_N,           1, { {7, 3} }},
    { HID_KEY_B,           1, { {7, 4} }},
  
    { HID_KEY_BACKSPACE,   2, { {0, 0}, {4, 0} }},
    { HID_KEY_PAUSE,       2, { {0, 0}, {7, 0} }},
    { HID_KEY_COMMA,       2, { {7, 1}, {7, 3} }},
    { HID_KEY_PERIOD,      2, { {7, 1}, {7, 2} }},
    { HID_KEY_SLASH,       2, { {7, 1}, {0, 4} }},
    { HID_KEY_SEMICOLON,   2, { {7, 1}, {5, 1} }},
    { HID_KEY_APOSTROPHE,  2, { {7, 1}, {4, 3} }},
    { HID_KEY_MINUS,       2, { {7, 1}, {6, 3} }},
    { HID_KEY_EQUAL,       2, { {7, 1}, {6, 1} }},
    { HID_KEY_EUROPE_1,    2, { {7, 1}, {3, 2} }},

    { HID_KEY_ARROW_LEFT,  2, { {0, 0}, {3, 4} }},
    { HID_KEY_ARROW_DOWN,  2, { {0, 0}, {4, 4} }},
    { HID_KEY_ARROW_UP,    2, { {0, 0}, {4, 3} }},
    { HID_KEY_ARROW_RIGHT, 2, { {0, 0}, {4, 2} }}
};

const static unsigned int KEYS_LEN = sizeof(keys) / sizeof(HidKey_t);
const static unsigned int KEY_SIZE = sizeof(HidKey_t);

static bool keys_sorted  = false;

static byte* matrix=0;

static int keys_comparator(const HidKey_t* k1, const HidKey_t* k2) {
    int kk1 = (int)k1->keycode;
    int kk2 = (int)k2->keycode;
    return kk2 - kk1;
}

static int keys_v_comparator(const void *k1, const void *k2) {
    return keys_comparator((HidKey_t*)k1, (HidKey_t*)k2);
}

static void sortHidKeys()
{
    if (!keys_sorted)
    {
        qsort(keys, KEYS_LEN, KEY_SIZE, keys_v_comparator);
    }
    keys_sorted = true;
}

static HidKey_t* findKey(const unsigned char keycode)
{
    if (keycode <= 1)
        return 0;
    const HidKey_t k0 = {keycode, 0, 0};
    return (HidKey_t *) bsearch(&k0, keys, KEYS_LEN, KEY_SIZE, keys_v_comparator);
}

static void reset() 
{
    for(int i = 0; i < 8; ++i)
    {
        matrix[i] = 0xff;
    }
}

static void press(unsigned int line, unsigned int key) 
{
    matrix[line] &= ~(1 << key);
}

// Public interface
void initialiseHid(byte* keyboard)
{
    matrix = keyboard;
    sortHidKeys();
}

void readUsbKeyboard(void)
{
    hid_keyboard_report_t report;
    int r = 0;

    hid_app_get_latest_keyboard_report(&report);

    const unsigned char m = report.modifier;
    reset();

    if (m & 0x22) press(0, 0); // Shift

    for(unsigned int i = 0; i < 6; ++i)
    {
        const HidKey_t* k = findKey(report.keycode[i]);
        if (k)
        {
            for (uint32_t c = 0; c < k->contacts; ++c)
            {
                const Contact_t* contact = &k->contact[c];
                press(contact->line, contact->key);
            }
        }
    }
}

int16_t keyboardToJoystick()
{
    hid_keyboard_report_t report;
    int16_t result = 0;

    hid_app_get_latest_keyboard_report(&report);

    // 5, 6, 7, 8 or direction key emulates joystick
    // Enter or 0 emulates joystick button
    for(unsigned int i = 0; i < 6; ++i)
    {
        switch(report.keycode[i])
        {
            case HID_KEY_5:
            case HID_KEY_ARROW_LEFT:
                result |= MASK_JOY_LEFT;
            break;

            case HID_KEY_6:
            case HID_KEY_ARROW_DOWN:
                result |= MASK_JOY_DOWN;
            break;

            case HID_KEY_7:
            case HID_KEY_ARROW_UP:
                result |= MASK_JOY_UP;
            break;

            case HID_KEY_8:
            case HID_KEY_ARROW_RIGHT:
                result |= MASK_JOY_RIGHT;
            break;

            case HID_KEY_0:
            case HID_KEY_ENTER:
                result |= MASK_JOY_BTN;
            break;
        }
    }
    return result;
}

void injectKey(uint8_t code)
{
    const HidKey_t* k = findKey(code);
    if (k)
    {
        for (uint32_t c = 0; c < k->contacts; ++c)
        {
            const Contact_t* contact = &k->contact[c];
            press(contact->line, contact->key);
        }
    }
}

int16_t readUsbJoystick(int instance)
{
    int16_t result = 0;
    joystick_state_t report;

    if (hid_app_get_latest_joystick_state(&report, instance))
    {
        result = report.button + report.lr + report.ud;
    }
    return result;
}

void joystickToKeyboard(int instance, byte up, byte down, byte left, byte right, byte button)
{
  int16_t val = readUsbJoystick(instance);
  if (val)
  {
    if (val & MASK_JOY_RIGHT)
    {
      injectKey(right);
    } else if (val & MASK_JOY_LEFT)
    {
      injectKey(left);
    }
    if (val & MASK_JOY_UP)
    {
      injectKey(up);
    } else if (val & MASK_JOY_DOWN)
    {
      injectKey(down);
    }
    if (val & MASK_JOY_BTN)
    {
      injectKey(button);
    }
  }
}
