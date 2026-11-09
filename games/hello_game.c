#include "game.h"

#include "hardware_config.h"
#include "lcd.h"
#include "ui.h"

static void hello_start(void *state)
{
    (void)state;
}

static void hello_draw_region(void *state, uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height)
{
    (void)state;
    fill_rect_rgb565(x, y, width, height, 0, 0, 0);
    draw_string_clipped("HELLO", 20, 5, x, y, width, height,
                        255, 255, 255, FONT_SCALE, 0, 0, 0);
    draw_string_clipped("HOLD START+B", 60, 5, x, y, width, height,
                        255, 255, 255, FONT_SCALE, 0, 0, 0);
}

static void hello_update(void *state, const GameInput *input, uint32_t delta_us)
{
    (void)state;
    (void)input;
    (void)delta_us;
}

static void hello_draw(void *state)
{
    (void)state;
}

static void hello_stop(void *state)
{
    (void)state;
    lcd_clear(0, 0, 0);
}

const Game hello_game = {
    .state = NULL,
    .start = hello_start,
    .update = hello_update,
    .draw = hello_draw,
    .draw_region = hello_draw_region,
    .stop = hello_stop,
};
