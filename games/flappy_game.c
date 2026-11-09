#include "game.h"

#include <stdio.h>
#include <string.h>

#include "fonts.h"
#include "hardware_config.h"
#include "lcd.h"
#include "ui.h"

/* The game is authored in landscape coordinates; the LCD remains portrait.
 * The renderer maps a landscape point (world_x, world_y) to LCD coordinates. */
#define WORLD_WIDTH SCREEN_HEIGHT
#define WORLD_HEIGHT SCREEN_WIDTH
/* Keep the HUD outside the ILI9486 scroll area; the remaining columns form a
 * circular viewport that hardware scrolling moves without repainting them. */
#define FIXED_WORLD_WIDTH 80
#define FIXED_WORLD_START (WORLD_WIDTH - FIXED_WORLD_WIDTH)
#define SCROLL_WIDTH (WORLD_WIDTH - FIXED_WORLD_WIDTH)
#define SCROLL_TOP_FIXED FIXED_WORLD_WIDTH
#define BIRD_X 86
#define BIRD_SCREEN_X (FIXED_WORLD_WIDTH + BIRD_X)
#define BIRD_WIDTH 22
#define BIRD_HEIGHT 18
#define PIPE_WIDTH 44
#define PIPE_CAP_WIDTH 54
#define PIPE_GAP_HEIGHT 116
#define PIPE_SPACING 250
#define GROUND_HEIGHT 28
#define GRAVITY 900.0f
#define FLAP_VELOCITY -330.0f
#define PIPE_SPEED 105.0f
#define MAX_SIMULATION_STEP_US 100000
#define GAME_FONT_SCALE 2

typedef struct
{
    /* Simulation state: positions are landscape world coordinates. */
    float bird_y;
    float bird_velocity;
    float camera_x;
    float pipe_x;
    float next_pipe_x;
    int pipe_gap_y;
    int next_pipe_gap_y;
    unsigned int score;
    /* Cache score text because scene_pixel samples individual pixels often. */
    char score_text[12];
    size_t score_text_length;
    /* LCD GRAM row where the circular playfield currently begins. */
    uint16_t scroll_start;
    /* Previous rendered values let draw() update only changed regions. */
    int rendered_camera_x;
    int rendered_bird_y;
    unsigned int rendered_score;
    bool started;
    bool game_over;
    bool pipe_scored;
    bool rendered_started;
    bool rendered_game_over;
} FlappyState;

static FlappyState flappy_state;
/* One LCD scanline's RGB565 output; reused to avoid large frame buffers. */
static uint8_t line_buffer[SCREEN_WIDTH * 2];

/* Convert 8-bit RGB components to the 16-bit pixel format configured on LCD. */
static uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void reset_game(FlappyState *state)
{
    memset(state, 0, sizeof(*state));
    state->bird_y = WORLD_HEIGHT / 2.0f;
    state->pipe_x = WORLD_WIDTH - FIXED_WORLD_WIDTH + 24.0f;
    state->next_pipe_x = state->pipe_x + PIPE_SPACING;
    state->pipe_gap_y = WORLD_HEIGHT / 2;
    state->next_pipe_gap_y = 82 + (int)((37u) % 156u);
    strcpy(state->score_text, "0");
    state->score_text_length = 1;
}

static void flappy_start(void *game_state)
{
    FlappyState *state = game_state;
    reset_game(state);
    /* In landscape, the fixed HUD is the first 80 display columns after
     * rotation. Configure the rest of the 480-row GRAM as a scrollable ring. */
    lcd_set_vertical_scroll_area(SCROLL_TOP_FIXED, SCROLL_WIDTH, 0);
    lcd_set_vertical_scroll_start(SCROLL_TOP_FIXED);
}

static uint16_t memory_row_for_display_y(const FlappyState *state, int display_y)
{
    /* Convert a visible display row to its backing GRAM row. Rows inside the
     * playfield wrap around; HUD rows always map directly. */
    if (display_y < SCROLL_TOP_FIXED)
        return (uint16_t)display_y;

    return (uint16_t)(SCROLL_TOP_FIXED +
                      (state->scroll_start + display_y - SCROLL_TOP_FIXED) % SCROLL_WIDTH);
}

/* Render a single string pixel. rotate_180 matches this game's landscape
 * orientation while keeping the font data shared with the UI. */
static uint16_t scene_pixel(const FlappyState *state, int screen_x,
                            int world_x, int world_y)
{
    /* Compose one pixel back-to-front: sky/clouds, pipes, ground, HUD, bird,
     * then the start/game-over overlay. This makes partial redraws complete. */
    uint16_t color = color565(105, 190, 245);
    int cloud_x = world_x % 280;

    if ((cloud_x >= 32 && cloud_x < 90 && world_y >= 48 && world_y < 66) ||
        (cloud_x >= 48 && cloud_x < 76 && world_y >= 36 && world_y < 48) ||
        (cloud_x >= 210 && cloud_x < 278 && world_y >= 122 && world_y < 140) ||
        (cloud_x >= 230 && cloud_x < 260 && world_y >= 110 && world_y < 122))
        color = color565(220, 240, 255);

    float pipes[] = {state->pipe_x, state->next_pipe_x};
    int gaps[] = {state->pipe_gap_y, state->next_pipe_gap_y};
    for (unsigned int i = 0; i < 2; ++i)
    {
        int pipe_x = (int)pipes[i];
        int gap_top = gaps[i] - PIPE_GAP_HEIGHT / 2;
        int gap_bottom = gaps[i] + PIPE_GAP_HEIGHT / 2;
        int pipe_start = pipe_x - (PIPE_CAP_WIDTH - PIPE_WIDTH) / 2;
        int pipe_end = pipe_x + PIPE_WIDTH + (PIPE_CAP_WIDTH - PIPE_WIDTH) / 2;
        bool in_cap = world_x >= pipe_start && world_x < pipe_end &&
                      ((world_y >= gap_top - 16 && world_y < gap_top) ||
                       (world_y >= gap_bottom && world_y < gap_bottom + 16));
        bool in_body = world_x >= pipe_x && world_x < pipe_x + PIPE_WIDTH &&
                       (world_y < gap_top || world_y >= gap_bottom);

        if (in_cap || in_body)
        {
            color = in_cap ? color565(35, 145, 60) : color565(50, 180, 75);
            if (!in_cap && world_x >= pipe_x + 7 && world_x < pipe_x + 11)
                color = color565(105, 215, 115);
        }
    }

    int ground_y = WORLD_HEIGHT - GROUND_HEIGHT;
    if (world_y >= ground_y)
        color = world_y < ground_y + 7 ? color565(90, 190, 75)
                                       : color565(175, 125, 70);

    if (screen_x >= FIXED_WORLD_START)
    {
        color = screen_x < FIXED_WORLD_START + 4
                    ? color565(255, 190, 55)
                    : color565(30, 65, 105);
        if (text_pixel(font5x7_ascii, GAME_FONT_SCALE, "SCORE", 5,
                       FIXED_WORLD_START + 8, 12, screen_x, world_y, true) ||
            text_pixel(font5x7_ascii, GAME_FONT_SCALE, "FLAP", 4,
                       FIXED_WORLD_START + 8, 66, screen_x, world_y, true))
            color = color565(255, 255, 255);

        if (text_pixel(font5x7_ascii, GAME_FONT_SCALE, state->score_text,
                       state->score_text_length, FIXED_WORLD_START + 8, 38,
                       screen_x, world_y, true))
            color = color565(255, 255, 255);
    }

    int bird_y = (int)state->bird_y;
    int bird_left = BIRD_SCREEN_X - BIRD_WIDTH / 2;
    int bird_top = bird_y - BIRD_HEIGHT / 2;
    if (screen_x >= bird_left && screen_x < bird_left + BIRD_WIDTH &&
        world_y >= bird_top && world_y < bird_top + BIRD_HEIGHT)
    {
        int bx = screen_x - bird_left;
        int by = world_y - bird_top;
        color = color565(255, 215, 55);
        if (bx < 5 && by > 8)
            color = color565(245, 160, 35);
        if (bx >= BIRD_WIDTH - 1 && by >= 6 && by <= 10)
            color = color565(245, 105, 35);
        if (bx >= 15 && bx <= 19 && by >= 2 && by <= 6)
            color = (bx >= 17 && by >= 3 && by <= 5)
                        ? color565(25, 35, 45)
                        : color565(255, 255, 255);
    }

    if (!state->started && screen_x >= 198 && screen_x < 354 &&
        world_y >= 110 && world_y < 200)
    {
        color = color565(40, 100, 145);
        if (text_pixel(font5x7_ascii, GAME_FONT_SCALE, "FLAPPY", 6,
                       220, 128, screen_x, world_y, true) ||
            text_pixel(font5x7_ascii, GAME_FONT_SCALE, "A TO START", 10,
                       196, 158, screen_x, world_y, true))
            color = color565(255, 255, 255);
    }
    else if (state->game_over && screen_x >= 190 && screen_x < 370 &&
             world_y >= 116 && world_y < 220)
    {
        color = color565(35, 75, 110);
        if (text_pixel(font5x7_ascii, GAME_FONT_SCALE, "GAME OVER", 9,
                       206, 132, screen_x, world_y, true) ||
            text_pixel(font5x7_ascii, GAME_FONT_SCALE, "A TO RETRY", 10,
                       196, 162, screen_x, world_y, true))
            color = color565(255, 255, 255);
    }

    return color;
}

static void render_memory_column(const FlappyState *state, int screen_x,
                                 int world_x, uint16_t memory_y,
                                 int physical_x0, int physical_x1)
{
    /* Generate one landscape column into a horizontal LCD scanline and write
     * it directly to the chosen GRAM row. */
    if (physical_x0 < 0)
        physical_x0 = 0;
    if (physical_x1 > SCREEN_WIDTH)
        physical_x1 = SCREEN_WIDTH;
    if (physical_x0 >= physical_x1)
        return;

    size_t index = 0;
    for (int physical_x = physical_x0; physical_x < physical_x1; ++physical_x)
    {
        uint16_t color = scene_pixel(state, screen_x, world_x, physical_x);
        line_buffer[index++] = color >> 8;
        line_buffer[index++] = color & 0xFF;
    }

    lcd_set_window(physical_x0, memory_y, physical_x1 - 1, memory_y);
    lcd_write(line_buffer, index);
}

/* Screen columns before the HUD are part of the scrolling world; the fixed
 * HUD maps to its own local horizontal position. */
static int world_x_for_screen(const FlappyState *state, int screen_x, int camera_x)
{
    if (screen_x >= FIXED_WORLD_START)
        return screen_x - FIXED_WORLD_START;

    return camera_x + screen_x;
}

static void render_screen_column(const FlappyState *state, int screen_x,
                                 int camera_x, int physical_x0, int physical_x1)
{
    if (screen_x < 0 || screen_x >= WORLD_WIDTH)
        return;

    int display_y = WORLD_WIDTH - 1 - screen_x;
    uint16_t memory_y = memory_row_for_display_y(state, display_y);
    render_memory_column(state, screen_x,
                         world_x_for_screen(state, screen_x, camera_x),
                         memory_y, physical_x0, physical_x1);
}

/* Used by the launch animation, which requests portrait LCD rectangles.
 * Convert each physical row back to its corresponding landscape column. */
static void flappy_draw_region(void *game_state, uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height)
{
    FlappyState *state = game_state;
    /* The game is rendered 180 degrees from the text UI's portrait orientation. */
    int right = x + width;
    int bottom = y + height;
    if (right > SCREEN_WIDTH)
        right = SCREEN_WIDTH;
    if (bottom > SCREEN_HEIGHT)
        bottom = SCREEN_HEIGHT;

    for (int physical_y = y; physical_y < bottom; ++physical_y)
    {
        int screen_x = WORLD_WIDTH - 1 - physical_y;
        render_memory_column(state, screen_x,
                             world_x_for_screen(state, screen_x, 0),
                             physical_y, x, right);
    }
}

static void flappy_update(void *game_state, const GameInput *input,
                          uint32_t delta_us)
{
    FlappyState *state = game_state;

    /* A starts the title screen. Once playing, each press gives the bird an
     * upward impulse; velocity and camera movement are time-based. */
    if (!state->started)
    {
        if (input->pressed[GAME_BUTTON_A])
        {
            state->started = true;
            state->bird_velocity = FLAP_VELOCITY;
        }
        return;
    }

    if (state->game_over)
    {
        if (input->pressed[GAME_BUTTON_A])
        {
            reset_game(state);
            state->started = true;
            state->bird_velocity = FLAP_VELOCITY;
            lcd_set_vertical_scroll_start(SCROLL_TOP_FIXED);
        }
        return;
    }

    if (input->pressed[GAME_BUTTON_A])
        state->bird_velocity = FLAP_VELOCITY;

    if (delta_us > MAX_SIMULATION_STEP_US)
        delta_us = MAX_SIMULATION_STEP_US;

    float delta_seconds = delta_us / 1000000.0f;
    state->bird_velocity += GRAVITY * delta_seconds;
    state->bird_y += state->bird_velocity * delta_seconds;
    state->camera_x += PIPE_SPEED * delta_seconds;

    /* The bird stays at one screen position while camera_x advances the world. */
    float bird_world_x = state->camera_x + BIRD_SCREEN_X;
    float pipe_right = state->pipe_x + PIPE_WIDTH;
    int bird_top = (int)state->bird_y - BIRD_HEIGHT / 2;
    int bird_bottom = bird_top + BIRD_HEIGHT;
    int gap_top = state->pipe_gap_y - PIPE_GAP_HEIGHT / 2;
    int gap_bottom = state->pipe_gap_y + PIPE_GAP_HEIGHT / 2;

    if (bird_top < 0 || bird_bottom >= WORLD_HEIGHT - GROUND_HEIGHT ||
        (bird_world_x + BIRD_WIDTH / 2 > state->pipe_x &&
         bird_world_x - BIRD_WIDTH / 2 < pipe_right &&
         (bird_top < gap_top || bird_bottom > gap_bottom)))
    {
        state->game_over = true;
        return;
    }

    if (!state->pipe_scored && pipe_right < bird_world_x)
    {
        ++state->score;
        snprintf(state->score_text, sizeof(state->score_text), "%u", state->score);
        state->score_text_length = strlen(state->score_text);
        state->pipe_scored = true;
    }

    if (state->pipe_x + PIPE_CAP_WIDTH < state->camera_x)
    {
        /* Promote the preconfigured second pipe instead of rebuilding it as it
         * enters the viewport. Prepare another one ahead of the camera. */
        state->pipe_x = state->next_pipe_x;
        state->pipe_gap_y = state->next_pipe_gap_y;
        state->next_pipe_x += PIPE_SPACING;
        state->next_pipe_gap_y =
            82 + (int)(((state->score + 1u) * 37u) % 156u);
        state->pipe_scored = false;
    }
}

static void render_columns(const FlappyState *state, int first, int last,
                           int camera_x, int physical_y0, int physical_y1)
{
    /* Render selected landscape columns over a vertical band of LCD pixels. */
    if (first < 0)
        first = 0;
    if (last > WORLD_WIDTH)
        last = WORLD_WIDTH;

    for (int screen_x = first; screen_x < last; ++screen_x)
        render_screen_column(state, screen_x, camera_x, physical_y0, physical_y1);
}

static void flappy_draw(void *game_state)
{
    FlappyState *state = game_state;
    int camera_x = (int)state->camera_x;
    int old_bird_x = BIRD_SCREEN_X - (camera_x - state->rendered_camera_x);
    int old_bird_y = state->rendered_bird_y;
    bool began = state->started != state->rendered_started ||
                 state->game_over != state->rendered_game_over;

    if (camera_x > state->rendered_camera_x)
    {
        /* Move the ILI9486 viewport by the integer camera delta, then draw only
         * the newly exposed GRAM rows at the far edge. */
        int steps = camera_x - state->rendered_camera_x;
        state->scroll_start =
            (uint16_t)((state->scroll_start + SCROLL_WIDTH - (steps % SCROLL_WIDTH)) %
                       SCROLL_WIDTH);
        lcd_set_vertical_scroll_start(SCROLL_TOP_FIXED + state->scroll_start);

        for (int i = 0; i < steps; ++i)
        {
            int display_y = SCROLL_TOP_FIXED + i;
            int screen_x = WORLD_WIDTH - 1 - display_y;
            int world_x = camera_x + screen_x;
            uint16_t memory_y = (uint16_t)(SCROLL_TOP_FIXED +
                                            (state->scroll_start + i) % SCROLL_WIDTH);
            render_memory_column(state, screen_x, world_x, memory_y, 0, SCREEN_WIDTH);
        }
    }

    if (began)
        /* Clear stale title/game-over pixels with a full scene redraw only on
         * these state transitions; ordinary frames use the partial paths. */
        render_columns(state, 0, WORLD_WIDTH, camera_x, 0, WORLD_HEIGHT);
    else
    {
        /* Redraw the union of the old and new bird bounds so the old sprite is
         * erased without touching the rest of the scrolling scene. */
        int bird_top = (old_bird_y < (int)state->bird_y
                            ? old_bird_y
                            : (int)state->bird_y) -
                       BIRD_HEIGHT / 2 - 3;
        int bird_bottom = (old_bird_y > (int)state->bird_y
                               ? old_bird_y
                               : (int)state->bird_y) +
                          BIRD_HEIGHT / 2 + 3;
        render_columns(state,
                       (old_bird_x < BIRD_SCREEN_X ? old_bird_x : BIRD_SCREEN_X) -
                           BIRD_WIDTH / 2 - 2,
                       (old_bird_x > BIRD_SCREEN_X ? old_bird_x : BIRD_SCREEN_X) +
                           BIRD_WIDTH / 2 + 3,
                       camera_x, bird_top, bird_bottom);
    }

    if (state->score != state->rendered_score)
    {
        /* Score changes affect only its text box in the fixed HUD. */
        int score_width = (int)state->score_text_length * 7 * GAME_FONT_SCALE;
        render_columns(state, FIXED_WORLD_START + 6,
                       FIXED_WORLD_START + 8 + score_width,
                       camera_x, 36, 56);
    }

    state->rendered_camera_x = camera_x;
    state->rendered_bird_y = (int)state->bird_y;
    state->rendered_score = state->score;
    state->rendered_started = state->started;
    state->rendered_game_over = state->game_over;
}

static void flappy_stop(void *game_state)
{
    reset_game(game_state);
    /* Restore full-screen scrolling before returning to launcher menus. */
    lcd_set_vertical_scroll_area(0, SCREEN_HEIGHT, 0);
    lcd_set_vertical_scroll_start(0);
    lcd_clear(0, 0, 0);
}

const Game flappy_game = {
    .state = &flappy_state,
    .start = flappy_start,
    .update = flappy_update,
    .draw = flappy_draw,
    .draw_region = flappy_draw_region,
    .stop = flappy_stop,
};
