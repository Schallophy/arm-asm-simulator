    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    BL Function
    MOV R0, #100

Done
    B Done

Function
    PUSH {LR}
    
    MOV R1, #50
    MOV R2, #50
    ADD R0, R1, R2
    
    POP {PC}

    END
