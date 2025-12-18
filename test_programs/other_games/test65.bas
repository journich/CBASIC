10 REM Test square root approximation
20 LET N = 50
30 LET G = N / 2
40 FOR I = 1 TO 10
50 LET G = (G + N/G) / 2
60 NEXT I
70 PRINT "SQRT(50) approx = "; G
80 PRINT "ACTUAL SQR(50) = "; SQR(50)
90 END
