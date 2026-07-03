    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #6
    MOV R1, #10
    MUL R2, R0, R1         ; R2 = 6 * 10 = 60
    
    MOV R3, #5
    MOV R4, #10
    MOV R5, #60
    MLA R6, R3, R4, R5     ; R6 = 5 * 10 + 60 = 110
    
Loop
    B Loop
    
    END
