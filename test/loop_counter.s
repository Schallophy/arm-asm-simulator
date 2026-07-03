    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #0
    MOV R1, #10

LoopStart
    CMP R1, #0
    BEQ LoopEnd
    
    ADD R0, R0, R1
    SUB R1, R1, #1
    
    B LoopStart

LoopEnd

Done
    B Done

    END
