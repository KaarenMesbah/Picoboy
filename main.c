#include "pico/stdlib.h"

#include "lcd.h"
#include "ui.h"

int main(void)
{
    int selected_item = 1;
    int previous_item = 1;
    char menu_items[][20] = {"Install", "Version", "Games", "Settings"};

    stdio_init_all();
    lcd_spi_init();
    lcd_init();
    button_init();

    draw_menu(selected_item, menu_items, 4, &previous_item);
    lcd_cmd(0x29);

    while (1)
    {
        if (selected_item != previous_item)
        {
            draw_menu(selected_item, menu_items, 4, &previous_item);
            previous_item = selected_item;
        }

        button_test(&selected_item, 4);
    }

    return 0;
}
