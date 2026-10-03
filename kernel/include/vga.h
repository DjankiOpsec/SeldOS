#ifndef SELD_VGA_H
#define SELD_VGA_H

#include <stdint.h>
#include <stddef.h>

#define HUMBOLDT_PALETTE_SIZE 39
#define HUMBOLDT_FB_WIDTH     680
#define HUMBOLDT_FB_HEIGHT    334

enum vga_color {
    // 0..15: Standard VGA Compatible Palette
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14,
    VGA_WHITE = 15,

    // 16..26: Monochromatic & High-Contrast Ergonomic UI Tokens
    COLOR_PENGUIN_TUXEDO = 16, // Charcoal Dark (Terminal / Main BG)
    COLOR_SLATE_BACK     = 17, // Slate Grey (Panel & Header BG)
    COLOR_CHEST_WHITE    = 18, // Pure White (High-contrast Primary Text)
    COLOR_CREAM_BELLY    = 19, // Warm Sand (Secondary Text / Muted Labels)
    COLOR_FLESH_PINK     = 20, // Rose Pink (Active Links / Highlights)
    COLOR_BEAK_CORAL     = 21, // Coral Red (Error Indicators)
    COLOR_BILL_OBSIDIAN  = 22, // Obsidian Dark (Frame Borders / Separators)
    COLOR_FEATHER_SILVER = 23, // Silver Metallic (Inactive Elements / Shortcuts)
    COLOR_WEBBED_FOOT    = 24, // Footprint Charcoal (Table Row Inactive)
    COLOR_PENGUIN_IRIS   = 25, // Amber Crimson (Warning Banner / Diagnostic)
    COLOR_THERMAL_CORE   = 26, // Core Alert Red (Kernel Panic / Fault Indicator)

    // 27..34: Extended UI & Oceanic Accent Tokens
    COLOR_HUMBOLDT_NAVY  = 27, // Abyssal Navy (Header Background)
    COLOR_PACIFIC_PELAGIC= 28, // Pelagic Blue (Selected Tabs & Frames)
    COLOR_UPWELLING_TEAL = 29, // Upwelling Teal (TCP Socket / Stream Indicator)
    COLOR_ANTARCTIC_MIST = 30, // Mist Cyan (Network Diagnostic Text)
    COLOR_KELP_FOREST    = 31, // Canopy Green (NIC Link Up / Success)
    COLOR_SEAWEED_GREEN  = 32, // Seaweed Green (SeldFS Mounted Volume)
    COLOR_OCEAN_CYAN     = 33, // Turquoise Ocean (URL / Search Entry Accent)
    COLOR_FOAM_CREST     = 34, // Seafoam White (Scrollbar Thumb & Pointer)

    // 35..38: Coastal Earth & Structural Boundary Tokens
    COLOR_ATACAMA_OCHRE  = 35, // Ochre Gold (Heap / Numerical Metrics)
    COLOR_CHANARAL_CLIFF = 36, // Earth Brown (Binary & Package Inodes)
    COLOR_DAMAS_SHORE    = 37, // Sand Khaki (Text File & Document Inodes)
    COLOR_ALGARROBO_ROCK = 38, // Granite Slate (Bottom Status Guard & Baseline)
};

struct fb_info {
    uint64_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
    uint8_t  type;
    uint16_t reserved;
};

void vga_init(void);
void vga_init_fb(uint64_t mb_magic, uint64_t mb_info_addr);
struct fb_info* vga_get_fb_info(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_humboldt_color(uint8_t fg, uint8_t bg);
const uint32_t* vga_get_humboldt_palette(void);
void vga_putchar(char c);
void vga_puts(const char* str);
void vga_clear(void);
void vga_enable_fb_console(void);
void vga_print_hex(uint64_t val);
void vga_print_dec(uint64_t val);
void vga_set_console_rows(size_t rows);
size_t vga_get_console_rows(void);
void vga_set_scroll_window(size_t top_row, size_t bottom_row);
void vga_draw_pixel(size_t x, size_t y, uint32_t color);
void vga_draw_bitmap(size_t x, size_t y, size_t w, size_t h, const uint8_t* indices, size_t stride);
void vga_draw_string_at(size_t col, size_t row, const char* str, uint8_t color);
void vga_fill_rect(size_t x, size_t y, size_t w, size_t h, uint32_t color);
int vga_driver_disable(void);
int vga_is_gpu_enabled(void);
uint32_t* vga_get_fb_ptr(void);
void vga_emergency_text_write(int col, int row, const char* text, uint8_t color_attr);

#endif
