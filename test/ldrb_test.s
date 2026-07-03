    AREA LDRB_Test, CODE, READONLY
    ENTRY
    LDR     R0, =STR
    LDRB    R1, [R0]            ; 'E' = 0x45
    LDRB    R2, [R0, #1]        ; 'm' = 0x6D
    LDRB    R3, [R0, #7]        ; 'd' = 0x64
    ADD     R4, R1, R2          ; 0x45 + 0x6D = 0xB2
STOP
    B   STOP
    AREA DATASEG, DATA, READWRITE
STR DCB "Embedded", 0
    END
