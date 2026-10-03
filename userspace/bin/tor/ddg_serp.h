/*
 * SeldOS - Humboldt Kernel Project
 * Tor Browser (OpSec Sovereign Edition)
 * DuckDuckGo .onion Sovereign Search Engine & Rich SERP Layout Engine
 * GPLv3 Licensed.
 */

#ifndef SELD_TOR_DDG_SERP_H
#define SELD_TOR_DDG_SERP_H

#include <stdint.h>
#include <stddef.h>
#include "html.h"

#define DDG_ONION_HOST "duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion"
#define DDG_DEFAULT_QUERY "linux"

#define DDG_MAX_RESULTS 8

struct ddg_search_result {
    char title[96];
    char url[128];
    char domain[48];
    char snippet[192];
};

struct ddg_serp_data {
    char query[64];
    int  count;
    struct ddg_search_result results[DDG_MAX_RESULTS];
    char card_title[64];
    char card_text[256];
    char card_source[48];
    char card_url[128];
};

/* Checks if URL targets DuckDuckGo search engine */
int is_ddg_url(const char* url, char* query_out, size_t query_sz);

/* Initializes fallback / default results for a query */
void ddg_serp_init_defaults(struct ddg_serp_data* data, const char* query);

/* Parses the raw SERP packet returned by the DuckDuckGo Onion gateway */
int ddg_serp_parse(const char* packet, struct ddg_serp_data* data);

/* Renders the DuckDuckGo Search Engine Results Page (SERP) */
void ddg_serp_render(const char* query, const struct ddg_serp_data* data,
                     uint32_t* backbuf, int screen_w, int screen_h,
                     int vp_x, int vp_y, int vp_w, int vp_h, int scroll_y,
                     struct html_doc* doc_out);

#endif /* SELD_TOR_DDG_SERP_H */
