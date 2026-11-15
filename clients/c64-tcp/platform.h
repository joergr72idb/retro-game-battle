/* Retro Game Battle - C64 client (FujiNet on the IEC bus, network device 16): the platform part.
 * Built with the C64 kit (cc65 + fujinet-lib >= 5.1.1), see Makefile. The shared part is
 * ../common/rgb_core.c; PLAT_PETSCII makes it convert lines between PETSCII and ASCII.
 */
#define BUILD           "4"
#define PLAT_ID         "C64"
#define PLAT_TITLE      "C64 FUJINET - TCP BUILD " BUILD
#define PLAT_CONTROLS   "USE JOYSTICK (PORT 2), FIRE = BUTTON OR SPACE"
#define PLAT_ENTER      "RETURN"
#define PLAT_NO_DEVICE  "NO FUJINET FOUND."
#define PLAT_BACKSPACE  "\x9d \x9d"
#define PLAT_PETSCII

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT "6502"
#endif
#ifndef PLAYER_NAME
#define PLAYER_NAME "C64FUJINET"
#endif
