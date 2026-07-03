cmake_minimum_required(VERSION 3.16)

project(arm_asm_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force UTF-8 as the source and execution character set on every platform.
# On non-Windows gcc this is already the default, but on Windows MinGW gcc
# defaults to the system code page (CP936 / GBK on a Chinese system), which
# would embed string literals as GBK. Windows Terminal then tries to render
# those bytes as UTF-8 and shows mojibake.
add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)

if(WIN32)
  # MSYS2 / MinGW: locate ncursesw manually.
  #
  # Two non-obvious things are going on here:
  #  1. We must link against the wide-char library (libncursesw.a) because
  #     the source code uses wide-character functions like wgetch / werase /
  #     wnoutrefresh. libncurses.a (the C-only build) does not export them.
  #  2. The Windows-native compiler (c++.exe) cannot resolve MSYS2-style
  #     paths like /mingw64/include, so we convert the include directories
  #     to Windows paths with `cygpath -m` before passing them on.
  find_program(CYGPATH NAMES cygpath REQUIRED)
  execute_process(
    COMMAND cygpath -m /mingw64/include
    OUTPUT_VARIABLE MINGW64_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(
    COMMAND cygpath -m /mingw64/lib
    OUTPUT_VARIABLE MINGW64_LIB_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(CURSES_INCLUDE_DIRS
    "${MINGW64_INCLUDE_DIR}/ncursesw"
    "${MINGW64_INCLUDE_DIR}/ncurses")

  find_library(CURSES_LIBRARIES NAMES ncursesw
    PATHS "${MINGW64_LIB_DIR}"
    NO_DEFAULT_PATH)
  if(NOT CURSES_LIBRARIES)
    message(FATAL_ERROR "ncursesw library not found in ${MINGW64_LIB_DIR}")
  endif()
else()
  find_package(Curses REQUIRED)
endif()

add_executable(arm_asm_simulator src/main.cpp)
target_link_libraries(arm_asm_simulator PRIVATE ${CURSES_LIBRARIES})
target_include_directories(arm_asm_simulator PRIVATE ${CURSES_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)

add_executable(arm_asm_simulator_en src/main.cpp)
target_link_libraries(arm_asm_simulator_en PRIVATE ${CURSES_LIBRARIES})
target_include_directories(arm_asm_simulator_en PRIVATE ${CURSES_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(arm_asm_simulator_en PRIVATE LANG_EN)
