    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    MOV R0, #0
    BL MyFunc
    MOV R0, #42
    
Done
    B Done

MyFunc
    BX LR

    END
