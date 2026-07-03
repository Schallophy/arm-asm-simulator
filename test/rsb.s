    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #20
    MOV R1, #30
    RSB R2, R0, R1        ; R2 = R1 - R0 = 30 - 20 = 10
    RSB R0, R1, R0        ; R0 = R0 - R1 = 20 - 30 = -10
    
Loop
    B Loop
    
    END
