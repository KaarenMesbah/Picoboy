#include "game.h"

#include "hardware_config.h"
#include "lcd.h"
#include "ui.h"

static void hello_start(void)
{
    lcd_clear(0, 0, 0);
    draw_string("HELLO", 20, 5, 255, 255, 255, FONT_SCALE, 0, 0, 0);
    draw_string("HOLD START+B", 60, 5, 255, 255, 255, FONT_SCALE, 0, 0, 0);
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
    .update = hello_update,
    .stop = hello_stop,
};
