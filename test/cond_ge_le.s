    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #15
    MOV R1, #15
    
    CMP R0, R1             ; 15 == 15: GE and LE both true
    
    MOVGE R2, #1           ; Should execute (GE = N == V)
    MOVLE R3, #1           ; Should execute (LE = Z || N != V)
    
    MOV R0, R2             ; R0 = 1
    MOV R1, R3             ; R1 = 1
    
Loop
    B Loop
    
    END
