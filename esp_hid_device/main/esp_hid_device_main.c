/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hidd.h"
#include "esp_hid_gap.h"
#include "driver/gpio.h"

// #define EN_LAYOUT
#define TR_LAYOUT

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
} local_param_t;

static local_param_t s_bt_hid_param = {0};
const unsigned char keyboardReportMap[] = {
  0x05, 0x01, /* Usage Page (Generic Desktop), */
  0x09, 0x06, /* Usage (Keyboard), */
  0xA1, 0x01, /* Collection (Application), */
    0x05, 0x07, /* Usage Page (Key Codes); */
    0x19, 0xE0, /* Usage Minimum (224), */
    0x29, 0xE7, /* Usage Maximum (231), */
    0x15, 0x00, /* Logical Minimum (0), */
    0x25, 0x01, /* Logpical Maximum (1), */
    0x75, 0x01, /* Report Size (1), */
    0x95, 0x08, /* Report Count (8), */
    0x81, 0x02, /* Input (Data, Variable, Absolute), */
    0x95, 0x01, /* Report Count (1), */
    0x75, 0x08, /* Report Size (8), */
    0x81, 0x01, /* Input (Constant), */
    0x95, 0x05, /* Report Count (5), */
    0x75, 0x01, /* Report Size (1), */
    0x05, 0x08, /* Usage Page (Page# for LEDs), */
    0x19, 0x01, /* Usage Minimum (1), */
    0x29, 0x05, /* Usage Maximum (5), */
    0x91, 0x02, /* Output (Data, Variable, Absolute), */
    0x95, 0x01, /* Report Count (1), */
    0x75, 0x03, /* Report Size (3), */
    0x91, 0x01, /* Output (Constant), */
    0x95, 0x02, /* Report Count (6), CHANGED TO 1 */
    0x75, 0x08, /* Report Size (8), */
    0x15, 0x00, /* Logical Minimum (0), */
    0x25, 0x65, /* Logical Maximum(101), */
    0x05, 0x07, /* Usage Page (Key Codes), */
    0x19, 0x00, /* Usage Minimum (0), */
    0x29, 0x65, /* Usage Maximum (101), */
    0x81, 0x00, /* Input (Data, Array), */
  0xC0/* End Collection */
};

static esp_hid_raw_report_map_t bt_report_maps[] = {
  {
    .data = keyboardReportMap,
    .len = sizeof(keyboardReportMap)
  },
};

static esp_hid_device_config_t bt_hid_config = {
  .vendor_id          = 0x16C0,
  .product_id         = 0x05DF,
  .version            = 0x0100,
  .device_name        = "ChatGPT",
  .manufacturer_name  = "Entes",
  .serial_number      = "1234567890",
  .report_maps        = bt_report_maps,
  .report_maps_len    = 1
};

#ifdef EN_LAYOUT

static const uint8_t usb_scancodes[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  //       !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /   
  0x2c, 0x1e, 0x34, 0x20, 0x21, 0x22, 0x24, 0x34, 0x26, 0x27, 0x25, 0x2e, 0x36, 0x2d, 0x37, 0x38,
  // 0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
  0x27, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x33, 0x33, 0x36, 0x2e, 0x37, 0x38,
  // @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
  0x1f,  0x4,  0x5 , 0x6 , 0x7 , 0x8 , 0x9 , 0xa , 0xb , 0xc , 0xd , 0xe , 0xf , 0x10 , 0x11, 0x12,
  // P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,  0x2f, 0x31, 0x30, 0x23, 0x2d, 
  // `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o   
  0x35,  0x4,  0x5 , 0x6 , 0x7 , 0x8 , 0x9 , 0xa , 0xb , 0xc , 0xd , 0xe , 0xf , 0x10 , 0x11, 0x12,
  // p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~     
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,  0x2f, 0x31, 0x30, 0x35, 0x00
};
  
static const uint16_t usb_modifier_shift[] = {
// 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0
  0x0020,
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000,
//   ! " # $ % & ' ( ) * + , - . /
// 0 1 1 1 1 1 1 0 1 1 1 1 0 0 0 0
  0x7ef0,
// 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
// 0 0 0 0 0 0 0 0 0 0 1 0 1 1 1 1
  0x002f,
// @ A B C D E F G H I J K L M N O
// 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
  0xffff,
// P Q R S T U V W X Y Z [ \ ] ^ _
// 1 1 1 1 1 1 1 1 1 1 1 0 0 0 1 1
  0xffe3,
// ` a b c d e f g h i j k l m n o   
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000,
// p q r s t u v w x y z { | } ~ 
// 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0
  0x001e
};
#endif

#ifdef TR_LAYOUT

static const uint8_t usb_scancodes[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  //       !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /   
  0x2c, 0x1e, 0x35, 0x20, 0x21, 0x22, 0x23, 0x1f, 0x25, 0x26, 0x2d, 0x21, 0x31, 0x2e, 0x38, 0x24,
  // 0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
  0x27, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x38, 0x31, 0x35, 0x27, 0x1e, 0x2d,
  // @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
  0x14,  0x4,  0x5 , 0x6 , 0x7 , 0x8 , 0x9 , 0xa , 0xb , 0xc , 0xd , 0xe , 0xf , 0x10 , 0x11, 0x12,
  // P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,  0x25, 0x2d, 0x26, 0x20, 0x2e, 
  // `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o   
  0x34,  0x4,  0x5 , 0x6 , 0x7 , 0x8 , 0x9 , 0xa , 0xb , 0x34 , 0xd , 0xe , 0xf , 0x10 , 0x11, 0x12,
  // p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~     
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,  0x24, 0x2e, 0x27, 0x30, 0x00
};
  
static const uint16_t usb_modifier_shift[] = {
// 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0
  0x0020,
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000,
//   ! " # $ % & ' ( ) * + , - . /
// 0 1 0 0 0 1 1 1 1 1 0 1 0 0 0 1
  0x47d1,
// 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
// 0 0 0 0 0 0 0 0 0 0 1 1 0 1 0 1
  0x0035,
// @ A B C D E F G H I J K L M N O
// 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
  0xffff,
// P Q R S T U V W X Y Z [ \ ] ^ _
// 1 1 1 1 1 1 1 1 1 1 1 0 0 0 1 1
  0xffe3,
// ` a b c d e f g h i j k l m n o   
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000,
// p q r s t u v w x y z { | } ~ 
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000
};

static const uint16_t usb_modifier_alt[] = {
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000,
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x0000,
//   ! " # $ % & ' ( ) * + , - . /
// 0 0 1 1 1 0 0 0 0 0 0 0 0 0 0 0
  0x3a00,
// 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
// 0 0 0 0 0 0 0 0 0 0 0 n 1 0 1 0
  0x000a,
// @ A B C D E F G H I J K L M N O
// 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x8000,
// P Q R S T U V W X Y Z [ \ ] ^ _
// 0 0 0 0 0 0 0 0 0 0 0 1 1 1 0 0
  0x001c,
// ` a b c d e f g h i j k l m n o
// 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
  0x8000,
// p q r s t u v w x y z { | } ~ 
// 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0
  0x001e
};

#endif

#define HAS_MODIFIER(a, i) (((a)[(int)(i)/16] & (1<<(15-(int)(i)%16))) != 0)

uint8_t hid_buffer[4] = {0};
int hid_modifier_auto = 1;

void hid_send_ascii_char(char c) {
  printf("key read: %c ", c);
  if (hid_modifier_auto) {
    hid_buffer[0] = HAS_MODIFIER(usb_modifier_shift, c) * (1 << 1);
#ifdef TR_LAYOUT
    hid_buffer[0] += HAS_MODIFIER(usb_modifier_alt, c) * (1 << 6);
#endif
  }
  hid_buffer[2] = usb_scancodes[(int)c];
  printf("Buffer transmitted: 0x%x 0x%x 0x%x \n", hid_buffer[0], hid_buffer[1], hid_buffer[2]);
  esp_hidd_dev_input_set(s_bt_hid_param.hid_dev, 0, 0, hid_buffer, 3);
}

#ifdef TR_LAYOUT

static const char usb_scancodes_special[][6] = {
// char, char, null,  key, shift, alt
  {0xc4, 0x9f, '\0', 0x2f,     0,   0}, // ğ 
  {0xc3, 0xbc, '\0', 0x30,     0,   0}, // ü 
  {0xc5, 0x9f, '\0', 0x33,     0,   0}, // ş 
  {0xc4, 0xb1, '\0', 0x0c,     0,   0}, // ı 
  {0xc3, 0xb6, '\0', 0x36,     0,   0}, // ö 
  {0xc3, 0xa7, '\0', 0x37,     0,   0}, // ç
  {0xc4, 0x9e, '\0', 0x2f,     1,   0}, // Ğ 
  {0xc3, 0x9c, '\0', 0x30,     1,   0}, // Ü 
  {0xc5, 0x9e, '\0', 0x33,     1,   0}, // Ş 
  {0xc4, 0xb0, '\0', 0x34,     1,   0}, // İ
  {0xc3, 0x96, '\0', 0x36,     1,   0}, // Ö
  {0xc3, 0x87, '\0', 0x37,     1,   0}, // Ç
};

static const int usb_scancodes_special_size = sizeof(usb_scancodes_special) / sizeof(char) / 6;

// Returns offset
int hid_send_nonascii_char(char *c) {
  static char temp[9];
  // Two-byte char
  if (((uint8_t)c[0] & 0xe0) == 0xc0) {
    strncpy(temp, c, 2);
    for (int i = 0; i < usb_scancodes_special_size; i++)
      if (strcmp(temp, usb_scancodes_special[i]) == 0) {
	hid_buffer[0] = ((int)usb_scancodes_special[i][4]) * (1 << 1) +
                        ((int)usb_scancodes_special[i][5]) * (1 << 6);
	hid_buffer[2] = usb_scancodes_special[i][3];
	printf("Buffer transmitted: 0x%x 0x%x 0x%x \n", hid_buffer[0], hid_buffer[1], hid_buffer[2]);
	esp_hidd_dev_input_set(s_bt_hid_param.hid_dev, 0, 0, hid_buffer, 3);
	return 2;
      }
  } else {
    hid_send_ascii_char(*c);
    return 1;
  }
  return 1;
}

#endif

void hid_send_capslock() {
  hid_buffer[2] = 0x39;
  esp_hidd_dev_input_set(s_bt_hid_param.hid_dev, 0, 0, hid_buffer, 3);
}

void hid_send_string(char *s) {
  static const int send_delay = 30;
  int i = 0;
  hid_modifier_auto = 1;
  while (s[i] != '\0') {
    i += hid_send_nonascii_char(s+i);
    vTaskDelay(send_delay/portTICK_PERIOD_MS);
    hid_send_ascii_char(0);
    vTaskDelay(send_delay/portTICK_PERIOD_MS);
  }
}

void bt_hid_task_shut_down(void)
{
    if (s_bt_hid_param.task_hdl) {
        vTaskDelete(s_bt_hid_param.task_hdl);
        s_bt_hid_param.task_hdl = NULL;
    }
}

static void bt_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT: {
      if (param->start.status == ESP_OK)
	esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
      break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
      if (param->connect.status == ESP_OK) {
	esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
      } 
      break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
      if (param->disconnect.status == ESP_OK) {
	bt_hid_task_shut_down();
	esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
      }
      break;
    }
    default: break;
    }
    return;
}

const char *example_denis = "Merhaba Denis! Hindistan'ı yöneterek zengin kültürü ve ekonomik potansiyeli bir araya getireceksiniz. Ben ChatGPT, her adımınızda size destek olacağım. Başarılar ve eğlenceli bir oyun dilerim!\n";

const char *example_bilge = "Merhaba Bilge! AB'nin entegre birliğini ve küresel etkisini yönetmek heyecan verici. Ben ChatGPT, başarınız için buradayım. Başarılar ve keyifli bir oyun geçirmeniz dileğiyle!\n";

const char *example_berat = "Merhaba Berat! Brezilya'nın doğal güzelliklerini ve ekonomik potansiyelini keşfetmek harika olacak. Ben ChatGPT olarak yanınızdayım. Bol şans ve keyifli bir oyun dilerim!\n";

const char *example_eda = "Merhaba Eda! ABD'nin güçlü ekonomisini ve çeşitliliğini yönetmek harika bir fırsat. Ben ChatGPT olarak stratejilerinizde size yardımcı olacağım. Kazanmak için en iyisi olun!\n";

const char *example_kavraz = "Merhaba Kavraz! Çin'in yükselen gücünü ve zengin kültürünü yönetmek muhteşem olacak. Ben ChatGPT olarak sizinle birlikteyim. Zaferlerle dolu bir oyun dilerim!\n";

const char *example_semih = "Merhaba Semih! Rusya'nın stratejik önemini ve çeşitli kaynaklarını yönetmek büyük bir sorumluluk. Ben ChatGPT, size her adımda rehberlik edeceğim. İyi şanslar ve harika bir oyun geçirmeniz dileğiyle!\n";

// Reading keys
const int key_pin[] = {13, 12, 14, 27, 26, 25, 33, 32, 22};
TaskHandle_t key_print_trigger = NULL;
uint8_t key_data = 0;

void key_interrupt_respond(void *arg) {
  key_data = 0;
  for (int i = 0; i < 8; i++) {
    key_data |= ((1-gpio_get_level(key_pin[i])) << i);
  }
  xTaskNotifyGive(key_print_trigger);
}

void key_print(void *arg) {
  while (1) {
    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
      switch (key_data) {
      case 0x81: hid_send_string(example_berat); break;
      case 0x82: hid_send_string(example_bilge); break;
      case 0x84: hid_send_string(example_denis); break;
      case 0x88: hid_send_string(example_eda); break;
      case 0x90: hid_send_string(example_kavraz); break;
      case 0xa0: hid_send_string(example_semih); break;
      default: hid_send_ascii_char((char)key_data); break;
      }
      vTaskDelay(200 / portTICK_PERIOD_MS);
      hid_send_ascii_char(0);
      ulTaskNotifyTake(pdTRUE, 0);
    }
  }
}


// Main

void app_main(void) {
  // GPIO Init
  uint64_t key_pin_bit_mask = 0;
  for (int i = 0; i < 9; i++)
    key_pin_bit_mask |= (1ULL << key_pin[i]);
  
  gpio_config_t key_pin_config = {
    key_pin_bit_mask,
    (gpio_mode_t) GPIO_MODE_INPUT,
    (gpio_pullup_t) 1, 
    (gpio_pulldown_t) 0, 
    (gpio_int_type_t) GPIO_INTR_NEGEDGE
  };
  
  gpio_config(&key_pin_config);
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
  gpio_isr_handler_add(key_pin[8], key_interrupt_respond, NULL);  
  xTaskCreate(key_print, "KEY_PRINT", 2048, NULL, 1, &key_print_trigger);

  // Bluetooth init
  esp_err_t ret;
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );

  ret = esp_hid_gap_init(HID_DEV_MODE);
  ESP_ERROR_CHECK( ret );

  esp_bt_dev_set_device_name(bt_hid_config.device_name);
    
  esp_bt_cod_t cod;
  cod.major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL;
  esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  ESP_ERROR_CHECK(esp_hidd_dev_init(&bt_hid_config, ESP_HID_TRANSPORT_BT, bt_hidd_event_callback, &s_bt_hid_param.hid_dev));
  printf("App started\n");

}
