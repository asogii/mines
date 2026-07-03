#include "game.h"
#include "game_state.h"
#include "generator.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

static inline bool is_mine(unsigned char c) {
    return (c & MINE) != 0;
}
static inline bool is_flagged(unsigned char c) {
    return (c & FLAGGED) != 0;
}
static inline bool is_blank(unsigned char c) {
    return (c & 0x0F) == 0;
}
static inline bool is_unveiled(unsigned char c) {
    return (c & UNVLD) != 0;
}

GameInstance createGameInstance(int width, int height, int amount_mines) {
  GameInstance g = calloc(1, sizeof(struct game));
  int size = width * height;
  char *cells = calloc(size, sizeof(char));
  g->size = size;
  g->cells = cells;
  g->width = width;
  g->height = height;
  g->mines = amount_mines;
  g->state = PLAYING;
  g->unveil_queue = malloc(size * sizeof(Cord));
  time(&g->started);
  return g;
}
GameInstance new_game(int width, int height, int amount_mines) {
  return new_game_no_guess(width, height, amount_mines, 0, 0);
}
GameInstance new_game_pure_random(int width, int height, int amount_mines) {
  GameInstance g = createGameInstance(width, height, amount_mines);
  int *indices = malloc(g->size * sizeof(int));
  for (int i = 0; i < g->size; i++) {
    indices[i] = i;
  }
  srand48(time(0));
  for (int i = 0; i < amount_mines; i++) {
    int j = i + (int)(drand48() * (g->size - i));
    int temp = indices[i];
    indices[i] = indices[j];
    indices[j] = temp;
    g->cells[indices[i]] = MINE;
  }
  free(indices);
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      if (!is_mine(g->cells[i * width + j])) {
        int mcount = 0;
        for (int k = i - 1; k <= i + 1; k++) {
          for (int l = j - 1; l <= j + 1; l++) {
            if (k >= 0 && k < height && l >= 0 && l < width) {
              if (is_mine(g->cells[k * width + l])) {
                mcount++;
              }
            }
          }
        }
        g->cells[i * width + j] = mcount;
      }
    }
  }
  return g;
}

void deleteGameInstance(GameInstance g) {
  free(g->cells);
  free(g->unveil_queue);
  free(g);
}

static inline CellType get_cell_type(unsigned char cell) {
  switch (cell & 0x0F) {
  case 1:
    return ONE;
  case 2:
    return TWO;
  case 3:
    return THREE;
  case 4:
    return FOUR;
  case 5:
    return FIVE;
  case 6:
    return SIX;
  case 7:
    return SEVEN;
  case 8:
    return EIGHT;
  }
  return UNVEILED;
}
GameView createView(GameInstance g) {
  GameView view = calloc(1, sizeof(struct game_view));
  CellType *cells = calloc(g->size, sizeof(enum cell_type));
  view->cells = cells;
  view->player = g->cord;
  time_t current;
  time(&current);
  view->mines = g->mines - g->flags;
  view->time = current - g->started;
  view->width = g->width;
  view->state = g->state;
  for (int i = 0; i < g->size; i++) {
    if (is_flagged(g->cells[i])) {
      view->cells[i] = FLAG;
    } else if (is_unveiled(g->cells[i])) {
      view->cells[i] = get_cell_type(g->cells[i]);
    } else {
      view->cells[i] = UNTOUCHED;
    }
  }
  view->height = g->height;
  view->width = g->width;
  return view;
}
GameView createViewGameover(GameInstance g) {
  GameView view = calloc(1, sizeof(struct game_view));
  CellType *cells = calloc(g->size, sizeof(enum cell_type));
  view->cells = cells;
  view->player = g->cord;
  time_t current;
  time(&current);
  view->mines = g->mines - g->flags;
  view->time = current - g->started;
  view->width = g->width;
  view->state = g->state;
  for (int i = 0; i < g->size; i++) {
    if (is_flagged(g->cells[i])) {
      if (is_mine(g->cells[i])) {
        view->cells[i] = FLAG;
      } else {
        view->cells[i] = FALSE_FLAG;
      }
    } else {
      if (is_mine(g->cells[i])) {
        view->cells[i] = FLAG_NOT_FOUND;
        continue;
      }
      view->cells[i] = get_cell_type(g->cells[i]);
    }
  }
  view->cells[g->cord.y * g->width + g->cord.x] = FALSE_FLAG;
  view->height = g->height;
  view->width = g->width;
  return view;
}

void deleteView(GameView view) {
  free(view->cells);
  free(view);
}

GameState game_state(GameInstance game) { return game->state; }
Cord player_position(GameInstance g) { return g->cord; }
void set_player_position_x(GameInstance g, int x) { g->cord.x = x; }
void set_player_position_y(GameInstance g, int y) { g->cord.y = y; }
int field_width(GameInstance g) { return g->width; }
int field_height(GameInstance g) { return g->height; }
unsigned total_mines(GameInstance g) { return g->mines; }

void flag_cell(GameInstance g) {
  int idx = g->cord.y * g->width + g->cord.x;
  int s = g->cells[idx];
  if (is_flagged(s)) {
    g->cells[idx] ^= FLAGGED;
    g->flags -= 1;
    if (!is_mine(s)) {
      g->faults -= 1;
    }
  } else {
    if (is_unveiled(s) || g->flags >= g->mines)
      return;
    g->cells[idx] |= FLAGGED;
    g->flags += 1;
    if (!is_mine(s)) {
      g->faults += 1;
    }
    if (g->flags == g->mines && g->faults == 0) {
      g->state = WON;
    }
  }
}

void unveil_recursive(GameInstance g, Cord start_pos) {
  Cord *queue = g->unveil_queue;

  int head = 0, tail = 0;
  int start_idx = start_pos.y * g->width + start_pos.x;

  if (!is_unveiled(g->cells[start_idx])) {
      g->cells[start_idx] |= UNVLD;
      g->unveiled++;
      queue[tail++] = start_pos;
  }

  while (head < tail) {
    Cord pos = queue[head++];
    int idx = pos.y * g->width + pos.x;

    if (g->unveiled + g->mines == g->size) {
      g->state = WON;
    }

    if (!is_blank(g->cells[idx])) continue;

    for (int y = pos.y - 1; y <= pos.y + 1; y++) {
      for (int x = pos.x - 1; x <= pos.x + 1; x++) {
        if (x < 0 || y < 0 || x >= g->width || y >= g->height) continue;
        if (x == pos.x && y == pos.y) continue;

        int n_idx = y * g->width + x;

        if (!is_unveiled(g->cells[n_idx])) {
          g->cells[n_idx] |= UNVLD;
          g->unveiled++;

          Cord next_pos = {x, y};
          queue[tail++] = next_pos;
        }
      }
    }
  }
}
void chord(GameInstance g, Cord pos) {
  char cur_s = g->cells[pos.y * g->width + pos.x];
  char flags_around = 0;
  for (int y = pos.y - 1; y <= pos.y + 1; y++) {
    for (int x = pos.x - 1; x <= pos.x + 1; x++) {
      if (x < 0 || y < 0 || x >= g->width || y >= g->height)
        continue;
      if (x == pos.x && y == pos.y) {
        continue;
      }
      if (is_flagged(g->cells[y * g->width + x])) {
        flags_around++;
      }
    }
  }
  if ((cur_s & 0x0F) == flags_around) {
    for (int y = pos.y - 1; y <= pos.y + 1; y++) {
      for (int x = pos.x - 1; x <= pos.x + 1; x++) {
        if (x < 0 || y < 0 || x >= g->width || y >= g->height)
          continue;
        if (x == pos.x && y == pos.y) {
          continue;
        }
        if (!is_unveiled(g->cells[y * g->width + x])) {
          Cord cord;
          cord.x = x;
          cord.y = y;
          unveil_cell_at(g, cord);
        }
      }
    }
  }
}

void unveil_cell(GameInstance g) { unveil_cell_at(g, g->cord); }
void unveil_cell_at(GameInstance g, Cord pos) {
  char cur_s = g->cells[pos.y * g->width + pos.x];
  if (is_flagged(cur_s)) {
    return;
  }
  if (is_mine(cur_s)) {
    g->state = LOST;
    return;
  }
  if (is_unveiled(cur_s)) {
    chord(g, pos);
    return;
  }
  if (cur_s > 0) {
    g->cells[pos.y * g->width + pos.x] |= UNVLD;
    g->unveiled++;
    if (g->unveiled + g->mines == g->size)
      g->state = WON;
    return;
  }
  unveil_recursive(g, pos);
}

Highscore generate_highscore(GameInstance g) {
  time_t current;
  time(&current);
  if (g->state != WON)
    exit(EXIT_FAILURE);
  Highscore h;
  h.width = g->width;
  h.height = g->height;
  h.mines = g->mines;
  h.time = current - g->started;
  h.date = current;
  return h;
}
