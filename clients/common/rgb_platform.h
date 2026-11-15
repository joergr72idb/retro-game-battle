/* Retro Game Battle - what each C client provides to the shared core (rgb_core.c).
 *
 * The core speaks the raw TCP protocol (docs/protocol.md) through the fujinet-lib network calls:
 * FujiNet's fujinet-lib on the Atari, C64 and Apple II, fujinet-network-cpc on the CPC + M4.
 * Each client folder has a platform.c with these functions and a platform.h with:
 *   PLAT_ID         protocol platform name ("ATARI", "C64", "CPC", "APPLE2")
 *   PLAT_TITLE      the line under the title, e.g. "ATARI XL/XE FUJINET - TCP BUILD " BUILD
 *   PLAT_CONTROLS   how to steer, shown at the start of a match
 *   PLAT_ENTER      the name of the RETURN key on the keyboard
 *   PLAT_NO_DEVICE  shown when plat_init finds no network device
 *   PLAT_BACKSPACE  what deletes the last character on the screen
 *   PLAT_PETSCII    defined on the C64: lines are converted ASCII <-> PETSCII
 * and the defaults SERVER_HOST, SERVER_PORT (strings) and PLAYER_NAME.
 */
#ifndef RGB_PLATFORM_H
#define RGB_PLATFORM_H

#define RGB_KEY_ENTER   13
#define RGB_KEY_DEL     8

unsigned char plat_init(void);          /* 1 = ok: network device found, keyboard/joystick ready */
void plat_clear(void);                  /* clear the screen */
void plat_print(const char *s);         /* '\n' starts a new line */
void plat_putc(char c);
void plat_cursor(unsigned char on);
unsigned char plat_getkey(void);        /* waits for a key: RGB_KEY_ENTER, RGB_KEY_DEL or the character */

/* Keys typed since the last call and the stick, without waiting: returns the direction
   (0 = none, 1-4 = up, down, left, right); *fire = 1 for a new press of fire or space. */
unsigned char plat_input(unsigned char *fire);

#endif
