    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #20
    
    CMP R0, R1             ; 10 - 20 = -10, N=1, Z=0, C=0, V=0
    
    ; Test that CMP doesn't modify registers
    MOV R2, #42
    CMP R2, #42            ; 42 - 42 = 0, N=0, Z=1, C=1, V=0
    
Loop
    B Loop
    
    END
