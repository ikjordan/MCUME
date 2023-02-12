/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board.h"
#include "tusb.h"
#include "hid_host_joy.h"
#include "hid_host_info.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

#define MAX_REPORT  4

static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

void process_kbd_mount(uint8_t dev_addr, uint8_t instance)
{
  printf("keyboard mounted\n");
}

void process_kbd_unmount(uint8_t dev_addr, uint8_t instance)
{
  printf("keyboard unmounted\n");
}

// Each HID instance can has multiple reports
static struct
{
  uint8_t report_count;
  tuh_hid_report_info2_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

static void process_kbd_report(hid_keyboard_report_t const *report);
static void process_mouse_report(hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

typedef struct {
  char* ud;
  char* lr;
  char* fire;
} joysick_state_t;

void hid_app_task(void)
{
    tusb_hid_simple_joysick_t* simple_joysticks[2];
    static tusb_hid_simple_joysick_values_t prev[2] = {0};
    static joysick_state_t prev_state[2];
    joysick_state_t state = {"centre", "centre", "blank"};

    int detected = tuh_hid_get_simple_joysticks(simple_joysticks, 2);

    for (int i=0; i<detected; ++i)
    {
        if (memcmp(&(simple_joysticks[i]->values), &prev[i], sizeof(tusb_hid_simple_joysick_values_t)))
        {
          // Store new values
          memcpy(&prev[i], &(simple_joysticks[i]->values), sizeof(tusb_hid_simple_joysick_values_t));

          // Determine: 
          // up /down /centre
          // left / right / centre
          // button 1 up / down
          char* ud = "centre";
          char* lr = "centre";
          char* fire = "blank";

          uint32_t range = simple_joysticks[i]->axis_x1.logical_max - simple_joysticks[i]->axis_x1.logical_min;

          if (simple_joysticks[i]->values.x1 > (((3 * range) >> 2) + simple_joysticks[i]->axis_x1.logical_min))
          {
            state.lr = "right";
          }
          else if (simple_joysticks[i]->values.x1 < ((range >> 2) + simple_joysticks[i]->axis_x1.logical_min))
          {
            state.lr = "left";
          }

          range = simple_joysticks[i]->axis_y1.logical_max - simple_joysticks[i]->axis_y1.logical_min;
          if (simple_joysticks[i]->values.y1 > (((3 * range) >> 2) + simple_joysticks[i]->axis_y1.logical_min))
          {
            state.ud = "down";
          }
          else if (simple_joysticks[i]->values.y1 < ((range >> 2) + simple_joysticks[i]->axis_y1.logical_min))
          {
            state.ud = "up";
          }

          if (simple_joysticks[i]->values.buttons & 1)
          {
            state.fire = "fire";
          }

          if (memcmp(&prev_state[i], &state, sizeof(joysick_state_t)))
          {
            //tusb_hid_print_simple_joysick_report(simple_joysticks[i]);
            printf("Joystick %i %6s %6s %6s\n", i, state.ud, state.lr, state.fire);
          }
          memcpy(&prev_state[i], &state, sizeof(joysick_state_t));

        }
    }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

static void handle_kbd_report(tusb_hid_host_info_t* info, const uint8_t* report, uint8_t report_length, uint8_t report_id)
{
  TU_LOG1("HID receive keyboard report\r\n");
  process_kbd_report((hid_keyboard_report_t*)report);
}

void handle_keyboard_unmount(tusb_hid_host_info_t* info) {
  TU_LOG1("HID keyboard unmount\n");
  // Inform that keyboard has been removed
  process_kbd_unmount(info->key.elements.dev_addr, info->key.elements.instance);
}

void __not_in_flash_func(handle_mouse_report)(tusb_hid_host_info_t* info, const uint8_t* report, uint8_t report_length, uint8_t report_id)
{
  TU_LOG1("HID receive mouse report\r\n");
  process_mouse_report((hid_mouse_report_t const *)report);
}

void __not_in_flash_func(handle_joystick_report)(tusb_hid_host_info_t* info, const uint8_t* report, uint8_t report_length, uint8_t report_id)
{ 
  TU_LOG1("HID receive joystick report\r\n");
  tusb_hid_simple_joysick_t* simple_joystick = tuh_hid_get_simple_joystick(
    info->key.elements.dev_addr, 
    info->key.elements.instance, 
    report_id);
    
  if (simple_joystick != NULL) {
    tusb_hid_simple_joysick_process_report(simple_joystick, report, report_length);
  }
}

void handle_joystick_unmount(tusb_hid_host_info_t* info) {
  TU_LOG1("HID joystick unmount\n");
  // Free up joystick definitions
  tuh_hid_free_simple_joysticks_for_instance(info->key.elements.dev_addr, info->key.elements.instance);
}

void handle_gamepad_report(tusb_hid_host_info_t* info, const uint8_t* report, uint8_t report_length, uint8_t report_id)
{
  TU_LOG1("HID receive gamepad report ");
  for(int i = 0; i < report_length; ++i) {
    printf("%02x", report[i]);
  }
  printf("\r\n");
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  // By default host stack will use activate boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
  if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
  {
    hid_info[instance].report_count = tuh_hid_parse_report_descriptor2(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    printf("HID has %u reports \r\n", hid_info[instance].report_count);

    for (uint8_t i = 0; i < hid_info[instance].report_count; ++i) {
      tuh_hid_report_info2_t *report = &hid_info[instance].report_info[i];
      bool has_report_id = hid_info[instance].report_count > 1 || (report[0].report_id > 0);
      
      printf("HID report usage_page=%d, usage=%d, has_report_id=%d dev_addr=%d instance=%d\n", report->usage_page, report->usage, has_report_id, dev_addr, instance);
      
      if (report->usage_page == HID_USAGE_PAGE_DESKTOP)
      {
        switch (report->usage)
        {
          case HID_USAGE_DESKTOP_KEYBOARD: {
            printf("HID receive keyboard report description dev_addr=%d instance=%d\r\n", dev_addr, instance);
            tuh_hid_allocate_info(dev_addr, instance, has_report_id, &handle_kbd_report, handle_keyboard_unmount);
            process_kbd_mount(dev_addr, instance);
            break;
          }
          case HID_USAGE_DESKTOP_JOYSTICK: {
            printf("HID receive joystick report description dev_addr=%d instance=%d\r\n", dev_addr, instance);
            if(tuh_hid_allocate_info(dev_addr, instance, has_report_id, &handle_joystick_report, handle_joystick_unmount)) {
              tuh_hid_joystick_parse_report_descriptor(desc_report, desc_len, dev_addr, instance);
            }
            break;
          }
          case HID_USAGE_DESKTOP_MOUSE: {
            printf("HID receive mouse report description dev_addr=%d instance=%d\r\n", dev_addr, instance);
            tuh_hid_allocate_info(dev_addr, instance, has_report_id, &handle_mouse_report, NULL);
            break;
          }
          case HID_USAGE_DESKTOP_GAMEPAD: {
            printf("HID receive gamepad report description dev_addr=%d instance=%d\r\n", dev_addr, instance);
            tuh_hid_allocate_info(dev_addr, instance, has_report_id, &handle_gamepad_report, NULL);
            // May be able to handle this in the same was as a the joystick. Needs a little investigation
            break;
          }
          default: {
            TU_LOG1("HID usage unknown usage:%d\r\n", report->usage);
            break;
          }
        }
      }
    }  
  }
  else if ( itf_protocol == HID_ITF_PROTOCOL_KEYBOARD )
  {
     tuh_hid_allocate_info(dev_addr, instance, false, handle_kbd_report, handle_keyboard_unmount);
     process_kbd_mount(dev_addr, instance);
  }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  
  // Invoke unmount functions adn free up host info structure 
  tuh_hid_free_info(dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report( (hid_keyboard_report_t const*) report );
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report( (hid_mouse_report_t const*) report );
    break;

    default:
      TU_LOG2("HID receive boot generic report\r\n");
      // Generic report requires matching ReportID and contents with previous parsed report info
      process_generic_report(dev_addr, instance, report, len);
    break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( report->keycode[i] )
    {
      if ( find_key_in_report(&prev_report, report->keycode[i]) )
      {
        // exist in previous report means the current key is holding
      }else
      {
        // not existed in previous report means the current key is pressed
        bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        uint8_t ch = keycode2ascii[report->keycode[i]][is_shift ? 1 : 0];
        putchar(ch);
        if ( ch == '\r' ) putchar('\n'); // added new line for enter key

        fflush(stdout); // flush right away, else nanolib will wait for newline
      }
    }
    // TODO example skips key released
  }

  prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

void cursor_movement(int8_t x, int8_t y, int8_t wheel)
{
#if USE_ANSI_ESCAPE
  // Move X using ansi escape
  if ( x < 0)
  {
    printf(ANSI_CURSOR_BACKWARD(%d), (-x)); // move left
  }else if ( x > 0)
  {
    printf(ANSI_CURSOR_FORWARD(%d), x); // move right
  }

  // Move Y using ansi escape
  if ( y < 0)
  {
    printf(ANSI_CURSOR_UP(%d), (-y)); // move up
  }else if ( y > 0)
  {
    printf(ANSI_CURSOR_DOWN(%d), y); // move down
  }

  // Scroll using ansi escape
  if (wheel < 0)
  {
    printf(ANSI_SCROLL_UP(%d), (-wheel)); // scroll up
  }else if (wheel > 0)
  {
    printf(ANSI_SCROLL_DOWN(%d), wheel); // scroll down
  }

  printf("\r\n");
#else
  printf("(%d %d %d)\r\n", x, y, wheel);
#endif
}

static void process_mouse_report(hid_mouse_report_t const * report)
{
  static hid_mouse_report_t prev_report = { 0 };

  //------------- button state  -------------//
  uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  if ( button_changed_mask & report->buttons)
  {
    printf(" %c%c%c ",
       report->buttons & MOUSE_BUTTON_LEFT   ? 'L' : '-',
       report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-',
       report->buttons & MOUSE_BUTTON_RIGHT  ? 'R' : '-');
  }

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel);
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void __not_in_flash_func(process_generic_report)(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  tusb_hid_host_info_t* info = tuh_hid_get_info(dev_addr, instance);
  
  if (info == NULL)
  {
    printf("Couldn't find the host report info for dev_addr=%d instance=%d\r\n", dev_addr, instance);
    return;
  }
  
  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info2_t* rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info2_t* rpt_info = NULL;
  uint8_t rpt_id = 0;

  if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
  {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  }else
  {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    rpt_id = report[0];

    // Find report id in the array
    for(uint8_t i=0; i<rpt_count; i++)
    {
      if (rpt_id == rpt_info_arr[i].report_id )
      {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info)
  {
    printf("Couldn't find the report info for this report !\r\n");
    return;
  }

  info->handler(info, report, len, rpt_id);
}
