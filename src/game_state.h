#ifndef GAME_STATE_H
#define GAME_STATE_H

#define MINE 0b0010000
#define UNVLD 0b0100000
#define FLAGGED 0b1000000

typedef struct game {
  char *mines;
  int width;
  int height;
  int length;
  int flagstotal;
  int flagsfound;
  int faults;
  int unveiled;
  GameState state;
  time_t started;
  Cord cord;
} *GameInstance;

#endif

