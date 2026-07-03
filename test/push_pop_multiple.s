    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #20
    MOV R2, #30
    
    PUSH {R0, R1, R2}
    
    MOV R0, #0
    MOV R1, #0
    MOV R2, #0
    
    POP {R2, R3, R4}

Done
    B Done

    END
