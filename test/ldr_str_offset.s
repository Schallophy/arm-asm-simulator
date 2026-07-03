    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #100
    MOV R1, #200
    MOV R2, #300
    
    STR R0, [SP, #-4]
    STR R1, [SP, #-8]
    STR R2, [SP, #-12]
    
    MOV R0, #0
    MOV R1, #0
    MOV R2, #0
    
    LDR R0, [SP, #-4]
    LDR R1, [SP, #-8]
    LDR R2, [SP, #-12]

Done
    B Done

    END
