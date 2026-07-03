    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #5
    MOV R1, #5
    
    ADDS R0, R0, R1        ; R0 = 5 + 5 = 10, sets flags
    
    ; Now test SUBS with zero result
    SUBS R0, R0, #10       ; R0 = 10 - 10 = 0, Z=1, C=1
    
Loop
    B Loop
    
    END
