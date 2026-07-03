    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #5
    BL Factorial

Done
    B Done

Factorial
    CMP R0, #1
    BLE FactorialEnd
    
    PUSH {LR, R0}
    SUB R0, R0, #1
    BL Factorial
    MOV R1, R0
    POP {R0, LR}
    MUL R0, R0, R1
    
FactorialEnd
    MOV PC, LR

    END
