    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #100
    MOV R1, #10
    STR R0, [SP, #-4]!
    
    MOV R2, #42
    STR R2, [SP], #4
    
    LDR R3, [SP, #-4]!

Done
    B Done

    END
