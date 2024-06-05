cmake_minimum_required(VERSION 3.10)
project(TicTacToe)

set(CMAKE_CXX_STANDARD 11)

# Find SFML
set(SFML_DIR "/usr/lib/x86_64-linux-gnu/cmake/SFML") # Adjust this to your SFML installation path
find_package(SFML 2.5 COMPONENTS system window graphics network audio REQUIRED)

include_directories(${SFML_INCLUDE_DIRS})

# Server executable
add_executable(server src/server.cpp)
target_link_libraries(server sfml-system sfml-network)

# Client executable
add_executable(client src/client.cpp)
target_link_libraries(client sfml-system sfml-window sfml-graphics sfml-network sfml-audio)

