#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    GAME_BUTTON_A,
    GAME_BUTTON_B,
    GAME_BUTTON_START,
    GAME_BUTTON_COUNT
} GameButton;

typedef struct
{
    bool down[GAME_BUTTON_COUNT];
    bool pressed[GAME_BUTTON_COUNT];
    bool released[GAME_BUTTON_COUNT];
} GameInput;

typedef struct
{
    /* Game-owned state; start initializes it and stop releases/resets it. */
    void *state;
    void (*start)(void *state);
    /* update runs before draw; delta_us is elapsed time since the prior update. */
    void (*update)(void *state, const GameInput *input, uint32_t delta_us);
    /* draw renders the complete current frame; draw_region is for transitions. */
    void (*draw)(void *state);
    void (*draw_region)(void *state, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height);
    void (*stop)(void *state);
} Game;

#endif
