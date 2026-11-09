# Creating a PicoBoy game

This guide describes the game interface currently used by the launcher and
shows how to add a game module. Games are compiled into the firmware; `stop()`
ends a game's lifecycle but does not unload its machine code.

## What the engine provides

The interface is declared in [`game.h`](./game.h), and the main loop in
[`../main.c`](../main.c) calls it.

- **Owned state:** `Game.state` points at the game's data. Use fixed/static
  storage where practical on the Pico. `start(state)` resets it for a new run;
  `stop(state)` releases any resources and resets it.
- **Input snapshot:** `GameInput` provides `down`, `pressed`, and `released`
  states for A, B, and START. `pressed` and `released` are one-frame events.
  The launcher reserves holding START+B for one second to exit the game.
- **Update timing:** `update(state, input, delta_us)` runs before drawing.
  `delta_us` is elapsed microseconds since the previous update; the first
  update after launch receives zero. The launcher targets 60 frames per second.
  Large elapsed values can happen after slow work, so clamp them for physics.
- **Full-frame drawing:** `draw(state)` renders the current frame. The launcher
  calls it after each update.
- **Clipped transition drawing:** `draw_region(state, x, y, width, height)`
  renders the requested physical-display rectangle. The launcher calls it
  during the chunked launch transition. Redraw every scene layer that intersects
  that rectangle, in background-to-foreground order.
- **LCD primitives:** [`../ui/lcd.h`](../ui/lcd.h) exposes pixel, rectangle,
  LCD-window, and vertical-scroll operations. `fill_rect_rgb565` accepts RGB
  components (despite its name) and clips to the display.
- **Text and font data:** [`../ui/ui.h`](../ui/ui.h) provides screen-clipped
  string drawing. `draw_string_clipped` paints both foreground and background,
  so it is opaque. The built-in font data is `font5x7_ascii[96][5]` in
  [`../asset/fonts.h`](../asset/fonts.h). The reusable `text_pixel` helper in
  `ui.h` accepts a font table with this same 96-character, 5-column glyph
  layout, a scale, and dynamic text. It is useful when generating a scene
  pixel-by-pixel.
- **Hardware scrolling:** the LCD scroll-area and scroll-start helpers control
  the ILI9486's GRAM. Flappy demonstrates using a fixed HUD area and a circular
  scrolling playfield. A game that changes scroll configuration must restore
  it in `stop()`.

The menu's game-exit combo and display coordinates are launcher-level behavior,
not per-game services. Current display constants are in
[`../hardware_config.h`](../hardware_config.h).

## Minimal game module

Create `games/my_game.c` and include it in `add_executable` in
[`../CMakeLists.txt`](../CMakeLists.txt):

```c
#include "game.h"
#include "hardware_config.h"
#include "lcd.h"

typedef struct
{
    unsigned int frames;
} MyGameState;

static MyGameState my_state;

static void my_start(void *data)
{
    MyGameState *state = data;
    state->frames = 0;
}

static void my_update(void *data, const GameInput *input, uint32_t delta_us)
{
    MyGameState *state = data;
    (void)delta_us;

    if (input->pressed[GAME_BUTTON_A])
        ++state->frames;
}

static void my_draw(void *data)
{
    (void)data;
}

static void my_draw_region(void *data, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height)
{
    (void)data;
    fill_rect_rgb565(x, y, width, height, 20, 30, 50);
}

static void my_stop(void *data)
{
    MyGameState *state = data;
    state->frames = 0;
}

const Game my_game = {
    .state = &my_state,
    .start = my_start,
    .update = my_update,
    .draw = my_draw,
    .draw_region = my_draw_region,
    .stop = my_stop,
};
```

In the current launcher, register it in `main.c`:

```c
extern const Game my_game;

static void launch_my_game(void)
{
    active_game = &my_game;
    active_game->start(active_game->state);
    play_game_launch_wipe();
}

static const MenuOption game_options[] = {
    {"Hello", launch_hello_game},
    {"Flappy", launch_flappy_game},
    {"My Game", launch_my_game},
    {NULL, NULL},
};
```

Also add `games/my_game.c` to the firmware's `add_executable(...)` source list.
The launcher currently has one statically-linked module per menu callback; the
registry can be made table-driven in a later engine improvement.

## Using input and elapsed time

Always use `pressed` for one-shot actions, rather than `down`, or an action can
repeat every frame while held:

```c
if (input->pressed[GAME_BUTTON_A])
    state->vertical_velocity = -300.0f;

float seconds = delta_us / 1000000.0f;
state->vertical_velocity += gravity * seconds;
state->y += state->vertical_velocity * seconds;
```

Clamp `delta_us` if a long pause should not cause a huge simulation jump.
START+B is also visible in the input snapshot, but holding both for one second
is interpreted by the launcher as exit.

## Drawing scene regions without flicker

A region draw must reconstruct its whole scene region, not only draw the
foreground object. A typical order is background, obstacles, player, then HUD.
For moving objects, remember the previous and current bounds and redraw their
union so the old position is erased. For scrolling games, redraw newly exposed
columns/rows and update the ILI9486 scroll start rather than repainting all
moving objects each frame. See [`flappy_game.c`](./flappy_game.c) for an
example.

The text renderer in `ui.c` rotates text 90 degrees to match the launcher UI.
Flappy uses landscape world coordinates and explicitly maps them to the
portrait physical display; account for this when placing pixels and text.

## Dynamic text with `text_pixel`

`text_pixel` samples one pixel from a string without drawing to the LCD. It
takes the font table, scale, text pointer and length, text origin, queried
pixel, and whether to rotate the text 180 degrees:

```c
#include "fonts.h"
#include "ui.h"

bool lit = text_pixel(font5x7_ascii, 2,
                      state->score_text, state->score_text_length,
                      text_x, text_y, pixel_x, pixel_y,
                      true);
```

`text` can be a changing buffer—for example, update it using `snprintf` when
the score changes. Cache its length when the text changes so the per-pixel
renderer does not repeatedly scan the string. Set the last argument to `false`
for normal orientation or `true` for 180° rotation. A `true` result means the
queried pixel belongs to a foreground glyph pixel; draw the desired foreground
color there and your background color otherwise. The helper treats characters
outside printable ASCII as `?` and assumes glyphs are 5 columns by 7 rows.
Supply another font table with the same layout to change the font; a font with
different glyph dimensions needs a correspondingly generalized helper.

## Memory and cleanup

Prefer static or bounded state. If a game allocates a resource, its game state
should own it and `stop(state)` must release it and reset the pointer/value.
The launcher calls `stop` when the user exits with START+B. Do not retain
pointers to stack-local buffers after a callback returns.
