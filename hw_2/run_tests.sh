#!/usr/bin/env bash
set -e

mkdir -p build
cd build
cmake ..
make
cd ..

cat << 'EOF' > config_test.txt
h = 8
w = 8
cnt_ships = 5
max_rounds = 20
mode_shots = 0
mode_strat = 1
mode_center = 0
flag_repeat = 0
delay_ms = 20
seed = 42
EOF

./build/battleship -c config_test.txt -l test_battle.log