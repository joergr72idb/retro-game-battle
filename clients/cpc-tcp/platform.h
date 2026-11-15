/* Retro Game Battle - Schneider/Amstrad CPC client (M4 board): the platform part.
 * The network calls are the same as on the FujiNet machines: fujinet-network-cpc provides
 * fujinet-lib's network functions on the M4. Built with SDCC, the CPC kit and
 * fujinet-network-cpc (../../lib/fujinet-network-cpc), see Makefile. The shared part is ../common/rgb_core.c.
 */
#define BUILD           "3"
#define PLAT_ID         "CPC"
#define PLAT_TITLE      "SCHNEIDER CPC - TCP BUILD " BUILD
#define PLAT_CONTROLS   "USE JOYSTICK, FIRE = BUTTON OR SPACE"
#define PLAT_ENTER      "ENTER"
#define PLAT_NO_DEVICE  "NO M4 BOARD FOUND."
#define PLAT_BACKSPACE  "\x08 \x08"

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif
#ifndef SERVER_PORT
#define SERVER_PORT "6502"
#endif
#ifndef PLAYER_NAME
#define PLAYER_NAME "SCHNEIDERCPC"
#endif
