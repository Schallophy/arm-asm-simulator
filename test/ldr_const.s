    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    LDR R0, =0x12345678
    LDR R1, =1000000

Done
    B Done

    END
