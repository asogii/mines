#include "generator.h"
#include "game_state.h"
#include "log.h"
#include <ccadical.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

#define SIM_HIDDEN 0
#define SIM_REVEALED 1
#define SIM_FLAGGED 2

static void add_comb_at_least(CCaDiCaL *solver, int *vars, int n, int k_req, int start, int depth, int *comb, int act_var) {
    if (depth == k_req) {
        ccadical_add(solver, -act_var);
        for (int i = 0; i < depth; i++) ccadical_add(solver, comb[i]);
        ccadical_add(solver, 0);
        return;
    }
    for (int i = start; i <= n - (k_req - depth); i++) {
        comb[depth] = vars[i];
        add_comb_at_least(solver, vars, n, k_req, i + 1, depth + 1, comb, act_var);
    }
}

static void add_comb_at_most(CCaDiCaL *solver, int *vars, int n, int k_req, int start, int depth, int *comb, int act_var) {
    if (depth == k_req) {
        ccadical_add(solver, -act_var);
        for (int i = 0; i < depth; i++) ccadical_add(solver, -comb[i]);
        ccadical_add(solver, 0);
        return;
    }
    for (int i = start; i <= n - (k_req - depth); i++) {
        comb[depth] = vars[i];
        add_comb_at_most(solver, vars, n, k_req, i + 1, depth + 1, comb, act_var);
    }
}

static void add_exactly_k(CCaDiCaL *solver, int *vars, int n, int k, int act_var) {
    int comb[10];
    if (k > 0) add_comb_at_least(solver, vars, n, n - k + 1, 0, 0, comb, act_var);
    if (k < n) add_comb_at_most(solver, vars, n, k + 1, 0, 0, comb, act_var);
}

static void recalculate_numbers(char *cells, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (cells[idx] & MINE) continue;
            int mcount = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy, nx = x + dx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        if (cells[ny * width + nx] & MINE) mcount++;
                    }
                }
            }
            cells[idx] = (cells[idx] & ~0b1111) | mcount;
        }
    }
}

static bool simulate_solve(CCaDiCaL * solver, char *cells, int width, int height, int sx, int sy, int *fx, int *fy, int act_var) {
    int size = width * height;
    char *sim = calloc(size, sizeof(char));

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

        for (int i = 0; i < size; i++) {
            if (sim[i] != SIM_REVEALED) continue;

            int cx = i % width, cy = i / width;
            int true_num = cells[i] & 0b1111;
            int hidden_count = 0, flag_count = 0;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = cy + dy, nx = cx + dx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        int nidx = ny * width + nx;
                        if (sim[nidx] == SIM_HIDDEN) hidden_count++;
                        if (sim[nidx] == SIM_FLAGGED) flag_count++;
                    }
                }
            }

            if (hidden_count == 0) continue;

            if (true_num == flag_count + hidden_count) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = cy + dy, nx = cx + dx;
                        if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                            int nidx = ny * width + nx;
                            if (sim[nidx] == SIM_HIDDEN) {
                                sim[nidx] = SIM_FLAGGED;
                                progress = true;
                            }
                        }
                    }
                }
            } else if (true_num == flag_count) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = cy + dy, nx = cx + dx;
                        if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                            int nidx = ny * width + nx;
                            if (sim[nidx] == SIM_HIDDEN) {
                                sim[nidx] = SIM_REVEALED;
                                progress = true;
                            }
                        }
                    }
                }
            }
        }

        if (!progress) {
            for (int i = 0; i < size; i++) {
                if (sim[i] != SIM_REVEALED) continue;

                int cx = i % width, cy = i / width;
                int true_num = cells[i] & 0b1111;
                int vars[8];
                int n_hidden = 0, flagged = 0;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = cx + dx, ny = cy + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            int nidx = ny * width + nx;
                            if (sim[nidx] == SIM_FLAGGED) flagged++;
                            else if (sim[nidx] == SIM_HIDDEN) {
                                vars[n_hidden++] = nidx + 1;
                            }
                        }
                    }
                }

                if (n_hidden > 0) {
                    int k = true_num - flagged;
                    if (k < 0) k = 0;
                    if (k > n_hidden) k = n_hidden;
                    add_exactly_k(solver, vars, n_hidden, k, act_var);
                }
            }

            for (int i = 0; i < size; i++) {
                if (sim[i] == SIM_HIDDEN) {
                    bool is_frontier = false;
                    int cx = i % width, cy = i / width;
                    for (int dy = -1; dy <= 1 && !is_frontier; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = cx + dx, ny = cy + dy;
                            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                if (sim[ny * width + nx] == SIM_REVEALED) {
                                    is_frontier = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (is_frontier) {
                        int var_id = i + 1;

                        ccadical_assume(solver, act_var);
                        ccadical_assume(solver, -var_id);
                        if (ccadical_solve(solver) == 20) {
                            sim[i] = SIM_FLAGGED;
                            progress = true;
                            continue;
                        }

                        ccadical_assume(solver, act_var);
                        ccadical_assume(solver, var_id);
                        if (ccadical_solve(solver) == 20) {
                            sim[i] = SIM_REVEALED;
                            progress = true;
                            continue;
                        }
                    }
                }
            }
        }
    }

    bool solved = true;
    for (int i = 0; i < size; i++) {
        if (!(cells[i] & MINE) && sim[i] != SIM_REVEALED) {
            solved = false;
            *fx = i % width;
            *fy = i / width;
            break;
        }
    }

    free(sim);
    return solved;
}

GameInstance try_create_no_guess_game(int width, int height, int amount_mines, int sx, int sy, atomic_bool *cancel_flag) {
    GameInstance g = createGameInstance(width, height, amount_mines);

#ifdef MAX_RETRIES
    int max_retries = MAX_RETRIES;
#else
    int max_retries = (amount_mines * 100) / g->size;
#endif

    CCaDiCaL *solver;

GLOBAL_RESTART:

    solver = ccadical_init();
    ccadical_set_option(solver, "time", 0);
    ccadical_set_option(solver, "ilb", 2);
    ccadical_declare_more_variables(solver, g->size + max_retries + 1);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand(ts.tv_nsec ^ pthread_self());

    for (int i = 0; i < g->size; i++) g->cells[i] = 0;

    int placed = 0;
    srand((unsigned int)time(NULL));
    while (placed < amount_mines) {
        int r = rand() % g->size;
        int rx = r % width;
        int ry = r / width;

        if (abs(rx - sx) <= 1 && abs(ry - sy) <= 1) continue;

        if (!(g->cells[r] & MINE)) {
            g->cells[r] |= MINE;
            placed++;
        }
    }

    int global_epoch = 0;
    int retries = 0;
    while (retries < max_retries) {
        if (cancel_flag && atomic_load(cancel_flag)) {
            ccadical_release(solver);
            deleteGameInstance(g);
            return NULL;
        }

        recalculate_numbers(g->cells, width, height);

        int act_var = g->size + global_epoch + 1;
        int fx = -1, fy = -1;
        if (simulate_solve(solver, g->cells, width, height, sx, sy, &fx, &fy, act_var)) {
            goto SUCCESS;
        }

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
                    if (g->cells[idx] & MINE) {
                        mines_list[m_count++] = idx;
                    } else {
                        safes_list[s_count++] = idx;
                    }
                }
            }
        }

        int target_mine_idx = -1;
        int target_safe_idx = -1;

        if (m_count > 0 && s_count > 0) {
            target_mine_idx = mines_list[rand() % m_count];
            target_safe_idx = safes_list[rand() % s_count];
        } else {
            int offset_m = rand() % g->size;
            for (int i = 0; i < g->size; i++) {
                int idx = (offset_m + i) % g->size;
                if (g->cells[idx] & MINE) {
                    target_mine_idx = idx;
                    break;
                }
            }
            int offset_s = rand() % g->size;
            for (int i = 0; i < g->size; i++) {
                int idx = (offset_s + i) % g->size;
                int cx = idx % width;
                int cy = idx / width;
                if (!(g->cells[idx] & MINE) && (abs(cx - sx) > 1 || abs(cy - sy) > 1)) {
                    target_safe_idx = idx;
                    break;
                }
            }
        }

        if (target_mine_idx != -1 && target_safe_idx != -1) {
            g->cells[target_mine_idx] &= ~MINE;
            g->cells[target_safe_idx] |= MINE;
        }

        ccadical_add(solver, -act_var);
        ccadical_add(solver, 0);

        global_epoch++;
        retries++;
    }

    ccadical_release(solver);
    goto GLOBAL_RESTART;

SUCCESS:
    ccadical_release(solver);
    g->cord.x = sx;
    g->cord.y = sy;
    return g;
}

typedef struct {
    int width;
    int height;
    int amount_mines;
    int sx;
    int sy;
    atomic_bool *cancel_flag;
    GameInstance *winner;
    pthread_mutex_t *mutex;
} GenWorkerArgs;

void* generator_worker(void *arg) {
    GenWorkerArgs *args = (GenWorkerArgs*)arg;
    GameInstance local_g = try_create_no_guess_game(
        args->width, args->height, args->amount_mines,
        args->sx, args->sy, args->cancel_flag
    );
    if (local_g != NULL) {
        pthread_mutex_lock(args->mutex);
        if (!atomic_load(args->cancel_flag)) {
            atomic_store(args->cancel_flag, true);
            *(args->winner) = local_g;
        } else {
            deleteGameInstance(local_g); /*[cite: 1] */
        }
        pthread_mutex_unlock(args->mutex);
    }
    return NULL;
}

GameInstance new_game_no_guess(int width, int height, int amount_mines, int sx, int sy) {

#ifdef GENERATOR_NUM_THREADS
    long n_threads = GENERATOR_NUM_THREADS;
#else
    long n_threads = sysconf(_SC_NPROCESSORS_ONLN) - 1;
    if (n_threads <= 0) {
        n_threads = 1;
    }
#endif

    pthread_t *threads = malloc(sizeof(pthread_t) * n_threads);
    GenWorkerArgs *thread_args = malloc(sizeof(GenWorkerArgs) * n_threads);

    atomic_bool global_cancel = false;
    GameInstance winning_game = NULL;
    pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < n_threads; i++) {
        thread_args[i].width = width;
        thread_args[i].height = height;
        thread_args[i].amount_mines = amount_mines;
        thread_args[i].sx = sx;
        thread_args[i].sy = sy;
        thread_args[i].cancel_flag = &global_cancel;
        thread_args[i].winner = &winning_game;
        thread_args[i].mutex = &write_mutex;

        pthread_create(&threads[i], NULL, generator_worker, &thread_args[i]);
    }

    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(thread_args);

    return winning_game;
}
