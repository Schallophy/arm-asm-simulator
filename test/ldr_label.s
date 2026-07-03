    AREA    TestCode, CODE, READONLY
    ENTRY

Start
    LDR R0, =MyData
    LDR R0, [R0]

Done
    B Done

    AREA    TestData, DATA, READWRITE
MyData
    DCD 42
    DCD 100
    DCD 200

    END
