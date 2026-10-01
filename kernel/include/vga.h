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

    // 16..26: Humboldt Penguin Anatomy & Thermal Biology (39°C Metabolic Core)
    COLOR_PENGUIN_TUXEDO = 16, // Deep dorsal black plumage
    COLOR_SLATE_BACK     = 17, // Wet back feather slate
    COLOR_CHEST_WHITE    = 18, // Pure chest plumage
    COLOR_CREAM_BELLY    = 19, // Warm belly down
    COLOR_FLESH_PINK     = 20, // Bare skin around bill and eyes
    COLOR_BEAK_CORAL     = 21, // Rose-coral beak margin
    COLOR_BILL_OBSIDIAN  = 22, // Heavy hooked beak black
    COLOR_FEATHER_SILVER = 23, // Juvenile silver chest band
    COLOR_WEBBED_FOOT    = 24, // Scaly webbed foot charcoal
    COLOR_PENGUIN_IRIS   = 25, // Distinctive reddish-brown iris
    COLOR_THERMAL_CORE   = 26, // 39.0°C Core body metabolic crimson

    // 27..34: Humboldt Marine Ecosystem & Pacific Ocean Currents
    COLOR_HUMBOLDT_NAVY  = 27, // Humboldt Trench abyssal navy
    COLOR_PACIFIC_PELAGIC= 28, // South Pacific ocean blue
    COLOR_UPWELLING_TEAL = 29, // Nutrient-rich subantarctic upwelling teal
    COLOR_ANTARCTIC_MIST = 30, // Camanchaca marine fog mist
    COLOR_KELP_FOREST    = 31, // Macrocystis giant kelp canopy
    COLOR_SEAWEED_GREEN  = 32, // Intertidal seaweed emerald
    COLOR_OCEAN_CYAN     = 33, // Shallow cove turquoise water
    COLOR_FOAM_CREST     = 34, // Wave breaker crest seafoam

    // 35..38: Chilean Breeding Colonies & Reserve Coordinates
    COLOR_ATACAMA_OCHRE  = 35, // Coastal Atacama desert bluffs
    COLOR_CHANARAL_CLIFF = 36, // Chañaral Island sea cliffs (29°01' S)
    COLOR_DAMAS_SHORE    = 37, // Isla Damas nesting sand beach (29°14' S)
    COLOR_ALGARROBO_ROCK = 38, // Algarrobo Colony southern nesting border (33.4° S / 334)
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

#endif
