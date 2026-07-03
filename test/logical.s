    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #0xFF          ; R0 = 255 (11111111)
    MOV R1, #0x0F          ; R1 = 15  (00001111)
    
    AND R2, R0, R1         ; R2 = 255 & 15 = 15 (00001111)
    ORR R3, R0, R1         ; R3 = 255 | 15 = 255
    EOR R4, R0, R1         ; R4 = 255 ^ 15 = 240 (11110000)
    BIC R5, R0, R1         ; R5 = 255 & ~15 = 240 (11110000)
    
    ; Test with different patterns
    MOV R0, #12            ; 1100
    MOV R1, #10            ; 1010
    
    AND R0, R0, R1         ; R0 = 12 & 10 = 8 (1000)
    ORR R1, R0, R1         ; R1 = 8 | 10 = 10 (1010)
    EOR R2, R0, R1         ; R2 = 8 ^ 10 = 2 (0010)
    BIC R3, R0, R1         ; R3 = 8 & ~10 = 0
    
Loop
    B Loop
    
    END
