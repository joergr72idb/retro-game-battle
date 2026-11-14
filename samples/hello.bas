' FastBasic sample: a short colour show, then waits for a key.
GRAPHICS 0
? "Hello from FastBasic!"
? "Press a key."
REPEAT
  INC C
  POKE 710, C
  PAUSE 2
UNTIL KEY()
GET K
