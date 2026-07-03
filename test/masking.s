    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    LDR R0, =0xFFFFFFFF
    LDR R1, =0x80000000
    LDR R2, =0x7FFFFFFF
    ADD R2, R2, #1

Done
    B Done

    END
