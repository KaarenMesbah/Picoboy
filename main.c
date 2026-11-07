#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "hardware_config.h"
#include "lcd.h"
#include "ui.h"

static void draw_submenu(char *title, char *message)
{
    lcd_clear(0, 0, 0);
    draw_string(title, 20, 5, 255, 255, 255, FONT_SCALE, 0, 0, 0);
    draw_string(message, 60, 5, 255, 255, 255, FONT_SCALE, 0, 0, 0);
}

static void open_selected_menu(int selected_item)
{
    switch (selected_item)
    {
    case 1:
        draw_submenu("VERSION", "PicoBoy 0.1");
        break;
    case 2:
        draw_submenu("GAMES", "No games yet");
        break;
    case 3:
        draw_submenu("ABOUT", "A pico game handheld by Kaaren");
        break;
    default:
        return;
    }
}

int main(void)
{
    int selected_item = 1;
    int previous_item = 1;
    char menu_items[][20] = {"Version", "Games", "About"};
    bool submenu_open = false;

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

        if (submenu_open)
        {
            if (button_was_pressed(BUTTON_B))
            {
                submenu_open = false;
                lcd_clear(0, 0, 0);
                previous_item = 0;
            }
        }
        else if (button_was_pressed(BUTTON_START))
        {
            open_selected_menu(selected_item);
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
