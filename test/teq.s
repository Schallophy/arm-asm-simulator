    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #5             ; R0 = 5 (0101)
    MOV R1, #5             ; R1 = 5
    
    TEQ R0, R1             ; 5 ^ 5 = 0, N=0, Z=1
    TEQ R0, #3             ; 5 ^ 3 = 6, N=0, Z=0
    
Loop
    B Loop
    
    END
