    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #42
    MVN R1, #42
    
Loop
    B Loop
    
    END
