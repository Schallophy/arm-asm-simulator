    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #5
    MOV R1, #8
    STR R0, [SP, R1]!
    
    MOV R0, #0
    ADD R1, SP, #8
    
    LDR R2, [SP, R1]

Done
    B Done

    END
