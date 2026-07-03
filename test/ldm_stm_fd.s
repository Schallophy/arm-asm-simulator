    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R4, #10
    MOV R5, #20
    
    STMFD SP!, {R4, R5}
    
    MOV R4, #0
    MOV R5, #0
    
    LDMFD SP!, {R4, R5}

Done
    B Done

    END
