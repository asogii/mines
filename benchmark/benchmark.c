#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "game.h"
#include "generator.h"

double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

void run_benchmark(int width, int height, int mines, int iterations) {
    struct timespec start, end;

    int sx = width / 2;
    int sy = height / 2;

    double *times = malloc(iterations * sizeof(double));
    if (!times) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }

    double total_time = 0.0;

    for (int i = 0; i < iterations; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        GameInstance g = create_no_guess_game_mt(width, height, mines, sx, sy);

        clock_gettime(CLOCK_MONOTONIC, &end);

        double iter_time = get_time_diff(start, end);
        times[i] = iter_time;
        total_time += iter_time;

        if (g) {
            deleteGameInstance(g);
        } else {
            fprintf(stderr, "createGameInstance failed!\n");
        }
    }

    double avg_time_ms = (total_time / iterations) * 1000.0;

    double variance = 0.0;
    for (int i = 0; i < iterations; i++) {
        double diff = (times[i] * 1000.0) - avg_time_ms;
        variance += diff * diff;
    }
    variance /= iterations;

    double std_dev = sqrt(variance);

    printf("  %-2dx%-2d, %-3d mines | avg: %8.3f ms | var: %10.3f | sd: %8.3f ms\n",
           width, height, mines, avg_time_ms, variance, std_dev);

    free(times);
}

int main() {
    int iterations = 100;
    printf("==========================================================================\n");
    printf(" [MAX_RETRIES: %-5d] Benchmark (Iterations: %d)\n", MAX_RETRIES, iterations);
    printf("==========================================================================\n");

    run_benchmark(30, 16, 99, iterations);
    run_benchmark(40, 35, 300, iterations);

    printf("\n");
    return 0;
}
