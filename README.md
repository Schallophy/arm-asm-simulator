# ARM/Keil 风格汇编模拟器

一个用 C++ 编写的轻量级 ARM/Keil 风格汇编解释器，支持：

- 输入汇编源码
- 标签跳转
- 常见数据处理指令：`MOV`、`MVN`、`ADD`、`SUB`、`CMP`、`AND`、`ORR`、`EOR`、`BIC`
- 简单访存：`LDR`、`STR`
- 条件跳转：`B`、`BEQ`、`BNE`、`BGT`、`BLT`、`BGE`、`BLE`
- 交互命令：`s` 单步，`r` 重新运行，`q` 退出，支持直接按键触发

## 构建

```bash
./build.sh
```

也可以传入构建类型，例如：

```bash
./build.sh Debug
```

## 运行

通过文件路径加载源码：

```bash
./build/arm_asm_simulator path/to/program.s
```

如果不传参数，会进入粘贴源码模式，输入 `END` 结束。

调试时也可以继续用粘贴源码模式：

```bash
./build/arm_asm_simulator
```

然后粘贴汇编源码，单独输入 `END` 结束输入。

进入交互后，直接按 `s`、`r`、`q` 即可，无需回车。

## 示例

```asm
MOV R0, #3
MOV R1, #0
LOOP: ADD R1, R1, R0
SUBS R0, R0, #1
BNE LOOP
END
```
