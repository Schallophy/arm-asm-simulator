    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #10
    
    CMP R0, R1             ; Equal
    BEQ EqualBranch        ; Should branch
    MOV R2, #0             ; Should be skipped
    
EqualBranch
    MOV R0, #1
    
    CMP R0, R1             ; 1 != 10
    BNE NotEqualBranch     ; Should branch
    MOV R3, #0             ; Should be skipped
    
NotEqualBranch
    MOV R1, #0
    
Loop
    B Loop
    
    END
