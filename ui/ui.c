#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "pico/bootrom.h"
#include "pico/stdlib.h"

#include "fonts.h"
#include "hardware_config.h"
#include "lcd.h"

void button_init(void)
{
    gpio_init(BTN_BOOTSEL);
    gpio_set_dir(BTN_BOOTSEL, GPIO_IN);
    gpio_pull_up(BTN_BOOTSEL);

    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);

    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);
}

void draw_char(char c, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
               uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb)
{
    // The font table covers printable ASCII plus DEL; use '?' for other bytes.
    if (c < 32 || c > 127)
        c = '?';

    const uint8_t *bitmap = font5x7_ascii[c - 32];

    uint16_t w = 5 * scale;
    uint16_t h = 7 * scale;
    // Text is rotated 90 degrees to match the intended screen-scrolling layout.
    uint16_t rw = h;
    uint16_t rh = w;

    uint8_t fg_hi = (r & 0xF8) | (g >> 5);
    uint8_t fg_lo = ((g & 0x1C) << 3) | (b >> 3);
    uint8_t bg_hi = (br & 0xF8) | (bg >> 5);
    uint8_t bg_lo = ((bg & 0x1C) << 3) | (bb >> 3);

    static uint8_t buf[7 * 5 * FONT_SCALE * FONT_SCALE * 2];
    memset(buf, 0, sizeof(buf));

    for (int row = 0; row < 7; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            bool set = bitmap[col] & (1 << (6 - row));

            for (int sy = 0; sy < scale; sy++)
            {
                for (int sx = 0; sx < scale; sx++)
                {
                    int px = col * scale + sx;
                    int py = row * scale + sy;
                    // Rotate each scaled font pixel before packing it for the LCD.
                    int rx = h - 1 - py;
                    int ry = px;
                    int idx = (ry * rw + rx) * 2;

                    if (set)
                    {
                        buf[idx] = fg_hi;
                        buf[idx + 1] = fg_lo;
                    }
                    else
                    {
                        buf[idx] = bg_hi;
                        buf[idx + 1] = bg_lo;
                    }
                }
            }
        }
    }

    lcd_set_window(x, y, x + rw - 1, y + rh - 1);
    lcd_write(buf, rw * rh * 2);
}

void draw_string(char *str, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
                 uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb)
{
    int space = FONT_SCALE * 7;
    int xx = (((FONT_SCALE + 2) * 7) / 2) - (space / 2);

    for (int i = 0; i <= strlen(str) - 1; i++)
    {
        draw_char(str[i], x + xx, y + space * i, r, g, b, scale, br, bg, bb);
    }
}

void button_test(int *scroll, int scrollable_items)
{
    if (gpio_get(BTN_A) == 0) // Active low
    {
        printf("BTN_A pressed!\n");
        if (*scroll >= 1)
        {
            *scroll = *scroll + 1;
        }
        if (*scroll > scrollable_items)
        {
            *scroll = 1;
        }
        sleep_ms(200); // Debounce delay
    }
    if (gpio_get(BTN_B) == 0) // Active low
    {
        if (*scroll > 1)
        {
            *scroll = *scroll - 1;
        }
        else if (*scroll <= 1)
        {
            *scroll = scrollable_items;
        }

        printf("BTN_B pressed!\n");
        sleep_ms(200); // Debounce delay
    }
    if (gpio_get(BTN_BOOTSEL) == 0)
    {
        reset_usb_boot(0, 0);
    }
    sleep_ms(50); // Polling delay
}

void draw_menu(int selected, char items[][20], int item_count, int *previous)
{
    static int initialized = 0;
    int space = FONT_SCALE * 12;

    if (!initialized)
    {
        // Draw all columns once; later updates only repaint the old/new selection.
        initialized = 1;
        for (int i = 1; i <= item_count; i++)
        {
            if (i == selected)
            {
                fill_rect_rgb565(space * (i - 1), 0, (FONT_SCALE + 2) * 7,
                                 SCREEN_HEIGHT, 0xff, 0xff, 0xff);
                draw_string(items[i - 1], space * (i - 1), 5, 0, 0, 0,
                            FONT_SCALE, 255, 255, 255);
            }
            else
            {
                fill_rect_rgb565(space * (i - 1), 0, (FONT_SCALE + 2) * 7,
                                 SCREEN_HEIGHT, 0, 0, 0);
                draw_string(items[i - 1], space * (i - 1), 5, 255, 255, 255,
                            FONT_SCALE, 0, 0, 0);
            }
        }
    }
    else if (*previous != selected)
    {
        fill_rect_rgb565(space * (*previous - 1), 0, (FONT_SCALE + 2) * 7,
                         SCREEN_HEIGHT, 0, 0, 0);
        draw_string(items[*previous - 1], space * (*previous - 1), 5, 255, 255, 255,
                    FONT_SCALE, 0, 0, 0);

        fill_rect_rgb565(space * (selected - 1), 0, (FONT_SCALE + 2) * 7,
                         SCREEN_HEIGHT, 0xff, 0xff, 0xff);
        draw_string(items[selected - 1], space * (selected - 1), 5, 0, 0, 0,
                    FONT_SCALE, 255, 255, 255);
    }
}
