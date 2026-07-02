#ifndef GENERATE_H
#define GENERATE_H

#include "game.h"
#include <stdbool.h>

GameInstance create_no_guess_game(int width, int height, int amount_mines, int sx, int sy);

#endif
