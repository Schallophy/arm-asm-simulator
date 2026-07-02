    AREA TEST, CODE, READONLY
    ENTRY

START
    LDR R1, =ARR
    MOV R0, #0
    MOV R2, #5

LOOP
    LDR R3, [R1], #4
    ADD R0, R0, R3
    SUBS R2, R2, #1
    BNE LOOP

STOP
    B STOP

ARR
    DCD 1,2,3,4,5
    
    END