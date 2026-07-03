    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    LDR R0, =0x7FFFFFFF
    MOV R1, #1
    ADDS R0, R0, R1

Done
    B Done

    END
