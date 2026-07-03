    AREA TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #50
    MOV R1, #50
    CMP R0, R1
    ADC R2, R0, R1

Done
    B Done

    END
