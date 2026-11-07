#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "hardware_config.h"
#include "games/game.h"
#include "lcd.h"
#include "ui.h"

#define SUBMENU_FONT_SCALE (FONT_SCALE - 1)
#define GAME_EXIT_HOLD_US 1000000
#define GAME_WIPE_TILE_SIZE 20
#define GAME_MENU_X_OFFSET 48

extern const Game hello_game;

static const Game *active_game;

static void draw_game_transition_pass(bool render_game)
{
    const int columns = (SCREEN_WIDTH + GAME_WIPE_TILE_SIZE - 1) / GAME_WIPE_TILE_SIZE;
    const int rows = (SCREEN_HEIGHT + GAME_WIPE_TILE_SIZE - 1) / GAME_WIPE_TILE_SIZE;
    const int max_wave = rows > columns ? rows - 1 : columns - 1;

    for (int wave = 0; wave <= max_wave; ++wave)
    {
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                int dx = column * 2 + 1 - columns;
                int dy = row * 2 + 1 - rows;
                if (dx < 0)
                    dx = -dx;
                if (dy < 0)
                    dy = -dy;

                int distance = dx > dy ? dx : dy;
                if (distance == wave)
                {
                    uint16_t x = column * GAME_WIPE_TILE_SIZE;
                    uint16_t y = row * GAME_WIPE_TILE_SIZE;
                    uint16_t width = SCREEN_WIDTH - x;
                    uint16_t height = SCREEN_HEIGHT - y;
                    if (width > GAME_WIPE_TILE_SIZE)
                        width = GAME_WIPE_TILE_SIZE;
                    if (height > GAME_WIPE_TILE_SIZE)
                        height = GAME_WIPE_TILE_SIZE;

                    if (render_game)
                        active_game->draw_region(x, y, width, height);
                    else
                        fill_rect_rgb565(x, y, width, height, 255, 255, 255);
                }
            }
        }

        sleep_ms(20);
    }
}

static void play_game_launch_wipe(void)
{
    draw_game_transition_pass(false);
    draw_game_transition_pass(true);
}

static void launch_hello_game(void)
{
    active_game = &hello_game;
    active_game->start();
    play_game_launch_wipe();
}

static const MenuOption game_options[] = {
    {"Hello", launch_hello_game},
    {NULL, NULL},
};

static const MenuOption menu_items[] = {
    {"Version", NULL},
    {"Games", NULL},
    {"About", NULL},
};

static int get_game_option_count(void)
{
    int count = 0;
    while (game_options[count].label != NULL)
        ++count;
    return count;
}

static void draw_games_menu(int selected_game, int *previous_game, bool redraw)
{
    if (redraw)
    {
        lcd_clear(0, 0, 0);
        draw_string("GAMES", 20, 5, 255, 255, 255,
                    SUBMENU_FONT_SCALE, 0, 0, 0);
        *previous_game = 0;
    }

    draw_menu_at(selected_game + 1, game_options, get_game_option_count(),
                 previous_game, GAME_MENU_X_OFFSET);
    *previous_game = selected_game + 1;
}

static void draw_submenu(const char *title, const char *message, uint8_t scale)
{
    lcd_clear(0, 0, 0);
    draw_string(title, 20, 5, 255, 255, 255, scale, 0, 0, 0);
    draw_string(message, 60, 5, 255, 255, 255, scale, 0, 0, 0);
}

static bool open_selected_menu(int selected_item, int selected_game, int *previous_game)
{
    switch (selected_item)
    {
    case 1:
        draw_submenu("VERSION", "PicoBoy 0.1", SUBMENU_FONT_SCALE);
        return false;
    case 2:
        draw_games_menu(selected_game, previous_game, true);
        return true;
    case 3:
        draw_submenu("ABOUT", "A pico game handheld by Kaaren", SUBMENU_FONT_SCALE);
        return false;
    default:
        return false;
    }
}

int main(void)
{
    int selected_item = 1;
    int previous_item = 1;
    int selected_game = 0;
    int previous_game = 0;
    uint64_t exit_combo_started_us = 0;
    bool exit_combo_active = false;
    bool submenu_open = false;
    bool game_menu_open = false;

    stdio_init_all();
    lcd_spi_init();
    lcd_init();
    button_init();

    draw_menu(selected_item, menu_items, 3, &previous_item);
    lcd_cmd(0x29);

    while (1)
    {
        button_update();

        if (button_was_pressed(BUTTON_BOOTSEL))
            reset_usb_boot(0, 0);

        if (active_game != NULL)
        {
            if (button_is_down(BUTTON_START) && button_is_down(BUTTON_B))
            {
                uint64_t now = time_us_64();
                if (!exit_combo_active)
                {
                    exit_combo_started_us = now;
                    exit_combo_active = true;
                }
                else if (now - exit_combo_started_us >= GAME_EXIT_HOLD_US)
                {
                    active_game->stop();
                    active_game = NULL;
                    exit_combo_active = false;
                    draw_games_menu(selected_game, &previous_game, true);
                }
            }
            else
            {
                exit_combo_active = false;
            }

            if (active_game != NULL)
                active_game->update();
        }
        else if (submenu_open)
        {
            if (button_was_pressed(BUTTON_B))
            {
                submenu_open = false;
                game_menu_open = false;
                lcd_clear(0, 0, 0);
                previous_item = 0;
            }
            else if (game_menu_open)
            {
                int game_count = get_game_option_count();
                if (game_count > 0 && button_was_pressed(BUTTON_A))
                {
                    selected_game = (selected_game + 1) % game_count;
                    draw_games_menu(selected_game, &previous_game, false);
                }
                if (game_count > 0 && button_was_pressed(BUTTON_START) &&
                    game_options[selected_game].on_select != NULL)
                {
                    game_options[selected_game].on_select();
                }
            }
        }
        else if (button_was_pressed(BUTTON_START))
        {
            game_menu_open = open_selected_menu(selected_item, selected_game, &previous_game);
            submenu_open = true;
        }
        else
        {
            if (button_was_pressed(BUTTON_A))
            {
                ++selected_item;
                if (selected_item > 3)
                    selected_item = 1;
            }
            if (button_was_pressed(BUTTON_B))
            {
                --selected_item;
                if (selected_item < 1)
                    selected_item = 3;
            }
        }

        if (selected_item != previous_item)
        {
            draw_menu(selected_item, menu_items, 3, &previous_item);
            previous_item = selected_item;
        }
    }

    return 0;
}
