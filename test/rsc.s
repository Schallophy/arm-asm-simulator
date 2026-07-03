    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #10
    MOV R1, #10
    
    ; Set carry
    CMP R0, R1            ; C=1, Z=1
    
    ; RSC: R0 = R1 - R0 - !C = 10 - 10 - 0 = 0
    ; But we cleared carry? No, CMP 10,10 sets C=1
    ; RSC R0, R1, R0 means R0 = R0 - R1 - !C = 10 - 10 - 0 = 0
    
    ; Clear carry first
    CMP R0, #100          ; 10 < 100, C=0
    RSC R0, R1, R0        ; R0 = R0 - R1 - !C = 10 - 10 - 1 = -1
    
Loop
    B Loop
    
    END
