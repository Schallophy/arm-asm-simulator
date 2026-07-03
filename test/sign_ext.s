    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    LDR R0, =0xFFFFFFFF
    MOV R1, R0
    CMP R0, #-1

Done
    B Done

    END
