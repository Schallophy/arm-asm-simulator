cmake_minimum_required(VERSION 3.16)

project(arm_asm_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(arm_asm_simulator
    src/main.cpp
)
