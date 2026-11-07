#include "ui.h"

#include <string.h>

#include "pico/stdlib.h"

#include "fonts.h"
#include "hardware_config.h"
#include "lcd.h"

#define BUTTON_DEBOUNCE_US 25000
#define BUTTON_HOLD_US 500000

static const uint button_pins[BUTTON_COUNT] = {
    BTN_A,
    BTN_B,
    BTN_BOOTSEL,
    BTN_START,
};

static bool button_candidate[BUTTON_COUNT];
static bool button_stable[BUTTON_COUNT];
static bool button_hold_reported[BUTTON_COUNT];
static uint64_t button_candidate_since_us[BUTTON_COUNT];
static uint64_t button_pressed_since_us[BUTTON_COUNT];
static uint8_t button_pressed_events;
static uint8_t button_released_events;
static uint8_t button_held_events;

void button_init(void)
{
    uint64_t now = time_us_64();

    for (unsigned int i = 0; i < BUTTON_COUNT; ++i)
    {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);

        // Initialize from the actual pin state to avoid a false event at startup.
        button_candidate[i] = gpio_get(button_pins[i]) == 0;
        button_stable[i] = button_candidate[i];
        button_hold_reported[i] = false;
        button_candidate_since_us[i] = now;
        button_pressed_since_us[i] = now;
    }

    button_pressed_events = 0;
    button_released_events = 0;
    button_held_events = 0;
}

void button_update(void)
{
    uint64_t now = time_us_64();
    button_pressed_events = 0;
    button_released_events = 0;
    button_held_events = 0;

    for (unsigned int i = 0; i < BUTTON_COUNT; ++i)
    {
        bool raw_pressed = gpio_get(button_pins[i]) == 0;

        if (raw_pressed != button_candidate[i])
        {
            button_candidate[i] = raw_pressed;
            button_candidate_since_us[i] = now;
        }
        else if (button_candidate[i] != button_stable[i] &&
                 now - button_candidate_since_us[i] >= BUTTON_DEBOUNCE_US)
        {
            button_stable[i] = button_candidate[i];
            if (button_stable[i])
            {
                button_pressed_events |= (uint8_t)(1u << i);
                button_pressed_since_us[i] = now;
                button_hold_reported[i] = false;
            }
            else
            {
                button_released_events |= (uint8_t)(1u << i);
                button_hold_reported[i] = false;
            }
        }

        if (button_stable[i] && !button_hold_reported[i] &&
            now - button_pressed_since_us[i] >= BUTTON_HOLD_US)
        {
            button_held_events |= (uint8_t)(1u << i);
            button_hold_reported[i] = true;
        }
    }
}

bool button_was_pressed(Button button)
{
    return (unsigned int)button < BUTTON_COUNT &&
           (button_pressed_events & (uint8_t)(1u << button)) != 0;
}

bool button_was_released(Button button)
{
    return (unsigned int)button < BUTTON_COUNT &&
           (button_released_events & (uint8_t)(1u << button)) != 0;
}

bool button_was_held(Button button)
{
    return (unsigned int)button < BUTTON_COUNT &&
           (button_held_events & (uint8_t)(1u << button)) != 0;
}

bool button_is_down(Button button)
{
    return (unsigned int)button < BUTTON_COUNT && button_stable[button];
}

void draw_char(char c, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
               uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb)
{
    if (scale == 0 || scale > FONT_SCALE)
        return;

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

void draw_string(const char *str, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
                 uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb)
{
    if (scale == 0 || scale > FONT_SCALE)
        return;

    int space = scale * 7;
    int xx = (((scale + 2) * 7) / 2) - (space / 2);
    size_t length = strlen(str);

    for (size_t i = 0; i < length; i++)
    {
        draw_char(str[i], x + xx, y + space * i, r, g, b, scale, br, bg, bb);
    }
}

void draw_menu(int selected, const MenuOption items[], int item_count, int *previous)
{
    static int initialized = 0;
    int space = FONT_SCALE * 12;

    if (!initialized || *previous < 1 || *previous > item_count)
    {
        // Redraw every column initially and after returning from a submenu.
        initialized = 1;
        for (int i = 1; i <= item_count; i++)
        {
            if (i == selected)
            {
                fill_rect_rgb565(space * (i - 1), 0, (FONT_SCALE + 2) * 7,
                                 SCREEN_HEIGHT, 0xff, 0xff, 0xff);
                draw_string(items[i - 1].label, space * (i - 1), 5, 0, 0, 0,
                            FONT_SCALE, 255, 255, 255);
            }
            else
            {
                fill_rect_rgb565(space * (i - 1), 0, (FONT_SCALE + 2) * 7,
                                 SCREEN_HEIGHT, 0, 0, 0);
                draw_string(items[i - 1].label, space * (i - 1), 5, 255, 255, 255,
                            FONT_SCALE, 0, 0, 0);
            }
        }
    }
    else if (*previous != selected)
    {
        fill_rect_rgb565(space * (*previous - 1), 0, (FONT_SCALE + 2) * 7,
                         SCREEN_HEIGHT, 0, 0, 0);
        draw_string(items[*previous - 1].label, space * (*previous - 1), 5, 255, 255, 255,
                    FONT_SCALE, 0, 0, 0);

        fill_rect_rgb565(space * (selected - 1), 0, (FONT_SCALE + 2) * 7,
                         SCREEN_HEIGHT, 0xff, 0xff, 0xff);
        draw_string(items[selected - 1].label, space * (selected - 1), 5, 0, 0, 0,
                    FONT_SCALE, 255, 255, 255);
    }
}
