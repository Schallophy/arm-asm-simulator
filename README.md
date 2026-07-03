# ARM/Keil 汇编模拟器

[![CI](https://github.com/schallophy/arm-asm-simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/schallophy/arm-asm-simulator/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

一个用 C++ 编写的轻量级 ARM/Keil 风格汇编模拟器，基于 ncurses 提供分屏 TUI 交互界面。
适用于教学、单步调试、算法验证和竞赛训练。

---

## 目录

- [特性一览](#特性一览)
- [5 分钟上手](#5-分钟上手)
  - [下载预编译二进制](#下载预编译二进制)
  - [从源码构建](#从源码构建)
  - [第一个程序](#第一个程序)
- [TUI 操作手册](#tui-操作手册)
  - [界面布局](#界面布局)
  - [按键速查](#按键速查)
  - [高亮与回溯](#高亮与回溯)
- [汇编语言参考](#汇编语言参考)
  - [数据处理指令](#数据处理指令)
  - [内存访问](#内存访问)
  - [分支](#分支)
  - [批量加载 / 存储](#批量加载--存储)
  - [标志位](#标志位)
  - [条件码](#条件码)
  - [伪指令与数据](#伪指令与数据)
  - [语法风格](#语法风格)
- [批处理模式](#批处理模式)
- [测试套件](#测试套件)
- [国际化和构建变体](#国际化和构建变体)
- [故障排查](#故障排查)
- [License](#license)

---

## 特性一览

- **TUI 分屏交互**：左侧源码面板（实时高亮当前指令行），右侧寄存器面板（十进制 + 8 位十六进制），底栏状态指示
- **完整数据处理指令集**：`MOV` `MVN` `ADD` `SUB` `RSB` `RSC` `ADC` `SBC` `MUL` `MLA` `AND` `ORR` `EOR` `BIC` `CMP` `CMN` `TST` `TEQ`
- **内存访问**：`LDR` `STR`（立即数偏移 / 寄存器偏移 / 前索引 / 后索引 / 字面池），`LDM` `STM`（IA/IB/DA/DB 全套寻址模式）
- **栈操作**：`PUSH` `POP`（含 `LDMFD/STMFD` 别名）
- **分支**：`B` `BL` `BX`，全部支持 14 种条件码
- **UAL 与传统语法**：`ADDNE` 与 `ADDNES` 同时支持
- **伪指令**：`DCD` `DCB` `DCW` `SPACE` `EQU`，`AREA` 段声明，`ENTRY` 入口标记
- **双语言界面**：编译时选择中文 / 英文
- **跨平台**：Linux、macOS、Windows (MSYS2 MinGW64) 全平台 CI 通过
- **45 个回归测试**：覆盖所有指令、标志位、条件码、内存操作、递归、溢出等边界情况

## 5 分钟上手

### 下载预编译二进制

每个 commit CI 完成后会生成 3 个平台的产物（保留 30 天），从 [Actions 页面](https://github.com/schallophy/arm-asm-simulator/actions) 进入任意一次绿色 run，底部 **Artifacts** 区下载：

| 平台 | 产物名 | 包含 |
|------|--------|------|
| Linux x86_64 | `arm_asm_simulator-linux-x86_64` | `arm_asm_simulator`（中文）`arm_asm_simulator_en`（英文）|
| macOS x86_64 | `arm_asm_simulator-macos-x86_64` | `arm_asm_simulator`、`arm_asm_simulator_en` |
| Windows x86_64 | `arm_asm_simulator-windows-x86_64` | `arm_asm_simulator.exe`、`arm_asm_simulator_en.exe` |

下载后赋予执行权限（Linux/macOS）：

```bash
chmod +x arm_asm_simulator arm_asm_simulator_en
./arm_asm_simulator program.s
```

### 从源码构建

依赖：**CMake 3.16+**、**C++17 编译器**、**ncurses 开发库**。

**Linux (Ubuntu/Debian)：**

```bash
sudo apt install libncurses-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**macOS：**

```bash
brew install cmake         # macOS 自带 ncurses
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**Windows (MSYS2 MinGW64)：**

```bash
pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,ncurses}
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### 第一个程序

新建 `hello.s`：

```asm
    AREA HELLO, CODE, READONLY
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

运行：

```bash
./arm_asm_simulator hello.s
```

按 `S` 单步，按 `R` 一键运行到底。最终 `R1 = 55`（1+2+…+10）。

不传文件名则进入**粘贴源码模式**：直接在终端里贴代码，单独一行输入 `END` 结束。

```bash
./arm_asm_simulator
```

## TUI 操作手册

### 界面布局

```
┌─ ARM 汇编模拟器 ─────────────────────────┬──────────────────┐
│  1  AREA HELLO, CODE, READONLY          │ R0 = 55  0x00000037│
│  2      ENTRY                           │ R1 = 0   0x00000000│
│  3                                       │ R2 = 0   0x00000000│
│  4      MOV   R0, #10     ◀ 当前行       │ R3 = 0   0x00000000│
│  5      MOV   R1, #0                    │ ...                │
│  6  LOOP                                │ R12= 0   0x00000000│
│  7      ADD   R1, R1, R0                │ R13= ...   SP      │
│  8      SUBS  R0, R0, #1                │ R14= ...   LR      │
│  9      BNE   LOOP                      │ R15= ...   PC      │
│ 10                                       │                   │
│ 11  STOP B STOP                          │ Flags: N=0 Z=0 ... │
│ 12      END                              │                   │
├──────────────────────────────────────────┴──────────────────┤
│ 运行中  PC=0x00000018  S:单步  R:运行  L:重载  ...  Q:退出  │
└─────────────────────────────────────────────────────────────┘
```

- **左侧**：带行号的源码窗口，当前 PC 指向的指令行以反色高亮
- **右侧**：R0–R15 寄存器面板，每行显示十进制值和 8 位十六进制；R13/SP、R14/LR、R15/PC 带别名标注
- **底栏**：运行状态（运行中 / 已结束）、当前 PC 值、操作提示

### 按键速查

| 按键 | 功能 |
|------|------|
| `S` | 单步执行（执行 1 条指令后暂停）|
| `R` | 连续运行，按**任意键**中断 |
| `L` | 重新加载源文件并重置模拟器 |
| `Q` | 退出 |
| `↑` `↓` | 上下滚动源码面板 |
| `PgUp` `PgDn` | 整页翻动源码 |
| `Home` `End` | 跳到源码首 / 末 |
| 鼠标滚轮 | 滚动源码（终端需支持 SGR 鼠标模式）|

### 高亮与回溯

- **当前指令行**：执行后高亮立即移动到下一条指令
- **分支目标行**：执行 `B` / `BL` / `BX` 时先闪烁目标标签行一次，再跳到目标指令（这样你能直观看到「跳到了哪」）
- **自循环分支**：`B STOP` 这类死循环不会自动终止，按 `R` 后用 `Q` 或 `L` 中断
- **退出条件**：执行到 `END` 之后、或者 PC 跑出有效范围、或者达到 10 万条指令上限时，底栏切换到「程序已结束」状态
- **终端太小**：需要至少 50×22 字符，否则显示 `终端太小（需要至少 50x22，当前 ...）。建议最大化或全屏终端` 后退出

## 汇编语言参考

### 数据处理指令

所有数据处理指令的 32 位结果都经过符号扩展，行为与真实 ARM 硬件一致。

| 助记符 | 含义 | 示例 |
|--------|------|------|
| `MOV` | 传送 | `MOV R0, #42` |
| `MVN` | 按位取反传送 | `MVN R0, #0` → R0 = -1 |
| `ADD` | 加 | `ADD R0, R1, R2` |
| `SUB` | 减 | `SUB R0, R1, #1` |
| `RSB` | 反向减 | `RSB R0, R1, #0` → R0 = -R1 |
| `RSC` | 反向带借位减 | `RSC R0, R1, R2` → R0 = R2 - R1 - !C |
| `ADC` | 带进位加 | `ADC R0, R1, R2` |
| `SBC` | 带借位减 | `SBC R0, R1, R2` → R0 = R1 - R2 - !C |
| `MUL` | 乘 | `MUL R0, R1, R2` |
| `MLA` | 乘加 | `MLA R0, R1, R2, R3` → R0 = R1*R2 + R3 |
| `AND` `ORR` `EOR` `BIC` | 位运算 | `BIC R0, R1, #0x0F` |
| `CMP` `CMN` `TST` `TEQ` | 比较 / 测试（不写回）| `CMP R0, #0` |
| `NOP` | 空操作 | `NOP` |

任一指令后缀 `S` 都会按结果更新 N/Z/C/V 标志位（`CMP` `CMN` `TST` `TEQ` 隐含更新）。

### 内存访问

**立即数 / 寄存器偏移：**

```asm
LDR R0, [R1]              ; R0 = mem[R1]
LDR R0, [R1, #4]          ; R0 = mem[R1+4]
LDR R0, [R1, R2]          ; R0 = mem[R1+R2]
LDR R0, [R1, R2, LSL #2]  ; R0 = mem[R1+R2*4]
```

**前索引 + 回写：** `[Rn, #imm]!` — 先更新 Rn，再访问新地址。

```asm
LDR R0, [R1, #4]!         ; R1 = R1+4; R0 = mem[R1]
```

**后索引：** `[Rn], #imm` — 先访问旧地址，再更新 Rn。

```asm
LDR R0, [R1], #4          ; R0 = mem[R1]; R1 = R1+4
```

**字面池：** `LDR Rd, =constant` 或 `LDR Rd, =label` — 汇编器在数据段生成常量，运行时加载。

```asm
LDR R0, =0x12345678       ; 加载立即数
LDR R1, =SOME_LABEL       ; 加载标签地址
```

> ⚠️ `STR` 不支持字面量操作数。`STR R0, =100` 会报错。

### 分支

```asm
B   LABEL          ; 无条件跳
BL  LABEL          ; 跳并将返回地址写入 LR
BX  Rm             ; 跳到 Rm 指定的地址（用于返回：BX LR）

BEQ LABEL          ; 条件跳转（EQ/NE/GT/LT/GE/LE/CS/CC/MI/PL/VS/VC/HI/LS 全部支持）
```

### 批量加载 / 存储

寄存器列表语法：`{R0, R1-R3, LR, PC}`，支持范围和特殊寄存器名。

```asm
STMIA R0!, {R1-R4}        ; R0 += 16; mem[R0..R0+12] = R1..R4
LDMIA R0,  {R1-R4}        ; R1..R4 = mem[R0..R0+12]
PUSH   {R0, LR}           ; 等价于 STMDB SP!, {R0, LR}
POP    {R0, PC}           ; 等价于 LDMIA SP!, {R0, PC}
```

支持的寻址模式后缀（与 ARM 汇编器一致）：

| 后缀 | 含义 | 等价别名 |
|------|------|----------|
| `IA` (Increment After) | 加载后地址递增 | `FD`（满递减栈）|
| `IB` (Increment Before) | 加载前地址递增 | — |
| `DA` (Decrement After) | 加载后地址递减 | `ED`（空递减栈）|
| `DB` (Decrement Before) | 加载前地址递减 | `FA`（满递增栈）|

> 寄存器在内存中按编号升序、最低编号存于最低地址，与真实 ARM 一致。

### 标志位

`N` (Negative) `Z` (Zero) `C` (Carry) `V` (Overflow) — 32 位结果的标准定义。

- `C` 对于 `CMP` 使用无符号比较；对于 `ADD`/`SUB`/`RSB` 系列根据操作数无符号进/借位设置
- `V` 仅在有符号溢出时设置
- 加 / 减 / 乘 / 逻辑结果都经过 32 位掩码，再做符号扩展

### 条件码

14 种标准条件码，与 ARM 架构手册一致：

| 后缀 | 含义 | 标志位条件 |
|------|------|------------|
| `EQ` | 相等 | Z=1 |
| `NE` | 不等 | Z=0 |
| `GT` | 大于（有符号）| Z=0 且 N=V |
| `LT` | 小于（有符号）| N≠V |
| `GE` | 大于等于（有符号）| N=V |
| `LE` | 小于等于（有符号）| Z=1 或 N≠V |
| `HI` | 高于（无符号）| C=1 且 Z=0 |
| `LS` | 低于等于（无符号）| C=0 或 Z=1 |
| `CS`/`HS` | 进位设置 | C=1 |
| `CC`/`LO` | 进位清除 | C=0 |
| `MI` | 负 | N=1 |
| `PL` | 正或零 | N=0 |
| `VS` | 溢出 | V=1 |
| `VC` | 无溢出 | V=0 |

### 伪指令与数据

```asm
    AREA NAME, CODE, READONLY    ; 段声明（CODE 或 DATA）
    ENTRY                         ; 入口标记（仅作语义提示）

LABEL                             ; 标签声明（独占一行）
LDR R0, =0xDEADBEEF              ; 字面池
DCD 1, 2, 3, 4                   ; 定义 32 位字
DCB 0x41, 0x42                  ; 定义字节
DCW 1000, 2000                   ; 定义 16 位半字
SPACE 256                        ; 分配 256 字节
N   EQU 100                      ; 符号常量

    END                           ; 源码结束标记
```

### 语法风格

两种写法都支持，混用也行：

```asm
ADDNE R0, R1, R2      ; UAL 风格（op + cond）
ADDNES R0, R1, R2     ; UAL 风格（op + cond + S）

ADD R0, R1, R2        ; 无条件无标志
ADDS R0, R1, R2       ; 无条件有标志
ADDNE R0, R1, R2      ; 条件无标志（旧风格：op + S + cond）
ADDNES R0, R1, R2     ; 条件有标志
```

## 批处理模式

跳过 TUI，直接跑完程序，输出最终寄存器与标志位，便于自动化测试与 CI 集成。

```bash
./arm_asm_simulator --batch program.s
# 输出形如：R0=15 R1=0 R2=0 ... R13=0 R14=0 R15=0 N=0 Z=1 C=0 V=0
```

退出码：成功 0，编译失败非 0。

## 测试套件

仓库自带 45 个回归测试，覆盖所有指令、寻址方式、条件码、栈操作、递归与边界溢出：

```bash
bash test/run_tests.sh
```

预期输出（节选）：

```
==========================================
ARM Assembly Simulator Test Suite
==========================================

MOV/MVN                                          PASSED
Immediate values                                 PASSED
ADD/SUB                                          PASSED
...
Recursive factorial(5)                           PASSED
Loop sum 1-10                                    PASSED
32-bit masking                                   PASSED
Overflow/carry flags                             PASSED

==========================================
Tests: 45 | Passed: 45 | Failed: 0
==========================================
All tests passed!
```

新增测试用例时，只需在 `test/` 目录新增一个 `.s` 文件，并在 `test/run_tests.sh` 里加一行：

```bash
run_test "My new test" test/my_new.s "R0=42" "Z=1"
```

## 国际化和构建变体

构建会同时产出两个可执行文件：

| 文件 | 界面语言 | 编译选项 |
|------|----------|----------|
| `arm_asm_simulator` | 中文 | 默认 |
| `arm_asm_simulator_en` | 英文 | `-DLANG_EN` |

切换语言不需要运行时环境变量，也不需要重启终端——重编译即可。源码中所有用户可见的字符串都集中定义在 `src/i18n.h`，新增翻译只需在那里加一行。

> 默认使用 UTF-8 编码。终端需要支持中文显示（Linux/macOS 终端默认支持；Windows 推荐使用 Windows Terminal 而非旧版 cmd）。

## 故障排查

| 现象 | 解决 |
|------|------|
| `终端太小（需要至少 50x22，当前 ...）。建议最大化或全屏终端` | 把终端窗口放大或全屏 |
| `重复定义标签 'X'` | 检查源码中 `X` 是否被声明两次（含 `AREA` 段名是否与标签冲突）|
| `跳转目标标签未定义: X` | 检查 `B X` 中的 `X` 是否有对应标签行 |
| `STR 不支持字面量操作数: ...` | `STR` 不接受 `=constant` 形式，改用先 `LDR Rtmp, =const` 再 `STR Rtmp, [...]` |
| Windows 链接报 `undefined reference to __imp_wgetch` | 缺少 ncursesw 包。MSYS2：`pacman -S mingw-w64-x86_64-ncurses`（已包含 wide-char）|
| macOS 下链接报 ncurses 错误 | 应使用系统自带的 ncurses，不需额外安装；如安装过 brew 的 ncurses 引发冲突，先 `brew unlink ncurses` |

## License

MIT
