    AREA Test, CODE, READONLY
    ENTRY

Start
    LDR R0, =0x12345678    ; Load constant
    STR R0, [SP, #-4]!     ; Store to stack
    MOV R0, #0             ; Clear
    LDR R0, [SP], #4       ; Load back
    
Loop
    B Loop
    
    END
