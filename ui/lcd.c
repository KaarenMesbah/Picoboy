#include "lcd.h"

#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "hardware_config.h"

static void rgb565(uint8_t r, uint8_t g, uint8_t b, uint8_t *hi, uint8_t *lo)
{
    *hi = (r & 0xF8) | (g >> 5);
    *lo = ((g & 0x1C) << 3) | (b >> 3);
}

void lcd_cmd(uint8_t cmd)
{
    gpio_put(LCD_DC, 0);
    gpio_put(LCD_CS, 0);
    spi_write_blocking(LCD_SPI_PORT, &cmd, 1);
    gpio_put(LCD_CS, 1);
}

void lcd_data(uint8_t data)
{
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    spi_write_blocking(LCD_SPI_PORT, &data, 1);
    gpio_put(LCD_CS, 1);
}

void lcd_write(const uint8_t *buf, size_t len)
{
    // Keep CS asserted for the whole transfer so pixel streams stay aligned.
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    spi_write_blocking(LCD_SPI_PORT, buf, len);
    gpio_put(LCD_CS, 1);
}

static void lcd_reset(void)
{
    gpio_put(LCD_RST, 0);
    sleep_ms(50);
    gpio_put(LCD_RST, 1);
    sleep_ms(200);
}

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Set the column and page ranges, then start the pixel write.
    lcd_cmd(0x2A);
    lcd_data(x0 >> 8);
    lcd_data(x0 & 0xFF);
    lcd_data(x1 >> 8);
    lcd_data(x1 & 0xFF);

    lcd_cmd(0x2B);
    lcd_data(y0 >> 8);
    lcd_data(y0 & 0xFF);
    lcd_data(y1 >> 8);
    lcd_data(y1 & 0xFF);

    lcd_cmd(0x2C);
}

void lcd_clear(uint8_t r, uint8_t g, uint8_t b)
{
    lcd_set_window(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    static uint8_t linebuf[2 * SCREEN_WIDTH];
    uint8_t hi;
    uint8_t lo;
    rgb565(r, g, b, &hi, &lo);
    for (int x = 0, idx = 0; x < SCREEN_WIDTH; ++x)
    {
        linebuf[idx++] = hi;
        linebuf[idx++] = lo;
    }

    for (int y = 0; y < SCREEN_HEIGHT; ++y)
    {
        lcd_write(linebuf, sizeof(linebuf));
    }
}

void lcd_init(void)
{
    lcd_reset();

    lcd_cmd(0x11);
    sleep_ms(200);

    lcd_cmd(0x3A);
    lcd_data(0x55); // RGB565: 16-bit color

    lcd_cmd(0x36);
    lcd_data(0x08); // Existing panel color order setting
    lcd_cmd(0x28);  // Display off while preparing the initial screen
    lcd_clear(0, 0, 0); // Clear to black
}

void lcd_spi_init(void)
{
    spi_init(LCD_SPI_PORT, 30000000);

    gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);

    gpio_init(LCD_CS);
    gpio_init(LCD_DC);
    gpio_init(LCD_RST);

    gpio_set_dir(LCD_CS, GPIO_OUT);
    gpio_set_dir(LCD_DC, GPIO_OUT);
    gpio_set_dir(LCD_RST, GPIO_OUT);
    gpio_put(LCD_CS, 1);
}

void draw_pixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;

    lcd_set_window(x, y, x, y);
    uint8_t hi;
    uint8_t lo;
    rgb565(r, g, b, &hi, &lo);
    uint8_t pixel[2] = {hi, lo};
    lcd_write(pixel, sizeof(pixel));
}

void fill_rect_rgb565(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      uint8_t r, uint8_t g, uint8_t b)
{
    if (x0 >= SCREEN_WIDTH || y0 >= SCREEN_HEIGHT)
        return;
    if (x0 + w > SCREEN_WIDTH)
        w = SCREEN_WIDTH - x0;
    if (y0 + h > SCREEN_HEIGHT)
        h = SCREEN_HEIGHT - y0;

    lcd_set_window(x0, y0, x0 + w - 1, y0 + h - 1);

    // Reuse one scanline buffer rather than allocating a full rectangle.
    static uint8_t linebuf[2 * SCREEN_WIDTH];
    uint8_t hi;
    uint8_t lo;
    rgb565(r, g, b, &hi, &lo);
    for (int x = 0, idx = 0; x < w; ++x)
    {
        linebuf[idx++] = hi;
        linebuf[idx++] = lo;
    }

    for (int y = 0; y < h; ++y)
        lcd_write(linebuf, w * 2);
}
