#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "vga.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Структура для хранения состояния клавиатуры
typedef struct {
    uint8_t shift_pressed : 1;
    uint8_t ctrl_pressed : 1;
    uint8_t alt_pressed : 1;
    uint8_t caps_lock : 1;
    uint8_t num_lock : 1;
    uint8_t scroll_lock : 1;
    uint8_t last_keycode; // Для отслеживания предыдущего кода
} keyboard_state_t;


#endif // KEYBOARD_H