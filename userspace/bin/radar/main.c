/*
 * SeldOS - Humboldt Kernel Project
 * SNL OpSec Tactical Air Surveillance Radar (/bin/radar)
 *
 * Real-time Plan Position Indicator (PPI) Air Defense & ADS-B Mode-S Radar
 * Rendered on 680x334 Linear Framebuffer with Phosphor Decay Simulation.
 * Passive Zero-Emission RF Operation & Constant-Time Sincos Mathematics.
 *
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"
#include "sincos_lut.h"

#define SCR_W 680
#define SCR_H 334

/* Radar Scope Geometry */
#define SCOPE_CX 175
#define SCOPE_CY 167
#define SCOPE_R  145

/* Phosphor and UI Colors (Humboldt Palette & High-Contrast Tokens) */
#define COL_BG_BLACK       0xFF0A0F0D
#define COL_SCOPE_BG       0xFF06140D
#define COL_SCOPE_RING     0xFF0F3D24
#define COL_SCOPE_GRID     0xFF0A2B19
#define COL_BEAM_HEAD      0xFF86EFAC
#define COL_BEAM_TRAIL_1   0xFF22C55E
#define COL_BEAM_TRAIL_2   0xFF16A34A
#define COL_BEAM_TRAIL_3   0xFF15803D
#define COL_BEAM_TRAIL_4   0xFF166534
#define COL_BEAM_TRAIL_5   0xFF14532D
#define COL_BEAM_TRAIL_6   0xFF052E16

#define COL_PANEL_BG       0xFF0D1418
#define COL_PANEL_BORDER   0xFF1E293B
#define COL_PANEL_HEADER   0xFF0284C7
#define COL_TEXT_WHITE     0xFFF8FAFC
#define COL_TEXT_GREEN     0xFF4ADE80
#define COL_TEXT_CYAN      0xFF38BDF8
#define COL_TEXT_AMBER     0xFFFBBF24
#define COL_TEXT_ALERT     0xFFEF4444
#define COL_TEXT_MUTED     0xFF64748B

#define MAX_TARGETS 32
#define MAX_TABLE_ROWS 7
#define MAX_HISTORY 5

struct target_track {
    int active;
    char hex[8];            // 24-bit ICAO hex address (e.g. "4B1802")
    char callsign[12];      // Flight callsign or hex (e.g. "AFL214")
    int32_t x_m;            // Exact position in meters
    int32_t y_m;
    int32_t x_km;           // Display km relative to receiver station
    int32_t y_km;
    int32_t vx_kmh;         // Ground velocity X component in km/h
    int32_t vy_kmh;         // Ground velocity Y component in km/h
    int32_t alt_m;          // Barometric/Geometric altitude in meters
    int32_t speed_kmh;      // Ground speed in km/h
    uint16_t squawk;        // Mode 3/A squawk code
    int vspeed_ms;          // Vertical rate in m/s (+/-)
    int track_deg;          // True track heading (0..359 deg)
    int bearing_deg;        // Azimuth from station (0..359 deg)
    int dist_km;            // Distance from station (km)
    int illuminated;        // Sweep beam intersection flag
    int fade_timer;         // Phosphor decay 0..60
    uint32_t last_seen_frame;
    int history_x[MAX_HISTORY];
    int history_y[MAX_HISTORY];
    int history_count;
};

/* 5x7 ASCII font for ultra-crisp compact telemetry columns */
static const uint8_t font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 '!'
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 '$'
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 '%'
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 '&'
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '''
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 '('
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 ')'
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 '+'
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ','
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 '-'
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 '.'
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 ':'
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ';'
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 '<'
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 '='
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 '>'
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 87 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 'Z'
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 '['
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ']'
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 '^'
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 '_'
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 '`'
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 'a'
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 'b'
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 'c'
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 'd'
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 'e'
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 'f'
    {0x08, 0x14, 0x54, 0x54, 0x3C}, // 103 'g'
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 'h'
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 'i'
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 'j'
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 'k'
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 'l'
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 'm'
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 'n'
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 'o'
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 'p'
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 'q'
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 'r'
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 's'
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 't'
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 'u'
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 'v'
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 'w'
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 'x'
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 'y'
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 'z'
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 '{'
    {0x00, 0x00, 0x77, 0x00, 0x00}, // 124 '|'
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 '}'
    {0x08, 0x08, 0x2A, 0x1C, 0x08}  // 126 '~'
};

static struct seld_fb_info s_fb;
static uint32_t* s_backbuf = NULL;
static int s_audio_enabled = 1;
static int s_max_range_km = 150;
static int s_sweep_deg = 0;
static uint32_t s_frame_count = 0;

static int s_live_mode = 0;
static int s_live_count = 0;
static int s_net_status = 0; // 0=CONNECTING, 1=ONLINE, <0=ERROR
static char s_sector_name[32] = "OFFLINE-SIM";
static char s_user_sector_buf[32] = "";
static const char* s_sectors[] = { "baltic", "spb", "berlin", "dubai", "mil", "auto" };
#define NUM_SECTORS 6
static int s_sector_idx = 0;
static const char* s_current_sector = "baltic";
static uint32_t s_last_fetch_frame = 0;

static struct target_track s_tracks[MAX_TARGETS];

/* Direct Pixel / Rect Primitives */
static inline void put_pixel(int x, int y, uint32_t col) {
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) {
        s_backbuf[y * SCR_W + x] = col;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;

    for (int r = y; r < y + h; r++) {
        uint32_t* row = &s_backbuf[r * SCR_W + x];
        for (int c = 0; c < w; c++) {
            row[c] = col;
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        put_pixel(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_circle(int xc, int yc, int r, uint32_t col) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        put_pixel(xc + x, yc + y, col);
        put_pixel(xc - x, yc + y, col);
        put_pixel(xc + x, yc - y, col);
        put_pixel(xc - x, yc - y, col);
        put_pixel(xc + y, yc + x, col);
        put_pixel(xc - y, yc + x, col);
        put_pixel(xc + y, yc - x, col);
        put_pixel(xc - y, yc - x, col);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

static void draw_circle_dashed(int xc, int yc, int r, uint32_t col, int mask) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    int step = 0;
    while (y >= x) {
        if ((step & mask) < (mask / 2)) {
            put_pixel(xc + x, yc + y, col);
            put_pixel(xc - x, yc + y, col);
            put_pixel(xc + x, yc - y, col);
            put_pixel(xc - x, yc - y, col);
            put_pixel(xc + y, yc + x, col);
            put_pixel(xc - y, yc + x, col);
            put_pixel(xc + y, yc - x, col);
            put_pixel(xc - y, yc - x, col);
        }
        step++;
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

static void draw_text_5x7(int x, int y, const char* str, uint32_t col) {
    while (*str) {
        char ch = *str++;
        if (ch < 32 || ch > 126) ch = '?';
        const uint8_t* glyph = font5x7[(int)ch - 32];
        for (int c = 0; c < 5; c++) {
            uint8_t line = glyph[c];
            for (int r = 0; r < 7; r++) {
                if (line & (1 << r)) {
                    put_pixel(x + c, y + r, col);
                }
            }
        }
        x += 6;
    }
}


/* Polar to Cartesian helpers using 4096 fixed point table */
static inline int get_cos_4096(int deg) {
    deg = (deg % 360 + 360) % 360;
    return cos4096[deg];
}

static inline int get_sin_4096(int deg) {
    deg = (deg % 360 + 360) % 360;
    return sin4096[deg];
}

static void init_tactical_tracks(void) {
    memset(s_tracks, 0, sizeof(s_tracks));
    s_live_count = 0;
}

/* Approximate Integer Square Root */
static uint32_t isqrt(uint32_t n) {
    uint32_t root = 0;
    uint32_t bit = 1U << 30;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/* Approximate Fast Integer Atan2 in degrees [0..359] */
static int fast_atan2_deg(int y, int x) {
    if (x == 0 && y == 0) return 0;
    int ax = (x >= 0) ? x : -x;
    int ay = (y >= 0) ? y : -y;
    int angle;

    if (ax >= ay) {
        angle = (ay * 45) / (ax ? ax : 1);
    } else {
        angle = 90 - (ax * 45) / (ay ? ay : 1);
    }

    if (x >= 0 && y >= 0) {
        return angle;
    } else if (x < 0 && y >= 0) {
        return 180 - angle;
    } else if (x < 0 && y < 0) {
        return 180 + angle;
    } else {
        return 360 - angle;
    }
}

/*
 * Fetch Real ADS-B Aircraft Transponders via Tor OpSec Gateway (Port 8080)
 * Zero DNS queries issued (PLEDGE_DNS blocked by kernel).
 * Remote SOCKS5 ATYP=0x03 onion routing with zero ISP/TSPU leakage.
 */
static int fetch_live_tracks(void) {
    seld_net_lease_acquire();
    uint32_t ip = SELD_MAKE_IP(10, 0, 2, 2);
    int sock = seld_tcp_connect(ip, 8080);
    if (sock < 0) {
        sock = seld_tcp_connect(SELD_MAKE_IP(127, 0, 0, 1), 8080);
    }
    if (sock < 0) {
        seld_net_lease_release();
        s_net_status = -1; // Connection refused / gateway offline
        return -1;
    }

    char req[192];
    snprintf(req, sizeof(req),
        "GET /api/radar/tracks?sector=%s HTTP/1.1\r\n"
        "Host: 10.0.2.2:8080\r\n"
        "User-Agent: SeldOS-Radar/1.0\r\n"
        "Connection: close\r\n\r\n", s_current_sector);

    if (seld_tcp_send(sock, req, strlen(req)) <= 0) {
        seld_tcp_close(sock);
        seld_net_lease_release();
        s_net_status = -2;
        return -2;
    }

    static char buf[2048];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int n = seld_tcp_recv(sock, buf + total, sizeof(buf) - 1 - total, 1500);
        if (n <= 0) break;
        total += n;
    }
    seld_tcp_close(sock);
    seld_net_lease_release();

    if (total <= 0) {
        s_net_status = -3;
        return -3;
    }
    buf[total] = '\0';

    char* body = strstr(buf, "\r\n\r\n");
    if (!body) body = strstr(buf, "\n\n");
    if (!body) body = buf;
    else body += (body[0] == '\r') ? 4 : 2;

    if (!strstr(buf, "SRP/1.0 200 OK") && !strstr(body, "SRP/1.0 200 OK")) {
        s_net_status = -4;
        return -4;
    }

    s_net_status = 1; // Stream online and valid

    // Extract Sector Name
    char* sec_p = strstr(body, "SECTOR: ");
    if (sec_p) {
        sec_p += 8;
        int idx = 0;
        while (*sec_p && *sec_p != '\r' && *sec_p != '\n' && idx < (int)sizeof(s_sector_name) - 1) {
            s_sector_name[idx++] = *sec_p++;
        }
        s_sector_name[idx] = '\0';
    }

    // Parse TRK lines: TRK <hex> <callsign> <x_km> <y_km> <vx_kmh> <vy_kmh> <alt_m> <spd_kmh> <squawk> <vspeed_ms> <track_deg> <bearing_deg> <dist_km>
    char* line = strstr(body, "TRK ");
    int count = 0;
    while (line && count < MAX_TARGETS) {
        char hx[16], cs[16];
        int x, y, vx, vy, alt, spd, sqk, vspeed, trk, brg, dist;
        int n = sscanf(line, "TRK %s %s %d %d %d %d %d %d %d %d %d %d %d",
                       hx, cs, &x, &y, &vx, &vy, &alt, &spd, &sqk, &vspeed, &trk, &brg, &dist);
        if (n == 13) {
            s_tracks[count].active = 1;
            strncpy(s_tracks[count].hex, hx, sizeof(s_tracks[count].hex) - 1);
            s_tracks[count].hex[sizeof(s_tracks[count].hex) - 1] = '\0';
            strncpy(s_tracks[count].callsign, cs, sizeof(s_tracks[count].callsign) - 1);
            s_tracks[count].callsign[sizeof(s_tracks[count].callsign) - 1] = '\0';
            s_tracks[count].x_km = x;
            s_tracks[count].y_km = y;
            s_tracks[count].x_m = x * 1000;
            s_tracks[count].y_m = y * 1000;
            s_tracks[count].vx_kmh = vx;
            s_tracks[count].vy_kmh = vy;
            s_tracks[count].alt_m = alt;
            s_tracks[count].speed_kmh = spd;
            s_tracks[count].squawk = (uint16_t)sqk;
            s_tracks[count].vspeed_ms = vspeed;
            s_tracks[count].track_deg = trk;
            s_tracks[count].bearing_deg = brg;
            s_tracks[count].dist_km = dist;
            s_tracks[count].last_seen_frame = s_frame_count;
            count++;
        }
        line = strstr(line + 4, "TRK ");
    }

    s_live_count = count;
    for (int i = count; i < MAX_TARGETS; i++) {
        s_tracks[i].active = 0;
    }
    if (count > 0) {
        s_live_mode = 1;
    }
    return count;
}

static void update_tracks(void) {
    s_frame_count++;

    for (int i = 0; i < MAX_TARGETS; i++) {
        if (!s_tracks[i].active) continue;

        // Track aging: drop if unrefreshed for > 45 seconds (~1350 frames at ~30 FPS)
        if (s_live_mode && (s_frame_count - s_tracks[i].last_seen_frame) > 1350) {
            s_tracks[i].active = 0;
            continue;
        }

        // Truthful dead reckoning: advance position in meters based on ground velocity
        // 1 km/h = 1000m / 3600s = 1000m / 108000 frames (at 30 FPS) = 10 / 108 m/frame
        int32_t dx_m = (s_tracks[i].vx_kmh * 10) / 108;
        int32_t dy_m = (s_tracks[i].vy_kmh * 10) / 108;
        s_tracks[i].x_m += dx_m;
        s_tracks[i].y_m += dy_m;
        s_tracks[i].x_km = s_tracks[i].x_m / 1000;
        s_tracks[i].y_km = s_tracks[i].y_m / 1000;

        // Motion history trail recorded every 15 frames (~0.5s)
        if ((s_frame_count % 15) == 0) {
            if (s_tracks[i].history_count < MAX_HISTORY) {
                s_tracks[i].history_count++;
            }
            for (int h = s_tracks[i].history_count - 1; h > 0; h--) {
                s_tracks[i].history_x[h] = s_tracks[i].history_x[h - 1];
                s_tracks[i].history_y[h] = s_tracks[i].history_y[h - 1];
            }
            int px = SCOPE_CX + (s_tracks[i].x_km * SCOPE_R) / s_max_range_km;
            int py = SCOPE_CY - (s_tracks[i].y_km * SCOPE_R) / s_max_range_km;
            s_tracks[i].history_x[0] = px;
            s_tracks[i].history_y[0] = py;
        }

        // Distance & Bearing calculation relative to receiver
        uint32_t dist_sq = (s_tracks[i].x_km * s_tracks[i].x_km) + (s_tracks[i].y_km * s_tracks[i].y_km);
        s_tracks[i].dist_km = (int)isqrt(dist_sq);

        // Convert Math angle (East=0, North=90) to Compass Bearing (North=0, East=90, Clockwise)
        int math_deg = fast_atan2_deg(s_tracks[i].y_km, s_tracks[i].x_km);
        int compass_deg = (90 - math_deg + 360) % 360;
        s_tracks[i].bearing_deg = compass_deg;

        // Beam intersection test
        int diff = compass_deg - s_sweep_deg;
        if (diff < 0) diff += 360;

        if (diff <= 3) {
            if (!s_tracks[i].illuminated) {
                s_tracks[i].illuminated = 1;
                s_tracks[i].fade_timer = 60; // Max phosphor glow
                if (s_audio_enabled) {
                    if (s_tracks[i].squawk == 7700 || s_tracks[i].squawk == 7500) {
                        seld_beep(2200, 30); // Emergency pitch
                    } else if (s_tracks[i].squawk == 7600) {
                        seld_beep(1980, 20); // Lost comm pitch
                    } else {
                        seld_beep(1650, 15); // Standard Mode-S RF ping
                    }
                }
            }
        } else if (diff > 25) {
            s_tracks[i].illuminated = 0;
        }

        if (s_tracks[i].fade_timer > 0) {
            s_tracks[i].fade_timer--;
        }
    }
}

static void render_scope(void) {
    // 1. Dark Sector Scope Background
    fill_rect(0, 0, 355, SCR_H, COL_BG_BLACK);

    // Circular fill inside scope
    for (int y = -SCOPE_R; y <= SCOPE_R; y++) {
        int w = isqrt(SCOPE_R * SCOPE_R - y * y);
        fill_rect(SCOPE_CX - w, SCOPE_CY + y, w * 2, 1, COL_SCOPE_BG);
    }

    // 2. Crosshairs
    draw_line(SCOPE_CX - SCOPE_R, SCOPE_CY, SCOPE_CX + SCOPE_R, SCOPE_CY, COL_SCOPE_GRID);
    draw_line(SCOPE_CX, SCOPE_CY - SCOPE_R, SCOPE_CX, SCOPE_CY + SCOPE_R, COL_SCOPE_GRID);

    // 3. Range Rings (25%, 50%, 75%, 100% of current max range)
    int r1 = (SCOPE_R * 1) / 4;
    int r2 = (SCOPE_R * 2) / 4;
    int r3 = (SCOPE_R * 3) / 4;
    int r4 = SCOPE_R;

    draw_circle_dashed(SCOPE_CX, SCOPE_CY, r1, COL_SCOPE_RING, 7);
    draw_circle_dashed(SCOPE_CX, SCOPE_CY, r2, COL_SCOPE_RING, 7);
    draw_circle_dashed(SCOPE_CX, SCOPE_CY, r3, COL_SCOPE_RING, 7);
    draw_circle(SCOPE_CX, SCOPE_CY, r4, COL_BEAM_TRAIL_3);

    // Outer Compass Azimuth Ring
    draw_circle(SCOPE_CX, SCOPE_CY, SCOPE_R, COL_TEXT_GREEN);
    draw_circle(SCOPE_CX, SCOPE_CY, SCOPE_R + 2, COL_SCOPE_RING);

    // Azimuth Tick Marks every 30 degrees
    for (int deg = 0; deg < 360; deg += 30) {
        int cosv = get_cos_4096(deg);
        int sinv = get_sin_4096(deg);
        int x_in  = SCOPE_CX + (cosv * (SCOPE_R - 6)) / 4096;
        int y_in  = SCOPE_CY - (sinv * (SCOPE_R - 6)) / 4096;
        int x_out = SCOPE_CX + (cosv * (SCOPE_R + 2)) / 4096;
        int y_out = SCOPE_CY - (sinv * (SCOPE_R + 2)) / 4096;
        draw_line(x_in, y_in, x_out, y_out, COL_TEXT_GREEN);
    }

    // Cardinal Labels (N, E, S, W)
    draw_text_5x7(SCOPE_CX - 2, SCOPE_CY - SCOPE_R + 5, "N", COL_BEAM_HEAD);
    draw_text_5x7(SCOPE_CX + SCOPE_R - 10, SCOPE_CY - 3, "E", COL_BEAM_HEAD);
    draw_text_5x7(SCOPE_CX - 2, SCOPE_CY + SCOPE_R - 12, "S", COL_BEAM_HEAD);
    draw_text_5x7(SCOPE_CX - SCOPE_R + 5, SCOPE_CY - 3, "W", COL_BEAM_HEAD);

    // Range Marker Labels along 45 degree axis
    char lbl1[12], lbl2[12], lbl3[12];
    snprintf(lbl1, sizeof(lbl1), "%dK", s_max_range_km / 4);
    snprintf(lbl2, sizeof(lbl2), "%dK", s_max_range_km / 2);
    snprintf(lbl3, sizeof(lbl3), "%dK", (s_max_range_km * 3) / 4);
    draw_text_5x7(SCOPE_CX + r1 - 6, SCOPE_CY - r1 + 3, lbl1, COL_SCOPE_RING);
    draw_text_5x7(SCOPE_CX + r2 - 6, SCOPE_CY - r2 + 3, lbl2, COL_SCOPE_RING);
    draw_text_5x7(SCOPE_CX + r3 - 6, SCOPE_CY - r3 + 3, lbl3, COL_SCOPE_RING);

    // 4. Phosphor Trail (Fading afterglow behind the sweep beam)
    uint32_t trail_cols[6] = {
        COL_BEAM_TRAIL_1, COL_BEAM_TRAIL_2, COL_BEAM_TRAIL_3,
        COL_BEAM_TRAIL_4, COL_BEAM_TRAIL_5, COL_BEAM_TRAIL_6
    };
    for (int t = 5; t >= 0; t--) {
        int trail_deg = (s_sweep_deg - (t * 3) + 360) % 360;
        int math_deg = (90 - trail_deg + 360) % 360;
        int bx = SCOPE_CX + (get_cos_4096(math_deg) * SCOPE_R) / 4096;
        int by = SCOPE_CY - (get_sin_4096(math_deg) * SCOPE_R) / 4096;
        draw_line(SCOPE_CX, SCOPE_CY, bx, by, trail_cols[t]);
    }

    // Active Sweeping Beam Line
    int beam_math_deg = (90 - s_sweep_deg + 360) % 360;
    int bx = SCOPE_CX + (get_cos_4096(beam_math_deg) * SCOPE_R) / 4096;
    int by = SCOPE_CY - (get_sin_4096(beam_math_deg) * SCOPE_R) / 4096;
    draw_line(SCOPE_CX, SCOPE_CY, bx, by, COL_BEAM_HEAD);

    // 5. Target Blips & Trails
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (!s_tracks[i].active) continue;
        if (s_tracks[i].dist_km > s_max_range_km) continue;

        int px = SCOPE_CX + (s_tracks[i].x_km * SCOPE_R) / s_max_range_km;
        int py = SCOPE_CY - (s_tracks[i].y_km * SCOPE_R) / s_max_range_km;

        // Skip if outside circle
        int dx = px - SCOPE_CX;
        int dy = py - SCOPE_CY;
        if (dx * dx + dy * dy > SCOPE_R * SCOPE_R) continue;

        // Draw Motion History Trail (fading dots)
        for (int h = 1; h < s_tracks[i].history_count; h++) {
            put_pixel(s_tracks[i].history_x[h], s_tracks[i].history_y[h], COL_SCOPE_RING);
        }

        // Determine Target Color based on Mode-S Squawk
        uint32_t blip_col;
        if (s_tracks[i].squawk == 7700 || s_tracks[i].squawk == 7500) {
            blip_col = COL_TEXT_ALERT; // Red Emergency / Hijack
        } else if (s_tracks[i].squawk == 7600) {
            blip_col = COL_TEXT_AMBER; // Amber Lost Comm
        } else {
            blip_col = COL_TEXT_GREEN; // Standard Mode-S Transponder
        }

        // If illuminated, bright flash
        if (s_tracks[i].fade_timer > 40) {
            blip_col = COL_TEXT_WHITE;
        }

        // Draw Target Symbol (Square with center core)
        put_pixel(px - 1, py - 1, blip_col);
        put_pixel(px,     py - 1, blip_col);
        put_pixel(px + 1, py - 1, blip_col);
        put_pixel(px - 1, py,     blip_col);
        put_pixel(px,     py,     0xFFFFFFFF); // Center hot spot
        put_pixel(px + 1, py,     blip_col);
        put_pixel(px - 1, py + 1, blip_col);
        put_pixel(px,     py + 1, blip_col);
        put_pixel(px + 1, py + 1, blip_col);

        // Vector Velocity Leader (Speed stick in direction of movement)
        int vx_stick = (s_tracks[i].vx_kmh * 12) / 800;
        int vy_stick = -(s_tracks[i].vy_kmh * 12) / 800;
        draw_line(px, py, px + vx_stick, py + vy_stick, blip_col);

        // Target Tag Label (Callsign and Altitude)
        char tag[16];
        snprintf(tag, sizeof(tag), "%s %dM", s_tracks[i].callsign, s_tracks[i].alt_m);
        draw_text_5x7(px + 5, py - 4, tag, blip_col);
    }
}

static void render_telemetry_panel(void) {
    int px = 356;
    fill_rect(px, 0, SCR_W - px, SCR_H, COL_PANEL_BG);

    // Left Border separator
    draw_line(px, 0, px, SCR_H, COL_PANEL_BORDER);
    draw_line(px + 1, 0, px + 1, SCR_H, COL_PANEL_BORDER);

    // Title Header
    fill_rect(px + 2, 0, SCR_W - px - 2, 22, 0xFF141E28);
    draw_text_5x7(px + 8, 4, "[ SELD-RADAR : MODE-S SURVEILLANCE TERMINAL ]", COL_TEXT_WHITE);
    char sec_hdr[64];
    if (s_net_status == 1) {
        snprintf(sec_hdr, sizeof(sec_hdr), "RECEIVER: %s | TOR STREAM ONLINE (ZERO-LEAK)", s_sector_name);
        draw_text_5x7(px + 8, 13, sec_hdr, COL_TEXT_GREEN);
    } else if (s_net_status == -1) {
        snprintf(sec_hdr, sizeof(sec_hdr), "RECEIVER: %s | NO CARRIER (GATEWAY OFFLINE)", s_sector_name);
        draw_text_5x7(px + 8, 13, sec_hdr, COL_TEXT_ALERT);
    } else {
        snprintf(sec_hdr, sizeof(sec_hdr), "RECEIVER: %s | CONNECTING TO TOR GATEWAY...", s_sector_name);
        draw_text_5x7(px + 8, 13, sec_hdr, COL_TEXT_AMBER);
    }

    // Status / Hardware Box
    fill_rect(px + 6, 26, 312, 36, 0xFF0F1720);
    for (int c = px + 6; c < px + 318; c++) {
        put_pixel(c, 26, COL_PANEL_BORDER);
        put_pixel(c, 62, COL_PANEL_BORDER);
    }
    for (int r = 26; r < 62; r++) {
        put_pixel(px + 6, r, COL_PANEL_BORDER);
        put_pixel(px + 318, r, COL_PANEL_BORDER);
    }

    char stat_buf1[64];
    snprintf(stat_buf1, sizeof(stat_buf1), "FREQ: 1090MHz RF | RANGE: %d KM | %s",
             s_max_range_km, (s_net_status == 1) ? "FEED: ONLINE" : "FEED: WAITING");
    draw_text_5x7(px + 12, 31, stat_buf1, (s_net_status == 1) ? COL_TEXT_GREEN : COL_TEXT_AMBER);

    char stat_buf2[64];
    snprintf(stat_buf2, sizeof(stat_buf2), "SWEEP: %03d DEG | AUDIO: [%s] | ACTIVE TRACKS: %d",
             s_sweep_deg, s_audio_enabled ? "ON" : "OFF", s_live_count);
    draw_text_5x7(px + 12, 45, stat_buf2, COL_TEXT_AMBER);

    // Table Header
    fill_rect(px + 6, 68, 312, 14, 0xFF1E293B);
    draw_text_5x7(px + 8, 72, "#  ICAO   CALLSIGN  BRG DIST   ALT  V/S  SPD SQWK", COL_TEXT_CYAN);

    // Table Rows (top 7 nearest targets)
    int row_y = 86;
    for (int i = 0; i < MAX_TABLE_ROWS; i++) {
        // Alternating row background
        if (i % 2 == 1) {
            fill_rect(px + 6, row_y - 2, 312, 13, 0xFF141C24);
        }

        if (i < MAX_TARGETS && s_tracks[i].active) {
            uint32_t row_col = COL_TEXT_WHITE;
            if (s_tracks[i].squawk == 7700 || s_tracks[i].squawk == 7500) {
                row_col = COL_TEXT_ALERT;
            } else if (s_tracks[i].squawk == 7600) {
                row_col = COL_TEXT_AMBER;
            }

            char row_buf[80];
            snprintf(row_buf, sizeof(row_buf), "%02d %-6s %-8s  %03d %3dK %5dM %+3d %4dK %04d",
                     i + 1,
                     s_tracks[i].hex,
                     s_tracks[i].callsign,
                     s_tracks[i].bearing_deg,
                     s_tracks[i].dist_km,
                     s_tracks[i].alt_m,
                     s_tracks[i].vspeed_ms,
                     s_tracks[i].speed_kmh,
                     s_tracks[i].squawk);

            draw_text_5x7(px + 8, row_y, row_buf, row_col);
        } else {
            // Empty slot placeholder
            draw_text_5x7(px + 8, row_y, "-- ------ --------   ---  ---K  -----M ---  ----K ----", COL_TEXT_MUTED);
        }
        row_y += 14;
    }

    // Telemetry & Demodulator Diagnostics Box
    fill_rect(px + 6, 190, 312, 90, 0xFF0A0F14);
    for (int c = px + 6; c < px + 318; c++) {
        put_pixel(c, 190, COL_PANEL_BORDER);
        put_pixel(c, 280, COL_PANEL_BORDER);
    }
    for (int r = 190; r < 280; r++) {
        put_pixel(px + 6, r, COL_PANEL_BORDER);
        put_pixel(px + 318, r, COL_PANEL_BORDER);
    }

    draw_text_5x7(px + 12, 195, "[ MODE-S DEMODULATOR & OPSEC TELEMETRY ]", COL_TEXT_GREEN);
    char d1[64], d5[64];
    snprintf(d1, sizeof(d1), "  STATION: %s (WGS-84 EQUIRECTANGULAR)", s_sector_name);
    draw_text_5x7(px + 12, 209, d1, COL_TEXT_CYAN);
    draw_text_5x7(px + 12, 221, "  TOR SOCKS5 ATYP=0x03: ZERO-LEAK REMOTE DNS", COL_TEXT_GREEN);
    draw_text_5x7(px + 12, 233, "  IN-KERNEL TCP DESYNC & ZERO-DNS (0 UDP 53)", COL_TEXT_GREEN);
    draw_text_5x7(px + 12, 245, "  AIR-GAP PULSE LEASE: 1090MHz RF PASSIVE", COL_TEXT_AMBER);
    if (s_net_status == 1 && s_live_count > 0) {
        snprintf(d5, sizeof(d5), "  TRACK MATRIX: %d CONTACTS ACQUIRED", s_live_count);
        draw_text_5x7(px + 12, 257, d5, COL_TEXT_WHITE);
    } else if (s_net_status == 1 && s_live_count == 0) {
        draw_text_5x7(px + 12, 257, "  STATION ACQUIRED | ZERO AIRCRAFT IN LOCAL RANGE", COL_TEXT_AMBER);
    } else if (s_net_status == -1) {
        draw_text_5x7(px + 12, 257, "  [!] GATEWAY OFFLINE (PORT 8080) | RETRYING...", COL_TEXT_ALERT);
    } else {
        draw_text_5x7(px + 12, 257, "  CONNECTING TO TOR OPSEC GATEWAY...", COL_TEXT_AMBER);
    }
    draw_text_5x7(px + 12, 269, "  SYSTEM POSTURE: PASSIVE SURVEILLANCE RUNNING", COL_TEXT_MUTED);

    // Interactive Controls Bar at Bottom
    fill_rect(px + 6, 286, 312, 42, 0xFF141E28);
    draw_text_5x7(px + 12, 292, "SURVEILLANCE TERMINAL CONTROLS:", COL_TEXT_WHITE);
    draw_text_5x7(px + 12, 303, "[SPACE] Audio | [R] Range (50-400KM) | [L] Poll Now", COL_TEXT_CYAN);
    draw_text_5x7(px + 12, 314, "[S] Station/Zip | [Q/ESC] Wipe Memory & Quit", COL_TEXT_GREEN);
}

int main(int argc, char* argv[]) {
    // 1. Seld-Pledge Sandboxing (OpSec Capability Sandbox)
    // Note: PLEDGE_NET & PLEDGE_PURGE granted for Tor Gateway lease; PLEDGE_DNS explicitly omitted!
    seld_pledge(PLEDGE_STDIO | PLEDGE_AUDIO | PLEDGE_NET | PLEDGE_PURGE);

    int auto_live = 1;
    if (argc > 1) {
        if (strcmp(argv[1], "sim") == 0 || strcmp(argv[1], "offline") == 0) {
            auto_live = 0;
        } else if (strcmp(argv[1], "live") == 0) {
            auto_live = 1;
            if (argc > 2) {
                strncpy(s_user_sector_buf, argv[2], sizeof(s_user_sector_buf) - 1);
                s_current_sector = s_user_sector_buf;
            }
        } else {
            strncpy(s_user_sector_buf, argv[1], sizeof(s_user_sector_buf) - 1);
            s_current_sector = s_user_sector_buf;
        }
    }

    // 2. Initialize Framebuffer & Backbuffer
    if (seld_get_framebuffer(&s_fb) < 0 || !s_fb.framebuffer) {
        printf("[-] Error: Unable to map linear framebuffer 680x334\n");
        return 1;
    }

    s_backbuf = (uint32_t*)malloc(SCR_W * SCR_H * sizeof(uint32_t));
    if (!s_backbuf) {
        printf("[-] Error: Out of memory allocating radar backbuffer\n");
        return 2;
    }

    // Initialize tactical targets (default autonomous simulation)
    init_tactical_tracks();

    // If live mode enabled, probe OpSec gateway for live transponders
    if (auto_live) {
        int res = fetch_live_tracks();
        if (res > 0) {
            s_live_mode = 1;
        }
    }

    int running = 1;

    // Main Tactical Loop
    while (running) {
        // 1. Update sweep angle (~24 RPM: ~2 degrees per frame at 30 FPS)
        s_sweep_deg = (s_sweep_deg + 2) % 360;

        // 2. Periodic Live Tor Poll every ~120 frames (~4 seconds)
        if (auto_live && (s_frame_count - s_last_fetch_frame) >= 120) {
            s_last_fetch_frame = s_frame_count;
            fetch_live_tracks();
        }

        // 3. Update Kinematics & Intersections
        update_tracks();

        // 4. Render Tactical Displays into Offscreen Backbuffer
        render_scope();
        render_telemetry_panel();

        // 5. Blit Backbuffer to Physical Framebuffer (Zero-Tearing)
        memcpy(s_fb.framebuffer, s_backbuf, SCR_W * SCR_H * sizeof(uint32_t));

        // 6. Poll User Input
        struct seld_kbd_event kev;
        while (seld_poll_key(&kev) > 0) {
            if (!kev.pressed) continue;
            uint8_t sc = kev.scancode;

            if (sc == 0x01 || sc == 0x10) { // ESC or 'Q'
                running = 0;
                break;
            } else if (sc == 0x39 || sc == 0x32) { // Space or 'M'
                s_audio_enabled = !s_audio_enabled;
                if (s_audio_enabled) seld_beep(1760, 40);
            } else if (sc == 0x13) { // 'R' - Cycle Range (50/100/200/400K)
                if (s_max_range_km == 50) s_max_range_km = 100;
                else if (s_max_range_km == 100) s_max_range_km = 150;
                else if (s_max_range_km == 150) s_max_range_km = 200;
                else if (s_max_range_km == 200) s_max_range_km = 400;
                else s_max_range_km = 50;
                if (s_audio_enabled) seld_beep(880, 25);
            } else if (sc == 0x26) { // 'L' - Poll / Refresh Live Tor Feed
                int n = fetch_live_tracks();
                if (n > 0) {
                    s_live_mode = 1;
                    if (s_audio_enabled) seld_beep(1760, 40);
                } else {
                    if (s_audio_enabled) seld_beep(440, 60);
                }
            } else if (sc == 0x1F) { // 'S' - Cycle Sector / Postal Station
                int total_sec = NUM_SECTORS + (s_user_sector_buf[0] ? 1 : 0);
                s_sector_idx = (s_sector_idx + 1) % total_sec;
                if (s_user_sector_buf[0] && s_sector_idx == NUM_SECTORS) {
                    s_current_sector = s_user_sector_buf;
                } else {
                    s_current_sector = s_sectors[s_sector_idx % NUM_SECTORS];
                }
                int n = fetch_live_tracks();
                if (n > 0) {
                    s_live_mode = 1;
                    if (s_audio_enabled) seld_beep(1320, 30);
                }
            }
        }

        // Target Frame Rate ~30-40 FPS (25ms sleep)
        seld_sleep(25);
    }

    // 7. OpSec Anti-Forensics Memory Sanitization (Zero-on-Exit)
    memset(s_tracks, 0, sizeof(s_tracks));
    memset(s_user_sector_buf, 0, sizeof(s_user_sector_buf));
    memset(s_sector_name, 0, sizeof(s_sector_name));
    s_current_sector = NULL;
    memset(s_backbuf, 0, SCR_W * SCR_H * sizeof(uint32_t));
    free(s_backbuf);
    s_backbuf = NULL;

    // Restore Terminal & Clear Video Memory
    seld_clear();
    seld_set_console_rows(20);

    return 0;
}
