    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #0
    BL FuncA
    
    ; After return, R0 should be 10
    ADD R1, R0, #10        ; R1 = 20
    
Loop
    B Loop
    
FuncA
    MOV R0, #10
    MOV PC, LR             ; Return
    
    END
