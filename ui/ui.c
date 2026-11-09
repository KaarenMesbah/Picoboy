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

bool text_pixel(const uint8_t font[96][5], uint8_t scale,
                const char *text, size_t text_length,
                int32_t x, int32_t y, int32_t pixel_x, int32_t pixel_y,
                bool rotate_180)
{
    if (font == NULL || text == NULL || scale == 0 ||
        pixel_x < x || pixel_y < y)
        return false;

    int32_t relative_x = pixel_x - x;
    int32_t relative_y = pixel_y - y;
    int32_t advance = 7 * scale;
    size_t position = (size_t)(relative_x / advance);
    int glyph_column = (relative_x % advance) / scale;
    int glyph_row = relative_y / scale;

    if (position >= text_length || glyph_column >= 5 || glyph_row >= 7)
        return false;

    if (rotate_180)
    {
        position = text_length - position - 1;
        glyph_column = 4 - glyph_column;
        glyph_row = 6 - glyph_row;
    }

    unsigned char character = (unsigned char)text[position];
    if (character < 32 || character > 127)
        character = '?';

    return (font[character - 32][glyph_column] &
            (1u << (6 - glyph_row))) != 0;
}

static void draw_char_clipped(char c, uint16_t x, uint16_t y,
                              uint16_t clip_x, uint16_t clip_y,
                              uint16_t clip_width, uint16_t clip_height,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t scale,
                              uint8_t br, uint8_t bg, uint8_t bb)
{
    if (scale == 0 || scale > FONT_SCALE || clip_width == 0 || clip_height == 0)
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

    uint32_t left = x > clip_x ? x : clip_x;
    uint32_t top = y > clip_y ? y : clip_y;
    uint32_t right = x + rw;
    uint32_t bottom = y + rh;
    uint32_t clip_right = (uint32_t)clip_x + clip_width;
    uint32_t clip_bottom = (uint32_t)clip_y + clip_height;
    if (right > clip_right)
        right = clip_right;
    if (bottom > clip_bottom)
        bottom = clip_bottom;
    if (right > SCREEN_WIDTH)
        right = SCREEN_WIDTH;
    if (bottom > SCREEN_HEIGHT)
        bottom = SCREEN_HEIGHT;

    if (left >= right || top >= bottom)
        return;

    uint16_t draw_width = right - left;
    uint16_t draw_height = bottom - top;
    uint16_t source_x = left - x;
    uint16_t source_y = top - y;
    lcd_set_window(left, top, right - 1, bottom - 1);
    for (uint16_t row = 0; row < draw_height; ++row)
        lcd_write(&buf[((source_y + row) * rw + source_x) * 2], draw_width * 2);
}

void draw_char(char c, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
               uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb)
{
    draw_char_clipped(c, x, y, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                      r, g, b, scale, br, bg, bb);
}

void draw_string_clipped(const char *str, uint16_t x, uint16_t y,
                         uint16_t clip_x, uint16_t clip_y,
                         uint16_t clip_width, uint16_t clip_height,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t scale,
                         uint8_t br, uint8_t bg, uint8_t bb)
{
    if (scale == 0 || scale > FONT_SCALE)
        return;

    int space = scale * 7;
    int xx = (((scale + 2) * 7) / 2) - (space / 2);
    size_t length = strlen(str);

    for (size_t i = 0; i < length; i++)
    {
        draw_char_clipped(str[i], x + xx, y + space * i,
                          clip_x, clip_y, clip_width, clip_height,
                          r, g, b, scale, br, bg, bb);
    }
}

void draw_string(const char *str, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b,
                 uint8_t scale, uint8_t br, uint8_t bg, uint8_t bb)
{
    draw_string_clipped(str, x, y, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                        r, g, b, scale, br, bg, bb);
}

void draw_menu_at(int selected, const MenuOption items[], int item_count, int *previous,
                  uint16_t x_offset)
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
                fill_rect_rgb565(x_offset + space * (i - 1), 0, (FONT_SCALE + 2) * 7,
                                 SCREEN_HEIGHT, 0xff, 0xff, 0xff);
                draw_string(items[i - 1].label, x_offset + space * (i - 1), 5, 0, 0, 0,
                            FONT_SCALE, 255, 255, 255);
            }
            else
            {
                fill_rect_rgb565(x_offset + space * (i - 1), 0, (FONT_SCALE + 2) * 7,
                                 SCREEN_HEIGHT, 0, 0, 0);
                draw_string(items[i - 1].label, x_offset + space * (i - 1), 5, 255, 255, 255,
                            FONT_SCALE, 0, 0, 0);
            }
        }
    }
    else if (*previous != selected)
    {
        fill_rect_rgb565(x_offset + space * (*previous - 1), 0, (FONT_SCALE + 2) * 7,
                         SCREEN_HEIGHT, 0, 0, 0);
        draw_string(items[*previous - 1].label, x_offset + space * (*previous - 1), 5, 255, 255, 255,
                    FONT_SCALE, 0, 0, 0);

        fill_rect_rgb565(x_offset + space * (selected - 1), 0, (FONT_SCALE + 2) * 7,
                         SCREEN_HEIGHT, 0xff, 0xff, 0xff);
        draw_string(items[selected - 1].label, x_offset + space * (selected - 1), 5, 0, 0, 0,
                    FONT_SCALE, 255, 255, 255);
    }
}

void draw_menu(int selected, const MenuOption items[], int item_count, int *previous)
{
    draw_menu_at(selected, items, item_count, previous, 0);
}
