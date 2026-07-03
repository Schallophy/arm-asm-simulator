        AREA BubbleSort, CODE, READONLY
        ENTRY
start
        ; 初始化
        LDR     r0, =ARR_BUF        ; r0指向数组起始地址
        MOV     r1, #6              ; r1 = 数组元素个数 - 1 (因为n个元素需要n-1轮比较)
                                    ; 数组有7个元素，所以需要6轮比较
outer_loop
        MOV     r2, r1              ; r2保存当前轮需要比较的次数
        LDR     r0, =ARR_BUF        ; 每轮开始重新指向数组开头     
inner_loop
        LDR     r3, [r0]            ; 读取当前元素
        LDR     r4, [r0, #4]        ; 读取下一个元素
        CMP     r3, r4              ; 比较两个元素
        BLE     no_swap             ; 如果当前元素 <= 下一个元素，则不交换
        ; 交换两个元素
        STR     r4, [r0]            ; 将下一个元素存入当前位置
        STR     r3, [r0, #4]        ; 将当前元素存入下一个位置
no_swap
        ADD     r0, r0, #4          ; 指向下一个元素
        SUBS    r2, r2, #1          ; 当前轮比较次数减1
        BNE     inner_loop          ; 如果当前轮未完成，继续
        SUBS    r1, r1, #1          ; 轮数减1
        BNE     outer_loop          ; 如果所有轮未完成，继续下一轮
stop
        B       stop                ; 程序结束
        AREA ArrayData, DATA, READWRITE
ARR_BUF DCD     77, 22, 33, 99, 88, 66, 44
        END