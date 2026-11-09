#!/usr/bin/env bash
# Запуск 9 независимых процессов (требования 7-10 баллов)

set -e
trap 'kill 0; exit' SIGINT SIGTERM

echo "[1/4] Compiling project..."
make clean && make

echo "[2/4] Starting Judge..."
./sea_battle judge -r 25 -d 150 -s 42 &
sleep 0.2

echo "[3/4] Starting Command Centers..."
./sea_battle cc 1 -s 42 &
./sea_battle cc 2 -s 42 &
sleep 0.2

echo "[4/4] Starting Ships..."
./sea_battle ship 1 0 3 -s 42 &
./sea_battle ship 1 1 2 -s 42 &
./sea_battle ship 1 2 1 -s 42 &

./sea_battle ship 2 0 3 -s 42 &
./sea_battle ship 2 1 2 -s 42 &
./sea_battle ship 2 2 1 -s 42 &

wait
echo "All processes completed successfully."
