#ifndef GAME_STATE_H
#define GAME_STATE_H

#define MINE 0b0010000
#define UNVLD 0b0100000
#define FLAGGED 0b1000000

typedef struct game {
  char *cells;
  int width;
  int height;
  int size;
  int mines;
  int flags;
  int faults;
  int unveiled;
  GameState state;
  time_t started;
  Cord cord;
  Cord *unveil_queue;
} *GameInstance;

#endif

