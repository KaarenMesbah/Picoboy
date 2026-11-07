#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BUTTON_A,
    BUTTON_B,
    BUTTON_BOOTSEL,
    BUTTON_START,
    BUTTON_COUNT
} Button;

void button_init(void);
// Call often from the main/game loop. Events below describe the latest update.
void button_update(void);
// Press/release events last until the next call to button_update().
bool button_was_pressed(Button button);
bool button_was_released(Button button);
// Returns the debounced held state.
bool button_is_down(Button button);
void draw_char(char c, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
               uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb);
void draw_string(const char *str, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
                 uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb);
void draw_menu(int selected, char items[][20], int item_count, int *previous);

#endif
