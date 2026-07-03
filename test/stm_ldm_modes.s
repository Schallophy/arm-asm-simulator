    AREA STM_LDM_Modes, CODE, READONLY
    ENTRY

; Verify all 4 ARM block-data-transfer addressing modes (IA/IB/DA/DB) by
; round-tripping four registers through memory and checking the LDM reads
; them back in the same order.
;
; Per ARM ARM, "the lowest-numbered register is loaded from the lowest
; memory address, through to the highest-numbered register from the
; highest memory address." For each mode below the expected pattern is
;   R1 (lowest) -> lowest address in the block
;   R5 (highest) -> highest address in the block
; To read the data back with the matching LDM, R0 must point at the
; ORIGINAL base (0x1000) for every mode.

;===========================================================
; 1. IA (Increment After): first access at base, addresses ascend
;    STMIA R0=0x1000 -> mem[0x1000..0x100C] = R1,R3,R4,R5
;    LDMIA R0=0x1000 -> R2=0xAA R6=0xBB R7=0xCC R8=0xDD
;===========================================================
    MOV     R0, #0x1000
    MOV     R1, #0xAA
    MOV     R3, #0xBB
    MOV     R4, #0xCC
    MOV     R5, #0xDD
    STMIA   R0!, {R1, R3, R4, R5}
    MOV     R0, #0x1000
    LDMIA   R0!, {R2, R6, R7, R8}

;===========================================================
; 2. IB (Increment Before): first access at base+4, addresses ascend
;    STMIB R0=0x1000 -> mem[0x1004..0x1010] = R1,R3,R4,R5
;    LDMIB R0=0x1000 -> R2=0xAA R6=0xBB R7=0xCC R8=0xDD
;===========================================================
    MOV     R0, #0x1000
    MOV     R1, #0xAA
    MOV     R3, #0xBB
    MOV     R4, #0xCC
    MOV     R5, #0xDD
    STMIB   R0!, {R1, R3, R4, R5}
    MOV     R0, #0x1000
    LDMIB   R0!, {R2, R6, R7, R8}

;===========================================================
; 3. DA (Decrement After): first access at base, addresses descend
;    STMDA R0=0x1000 -> mem[0x0FF4..0x1000] = R1,R3,R4,R5
;    LDMDA R0=0x1000 -> R2=0xAA R6=0xBB R7=0xCC R8=0xDD
;===========================================================
    MOV     R0, #0x1000
    MOV     R1, #0xAA
    MOV     R3, #0xBB
    MOV     R4, #0xCC
    MOV     R5, #0xDD
    STMDA   R0!, {R1, R3, R4, R5}
    MOV     R0, #0x1000
    LDMDA   R0!, {R2, R6, R7, R8}

;===========================================================
; 4. DB (Decrement Before): first access at base-4, addresses descend
;    STMDB R0=0x1000 -> mem[0x0FF0..0x0FFC] = R1,R3,R4,R5
;    LDMDB R0=0x1000 -> R2=0xAA R6=0xBB R7=0xCC R8=0xDD
;===========================================================
    MOV     R0, #0x1000
    MOV     R1, #0xAA
    MOV     R3, #0xBB
    MOV     R4, #0xCC
    MOV     R5, #0xDD
    STMDB   R0!, {R1, R3, R4, R5}
    MOV     R0, #0x1000
    LDMDB   R0!, {R2, R6, R7, R8}

stop
    B       stop
    END
