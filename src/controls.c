#include "display.h"
#include "game.h"
#include <ncurses.h>
#include <stdlib.h>

int cmove(GameInstance g, WINDOW **window, int timeout_ms) {
  extern char g_helper_mode;
  extern char print_highscore_flag;
  wtimeout(*window, timeout_ms);
  int ch = getch();
  if (ch == ERR) {
    return 0;
  }
  Cord old = player_position(g);
  switch (ch) {

  case KEY_DOWN:
  case 'j':
    set_player_position_y(g, old.y + 1);
    if (player_position(g).y == field_height(g))
      set_player_position_y(g, 0);
    break;

  case KEY_UP:
  case 'k':
    set_player_position_y(g, old.y - 1);
    if (player_position(g).y == -1)
      set_player_position_y(g, field_height(g) - 1);
    break;

  case KEY_LEFT:
  case 'h':
    set_player_position_x(g, old.x - 1);
    if (player_position(g).x == -1)
      set_player_position_x(g, field_width(g) - 1);
    break;

  case KEY_RIGHT:
  case 'l':
    set_player_position_x(g, old.x + 1);
    if (player_position(g).x == field_width(g))
      set_player_position_x(g, 0);
    break;

  case 'q':
    return -1;
  case 'f':
    flag_cell(g);
    break;
  case 's':
    unveil_cell(g);
    break;
  case 'e':
    g_helper_mode ^= 1;
    break;
  case 'b':
    print_highscore_flag ^= 1;
    break;
  case 'm':
    endwin();
    if (system("man mines-tui") != 0) {
      printf("Please press enter to continue.\n");
      while (getchar() == 0)
        ;
    }
    *window = create_window();
    break;
  }
  return 0;
}
