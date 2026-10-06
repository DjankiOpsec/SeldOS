/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * Fast Sovereign HTML Layout & Flow Engine
 * GPLv3 Licensed.
 */

#ifndef SELD_TOR_HTML_H
#define SELD_TOR_HTML_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PAGE_LINKS 128

struct html_link {
    int  x;
    int  y;
    int  w;
    int  h;
    char href[512];
};

struct html_doc {
    char title[64];
    int  total_height;
    int  link_count;
    struct html_link links[MAX_PAGE_LINKS];
};

/*
 * Parses raw HTML string and renders document flow into backbuffer.
 * viewport_x, viewport_y, viewport_w, viewport_h: viewport clipping rectangle.
 * scroll_y: vertical scroll offset in document pixels.
 * Returns populated html_doc structure containing page title and clickable link hitboxes.
 */
void html_render(const char* html, uint32_t* backbuf, int screen_w, int screen_h,
                 int vp_x, int vp_y, int vp_w, int vp_h, int scroll_y,
                 struct html_doc* doc_out);

#endif /* SELD_TOR_HTML_H */
