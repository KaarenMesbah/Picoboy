#ifndef GAME_H
#define GAME_H

#include <stdint.h>

typedef struct
{
    void (*start)(void);
    void (*draw_region)(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void (*update)(void);
    void (*stop)(void);
} Game;

#endif
