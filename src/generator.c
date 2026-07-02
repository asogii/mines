#include "generator.h"
#include "game.h"
#include "game_state.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIM_HIDDEN 0
#define SIM_REVEALED 1
#define SIM_FLAGGED 2

static void recalculate_numbers(char *mines, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (mines[idx] & MINE) continue;

            int mcount = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy, nx = x + dx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        if (mines[ny * width + nx] & MINE) mcount++;
                    }
                }
            }
            mines[idx] = (mines[idx] & ~0b1111) | mcount;
        }
    }
}

static bool simulate_solve(char *mines, int width, int height, int sx, int sy, int *frontier_x, int *frontier_y) {
    int length = width * height;
    char *sim = calloc(length, sizeof(char));

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int ny = sy + dy, nx = sx + dx;
            if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                sim[ny * width + nx] = SIM_REVEALED;
            }
        }
    }

    bool progress = true;
    while (progress) {
        progress = false;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;
                if (sim[idx] != SIM_REVEALED) continue;

                int true_num = mines[idx] & 0b1111;
                int hidden_count = 0;
                int flag_count = 0;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy, nx = x + dx;
                        if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                            if (sim[ny * width + nx] == SIM_HIDDEN) hidden_count++;
                            if (sim[ny * width + nx] == SIM_FLAGGED) flag_count++;
                        }
                    }
                }

                if (hidden_count == 0) continue;

                if (true_num == flag_count + hidden_count) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int ny = y + dy, nx = x + dx;
                            if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                                if (sim[ny * width + nx] == SIM_HIDDEN) {
                                    sim[ny * width + nx] = SIM_FLAGGED;
                                    progress = true;
                                }
                            }
                        }
                    }
                }
                else if (true_num == flag_count) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int ny = y + dy, nx = x + dx;
                            if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                                if (sim[ny * width + nx] == SIM_HIDDEN) {
                                    sim[ny * width + nx] = SIM_REVEALED;
                                    progress = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. Pairwise Solver
    }

    bool solved = true;
    for (int i = 0; i < length; i++) {
        if (!(mines[i] & MINE) && sim[i] != SIM_REVEALED) {
            solved = false;
            int x = i % width;
            int y = i / width;
            *frontier_x = x;
            *frontier_y = y;
            break;
        }
    }

    free(sim);
    return solved;
}

void reassign_flat(
    GameInstance g, int width, int height, int length,
    int sx, int sy, int fx, int fy
) {
    int target_mine_idx = -1;
    int target_safe_idx = -1;

    int offset_m = rand() % length;
    for(int i = 0; i < length; i++) {
        int idx = (offset_m + i) % length;
        if ((g->mines[idx] & MINE)) {
            target_mine_idx = idx;
            break;
        }
    }

    int offset_s = rand() % length;
    for(int i = 0; i <= length; i++) {
        int idx = (offset_s + i) % length;
        int cx = idx % width;
        int cy = idx / width;
        if (!(g->mines[idx] & MINE) && (abs(cx - sx) > 1 || abs(cy - sy) > 1)) {
            target_safe_idx = idx;
            break;
        }
    }

    if (target_mine_idx != -1 && target_safe_idx != -1) {
        g->mines[target_mine_idx] &= ~MINE;
        g->mines[target_safe_idx] |= MINE;
    }
}

void reassign_local(
    GameInstance g, int width, int height, int length,
    int sx, int sy, int fx, int fy
) {
    int target_mine_idx = -1;
    int target_safe_idx = -1;

    int mines_list[25];
    int safes_list[25];
    int m_count = 0, s_count = 0;

    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int nx = fx + dx;
            int ny = fy + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (abs(nx - sx) <= 1 && abs(ny - sy) <= 1) continue;

                int idx = ny * width + nx;
                if (g->mines[idx] & MINE) {
                    mines_list[m_count++] = idx;
                } else {
                    safes_list[s_count++] = idx;
                }
            }
        }
    }

    if (m_count > 0 && s_count > 0) {
        target_mine_idx = mines_list[rand() % m_count];
        target_safe_idx = safes_list[rand() % s_count];
    }
    else {
        int offset_m = rand() % length;
        for (int i = 0; i < length; i++) {
            int idx = (offset_m + i) % length;
            if (g->mines[idx] & MINE) {
                target_mine_idx = idx;
                break;
            }
        }
        int offset_s = rand() % length;
        for (int i = 0; i < length; i++) {
            int idx = (offset_s + i) % length;
            int cx = idx % width;
            int cy = idx / width;
            if (!(g->mines[idx] & MINE) && (abs(cx - sx) > 1 || abs(cy - sy) > 1)) {
                target_safe_idx = idx;
                break;
            }
        }
    }

    if (target_mine_idx != -1 && target_safe_idx != -1) {
        g->mines[target_mine_idx] &= ~MINE;
        g->mines[target_safe_idx] |= MINE;
    }
}

GameInstance create_no_guess_game(int width, int height, int amount_mines, int sx, int sy) {
    GameInstance g = createGameInstanceNormal(width, height, 0);
    int length = width * height;

    for (int i = 0; i < length; i++) g->mines[i] = 0;

    int placed = 0;
    srand((unsigned int)time(NULL));
    while (placed < amount_mines) {
        int r = rand() % length;
        int rx = r % width;
        int ry = r / width;

        if (abs(rx - sx) <= 1 && abs(ry - sy) <= 1) continue;

        if (!(g->mines[r] & MINE)) {
            g->mines[r] |= MINE;
            placed++;
        }
    }

    int max_retries = 5000;
    int retries = 0;

    while (retries < max_retries) {
        recalculate_numbers(g->mines, width, height);

        int fx = 0, fy = 0;
        if (simulate_solve(g->mines, width, height, sx, sy, &fx, &fy)) {
            break;
        }

        // reassign_flat(g, width, height, length, sx, sy, fx, fy);
        reassign_local(g, width, height, length, sx, sy, fx, fy);

        retries++;
    }

    g->flagstotal = amount_mines;
    g->flagsfound = 0;
    g->unveiled = 0;
    g->state = PLAYING;
    g->cord.x = sx;
    g->cord.y = sy;

    return g;
}
