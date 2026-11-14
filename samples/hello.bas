10 REM BASIC sample: M4 network status and an HTTP download into memory (like the game clients)
20 MEMORY &7FFF:BUF=&8000:BL=200
30 PRINT "Hello from Locomotive BASIC!"
40 |NETSTAT
50 FOR I=0 TO BL-1:POKE BUF+I,0:NEXT
60 U$="http://127.0.0.1:8765/hello.txt"
70 |HTTPMEM,U$,BUF,BL
80 R$="":FOR I=0 TO BL-1:B=PEEK(BUF+I):IF B=0 THEN 100
90 IF B<>13 THEN R$=R$+CHR$(B)
95 NEXT
100 PRINT "HTTP: ";R$
