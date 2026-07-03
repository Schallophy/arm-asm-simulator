    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #100
    MOV R1, #50
    
    CMP R0, R1             ; 100 > 50 unsigned: CS (carry set)
    
    MOVCS R2, #1           ; Should execute (CS = C)
    MOVCC R3, #1           ; Should not execute (CC = !C)
    
    MOV R0, R2             ; R0 = 1
    MOV R1, R3             ; R1 = 0
    
Loop
    B Loop
    
    END
