#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lcd.h"
#include "ui.h"

int main(void)
{
    int selected_item = 1;
    int previous_item = 1;
    char menu_items[][20] = {"Version", "Games", "About"};

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

        if (selected_item != previous_item)
        {
            draw_menu(selected_item, menu_items, 3, &previous_item);
            previous_item = selected_item;
        }
    }

    return 0;
}
