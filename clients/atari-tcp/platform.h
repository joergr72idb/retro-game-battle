/* Retro Game Battle - Atari XL/XE client (FujiNet over SIO): the platform part.
 * Talks to the FujiNet's network device directly (fujinet-lib), so no N: handler is needed;
 * a binary (rgbtcp.xex) that the FujiNet can boot from its SD card. Built with the Atari kit
 * (cc65 + fujinet-lib), see Makefile. The shared part is ../common/rgb_core.c.
 */
#define BUILD           "4"
#define PLAT_ID         "ATARI"
#define PLAT_TITLE      "ATARI XL/XE FUJINET - TCP BUILD " BUILD
#define PLAT_CONTROLS   "USE JOYSTICK 1 OR CTRL+ARROWS,\nFIRE = BUTTON OR SPACE"
#define PLAT_ENTER      "RETURN"
#define PLAT_NO_DEVICE  "NO FUJINET FOUND."
#define PLAT_BACKSPACE  "\b \b"

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT "6502"
#endif
#ifndef PLAYER_NAME
#define PLAYER_NAME "ATARIXL"
#endif
