    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #20
    ADD R2, R0, R1        ; R2 = 30
    SUB R0, R2, R1        ; R0 = 10
    
Loop
    B Loop
    
    END
