cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(battleship C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")

add_executable(battleship
    main.c
    utils.c
    battle.c
)

target_link_libraries(battleship pthread)
