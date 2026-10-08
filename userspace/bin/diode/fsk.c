/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign Acoustic Air-Gap FSK (Frequency Shift Keying) Modulator
 * 1200 Hz (Space) / 2200 Hz (Mark) Synchronous Bitstream via PC Speaker / Sound Card
 * GPLv3 Licensed.
 */

#include "fsk.h"
#include "diode_crypto.h"
#include "seld.h"
#include <string.h>

void fsk_build_key_packet(struct fsk_key_packet* pkt, uint32_t session_id, uint32_t key_id, const uint8_t key[16]) {
    if (!pkt) return;
    pkt->sync[0] = FSK_SYNC_BYTE1;
    pkt->sync[1] = FSK_SYNC_BYTE2;
    pkt->pkt_type = FSK_PKT_TYPE_KEY;
    pkt->session_id = session_id;
    pkt->key_id = key_id;
    if (key) {
        memcpy(pkt->master_key, key, 16);
    } else {
        memset(pkt->master_key, 0, 16);
    }

    size_t payload_len = sizeof(struct fsk_key_packet) - sizeof(uint32_t);
    pkt->crc32 = diode_crc32(pkt, payload_len);
}

static inline uint64_t fsk_rdtsc(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#elif defined(__riscv)
    uint64_t cycles;
    __asm__ volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
#else
    return seld_uptime();
#endif
}

static uint64_t fsk_calibrate_cycles_per_ms(void) {
    uint64_t u0 = seld_uptime();
    uint64_t spin = 0;
    while (seld_uptime() == u0 && spin < 50000000ULL) {
        __asm__ volatile ("pause");
        spin++;
    }
    if (spin >= 50000000ULL) {
        return 2500000;
    }

    uint64_t t0 = fsk_rdtsc();
    uint64_t target_up = seld_uptime() + 40;
    spin = 0;
    while (seld_uptime() < target_up && spin < 100000000ULL) {
        __asm__ volatile ("pause");
        spin++;
    }
    if (spin >= 100000000ULL) {
        return 2500000;
    }
    uint64_t t1 = fsk_rdtsc();

    uint64_t per_ms = (t1 - t0) / 40;
    if (per_ms < 100000) {
        per_ms = 2500000;
    }
    return per_ms;
}

void fsk_transmit_key_packet(const struct fsk_key_packet* pkt, uint32_t symbol_ms) {
    if (!pkt) return;
    if (symbol_ms == 0) symbol_ms = FSK_DEFAULT_SYMBOL_MS;

    const uint8_t* raw = (const uint8_t*)pkt;
    size_t len = sizeof(struct fsk_key_packet);

    uint64_t cycles_per_ms = fsk_calibrate_cycles_per_ms();
    uint64_t symbol_cycles = (uint64_t)symbol_ms * cycles_per_ms;

    /* 1. Pilot Tone (1800 Hz) for 350 ms to alert receiver and stabilize AGC */
    seld_beep(FSK_FREQ_PREAMBLE, 0);
    uint64_t next_tsc = fsk_rdtsc() + (350 * cycles_per_ms);
    while (fsk_rdtsc() < next_tsc) {
        __asm__ volatile ("pause");
    }

    /* Short silence between pilot and preamble (20 ms) */
    seld_beep(0, 0);
    next_tsc = fsk_rdtsc() + (20 * cycles_per_ms);
    while (fsk_rdtsc() < next_tsc) {
        __asm__ volatile ("pause");
    }

    /* 2. Preamble bit training sequence: 24 alternating bits 101010... (720 ms)
     * Continuous carrier: frequencies shift seamlessly at symbol boundaries without audio stop clicks */
    next_tsc = fsk_rdtsc();
    for (int i = 0; i < 24; i++) {
        uint32_t freq = (i % 2 == 0) ? FSK_FREQ_MARK : FSK_FREQ_SPACE;
        seld_beep(freq, 0);
        next_tsc += symbol_cycles;
        while (fsk_rdtsc() < next_tsc) {
            __asm__ volatile ("pause");
        }
    }

    /* 3. Synchronous bitstream: MSB first (zero cumulative drift) */
    for (size_t b = 0; b < len; b++) {
        uint8_t byte_val = raw[b];
        for (int bit = 7; bit >= 0; bit--) {
            uint32_t freq = ((byte_val >> bit) & 1) ? FSK_FREQ_MARK : FSK_FREQ_SPACE;
            seld_beep(freq, 0);
            next_tsc += symbol_cycles;
            while (fsk_rdtsc() < next_tsc) {
                __asm__ volatile ("pause");
            }
        }
    }

    /* 4. Postamble trailer tone (1800 Hz) for 200 ms */
    seld_beep(FSK_FREQ_PREAMBLE, 0);
    next_tsc = fsk_rdtsc() + (200 * cycles_per_ms);
    while (fsk_rdtsc() < next_tsc) {
        __asm__ volatile ("pause");
    }

    /* 5. Clean silence & audio shutdown */
    seld_beep(0, 0);
}
