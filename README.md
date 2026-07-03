# ARM/Keil 汇编模拟器

[![CI](https://github.com/schallophy/arm-asm-simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/schallophy/arm-asm-simulator/actions/workflows/ci.yml)

一个用 C++ 编写的轻量级 ARM/Keil 风格汇编模拟器，支持 ncurses TUI 分屏交互。

## 功能

- **TUI 界面**：左侧源码面板（当前行高亮），右侧寄存器面板（十进制 + 十六进制）
- **完整指令集**：MOV, MVN, ADD, SUB, RSB, RSC, ADC, SBC, MUL, MLA, AND, ORR, EOR, BIC, CMP, CMN, TST, TEQ
- **内存访问**：LDR, STR（立即数偏移/寄存器偏移/前索引/后索引/字面池）, LDM, STM, PUSH, POP
- **分支指令**：B, BL, BX
- **条件码**：全部 14 种（EQ, NE, GT, LT, GE, LE, CS, CC, MI, PL, VS, VC, HI, LS）
- **UAL 语法**：支持 `op{cond}{S}` 和传统 `{op}{S}{cond}` 格式
- **AREA 伪指令**：支持 CODE / DATA 段
- **汇编指令**：DCD, DCB, DCW, SPACE, EQU
- **双语言**：中英文界面切换（编译时选择）

## 构建

需要 CMake 3.16+ 和 ncurses 开发库。

**Linux (Ubuntu/Debian)：**

```bash
sudo apt install libncurses-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**macOS：**

ncurses 为系统自带，直接构建：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**Windows (MSYS2 MinGW64)：**

```bash
pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,ncurses}
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

构建产物：
- `build/arm_asm_simulator` — 中文界面
- `build/arm_asm_simulator_en` — 英文界面

## 运行

加载文件：

```bash
./build/arm_asm_simulator program.s
```

不传参数进入粘贴源码模式，输入 `END` 结束。

### TUI 操作

| 按键 | 功能 |
|------|------|
| `s` | 单步执行 |
| `r` | 连续运行（按任意键中断）|
| `l` | 重新加载源码 |
| `q` | 退出 |
| `↑` `↓` | 滚动源码 |
| `PgUp` `PgDn` | 翻页 |
| 鼠标滚轮 | 滚动源码 |

### 批处理模式

用于自动化测试，输出最终寄存器和标志位状态：

```bash
./build/arm_asm_simulator --batch program.s
# 输出: R0=15 R1=0 ... N=0 Z=1 C=0 V=0
```

## 示例

```asm
    AREA CODE, READONLY
    ENTRY

    MOV R0, #10
    MOV R1, #0
LOOP
    ADD R1, R1, R0
    SUBS R0, R0, #1
    BNE LOOP

STOP B STOP
    END
```

```asm
    ; 递归阶乘
    MOV R0, #5
    BL FACT
STOP B STOP

FACT
    STMFD SP!, {R1, LR}
    CMP R0, #1
    MOVLE R0, #1
    BLE DONE
    MOV R1, R0
    SUB R0, R0, #1
    BL FACT
    MUL R0, R1, R0
DONE
    LDMFD SP!, {R1, PC}
    END
```

## 测试

```bash
bash test/run_tests.sh
```

包含 45 个测试用例，覆盖所有指令、条件码、内存操作和边界情况。

## License

MIT
