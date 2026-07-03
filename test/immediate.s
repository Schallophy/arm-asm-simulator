    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #0xFF
    LDR R1, =0xFFFF
    LDR R2, =0x100000
    
Loop
    B Loop
    
    END
