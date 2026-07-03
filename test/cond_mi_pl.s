    AREA Test, CODE, READONLY
    ENTRY

Start
    MVN R0, #0             ; R0 = -1 (negative)
    
    CMP R0, #0             ; -1 < 0: MI (minus/negative)
    
    MOVMI R2, #1           ; Should execute (MI = N)
    MOVPL R3, #1           ; Should not execute (PL = !N)
    
    MOV R0, R2             ; R0 = 1
    MOV R1, R3             ; R1 = 0
    
Loop
    B Loop
    
    END
