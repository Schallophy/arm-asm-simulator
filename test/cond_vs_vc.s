    AREA Test, CODE, READONLY
    ENTRY

Start
    ; Force overflow: 0x7FFFFFFF + 1 should overflow in signed
    LDR R0, =0x7FFFFFFF
    MOV R1, #1
    
    ADDS R0, R0, R1        ; 0x7FFFFFFF + 1 = 0x80000000, overflow: V=1
    
    MOVVS R2, #1           ; Should execute (VS = V)
    MOVVC R3, #1           ; Should not execute (VC = !V)
    
    MOV R0, R2             ; R0 = 1
    MOV R1, R3             ; R1 = 0
    
Loop
    B Loop
    
    END
