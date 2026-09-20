10 rem ===================================
11 rem retro game battle - c64 client (http-polling, meatloaf)
12 rem server draws the board - this c64 just
13 rem steers. uses the confirmed working
14 rem meatloaf http-get pattern (device 8,
15 rem secondary address 3) instead of the
16 rem unconfirmed raw socket.
17 rem joystick in port 2 (port 1 shares lines with the keyboard).
18 rem ===== restart point, reached via "run" (see below) =====
20 print chr$(147);chr$(14)
21 print chr$(18);
22 for i=1 to 40:print " ";:next i:print
23 print "   retro game battle -"
24 print "   classic computing 2026"
25 print "     commodore 64 edition - build 1"
26 for i=1 to 40:print " ";:next i:print
27 print chr$(146)
28 print
30 ho$="192.168.17.138":rem <-- edit: server ip
40 po$="8080":rem <-- edit: http bridge port! (see server startup message,
41 rem     NOT the tcp game port 6502 - the http bridge runs separately)
50 na$="COMMODORE64":rem <-- edit: your player name
100 print "enter your pin or press enter:"
110 input pi$
111 rem -- port-1 joystick shares lines with the keyboard matrix; a
112 rem    phantom keypress can reset the cursor onto the prompt line
113 rem    above, so that "input" reads the prompt text itself as the pin.
114 rem    length/whitespace check catches that.
115 bad=0:if len(pi$)>10 then bad=1:goto 118
116 for pc=1 to len(pi$):if mid$(pi$,pc,1)=" " then bad=1
117 next pc
118 if bad=1 then pi$=""
120 if pi$="" then pi$="NONE"
130 print "connecting..."
140 u$="http://"+ho$+":"+po$+"/join/C64/"+na$+"/C64/"+pi$
150 gosub 2000:rem http get -> r$
160 if left$(r$,7)="session" or left$(r$,7)="SESSION" then 220
170 for dl=1 to 3:for w=1 to 1500:next w:next dl:goto 140
220 rem extract first line "session <id>"
230 sp=1
240 if mid$(r$,sp,1)=" " then 260
250 sp=sp+1:if sp<=len(r$) then 240
260 ss=sp+1:se=ss
270 if se>len(r$) then 300
280 if mid$(r$,se,1)=chr$(10) or mid$(r$,se,1)=chr$(13) then 300
290 se=se+1:goto 270
300 sn$=mid$(r$,ss,se-ss)
301 rem -- rs$ = whatever came after the "session <id>" line (usually
302 rem    already a WAIT GAMES list, or even a START if we were auto-
303 rem    matched as the second player - see gosub 3000 below) --
304 rs$="":if se<=len(r$) then rs$=mid$(r$,se+1,len(r$)-se)
310 gs=0
315 gosub 3000
320 if left$(rs$,5)="start" or left$(rs$,5)="START" then 340
330 goto 400
340 print "use joystick - port 2":gs=1
400 rem ===== main loop =====
405 dr$="N"
410 j=(255-peek(56320)) and 31
420 nd$="N"
430 if (j and 1)<>0 then nd$="U"
440 if (j and 2)<>0 then nd$="D"
450 if (j and 4)<>0 then nd$="L"
460 if (j and 8)<>0 then nd$="R"
470 if nd$<>"N" then dr$=nd$
480 u$="http://"+ho$+":"+po$+"/tick/"+sn$+"/"+dr$
490 gosub 2000:rem http get -> r$
491 if r$="" then 560
492 ec=0
493 if left$(r$,3)="err" or left$(r$,3)="ERR" then 800
494 if gs=1 then 510
495 if left$(r$,5)<>"start" and left$(r$,5)<>"START" then 510
496 print "use joystick - port 2":gs=1
510 if left$(r$,3)<>"end" and left$(r$,3)<>"END" then 400
520 rem -- game over: show result, then restart --
521 print "game over":print r$
525 print "restarting..."
527 for dl=1 to 3:for w=1 to 1500:next w:next dl :rem ~3 sec, adjust if needed
528 run
560 if gs=0 then 400 :rem no opponent yet - this is allowed to take as long as it wants
561 ec=ec+1
562 if ec>500 then print "no response - restarting...":run
563 goto 400
800 print "restarting...":run
2000 rem -- http get u$ -> r$ (the confirmed working pattern) --
2010 open 1,8,3,u$
2020 r$=""
2030 get#1,a$
2035 if a$<>"" then if len(r$)<250 then r$=r$+a$
2040 if st<>0 then 2070
2060 goto 2030
2070 close 1
2080 return
3000 rem -- interpret rs$, looping through game-selection rounds until
3001 rem    a terminal state (plain "wait" or a "start ..." line) --
3010 if left$(rs$,10)="wait games" or left$(rs$,10)="WAIT GAMES" then sp=12:gosub 4000:goto 3010
3020 if left$(rs$,16)="err incompatible" or left$(rs$,16)="ERR INCOMPATIBLE" then sp=24:gosub 4000:goto 3010
3030 if rs$="WAIT" or rs$="wait" then print "waiting for opponent...":return
3040 if left$(rs$,3)="err" or left$(rs$,3)="ERR" then print rs$:goto 800
3050 return
4000 rem -- parse game list from rs$ at sp, show menu, read choice,
4001 rem    send it as /tick/<session>/GAME<n> --
4010 gosub 4200 :rem tok$=game count
4020 ng=val(tok$)
4030 if ng=0 then print "no games available - restarting...":goto 800
4040 print "pick a game:"
4050 for gi=1 to ng
4060 gosub 4200 :rem tok$="id:label"
4070 cp=1
4080 if mid$(tok$,cp,1)=":" then 4110
4090 cp=cp+1
4095 if cp<=len(tok$) then 4080
4110 lbl$=mid$(tok$,cp+1,len(tok$)-cp)
4120 print gi;". ";lbl$
4130 next gi
4140 print "enter number";
4150 input nsel
4160 if nsel<1 or nsel>ng then 4140
4170 ns$=mid$(str$(nsel),2) :rem str$() reserves a leading sign space - strip it
4175 u$="http://"+ho$+":"+po$+"/tick/"+sn$+"/GAME"+ns$
4180 gosub 2000 :rem http get -> r$
4190 rs$=r$
4195 return
4200 rem -- tok$ = next space-delimited token from rs$ at sp; advances sp --
4210 te=sp
4220 if te>len(rs$) then 4250
4230 if mid$(rs$,te,1)=" " then 4250
4240 te=te+1:goto 4220
4250 tok$=""
4260 if te>sp then tok$=mid$(rs$,sp,te-sp)
4270 sp=te+1
4280 return
