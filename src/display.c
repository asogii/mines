#include "display.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

WINDOW *create_window() {
  WINDOW *window = initscr();
  start_color();
  curs_set(0);
  noecho();
  keypad(window, TRUE);
  return window;
}

void print_top_margin(unsigned int terminal_y, unsigned int game_y) {
  int l = terminal_y / 2 - ((game_y + 5) / 2);
  for (int i = 0; i < l; i++) {
    printw("\n");
  }
}
void print_left_margin(unsigned int terminal_x, unsigned int game_x) {
  int l = terminal_x / 2 - ((game_x + 2) / 2);
  if (l > 0) {
    printw("%*s", l, "");
  }
}

void print_scrollable(char **text, unsigned terminal_width,
                      unsigned text_width) {
  print_left_margin(terminal_width, text_width);
  printw("╭");
  for (int i = 0; i < text_width; i++) {
    printw("─");
  }
  printw("╮\n");
  char **tmp = text;
  if (text == NULL)
    return;
  unsigned index = 0;
  while (*tmp != NULL && index < highscore_window_height) {
    print_left_margin(terminal_width, text_width);
    printw("│");
    printw("%s", *tmp);
    for (int i = 0; i < text_width - strlen(*tmp); i++)
      printw(" ");
    printw("│\n");
    tmp++;
    index++;
  }
  print_left_margin(terminal_width, text_width);
  printw("╰");
  for (int i = 0; i < text_width; i++) {
    printw("─");
  }
  printw("╯\n");
}

void print_header(GameView view, unsigned int terminal_x, unsigned int game_x) {
  print_left_margin(terminal_x, game_x * 2);
  printw("│");
  printw("    %03d    │", view->mines);
  for (int i = 0; i < (view->width * 2) - 23; i++) {
    if (i == (view->width * 2 - 23) / 2) {
      switch (view->state) {
      case PLAYING:
        printw(" ");
        break;
      case WON:
        attron(COLOR_PAIR(5));
        printw("☆");
        attroff(COLOR_PAIR(5));
        break;
      case LOST:
        attron(COLOR_PAIR(3));
        printw("☠");
        attroff(COLOR_PAIR(3));
      }
      continue;
    }
    printw(" ");
  }
  printw("│   %02ld:%02ld  │\n", (long)view->time / 60, (long)view->time % 60);
  print_left_margin(terminal_x, game_x * 2);
  printw("╰");
  for (int i = 0; i < view->width; i++) {
    if (i == 5 || i == view->width - 6) {
      printw("─┴");
      continue;
    }
    printw("──");
  }
  printw("╯\n");
}

void print(GameView view, unsigned int terminal_x, unsigned int game_x) {
  char is_in_radius = 0;
  extern char g_helper_mode;
  init_pair(1, COLOR_BLUE, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_RED, COLOR_BLACK);
  init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(5, COLOR_YELLOW, COLOR_BLACK);
  init_pair(6, COLOR_CYAN, COLOR_BLACK);
  print_left_margin(terminal_x, game_x * 2);
  printw("╭");
  for (int i = 0; i < view->width; i++) {
    printw("──");
  }
  printw("╮\n");
  for (int i = 0; i < view->width * view->height; i++) {
    if (i % view->width == 0 && i > 0) {
      printw("│\n");
    }
    if (i % view->width == 0) {
      print_left_margin(terminal_x, game_x * 2);
      printw("│");
    }
    if (i == (view->player.y * view->width + view->player.x))
      attron(A_STANDOUT);
    if ((i == view->player.y * view->width + view->player.x + 1 ||
         i == view->player.y * view->width + view->player.x - 1 ||
         i == view->player.y * view->width + view->player.x + 1 + view->width ||
         i == view->player.y * view->width + view->player.x - 1 + view->width ||
         i == view->player.y * view->width + view->player.x + 1 - view->width ||
         i == view->player.y * view->width + view->player.x - 1 - view->width ||
         i == view->player.y * view->width + view->player.x - view->width ||
         i == view->player.y * view->width + view->player.x + view->width) &&
        i % view->width >= 0 &&
        abs(i % (int)view->width - view->player.x) < 2 && g_helper_mode) {
      is_in_radius = 1;
      attron(A_BOLD);
    } else {
      attroff(A_BOLD);
      is_in_radius = 0;
    }

    switch (view->cells[i]) {
    case UNTOUCHED:
      if (is_in_radius) {
        printw("▒▒");
      } else {
        printw("░░");
      }
      break;
    case UNVEILED:
      if (i == (view->player.y * view->width + view->player.x)) {
        attroff(A_STANDOUT);
        printw("▒▒");
        attron(A_STANDOUT);
      } else {
        printw("　");
      }
      break;
    case FLAG:
      attron(COLOR_PAIR(6));
      printw("🚩");
      attroff(COLOR_PAIR(6));
      break;
    case FALSE_FLAG:
      attron(COLOR_PAIR(3));
      printw("Ｘ");
      attroff(COLOR_PAIR(3));
      break;
    case FLAG_NOT_FOUND:
      printw("💣");
      break;
    case ONE:
      attron(COLOR_PAIR(1));
      printw("１");
      attroff(COLOR_PAIR(1));
      break;
    case TWO:
      attron(COLOR_PAIR(2));
      printw("２");
      attroff(COLOR_PAIR(2));
      break;
    case THREE:
      attron(COLOR_PAIR(3));
      printw("３");
      attroff(COLOR_PAIR(3));
      break;
    case FOUR:
      attron(COLOR_PAIR(4));
      printw("４");
      attroff(COLOR_PAIR(4));
      break;
    case FIVE:
      attron(COLOR_PAIR(5));
      printw("５");
      attroff(COLOR_PAIR(5));
      break;
    case SIX:
      attron(COLOR_PAIR(4));
      printw("６");
      attroff(COLOR_PAIR(4));
      break;
    case SEVEN:
      attron(COLOR_PAIR(5));
      printw("７");
      attroff(COLOR_PAIR(5));
      break;
    case EIGHT:
      attron(COLOR_PAIR(5));
      printw("８");
      attroff(COLOR_PAIR(5));
      break;
    }
    if (i == (view->player.y * view->width + view->player.x))
      attroff(A_STANDOUT);
  }
  attroff(A_BOLD);
  printw("│\n");
  print_left_margin(terminal_x, game_x * 2);
  printw("├");
  for (int i = 0; i < view->width; i++) {
    if (i == 5 || i == view->width - 6) {
      printw("─┬");
      continue;
    }
    printw("──");
  }
  printw("┤\n");
  print_header(view, terminal_x, game_x);
}
