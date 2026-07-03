    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #20
    MOV R1, #10
    
    CMP R0, R1             ; 20 > 10: GT
    
    MOVGT R2, #1           ; Should execute (GT = !Z && N==V)
    MOVLT R3, #1           ; Should not execute (LT = N != V)
    
    MOV R0, R2             ; R0 = 1
    MOV R1, R3             ; R1 = 0
    
Loop
    B Loop
    
    END
