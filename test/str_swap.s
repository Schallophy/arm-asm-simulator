    AREA TEST, CODE, READONLY
    ENTRY

;-----------------------------------------
; 主程序开始
;-----------------------------------------
START
    ; --- 求 STR1 长度 ---
    LDR     R0, =STR1      ; R0 = STR1 地址
    BL      STRLEN          ; 调 strlen()，返回长度在 R1
    MOV     R4, R1          ; 保存 STR1 长度到 R4

    ; --- 求 STR2 长度 ---
    LDR     R0, =STR2
    BL      STRLEN
    MOV     R5, R1          ; 保存 STR2 长度到 R5

    ; --- 比较两个长度 ---
    CMP     R4, R5          ; 比较 STR1 和 STR2 长度
    BLE     NO_SWAP         ; 如果 STR1 <= STR2，不交换

    ; --- 否则执行交换 ---
    BL      SWAP

NO_SWAP
STOP
    B       STOP            ; 死循环（程序停止）

;-----------------------------------------
; strlen 函数：计算字符串长度
; 输入：R0 = 字符串首地址
; 返回：R1 = 长度
;-----------------------------------------
STRLEN
    MOV     R1, #0          ; len = 0

LEN_LOOP
    LDRB    R2, [R0], #1    ; 读取1字节字符，R0自动 +1
    CMP     R2, #0          ; 判断是否到 '\0'
    BEQ     LEN_END
    ADD     R1, R1, #1      ; len++
    B       LEN_LOOP

LEN_END
    BX      LR               ; 返回

;-----------------------------------------
; SWAP 函数：交换 STR1 和 STR2 的内容
; 用 TEMP 做中间缓存
;-----------------------------------------
SWAP
    ; STR1 → TEMP
    LDR     R0, =STR1
    LDR     R1, =TEMP

COPY1
    LDRB    R2, [R0], #1
    STRB    R2, [R1], #1
    CMP     R2, #0
    BNE     COPY1

    ; STR2 → STR1
    LDR     R0, =STR2
    LDR     R1, =STR1

COPY2
    LDRB    R2, [R0], #1
    STRB    R2, [R1], #1
    CMP     R2, #0
    BNE     COPY2

    ; TEMP → STR2
    LDR     R0, =TEMP
    LDR     R1, =STR2

COPY3
    LDRB    R2, [R0], #1
    STRB    R2, [R1], #1
    CMP     R2, #0
    BNE     COPY3

    BX      LR

;-----------------------------------------
; 数据段：字符串与缓冲区
;-----------------------------------------
    AREA DATASEG, DATA, READWRITE

STR1    DCB "Embedded ARM Programming",0
STR2    DCB "Chongqing University of Science and Technology",0
TEMP    SPACE 100            ; 临时缓冲区

    END
