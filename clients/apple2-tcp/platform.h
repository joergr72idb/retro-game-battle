/* Retro Game Battle - Apple II client (FujiNet on the SmartPort): the platform part.
 * Apple //e enhanced or later; a ProDOS SYS program (RGBTCP.SYSTEM) that starts by itself from
 * the boot disk. Built with the Apple II kit (cc65 + fujinet-lib >= 5.1.1), see Makefile.
 * The shared part is ../common/rgb_core.c, the interface ../common/rgb_platform.h.
 */
#define BUILD           "3"
#define PLAT_ID         "APPLE2"
#define PLAT_TITLE      "APPLE II FUJINET - TCP BUILD " BUILD
#define PLAT_CONTROLS   "USE JOYSTICK OR ARROW KEYS,\nFIRE = BUTTON 0 OR SPACE"
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
#define PLAYER_NAME "APPLE2"
#endif
