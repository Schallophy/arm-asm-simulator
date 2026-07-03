#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE=Release
CLEAN=0
RUN_TESTS=0
INSTALL_DEPS=0
USE_NINJA=0
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --debug           Debug build (default: Release)
  --clean           Remove build/ before configuring
  --test            Run test suite after a successful build
  --install-deps    Install missing build dependencies via the system
                    package manager (may need sudo / root)
  --ninja           Prefer the Ninja generator when available
  -j N              Parallel build jobs (default: $JOBS)
  -h, --help        Show this help

Artifacts after a successful build:
  build/arm_asm_simulator         (Chinese UI)
  build/arm_asm_simulator_en      (English UI)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)        BUILD_TYPE=Debug; shift ;;
        --clean)        CLEAN=1; shift ;;
        --test)         RUN_TESTS=1; shift ;;
        --install-deps) INSTALL_DEPS=1; shift ;;
        --ninja)        USE_NINJA=1; shift ;;
        -j)             JOBS="$2"; shift 2 ;;
        -h|--help)      usage; exit 0 ;;
        *)              echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

SUDO=""
if [[ "${EUID:-$(id -u)}" -ne 0 ]] && command -v sudo >/dev/null 2>&1; then
    SUDO=sudo
fi

install_deps() {
    case "$OSTYPE" in
        darwin*)
            command -v brew >/dev/null 2>&1 || {
                echo "Error: Homebrew not found. Install it from https://brew.sh first." >&2
                exit 1
            }
            brew install cmake
            ;;
        linux*)
            if   command -v apt-get >/dev/null 2>&1; then
                $SUDO apt-get update
                $SUDO apt-get install -y libncurses-dev cmake build-essential
            elif command -v dnf     >/dev/null 2>&1; then
                $SUDO dnf install -y ncurses-devel cmake gcc-c++
            elif command -v pacman  >/dev/null 2>&1; then
                $SUDO pacman -S --needed --noconfirm ncurses cmake gcc make
            else
                echo "Error: no supported package manager found (apt/dnf/pacman)." >&2
                exit 1
            fi
            ;;
        msys*|cygwin*)
            if [[ -n "${MSYSTEM:-}" ]]; then
                pacman -S --needed --noconfirm \
                    mingw-w64-x86_64-gcc \
                    mingw-w64-x86_64-cmake \
                    mingw-w64-x86_64-ninja \
                    mingw-w64-x86_64-ncurses
            else
                echo "Error: run this from inside an MSYS2 shell (e.g. MINGW64)." >&2
                exit 1
            fi
            ;;
        *)
            echo "Error: unsupported platform: $OSTYPE" >&2
            exit 1
            ;;
    esac
}

check_deps() {
    local missing=()
    command -v cmake >/dev/null 2>&1 || missing+=("cmake")
    command -v g++   >/dev/null 2>&1 \
    || command -v clang++ >/dev/null 2>&1 \
    || missing+=("C++ compiler (g++ / clang++)")
    case "$OSTYPE" in
        linux*|darwin*)
            if ! pkg-config --exists ncursesw 2>/dev/null \
            && ! pkg-config --exists ncurses  2>/dev/null \
            && [[ "$OSTYPE" != "darwin"* ]]; then
                missing+=("ncurses development headers")
            fi
            ;;
        msys*|cygwin*)
            [[ -f /mingw64/include/ncurses/ncurses.h \
            || -f /mingw64/include/ncursesw/ncurses.h ]] \
                || missing+=("mingw-w64-x86_64-ncurses")
            ;;
    esac
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "Error: missing dependencies:" >&2
        printf '  - %s\n' "${missing[@]}" >&2
        echo "Re-run with --install-deps to install them automatically." >&2
        exit 1
    fi
}

GENERATOR_ARGS=""
if [[ $USE_NINJA -eq 1 ]] && command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS="-G Ninja"
fi

if [[ $CLEAN -eq 1 ]]; then
    echo "==> Cleaning build/"
    rm -rf build
fi

if [[ $INSTALL_DEPS -eq 1 ]]; then
    echo "==> Installing build dependencies"
    install_deps
fi

check_deps

if [[ ! -d build ]]; then
    mkdir build
fi

echo "==> Configuring (${BUILD_TYPE}${GENERATOR_ARGS:+ via Ninja})"
# shellcheck disable=SC2086
cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $GENERATOR_ARGS

echo "==> Building (-j $JOBS)"
cmake --build build --parallel "$JOBS"

echo
echo "Build successful. Artifacts:"
ls -1 build/arm_asm_simulator* 2>/dev/null | sed 's/^/  /'

if [[ $RUN_TESTS -eq 1 ]]; then
    echo
    echo "==> Running test suite"
    bash test/run_tests.sh
fi
