    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #200
    MOV R1, #100
    CMP R0, R1
    MOVHI R2, #1
    MOVLS R3, #1
    
    MOV R0, #50
    MOV R1, #100
    CMP R0, R1
    MOVHI R4, #1
    MOVLS R5, #1

Done
    B Done

    END
