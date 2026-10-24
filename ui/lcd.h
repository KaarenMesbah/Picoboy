#ifndef LCD_H
#define LCD_H

#include <stddef.h>
#include <stdint.h>

void lcd_spi_init(void);
void lcd_init(void);
void lcd_clear(uint8_t r, uint8_t g, uint8_t b);
void lcd_cmd(uint8_t cmd);
void lcd_data(uint8_t data);
void lcd_write(const uint8_t *buf, size_t len);
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
void fill_rect_rgb565(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      uint8_t r, uint8_t g, uint8_t b);

#endif
