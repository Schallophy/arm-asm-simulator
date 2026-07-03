    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #5             ; R0 = 5 (0101)
    MOV R1, #3             ; R1 = 3 (0011)
    
    TST R0, #1             ; 5 & 1 = 1, N=0, Z=0
    TST R0, #2             ; 5 & 2 = 0, N=0, Z=1
    
Loop
    B Loop
    
    END
