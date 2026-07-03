#include "generator.h"
#include "game_state.h"
#include "log.h"
#include <ccadical.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIM_HIDDEN 0
#define SIM_REVEALED 1
#define SIM_FLAGGED 2

#ifndef MAX_RETRIES
#define MAX_RETRIES 1000
#endif

// ---------------------------------------------------------
// CNF 翻译层：组合数学生成器 (用于表达“N个格子中有K个雷”)
// SAT 的变量 ID 必须从 1 开始，不能为 0
// ---------------------------------------------------------

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
        ccadical_add(solver, -act_var); // NEW: 埋入激活变量的否定
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


// ---------------------------------------------------------
// 辅助与求解器核心
// ---------------------------------------------------------

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

// 模拟求解：返回 true 表示逻辑通顺无猜，false 表示出现必须猜的死局
static bool simulate_solve(CCaDiCaL * solver, char *mines, int width, int height, int sx, int sy, int *fx, int *fy, int act_var) {
    int length = width * height;
    char *sim = calloc(length, sizeof(char));

    // 初始化起始 3x3 区域
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

        // --- 阶段一：Trivial Solver (高速过滤) ---
        for (int i = 0; i < length; i++) {
            if (sim[i] != SIM_REVEALED) continue;

            int cx = i % width, cy = i / width;
            int true_num = mines[i] & 0b1111;
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

        // --- 阶段二：CaDiCaL 终极反证推导 ---
        // 只有当 Trivial 扫不动时才启动 SAT，极大地压榨了性能
        if (!progress) {
            // 录入场上所有的客观线索
            for (int i = 0; i < length; i++) {
                if (sim[i] != SIM_REVEALED) continue;

                int cx = i % width, cy = i / width;
                int true_num = mines[i] & 0b1111;
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
                                // 变量 ID 从 1 开始
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

            // 对所有前线的未知格子进行 assume 测试
            for (int i = 0; i < length; i++) {
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

                        // 测试 1：假设它是安全的，如果 UNSAT，那它必定是雷
                        ccadical_assume(solver, act_var);
                        ccadical_assume(solver, -var_id);
                        if (ccadical_solve(solver) == 20) {
                            sim[i] = SIM_FLAGGED;
                            progress = true;
                            continue;
                        }

                        // 测试 2：假设它是雷，如果 UNSAT，那它必定是安全的
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

    // 检查是否全图无猜解锁并定位死局前线
    bool solved = true;
    for (int i = 0; i < length; i++) {
        if (!(mines[i] & MINE) && sim[i] != SIM_REVEALED) {
            solved = false;
            *fx = i % width;
            *fy = i / width;
            break;
        }
    }

    free(sim);
    return solved;
}


// ---------------------------------------------------------
// 主生成循环
// ---------------------------------------------------------

GameInstance create_no_guess_game(int width, int height, int amount_mines, int sx, int sy) {
    GameInstance g = createGameInstanceNormal(width, height, 0);
    int length = width * height;
    CCaDiCaL *solver;

GLOBAL_RESTART:

    solver = ccadical_init();
    ccadical_set_option(solver, "time", 0);

    int global_epoch = 0;
    ccadical_declare_more_variables(solver, length + MAX_RETRIES + 1);

    // 强制清空
    for (int i = 0; i < length; i++) g->mines[i] = 0;

    // 初始撒雷
    int placed = 0;
    srand((unsigned int)time(NULL));
    while (placed < amount_mines) {
        int r = rand() % length;
        int rx = r % width;
        int ry = r / width;

        // 避开玩家点亮的首个区域
        if (abs(rx - sx) <= 1 && abs(ry - sy) <= 1) continue;

        if (!(g->mines[r] & MINE)) {
            g->mines[r] |= MINE;
            placed++;
        }
    }

    int retries = 0;
    while (retries < MAX_RETRIES) {
        recalculate_numbers(g->mines, width, height);

        int act_var = length + global_epoch + 1;
        global_epoch++;

        int fx = -1, fy = -1;
        if (simulate_solve(solver, g->mines, width, height, sx, sy, &fx, &fy, act_var)) {
            goto SUCCESS;
        }

        // 局部洗牌：SAT 撞墙证明是真死局，打散重来
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

        int target_mine_idx = -1;
        int target_safe_idx = -1;

        if (m_count > 0 && s_count > 0) {
            target_mine_idx = mines_list[rand() % m_count];
            target_safe_idx = safes_list[rand() % s_count];
        } else {
            // 全局抽取兜底
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

        retries++;
    }

    // 突破洗牌次数上限，放弃污染残局，执行纯随机重启
    ccadical_release(solver);
    goto GLOBAL_RESTART;

SUCCESS:
    ccadical_release(solver);
    g->flagstotal = amount_mines;
    g->flagsfound = 0;
    g->unveiled = 0;
    g->state = PLAYING;
    g->cord.x = sx;
    g->cord.y = sy;
    return g;
}
