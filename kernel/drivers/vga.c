#include "vga.h"
#include "io.h"
#include "serial.h"
#include "vmm.h"
#include "string.h"
#include "font8x16.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define FB_COLS 85
#define FB_ROWS 20
#define VGA_MEMORY ((volatile uint16_t*)(0xFFFF800000000000ULL + 0xB8000))

// Bochs Graphics Adapter (BGA) port definitions
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF
#define VBE_DISPI_INDEX_ID     0x0
#define VBE_DISPI_INDEX_XRES   0x1
#define VBE_DISPI_INDEX_YRES   0x2
#define VBE_DISPI_INDEX_BPP    0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40

static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_color = 0x07;
static uint8_t s_fg_color = 15;
static uint8_t s_bg_color = 0;

static struct fb_info primary_fb = {0};
static uint32_t* fb_base = NULL;
static int fb_console_active = 0;
static size_t fb_console_rows = FB_ROWS;

void vga_set_console_rows(size_t rows) {
    if (rows == 0 || rows > FB_ROWS) {
        rows = FB_ROWS;
    }
    fb_console_rows = rows;
    if (vga_row >= fb_console_rows) {
        vga_row = fb_console_rows - 1;
    }
}

size_t vga_get_console_rows(void) {
    return fb_console_rows;
}

/* The Humboldt Penguin 39-Color Palette (Core Temperature: 39.0°C) */
static const uint32_t humboldt_palette[HUMBOLDT_PALETTE_SIZE] = {
    // 0..15: Classic VGA Compatibility
    0x00000000, // 0: Black (Spheniscus Tuxedo)
    0x000000AA, // 1: Blue (Pacific Deep)
    0x0000AA00, // 2: Green (Coastal Algae)
    0x0000AAAA, // 3: Cyan (Upwelling Current)
    0x00AA0000, // 4: Red (Colony Boundary)
    0x00AA00AA, // 5: Magenta (Dusk Plumage)
    0x00AA5500, // 6: Brown (Desert Rock)
    0x00AAAAAA, // 7: Light Grey (Fog Marine)
    0x00555555, // 8: Dark Grey (Molt Charcoal)
    0x005555FF, // 9: Light Blue (Humboldt Wave)
    0x0055FF55, // 10: Light Green (Plankton Bloom)
    0x0055FFFF, // 11: Light Cyan (Glacial Melt)
    0x00FF5555, // 12: Light Red (Penguin Fleshy Patch)
    0x00FF55FF, // 13: Light Magenta (Coral Rose)
    0x00FFFF55, // 14: Yellow (Beak Streak)
    0x00FFFFFF, // 15: White (Antarctic Snow)

    // 16..26: Humboldt Penguin Anatomy & Thermal Biology (39°C Core)
    0x0018181A, // 16: Penguin Tuxedo Black
    0x002A2D34, // 17: Slate Back Plumage
    0x00F2F4F8, // 18: Chest White
    0x00E5D9C5, // 19: Cream Belly
    0x00FF6B8B, // 20: Fleshy Pink (Beak/Eye Skin)
    0x00E84A5F, // 21: Beak Coral
    0x002C3539, // 22: Bill Obsidian
    0x00848B98, // 23: Feather Silver
    0x003D3635, // 24: Webbed Foot Charcoal
    0x009E2A2B, // 25: Reddish-Brown Iris
    0x00E0533C, // 26: 39.0°C Core Metabolic Crimson

    // 27..34: Humboldt Marine Ecosystem & Pacific Ocean Currents
    0x00001F3F, // 27: Humboldt Deep Navy
    0x00005B96, // 28: Pacific Pelagic Blue
    0x00018E9A, // 29: Cold Upwelling Teal
    0x006497B1, // 30: Antarctic Coastal Mist
    0x000B6623, // 31: Kelp Canopy Green
    0x002E8B57, // 32: Intertidal Seaweed
    0x0040E0D0, // 33: Turquoise Ocean
    0x00B0E0E6, // 34: Breaker Foam Crest

    // 35..38: Chilean Colony & Reserve Coordinates
    0x00D27D2D, // 35: Atacama Desert Ochre
    0x008B4513, // 36: Chañaral Island Cliff (29°01' S)
    0x00C2B280, // 37: Isla Damas Shore Sand (29°14' S)
    0x005C6B73  // 38: Algarrobo Colony Nesting Border (33.4° S / 334)
};

static const uint32_t* vga_to_rgb32 = humboldt_palette;

const uint32_t* vga_get_humboldt_palette(void) {
    return humboldt_palette;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | ((uint16_t) color << 8);
}

static void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    s_fg_color = fg % HUMBOLDT_PALETTE_SIZE;
    s_bg_color = bg % HUMBOLDT_PALETTE_SIZE;
    vga_color = (fg & 0x0F) | ((bg & 0x0F) << 4);
}

void vga_set_humboldt_color(uint8_t fg, uint8_t bg) {
    s_fg_color = fg % HUMBOLDT_PALETTE_SIZE;
    s_bg_color = bg % HUMBOLDT_PALETTE_SIZE;
    vga_color = (fg & 0x0F) | ((bg & 0x0F) << 4);
}

static void fb_draw_char(size_t col, size_t row, char c, uint8_t color) {
    if (!fb_base || col >= FB_COLS || row >= fb_console_rows) return;

    uint32_t fg = humboldt_palette[s_fg_color % HUMBOLDT_PALETTE_SIZE];
    uint32_t bg = humboldt_palette[s_bg_color % HUMBOLDT_PALETTE_SIZE];
    if (color != vga_color) {
        fg = humboldt_palette[(color & 0x0F)];
        bg = humboldt_palette[((color >> 4) & 0x0F)];
    }

    uint32_t pitch_pixels = primary_fb.pitch / 4;
    if (pitch_pixels == 0) pitch_pixels = primary_fb.width;

    size_t x_start = col * 8;
    size_t y_start = row * 16;

    const uint8_t* glyph = font8x16[(uint8_t)c];
    for (size_t r = 0; r < 16; r++) {
        uint8_t row_bits = glyph[r];
        uint32_t* dst = &fb_base[(y_start + r) * pitch_pixels + x_start];
        for (size_t b = 0; b < 8; b++) {
            dst[b] = (row_bits & (0x80 >> b)) ? fg : bg;
        }
    }
}

static void fb_scroll(void) {
    if (!fb_base) return;
    uint32_t pitch_pixels = primary_fb.pitch / 4;
    if (pitch_pixels == 0) pitch_pixels = primary_fb.width;

    size_t rows = fb_console_rows;
    if (rows <= 1) return;

    size_t scanlines_to_copy = (rows - 1) * 16;
    size_t scanlines_to_clear = 16;

    memmove(fb_base, fb_base + 16 * pitch_pixels, scanlines_to_copy * pitch_pixels * sizeof(uint32_t));

    uint32_t bg = humboldt_palette[s_bg_color % HUMBOLDT_PALETTE_SIZE];
    uint32_t* last_row_start = fb_base + scanlines_to_copy * pitch_pixels;
    for (size_t i = 0; i < scanlines_to_clear * pitch_pixels; i++) {
        last_row_start[i] = bg;
    }
}

static void fb_clear_screen(void) {
    if (!fb_base) return;
    uint32_t pitch_pixels = primary_fb.pitch / 4;
    if (pitch_pixels == 0) pitch_pixels = primary_fb.width;
    uint32_t bg = humboldt_palette[s_bg_color % HUMBOLDT_PALETTE_SIZE];

    size_t scanlines = primary_fb.height ? primary_fb.height : (FB_ROWS * 16);
    for (size_t y = 0; y < scanlines; y++) {
        for (size_t x = 0; x < primary_fb.width; x++) {
            fb_base[y * pitch_pixels + x] = bg;
        }
    }
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_col = 0;
    update_cursor(0, 0);
    if (fb_console_active && fb_base) {
        fb_clear_screen();
    }
}

void vga_enable_fb_console(void) {
    // Check and program Bochs Graphics Adapter (QEMU / Bochs standard display)
    // to strictly enforce Humboldt Anatomical Scale: 680x334x32
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    uint16_t bga_id = inw(VBE_DISPI_IOPORT_DATA);
    if (bga_id >= 0xB0C0 && bga_id <= 0xB0C6) {
        outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
        outw(VBE_DISPI_IOPORT_DATA, VBE_DISPI_DISABLED);
        outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_XRES);
        outw(VBE_DISPI_IOPORT_DATA, 680);
        outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
        outw(VBE_DISPI_IOPORT_DATA, 334);
        outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_BPP);
        outw(VBE_DISPI_IOPORT_DATA, 32);
        outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
        outw(VBE_DISPI_IOPORT_DATA, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
        primary_fb.width = 680;
        primary_fb.height = 334;
        primary_fb.pitch = 680 * 4;
        primary_fb.bpp = 32;
        if (primary_fb.phys_addr == 0) {
            primary_fb.phys_addr = 0xFD000000;
        }
        serial_puts("[+] Bochs/QEMU BGA configured: 680x334x32 (Humboldt Anatomical Scale)\n");
    }

    if (primary_fb.phys_addr != 0 && primary_fb.bpp == 32) {
        fb_base = (uint32_t*)phys_to_virt(primary_fb.phys_addr);
        fb_console_active = 1;
        fb_clear_screen();
        vga_row = 0;
        vga_col = 0;
        serial_puts("[+] Graphical Framebuffer Console online (680x334x32, 85x20 text, 39 Humboldt colors)\n");
    }
}

void vga_init(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

static void vga_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row >= (fb_console_active ? fb_console_rows : VGA_HEIGHT)) {
            if (fb_console_active) {
                fb_scroll();
                vga_row = fb_console_rows - 1;
            } else {
                vga_scroll();
                vga_row = VGA_HEIGHT - 1;
            }
        }
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            if (!fb_console_active) {
                VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
            } else {
                fb_draw_char(vga_col, vga_row, ' ', vga_color);
            }
        }
    } else if (c == '\t') {
        size_t next_tab = (vga_col + 4) & ~3;
        size_t max_cols = fb_console_active ? FB_COLS : VGA_WIDTH;
        while (vga_col < next_tab && vga_col < max_cols) {
            if (!fb_console_active) {
                VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
            } else {
                fb_draw_char(vga_col, vga_row, ' ', vga_color);
            }
            vga_col++;
        }
        if (vga_col >= max_cols) {
            vga_col = 0;
            if (++vga_row >= (fb_console_active ? fb_console_rows : VGA_HEIGHT)) {
                if (fb_console_active) {
                    fb_scroll();
                    vga_row = fb_console_rows - 1;
                } else {
                    vga_scroll();
                    vga_row = VGA_HEIGHT - 1;
                }
            }
        }
    } else {
        size_t max_cols = fb_console_active ? FB_COLS : VGA_WIDTH;
        if (!fb_console_active) {
            VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color);
        } else {
            fb_draw_char(vga_col, vga_row, c, vga_color);
        }
        if (++vga_col >= max_cols) {
            vga_col = 0;
            if (++vga_row >= (fb_console_active ? fb_console_rows : VGA_HEIGHT)) {
                if (fb_console_active) {
                    fb_scroll();
                    vga_row = fb_console_rows - 1;
                } else {
                    vga_scroll();
                    vga_row = VGA_HEIGHT - 1;
                }
            }
        }
    }
    if (!fb_console_active) {
        update_cursor(vga_col, vga_row);
    }
}

void vga_puts(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_print_hex(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        vga_putchar(hex_chars[(val >> i) & 0xF]);
    }
}

void vga_print_dec(uint64_t val) {
    if (val == 0) {
        vga_putchar('0');
        return;
    }
    char buf[32];
    int i = 0;
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

struct fb_info* vga_get_fb_info(void) {
    return (primary_fb.phys_addr != 0) ? &primary_fb : NULL;
}

// Multiboot 1 / 2 parsing structures
struct mb2_fb_tag {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
} __attribute__((packed));

struct mb1_fb_info {
    uint32_t flags;
    uint32_t unused[18];
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed));

void vga_init_fb(uint64_t mb_magic, uint64_t mb_info_addr) {
    primary_fb.phys_addr = 0;
    if (mb_info_addr == 0) return;

    if (mb_magic == 0x36d76289) { // Multiboot 2
        uint8_t* tag_ptr = (uint8_t*)(mb_info_addr + 8);
        while (1) {
            uint32_t type = *(uint32_t*)tag_ptr;
            uint32_t size = *(uint32_t*)(tag_ptr + 4);
            if (type == 0) break; // End tag

            if (type == 8) { // Framebuffer info tag (type 8 in Multiboot 2)
                struct mb2_fb_tag* fb = (struct mb2_fb_tag*)tag_ptr;
                primary_fb.phys_addr = fb->framebuffer_addr;
                primary_fb.pitch = fb->framebuffer_pitch;
                primary_fb.width = fb->framebuffer_width;
                primary_fb.height = fb->framebuffer_height;
                primary_fb.bpp = fb->framebuffer_bpp;
                primary_fb.type = fb->framebuffer_type;
                serial_puts("[+] FB Tag found: addr=0x");
                serial_print_hex(primary_fb.phys_addr);
                serial_puts(" w=");
                serial_print_dec(primary_fb.width);
                serial_puts(" h=");
                serial_print_dec(primary_fb.height);
                serial_puts(" pitch=");
                serial_print_dec(primary_fb.pitch);
                serial_puts(" bpp=");
                serial_print_dec(primary_fb.bpp);
                serial_puts("\n");
                break;
            }
            tag_ptr += ((size + 7) & ~7);
        }
    } else if (mb_magic == 0x2badb002) { // Multiboot 1
        uint32_t flags = *(uint32_t*)mb_info_addr;
        if (flags & (1 << 12)) { // Video info valid
            struct mb1_fb_info* mb1 = (struct mb1_fb_info*)mb_info_addr;
            primary_fb.phys_addr = mb1->framebuffer_addr;
            primary_fb.pitch = mb1->framebuffer_pitch;
            primary_fb.width = mb1->framebuffer_width;
            primary_fb.height = mb1->framebuffer_height;
            primary_fb.bpp = mb1->framebuffer_bpp;
            primary_fb.type = mb1->framebuffer_type;
        }
    }
}

