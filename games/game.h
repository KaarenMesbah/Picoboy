#ifndef GAME_H
#define GAME_H

typedef struct
{
    void (*start)(void);
    void (*update)(void);
    void (*stop)(void);
} Game;

#endif
