#ifndef UI_H
#define UI_H

#include <stdint.h>

void button_init(void);
void button_test(int *scroll, int scrollable_items);
void draw_char(char c, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
               uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb);
void draw_string(char *str, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
                 uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb);
void draw_menu(int selected, char items[][20], int item_count, int *previous);

#endif
