#ifndef GENERATE_H
#define GENERATE_H

#include "game.h"
#include <stdbool.h>
#include <stdatomic.h>

GameInstance new_game_no_guess(int width, int height, int amount_mines, int sx, int sy);

#endif
