10 REM Test modulo using INT
20 FOR I = 1 TO 10
30 LET M = I - INT(I / 3) * 3
40 PRINT I; " MOD 3 = "; M
50 NEXT I
60 END
