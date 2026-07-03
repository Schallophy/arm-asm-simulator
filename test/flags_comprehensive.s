    AREA Test, CODE, READONLY
    ENTRY

Start
    LDR R0, =0xFFFFFFFF
    MOV R1, #1
    ADDS R2, R0, R1
    
    LDR R0, =0x7FFFFFFF
    MOV R1, #1
    ADDS R3, R0, R1
    
    MOV R0, #5
    MOV R1, #10
    SUBS R4, R0, R1
    
    MOV R0, #10
    MOV R1, #5
    SUBS R5, R0, R1

Done
    B Done

    END
