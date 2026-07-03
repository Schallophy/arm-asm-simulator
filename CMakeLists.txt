cmake_minimum_required(VERSION 3.16)

project(arm_asm_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Curses REQUIRED)

add_executable(arm_asm_simulator
    src/main.cpp
)

target_link_libraries(arm_asm_simulator PRIVATE ${CURSES_LIBRARIES})
target_include_directories(arm_asm_simulator PRIVATE ${CURSES_INCLUDE_DIRS})
