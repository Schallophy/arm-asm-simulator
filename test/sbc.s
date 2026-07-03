    AREA TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #50
    MOV R1, #50
    MOV R2, #0
    CMP R2, #1
    SBC R4, R0, R1

Done
    B Done

    END
