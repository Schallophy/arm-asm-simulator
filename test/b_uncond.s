    AREA Test, CODE, READONLY
    ENTRY

Start
    MOV R0, #0
    
    B SkipAdd
    ADD R0, R0, #100       ; Should be skipped
    
SkipAdd
    MOV R0, #42            ; Should execute
    
Loop
    B Loop
    
    END
