
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"


#define PS2_CLK_PIN 22
#define PS2_DATA_PIN 15

static const char ps2_scancodes[] = {
  // 0x0
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 
  // 0x10
  '\0', '\0', '\0', '\0', '\0', 'q',  '1',  '\0', '\0', '\0', 'z',  's',  'a',  'w',  '2',  '\0', 
  // 0x20
  '\0', 'c',  'x',  'd',  'e',  '4',  '3',  '\0', '\0', ' ',  'v',  'f',  't',  'R',  '5',  '\0', 
  // 0x30
  '\0', 'n',  'b',  'h',  'g',  'y',  '6',  '\0', '\0', '\0', 'm',  'j',  'u',  '7',  '8',  '\0', 
  // 0x40
  '\0', ',',  'k',  'i',  'o',  '0',  '9',  '\0', '\0', '.',  '/',  'l',  ';',  'p',  '\0', '\0', 
  // 0x50
  '\0', '\0', '\'', '\0', '[',  '\0', '\0', '\0', '\0', '\0', '\n', ']',  '\0', '\0', '\0', '\0', 
  // 0x60
  '\0', '\0', '\0', '\0', '\0', '\0', '\b', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 
  };

gpio_config_t ps2_pin_config = {
  (uint64_t)  ((1UL << PS2_DATA_PIN) |
	      (1UL << PS2_CLK_PIN)), // pin_bit_mask
  (gpio_mode_t) GPIO_MODE_INPUT, // mode
  (gpio_pullup_t) 0, // pull_up_en
  (gpio_pulldown_t) 0, // pull_down_en
  (gpio_int_type_t) GPIO_INTR_POSEDGE// intr_type
};

static QueueHandle_t ps2_key_queue = NULL;
static int blocked = 0;

static void interrupt_respond(void* arg) {
  return;
  static int ps2_bits_count = 0;
  static int ps2_data = 0;
  
  ps2_data |= (gpio_get_level(PS2_DATA_PIN) << ps2_bits_count);
  ps2_bits_count++;
  if (ps2_bits_count >= 11) {
    ps2_data = ~ps2_data;
    ps2_data &= 0x1FE;
    ps2_data >>= 1;
    xQueueSendFromISR(ps2_key_queue, &ps2_data, NULL);
    ps2_bits_count = 0;
    ps2_data = 0;
  }  
}

const char *debug_example =
"Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.\n"
"Core 1 register dump:\n"
"PC      : 0x400d5652\n"  
"PS      : 0x00060130\n"  
"A0      : 0x800d1537\n"  
"A1      : 0x3ffb1ee0\n"   
"A2      : 0x3ffc10cc\n" 
"A3      : 0x00000001\n" 
"A4      : 0x3ffb1eed\n" 
"A5      : 0x000000ff\n"  
"A6      : 0x0000fe01\n" 
"A7      : 0x00000000\n" 
"A8      : 0x000000ff\n"
"A9      : 0x3ffb1eef\n"  
"A10     : 0x00000000\n" 
"A11     : 0x3ffb1eed\n" 
"A12     : 0x000000ff\n" 
"A13     : 0x3ffb1eee\n"  
"A14     : 0x00060f20\n" 
"A15     : 0x00000000\n" 
"SAR     : 0x0000001f\n" 
"EXCCAUSE: 0x0000001c\n"  
"EXCVADDR: 0x00000000\n" 
"LBEG    : 0x00000000\n" 
"LEND    : 0x00000000\n" 
"LCOUNT  : 0x00000000\n";

void ps2_print(void *arg) {
  int key = 0;
  while (1) {
    hid_modifier_auto = 0;
    if (xQueueReceive(ps2_key_queue, &key, portMAX_DELAY)) {
      if (key == 0xF0) {
	blocked = 1;
	continue;
      }

      switch (key) {
	// Shift
      case 0x12: hid_buffer[0] = 2 * (1 - blocked); break;
      case 0x59: hid_buffer[0] = 2 * (1 - blocked); break;
	// Capslock
      case 0x58: hid_send_capslock(1-blocked); break;
	// Tab (debug message)
	//      case 0x0D: debug_test(); break;
	// Regular key
      default: hid_send_ascii_char(ps2_scancodes[key]*(1 - blocked)); break;
      }
      printf("key: %c, shift: %s\n", ps2_scancodes[key], ((hid_buffer[0] == 2) ? "on" : "off"));
      
      if (key != 0xF0) blocked = 0;	
    }
  }
}
