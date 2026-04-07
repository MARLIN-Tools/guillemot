cmake_minimum_required(VERSION 3.20)
project(Guillemot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(guillemot_core
    src/bitboard.cpp
    src/evaluator.cpp
    src/eval_tables.cpp
    src/hce_evaluator.cpp
    src/pawn_hash.cpp
    src/zobrist.cpp
    src/position.cpp
    src/movegen.cpp
    src/movepicker.cpp
    src/see.cpp
    src/perft.cpp
    src/search.cpp
    src/process_hack.cpp
    src/comedy.cpp
)

target_include_directories(guillemot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (MSVC)
    target_compile_options(guillemot_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(guillemot_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(guillemot
    src/main.cpp
)
target_link_libraries(guillemot PRIVATE guillemot_core)

# Windows libs for process memory manipulation
if (WIN32)
    target_link_libraries(guillemot_core PUBLIC psapi)
endif()
