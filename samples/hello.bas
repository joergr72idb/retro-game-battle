10 rem basic sample: a short colour show, then waits for a key
20 print "{clr}hello from c64 basic!"
30 print "press a key."
40 c=c+1:poke 53280,c and 15
50 for d=1 to 30:next d
60 get k$:if k$="" then 40
70 poke 53280,14
