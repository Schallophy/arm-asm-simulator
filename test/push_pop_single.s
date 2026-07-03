    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #42
    PUSH {R0}
    
    MOV R0, #0
    POP {R1}

Done
    B Done

    END
