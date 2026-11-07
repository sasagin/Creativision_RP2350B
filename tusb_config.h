#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// Включаем режим Host для порта 0
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_HOST

// Включаем поддержку HID (клавиатуры)
#define CFG_TUH_HID             4  // Макс. количество HID интерфейсов
#define CFG_TUH_DEVICE_MAX      1

// Настройки хаба (если используете)
#define CFG_TUH_HUB             0

#endif
