#include "../config.h"
#include "controls.h"
#include "display.h"
#include "game.h"
#include "helper.h"
#include "highscore.h"
#include "menu.h"
#include "log.h"
#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static long to_wait() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (1000 - (ts.tv_nsec / 1000000)) % 1000;
}

void print_highscore(unsigned terminal_width, unsigned terminal_height,
                     GameInstance game) {
    unsigned scroll_index = 0;
  struct highscore cmp;
  cmp.width = field_width(game);
  cmp.height = field_height(game);
  cmp.mines = total_mines(game);

  UserHighscore *highscores = load_highscores(cmp);
  char **printable_highscores = userHighscores2string(highscores);
  struct dimension text_max =
      buff_max_dimensions(printable_highscores, highscore_capacity);

  while (1) {
    erase();
    print_top_margin(terminal_height, highscore_window_height + 2);
    print_scrollable(printable_highscores + scroll_index, terminal_width,
                     text_max.width);

    int ch = getch();

    switch (ch) {
    case 'j':
    case KEY_DOWN:
      if (scroll_index + highscore_window_height < text_max.len)
        scroll_index++;
      break;

    case 'k':
    case KEY_UP:
      if (scroll_index > 0)
        scroll_index--;
      break;

    case 'h':
    case KEY_LEFT:
      break;

    case 'q':
    case 'b':
      clear_char_buff(printable_highscores, highscore_capacity);
      print_highscore_flag ^= 1;
      return;
    }
  }
}

void print_game(GameInstance game, WINDOW *window) {
  if (print_highscore_flag) {
    wtimeout(window, -1);
    print_highscore(getmaxx(window), getmaxy(window), game);
  }
  erase();
  print_top_margin(getmaxy(window), field_height(game) + 2);
  GameView view;
  if (game_state(game) == LOST) {
    view = createViewGameover(game);
  } else {
    view = createView(game);
  }
  print(view, getmaxx(window), field_width(game));
  deleteView(view);
}

int main(void) {
  log_init();
  FILE *local_highscores = init_state_files();
  fclose(local_highscores);
  setlocale(LC_CTYPE, "");
  WINDOW *window = create_window();
  GameInstance game = select_mode(&window);
  if (game == NULL) {
    goto end;
  }

start:
  local_highscores = init_state_files();
  if (local_highscores == NULL)
    exit(EXIT_FAILURE);
  while (1) {
    print_game(game, window);
    if (cmove(game, &window, to_wait()) == -1)
      break;
    GameState state = game_state(game);
    if (state == PLAYING) {
      continue;
    }
    if (state == WON) {
      Highscore highscore = generate_highscore(game);
      save_highscore(highscore, local_highscores);
    }
    print_game(game, window);
    while (1) {
      if (cmove(game, &window, -1) == -1)
        goto new_game;
    }
  }
new_game:
  deleteGameInstance(game);
  fclose(local_highscores);
  nodelay(window, 0);
  game = select_mode(&window);
  if (game != NULL)
    goto start;
end:
  endwin();
  log_close();
  return 0;
}
