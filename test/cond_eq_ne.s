    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #10
    
    CMP R0, R1             ; Equal: Z=1
    
    MOVEQ R2, #1           ; Should execute (EQ = Z)
    MOVNE R3, #1           ; Should not execute (NE = !Z)
    
    MOV R0, R2             ; R0 = 1 (from MOVEQ)
    MOV R1, R3             ; R1 = 0 (from initial, MOVNE didn't execute)
    
Loop
    B Loop
    
    END
