/* a2shot - headless run of FujiNet Go Apple II (its session core, no window), for `make shot`:
 * boots the machine with the built-in FujiNet, optionally straight from a disk image on the
 * FujiNet SD card (CONFIG off), types a key script, and after FRAMES frames writes the screen as
 * PNG, the 40-column text screen as text and the FujiNet console log. The sound of the whole run
 * (speaker and Mockingboard) goes to OUT_PREFIX.wav.
 *
 *   a2shot CONFIG_DIR DATA_DIR RUNTIME_SRC FRAMES 'SCRIPT' OUT_PREFIX [DISK_NAME]
 *
 * SCRIPT: commands separated by ';', one after another from power-on (60 frames = 1 s):
 *   wait N            nothing pressed for N frames
 *   type TEXT         types TEXT, each key 3 frames down and 3 up
 *   key NAME [N]      RETURN ESC TAB BACKSPACE DELETE UP DOWN LEFT RIGHT SPACE, a character,
 *                     or CTRL-x; held N frames (default 3), then 3 up
 *   joy DIR [N]       paddles 0/1 as a joystick: left right up down upleft ... center; held N
 *                     frames (default 10), then centred
 *   button B [N]      paddle button 0 (Open Apple) or 1 (Closed Apple), N frames (default 10)
 *   reset [cold]      Ctrl-Reset (cold: power cycle)
 *
 * It links FujiNet Go's GPL-3.0-or-later session library, so the a2shot binary is GPL-3 too.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <zlib.h>

#include "apple2session.h"
#include "apple2debug.h"

static apple2session *S;

static void die(const char *msg, const char *arg)
{
    fprintf(stderr, "a2shot: %s%s\n", msg, arg ? arg : "");
    if (S) { apple2session_stop(S); apple2session_free(S); }
    exit(1);
}

/* ---- sound: pulled from the session (44100 Hz stereo S16) into a WAV file ------------------ */
static FILE *wav;
static volatile int wav_stop;
static uint32_t wav_bytes;

static void wav_header(void)
{
    unsigned char h[44];
    uint32_t rate = 44100, byterate = rate * 4;
    memcpy(h, "RIFF", 4); h[4] = (36 + wav_bytes) & 255; h[5] = (36 + wav_bytes) >> 8 & 255;
    h[6] = (36 + wav_bytes) >> 16 & 255; h[7] = (36 + wav_bytes) >> 24;
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[17] = h[18] = h[19] = 0; h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;
    h[24] = rate & 255; h[25] = rate >> 8 & 255; h[26] = rate >> 16; h[27] = 0;
    h[28] = byterate & 255; h[29] = byterate >> 8 & 255; h[30] = byterate >> 16 & 255; h[31] = 0;
    h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4); h[40] = wav_bytes & 255; h[41] = wav_bytes >> 8 & 255;
    h[42] = wav_bytes >> 16 & 255; h[43] = wav_bytes >> 24;
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
}

/* render_audio waits for a whole block from the emulator (or pads with silence), so this paces
 * itself at the emulator's speed */
static void *wav_thread(void *arg)
{
    static int16_t buf[1470];             /* 1/60 s, stereo */
    (void)arg;
    while (!wav_stop) {
        apple2session_render_audio(S, buf, 1470);
        wav_bytes += fwrite(buf, 2, 1470, wav) * 2;
    }
    return NULL;
}

/* ---- frame clock ------------------------------------------------------------------------- */
static uint64_t start_frame;
static int dry;            /* first pass: only check the script and count its frames */

static uint64_t frame_now(void) { return apple2session_frame_count(S) - start_frame; }

static void wait_until(uint64_t f)
{
    if (dry) return;
    while (frame_now() < f) {
        if (!apple2session_is_running(S)) die("the emulator stopped", NULL);
        usleep(2000);
    }
}

/* ---- keys -------------------------------------------------------------------------------- */
#define XK_RETURN 0xFF0D
#define XK_ESCAPE 0xFF1B
#define XK_TAB    0xFF09
#define XK_BACKSP 0xFF08
#define XK_DELETE 0xFFFF
#define XK_LEFT   0xFF51
#define XK_UP     0xFF52
#define XK_RIGHT  0xFF53
#define XK_DOWN   0xFF54

static uint64_t cursor;     /* frame the next command starts at */

static void press(uint32_t keysym, uint32_t ch, int ctrl, int shift, int n)
{
    wait_until(cursor);
    if (!dry) apple2session_key(S, 1, keysym, ch, ctrl, shift);
    cursor += n;
    wait_until(cursor);
    if (!dry) apple2session_key(S, 0, keysym, ch, ctrl, shift);
    cursor += 3;
}

static int shifted(int c)
{
    /* the emulator starts with Caps Lock on: letters come out upper case, with Shift lower case */
    return (c >= 'a' && c <= 'z') || (c && strchr("!@#$%^&*()_+{}|:\"<>?~", c) != NULL);
}

static void key_named(const char *name, int n)
{
    static const struct { const char *name; uint32_t sym; uint32_t ch; } k[] = {
        { "RETURN", XK_RETURN, 0 }, { "ESC", XK_ESCAPE, 0 }, { "TAB", XK_TAB, 0 },
        { "BACKSPACE", XK_BACKSP, 0 }, { "DELETE", XK_DELETE, 0 }, { "LEFT", XK_LEFT, 0 },
        { "RIGHT", XK_RIGHT, 0 }, { "UP", XK_UP, 0 }, { "DOWN", XK_DOWN, 0 },
        { "SPACE", ' ', ' ' },
    };
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++)
        if (strcasecmp(name, k[i].name) == 0) { press(k[i].sym, k[i].ch, 0, 0, n); return; }
    if (strncasecmp(name, "CTRL-", 5) == 0 && name[5] && !name[6]) {
        int c = name[5] | 0x20;     /* the core resolves Ctrl combos from the lower-case key */
        press(c, 0, 1, 0, n);
        return;
    }
    if (name[0] && !name[1]) { press((unsigned char)name[0], (unsigned char)name[0], 0, shifted(name[0]), n); return; }
    die("unknown key: ", name);
}

static void joy(const char *dir, int n)
{
    static const struct { const char *d; float x, y; } j[] = {
        { "center", 0, 0 }, { "left", -1, 0 }, { "right", 1, 0 }, { "up", 0, -1 }, { "down", 0, 1 },
        { "upleft", -1, -1 }, { "upright", 1, -1 }, { "downleft", -1, 1 }, { "downright", 1, 1 },
    };
    for (size_t i = 0; i < sizeof j / sizeof j[0]; i++)
        if (strcasecmp(dir, j[i].d) == 0) {
            wait_until(cursor);
            if (!dry) apple2session_paddle(S, j[i].x, j[i].y);
            cursor += n;
            wait_until(cursor);
            if (!dry) apple2session_paddle(S, 0, 0);
            cursor += 3;
            return;
        }
    die("unknown direction: ", dir);
}

static void run_script(char *script)
{
    for (char *cmd = strtok(script, ";"); cmd; cmd = strtok(NULL, ";")) {
        while (*cmd == ' ') cmd++;
        if (!*cmd) continue;
        char word[16] = "", a1[64] = "";
        int n = -1;
        if (strncmp(cmd, "type ", 5) == 0) {
            for (const char *p = cmd + 5; *p; p++)
                press((unsigned char)*p, (unsigned char)*p, 0, shifted(*p), 3);
            continue;
        }
        sscanf(cmd, "%15s %63s %d", word, a1, &n);
        if (strcmp(word, "wait") == 0)        cursor += atoi(a1);
        else if (strcmp(word, "key") == 0)    key_named(a1, n > 0 ? n : 3);
        else if (strcmp(word, "joy") == 0)    joy(a1, n > 0 ? n : 10);
        else if (strcmp(word, "button") == 0) {
            int b = atoi(a1);
            wait_until(cursor);
            if (!dry) apple2session_paddle_button(S, b, 1);
            cursor += n > 0 ? n : 10;
            wait_until(cursor);
            if (!dry) apple2session_paddle_button(S, b, 0);
            cursor += 3;
        } else if (strcmp(word, "reset") == 0) {
            wait_until(cursor);
            if (!dry) apple2session_reset(S, strcmp(a1, "cold") == 0);
            cursor += 3;
        } else die("unknown command: ", word);
    }
}

/* ---- output ------------------------------------------------------------------------------ */
static void png_chunk(FILE *f, const char *type, const unsigned char *d, uint32_t n)
{
    unsigned char hdr[8] = { n >> 24, n >> 16, n >> 8, n, type[0], type[1], type[2], type[3] };
    uLong crc = crc32(crc32(0, hdr + 4, 4), d, n);
    unsigned char c[4] = { crc >> 24, crc >> 16, crc >> 8, crc };
    fwrite(hdr, 1, 8, f); fwrite(d, 1, n, f); fwrite(c, 1, 4, f);
}

static void write_png(const char *path, const uint32_t *px, int w, int h)
{
    size_t rawn = (size_t)h * (1 + 3 * w);
    unsigned char *raw = malloc(rawn), *z;
    uLongf zn = compressBound(rawn);
    z = malloc(zn);
    for (int y = 0; y < h; y++) {
        unsigned char *r = raw + (size_t)y * (1 + 3 * w);
        r[0] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t p = px[y * w + x];
            r[1 + 3 * x] = p >> 16; r[2 + 3 * x] = p >> 8; r[3 + 3 * x] = p;
        }
    }
    compress2(z, &zn, raw, rawn, 6);
    FILE *f = fopen(path, "wb");
    if (!f) die("cannot write ", path);
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    unsigned char ihdr[13] = { w >> 24, w >> 16, w >> 8, w, h >> 24, h >> 16, h >> 8, h, 8, 2, 0, 0, 0 };
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, zn);
    png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw); free(z);
}

/* text page 1 ($0400), 24 rows of 40: normal, inverse and flashing characters as plain ASCII */
static void write_text(const char *path)
{
    apple2debug *d = apple2session_debugger(S);
    FILE *f = fopen(path, "w");
    if (!f) die("cannot write ", path);
    for (int row = 0; row < 24; row++) {
        uint8_t b[40];
        char line[41];
        apple2debug_read(d, 0x400 + (row % 8) * 0x80 + (row / 8) * 0x28, b, 40);
        for (int i = 0; i < 40; i++) {
            int c = b[i] & 0x7f;
            if (b[i] < 0x80) c = (b[i] & 0x3f) < 0x20 ? (b[i] & 0x3f) + 0x40 : (b[i] & 0x3f);
            line[i] = c >= 0x20 && c < 0x7f ? c : ' ';
        }
        int n = 40;
        while (n > 0 && line[n - 1] == ' ') n--;
        line[n] = 0;
        fprintf(f, "%s\n", line);
    }
    fclose(f);
}

/* ---- setup ------------------------------------------------------------------------------- */
static int port_free(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1, ok;
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    ok = bind(fd, (struct sockaddr *)&a, sizeof a) == 0;
    close(fd);
    return ok;
}

/* fnconfig.ini: CONFIG off and the disk in drive slot 1 from host slot 1 (the SD card), or CONFIG
 * on and no mounts. Other lines stay as they are. */
static void set_boot_disk(const char *ini, const char *disk)
{
    FILE *f = fopen(ini, "r");
    if (!f) die("no FujiNet config: ", ini);
    char *out = NULL, line[512];
    size_t outn = 0;
    FILE *o = open_memstream(&out, &outn);
    int skip = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '[') skip = strncmp(line, "[Mount", 6) == 0;
        if (skip) continue;
        if (strncmp(line, "configenabled=", 14) == 0)
            fprintf(o, "configenabled=%d\n", disk ? 0 : 1);
        else
            fputs(line, o);
    }
    if (disk) fprintf(o, "\n[Mount1]\nhostslot=1\npath=/%s\nmode=r\n", disk);
    fclose(o); fclose(f);
    f = fopen(ini, "w");
    if (!f) die("cannot write ", ini);
    fwrite(out, 1, outn, f);
    fclose(f);
    free(out);
}

int main(int argc, char **argv)
{
    if (argc < 7 || argc > 8) {
        fprintf(stderr, "usage: a2shot CONFIG_DIR DATA_DIR RUNTIME_SRC FRAMES 'SCRIPT' OUT_PREFIX [DISK_NAME]\n");
        return 2;
    }
    int frames = atoi(argv[4]);
    char *script = strdup(argv[5]);

    /* check the script before anything starts */
    dry = 1;
    run_script(strdup(argv[5]));
    if (cursor > (uint64_t)frames) {
        fprintf(stderr, "a2shot: the script needs %llu frames, FRAMES is %d\n", (unsigned long long)cursor, frames);
        return 1;
    }
    dry = 0;
    cursor = 0;
    const char *outp = argv[6], *disk = argc > 7 && argv[7][0] ? argv[7] : NULL;
    char path[1024];

    if (!port_free(APPLE2SESSION_SLIP_PORT) || !port_free(APPLE2SESSION_WEBUI_PORT)) {
        fprintf(stderr, "a2shot: port %d or %d is in use - is FujiNet Go (make run) still open?\n",
                APPLE2SESSION_SLIP_PORT, APPLE2SESSION_WEBUI_PORT);
        return 1;
    }

    apple2session_paths paths = { argv[1], argv[2], NULL, argv[3] };
    S = apple2session_new(&paths);
    if (!S) die("cannot create the session", NULL);
    apple2session_start_opts o;
    apple2session_default_opts(S, &o);
    o.enable_audio = 0;
    o.enable_gamepad = 0;
    o.enable_fujinet = 1;
    o.external_fujinet = 0;
    /* the config is read by FujiNet when it starts: write it first (provisioned on the first run
     * by a start/stop cycle) */
    snprintf(path, sizeof path, "%s/fujinet/fnconfig.ini", apple2session_data_path(S));
    if (access(path, F_OK) != 0) {
        if (apple2session_start(S, &o) != 0) die("start failed: ", apple2session_last_error(S));
        apple2session_stop(S);
    }
    set_boot_disk(path, disk);
    if (apple2session_start(S, &o) != 0) die("start failed: ", apple2session_last_error(S));
    start_frame = apple2session_frame_count(S);
    pthread_t wav_tid;
    snprintf(path, sizeof path, "%s.wav", outp);
    wav = fopen(path, "wb");
    if (wav) {
        wav_header();
        pthread_create(&wav_tid, NULL, wav_thread, NULL);
    }

    run_script(script);
    /* PEEK=addr:len with PEEK_EVERY=N: print that memory every N frames while waiting */
    const char *every = getenv("PEEK_EVERY");
    if (every && atoi(every) > 0 && getenv("PEEK")) {
        int n = atoi(every);
        unsigned addr = (unsigned)strtoul(getenv("PEEK"), NULL, 16), len = 1;
        const char *colon = strchr(getenv("PEEK"), ':');
        if (colon) len = (unsigned)atoi(colon + 1);
        if (len > 256) len = 256;
        for (uint64_t f = cursor - cursor % n + n; f < (uint64_t)frames; f += n) {
            uint8_t b[256];
            wait_until(f);
            apple2debug_read(apple2session_debugger(S), (uint16_t)addr, b, len);
            printf("a2shot: frame %llu peek %04X:", (unsigned long long)f, addr);
            for (unsigned i = 0; i < len; i++) printf(" %02X", b[i]);
            printf("\n");
        }
    }
    wait_until(frames);
    if (wav) {
        wav_stop = 1;
        pthread_join(wav_tid, NULL);
        wav_header();
        fclose(wav);
    }

    static uint32_t px[APPLE2SESSION_FB_MAX_WIDTH * APPLE2SESSION_FB_MAX_HEIGHT];
    uint64_t serial = 0;
    int w = 0, h = 0;
    if (!apple2session_copy_frame(S, px, &serial, &w, &h)) die("no frame", NULL);
    snprintf(path, sizeof path, "%s.png", outp);
    write_png(path, px, w, h);
    snprintf(path, sizeof path, "%s.txt", outp);
    write_text(path);

    static char log[1 << 16];
    int n = apple2session_fujinet_copy_log(S, log, sizeof log);
    snprintf(path, sizeof path, "%s.fujinet.log", outp);
    FILE *f = fopen(path, "w");
    if (f) { fwrite(log, 1, n, f); fclose(f); }

    /* PEEK=addr:len (hex address): print that memory at the end, e.g. a program's frame counter */
    const char *peek = getenv("PEEK");
    if (peek && *peek) {
        unsigned addr = (unsigned)strtoul(peek, NULL, 16), len = 1;
        const char *colon = strchr(peek, ':');
        if (colon) len = (unsigned)atoi(colon + 1);
        if (len > 256) len = 256;
        uint8_t b[256];
        apple2debug_read(apple2session_debugger(S), (uint16_t)addr, b, len);
        printf("a2shot: peek %04X:", addr);
        for (unsigned i = 0; i < len; i++) printf(" %02X", b[i]);
        printf("\n");
    }
    printf("a2shot: %d frames, %dx%d, FujiNet link %s\n", frames, w, h,
           apple2session_fujinet_link(S) ? "up" : "down");
    apple2session_stop(S);
    apple2session_free(S);
    return 0;
}
