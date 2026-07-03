#!/bin/bash

cd "$(dirname "$0")" || exit 1

RETRIES_LIST=(20 30 40 50 60)

echo "start benchmark..."

for r in "${RETRIES_LIST[@]}"; do
    echo "========================================"
    echo "compiling MAX_RETRIES=$r ..."

    clang -O3 -march=native \
        -I../src \
        -DMAX_RETRIES=$r \
        benchmark.c \
        ../src/game.c \
        ../src/generator.c \
        -lcadical -lstdc++ -lm \
        -o bench_$r

    if [ $? -eq 0 ]; then
        ./bench_$r
    else
        echo "compile failed (MAX_RETRIES=$r)"
    fi
done

echo "========================================"
echo "all benchmark done"
