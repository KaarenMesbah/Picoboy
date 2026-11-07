#include "game.h"

#include "hardware_config.h"
#include "lcd.h"
#include "ui.h"

static void hello_start(void)
{
}

static void hello_draw_region(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    fill_rect_rgb565(x, y, width, height, 0, 0, 0);
    draw_string_clipped("HELLO", 20, 5, x, y, width, height,
                        255, 255, 255, FONT_SCALE, 0, 0, 0);
    draw_string_clipped("HOLD START+B", 60, 5, x, y, width, height,
                        255, 255, 255, FONT_SCALE, 0, 0, 0);
}

static void hello_update(void)
{
}

static void hello_stop(void)
{
    lcd_clear(0, 0, 0);
}

const Game hello_game = {
    .start = hello_start,
    .draw_region = hello_draw_region,
    .update = hello_update,
    .stop = hello_stop,
};
