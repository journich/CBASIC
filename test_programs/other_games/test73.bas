10 REM Test binary to decimal
20 LET B$ = "1101"
30 LET D = 0
40 FOR I = 1 TO LEN(B$)
50 LET D = D * 2
60 IF MID$(B$,I,1) = "1" THEN D = D + 1
70 NEXT I
80 PRINT B$; " binary = "; D; " decimal"
90 END
