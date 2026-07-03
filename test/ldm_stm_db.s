    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R4, #1
    MOV R5, #2
    MOV R6, #3
    
    STMDB SP!, {R4, R5, R6}
    
    MOV R4, #0
    MOV R5, #0
    MOV R6, #0
    
    LDMIA SP!, {R4, R5, R6}

Done
    B Done

    END
