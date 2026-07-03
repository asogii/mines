#!/bin/bash

# 切换到脚本所在目录，确保相对路径准确
cd "$(dirname "$0")" || exit 1

RETRIES_LIST=(100 500 1000 2500 5000 10000)

echo "开始编译和测试..."

for r in "${RETRIES_LIST[@]}"; do
    echo "========================================"
    echo "正在编译 MAX_RETRIES=$r ..."
    
    # -I../src 指定头文件目录
    # 指定 ../src/ 下的源文件
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
        echo "编译失败 (MAX_RETRIES=$r)"
    fi
done

echo "========================================"
echo "所有测试完成！"
