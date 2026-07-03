    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #10
    
    CMN R0, #10            ; 10 + 10 = 20, N=0, Z=0, C=0, V=0
    
    CMN R0, #0xFFFFF      ; 10 + 1048575 = 1048585, N=0, Z=0
    
    ; Set negative flag
    MVN R1, #0             ; R1 = 0xFFFFFFFF = -1
    CMN R0, R1             ; 10 + (-1) = 9, N=0, Z=0
    
    ; Set negative
    MVN R2, #5             ; R2 = -6
    CMN R0, R2             ; 10 + (-6) = 4, N=0, Z=0
    
    ; Negative result
    MVN R3, #100           ; R3 = -101
    CMN R0, R3             ; 10 + (-101) = -91, N=1, Z=0
    
Loop
    B Loop
    
    END
