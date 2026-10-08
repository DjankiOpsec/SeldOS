/*
 * SeldOS - Humboldt Kernel Project
 * SNL Userland Utility: fetch (/bin/fetch)
 * Sovereign System Information & Mascot Display (GPLv3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <seld.h>
#include "banner_logo.h"

static void draw_banner_vector_logo(struct seld_fb_info* fb_info) {
    if (!fb_info || !fb_info->framebuffer || fb_info->bpp != 32) return;
    uint32_t* fb = (uint32_t*)fb_info->framebuffer;
    uint32_t pitch_p = fb_info->pitch / 4;
    static const uint32_t palette39[39] = {
        0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
        0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
        0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
        0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
        0x0018181A, 0x002A2D34, 0x00F2F4F8, 0x00E5D9C5,
        0x00FF6B8B, 0x00E84A5F, 0x002C3539, 0x00848B98,
        0x003D3635, 0x009E2A2B, 0x00E0533C, 0x00001F3F,
        0x00005B96, 0x00018E9A, 0x006497B1, 0x000B6623,
        0x002E8B57, 0x0040E0D0, 0x00B0E0E6, 0x00D27D2D,
        0x008B4513, 0x00C2B280, 0x005C6B73
    };
    int start_x = 44;
    int start_y = 18;
    for (int y = 0; y < BANNER_LOGO_H; y++) {
        int py = start_y + y;
        if (py >= (int)fb_info->height) break;
        for (int x = 0; x < BANNER_LOGO_W; x++) {
            int px = start_x + x;
            if (px >= (int)fb_info->width) break;
            uint8_t idx = s_banner_logo[y][x];
            if (idx != 0xFF && idx < 39) {
                fb[py * pitch_p + px] = palette39[idx];
            }
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    seld_clear();
    printf("=======================================================================\n");
    printf("                          SNL (Seld Not Linux) Sovereign Shell v0.1\n");
    printf("                          Ring 3 Sovereign CLI Environment (GPLv3)\n");
    printf("                          Humboldt Framebuffer 680x334 | 39-Color Palette\n");
#if defined(__riscv)
    printf("                          RISC-V 64-bit (RV64GC) Isolated Execution\n");
#else
    printf("                          x86_64 Long Mode Isolated Execution\n");
#endif
    printf("\n");
    printf("\n");
    printf("\n");
    printf("=======================================================================\n");

    struct seld_fb_info fb;
    if (seld_get_framebuffer(&fb) == 0) {
        draw_banner_vector_logo(&fb);
    }
    return 0;
}
