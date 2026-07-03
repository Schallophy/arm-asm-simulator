    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #0
    BL Outer
    
Done
    B Done

Outer
    PUSH {LR}
    
    MOV R0, #1
    BL Inner
    ADD R0, R0, #1
    
    POP {PC}

Inner
    PUSH {LR}
    
    ADD R0, R0, #1
    
    POP {PC}

    END
