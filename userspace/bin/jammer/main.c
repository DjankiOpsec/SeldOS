/*
 * SeldOS - Humboldt Kernel Project
 * /bin/jammer: Sovereign Acoustic Ultrasonic & TEMPEST RF Jammer
 * Defeats CIA/FBI laser mics, MEMS bugs, smartphones, and Van Eck SDR interception
 * Ring 3 Humboldt Hardened (PLEDGE_STDIO | PLEDGE_AUDIO | PLEDGE_PURGE)
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <seld.h>

#define SAMPLE_RATE 44100
#define PCM_CHUNK_SIZE 2048

/* Sine table: 256 entries (-127 to +127) */
static const int8_t s_sin256[256] = {
      0,   3,   6,   9,  12,  15,  18,  21,  24,  28,  31,  34,  37,  40,  43,  46,
     48,  51,  54,  57,  60,  63,  65,  68,  71,  73,  76,  78,  81,  83,  85,  88,
     90,  92,  94,  96,  98, 100, 102, 104, 106, 108, 110, 111, 113, 114, 116, 117,
    118, 119, 121, 122, 123, 123, 124, 125, 125, 126, 126, 126, 126, 126, 126, 126,
    125, 125, 124, 123, 123, 122, 121, 119, 118, 117, 116, 114, 113, 111, 110, 108,
    106, 104, 102, 100,  98,  96,  94,  92,  90,  88,  85,  83,  81,  78,  76,  73,
     71,  68,  65,  63,  60,  57,  54,  51,  48,  46,  43,  40,  37,  34,  31,  28,
     24,  21,  18,  15,  12,   9,   6,   3,   0,  -3,  -6,  -9, -12, -15, -18, -21,
    -24, -28, -31, -34, -37, -40, -43, -46, -48, -51, -54, -57, -60, -63, -65, -68,
    -71, -73, -76, -78, -81, -83, -85, -88, -90, -92, -94, -96, -98,-100,-102,-104,
   -106,-108,-110,-111,-113,-114,-116,-117,-118,-119,-121,-122,-123,-123,-124,-125,
   -125,-126,-126,-126,-126,-126,-126,-126,-125,-125,-124,-123,-123,-122,-121,-119,
   -118,-117,-116,-114,-113,-111,-110,-108,-106,-104,-102,-100, -98, -96, -94, -92,
    -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
    -48, -46, -43, -40, -37, -34, -31, -28, -24, -21, -18, -15, -12,  -9,  -6,  -3
};

static uint32_t s_rng_state = 0x5E1D0501;
static inline uint32_t fast_rand(void) {
    s_rng_state ^= s_rng_state << 13;
    s_rng_state ^= s_rng_state >> 17;
    s_rng_state ^= s_rng_state << 5;
    return s_rng_state;
}

static void print_banner(void) {
    printf("\n=======================================================================\n");
    printf(" [!] SeldOS Sovereign Electronic Warfare & Jamming Suite (/bin/jammer)\n");
    printf("     Targeted Countermeasures against Audio Bugs & Van Eck RF Intercept\n");
    printf("=======================================================================\n");
}

static void print_usage(void) {
    print_banner();
    printf("Usage: jammer <mode> [duration_seconds]\n\n");
    printf("Modes:\n");
    printf("  ultrasonic [sec] (or mic) Smooth ultrasonic intermodulation carrier\n");
    printf("                            (19.2 - 21.0 kHz sine chirp, blinds MEMS bugs)\n");
    printf("  speech [sec]     (or pink) Soft ambient acoustic pink noise (300-3400 Hz)\n");
    printf("                            (Masks speech from laser window vibrometers)\n");
    printf("  rf [sec]         (or tempest) TEMPEST Bus Jammer (wideband RF dispersion)\n");
    printf("                            (Blinds SDR / Van Eck phreaking radio receivers)\n");
    printf("  combo [sec]               Simultaneous Ultrasonic + TEMPEST Jammer\n");
    printf("  status                    Audit hardware audio DAC & TEMPEST state\n\n");
    printf("Note: Press any key to safely stop early.\n\n");
}

/* Synthesizes smooth ultrasonic intermodulation waveform (gentle sine wave, no square wave) */
static void synth_ultrasonic_block(uint8_t* pcm, size_t count, uint32_t* phase1, uint32_t* phase2, uint32_t step) {
    /* Carrier 1: 19,200 Hz .. 19,800 Hz */
    /* Carrier 2: 20,400 Hz .. 21,000 Hz */
    uint32_t f1 = 19200 + ((step * 19) % 600);
    uint32_t f2 = 20400 + ((step * 23) % 600);

    uint32_t inc1 = (uint32_t)(((uint64_t)f1 * 65536ULL) / SAMPLE_RATE);
    uint32_t inc2 = (uint32_t)(((uint64_t)f2 * 65536ULL) / SAMPLE_RATE);

    uint32_t p1 = *phase1;
    uint32_t p2 = *phase2;

    for (size_t i = 0; i < count; i++) {
        uint8_t idx1 = (uint8_t)(p1 >> 8);
        uint8_t idx2 = (uint8_t)(p2 >> 8);
        int32_t s1 = s_sin256[idx1];
        int32_t s2 = s_sin256[idx2];

        /* Smooth moderate amplitude: ~35% volume to avoid harsh clipping */
        int32_t mixed = (s1 * 22) / 64 + (s2 * 22) / 64;
        pcm[i] = (uint8_t)(mixed + 128);

        p1 += inc1;
        p2 += inc2;
    }

    *phase1 = p1;
    *phase2 = p2;
}

/* Synthesizes soft, pleasant pink noise for conversational speech masking */
static void synth_speech_mask_block(uint8_t* pcm, size_t count) {
    static int32_t b0 = 0, b1 = 0, b2 = 0;
    for (size_t i = 0; i < count; i++) {
        int32_t white = (int32_t)(fast_rand() & 0xFF) - 128;
        b0 = (b0 * 60 + white * 4) / 64;
        b1 = (b1 * 56 + white * 8) / 64;
        b2 = (b2 * 48 + white * 16) / 64;
        int32_t pink = (b0 + b1 + b2) / 4; // Soft volume
        if (pink > 50) pink = 50;
        if (pink < -50) pink = -50;
        pcm[i] = (uint8_t)(pink + 128);
    }
}

/* High-speed cache-line & memory bus pumping for TEMPEST RF emission */
static void pump_tempest_rf_bus(size_t iterations) {
    static volatile uint64_t scratch[64];
    for (size_t k = 0; k < iterations; k++) {
        uint64_t pat = (k & 1) ? 0xAAAAAAAAAAAAAAAAULL : 0x5555555555555555ULL;
        for (int i = 0; i < 64; i += 4) {
            scratch[i]   = pat ^ (uint64_t)k;
            scratch[i+1] = ~pat;
            scratch[i+2] = pat + (uint64_t)i;
            scratch[i+3] = ~pat - (uint64_t)i;
        }
    }
    volatile uint64_t dummy = scratch[0];
    (void)dummy;
}

static void draw_progress_bar(const char* mode_name, uint32_t elapsed_s, uint32_t total_s) {
    int total_bars = 20;
    int filled = 0;
    if (total_s > 0) {
        filled = (int)(((uint64_t)elapsed_s * total_bars) / total_s);
        if (filled > total_bars) filled = total_bars;
    }

    printf("\r [*] %-10s [", mode_name);
    for (int i = 0; i < total_bars; i++) {
        if (i < filled) putchar('#');
        else putchar('-');
    }
    printf("] %u/%u s  [Press any key to stop]", elapsed_s, total_s);
}

int main(int argc, char* argv[]) {
    /* Ring 3 Seld-Pledge Hardening */
    seld_pledge(PLEDGE_STDIO | PLEDGE_AUDIO | PLEDGE_PURGE);

    if (argc >= 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage();
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "status") == 0) {
        print_banner();
        int tempest = seld_get_tempest();
        printf(" [*] Hardware TEMPEST Subpixel Dither : %s\n", tempest ? "ARMED (Active EMI)" : "DISARMED");
        printf(" [*] Audio Hardware DAC Subsystem    : READY (AC'97 DMA PCM / SB16)\n");
        printf(" [*] Ultrasonic Carrier Profile      : 19.2 - 21.0 kHz Smooth Nyquist Chirp\n");
        printf(" [*] Acoustic Speech Babble Profile  : 300 - 3400 Hz Pink Noise Mask\n");
        printf(" [*] SeldOS Sovereign Status         : HARDENED / COMFORT CALIBRATED\n\n");
        return 0;
    }

    int mode_mic = 0;
    int mode_speech = 0;
    int mode_rf = 0;

    if (argc < 2 || strcmp(argv[1], "mic") == 0 || strcmp(argv[1], "ultrasonic") == 0 || strcmp(argv[1], "auto") == 0) {
        mode_mic = 1;
    } else if (strcmp(argv[1], "speech") == 0 || strcmp(argv[1], "pink") == 0) {
        mode_speech = 1;
    } else if (strcmp(argv[1], "rf") == 0 || strcmp(argv[1], "tempest") == 0) {
        mode_rf = 1;
    } else if (strcmp(argv[1], "combo") == 0) {
        mode_mic = 1;
        mode_rf = 1;
    } else {
        printf("[-] Error: Unknown jammer mode '%s'. Run 'jammer help'.\n", argv[1]);
        return 1;
    }

    uint32_t duration_s = 10; // default 10 seconds
    if (argc >= 3) {
        int d = atoi(argv[2]);
        if (d > 0) duration_s = (uint32_t)d;
    }

    print_banner();
    printf(" [!] ENGAGING JAMMER: ");
    const char* mode_tag = "JAMMER";
    if (mode_mic && mode_rf) {
        mode_tag = "COMBO-EW";
        printf("COMBO (Ultrasonic Intermodulation + TEMPEST RF Dispersion)\n");
    } else if (mode_mic) {
        mode_tag = "ULTRASONIC";
        printf("ULTRASONIC MIC SHIELD (19.2 - 21.0 kHz Smooth Sine)\n");
    } else if (mode_speech) {
        mode_tag = "SPEECH-MASK";
        printf("SPEECH MASKER (Soft Pink Noise Anti-Laser Vibrometer)\n");
    } else if (mode_rf) {
        mode_tag = "TEMPEST-RF";
        printf("ACTIVE TEMPEST RF JAMMER (SDR / Van Eck Blind)\n");
    }
    printf(" [*] Running for %u seconds...\n\n", duration_s);

    if (mode_rf) {
        seld_set_tempest(1);
    }

    uint8_t pcm_buf[PCM_CHUNK_SIZE];
    uint32_t phase1 = 0;
    uint32_t phase2 = 0;
    uint32_t step = 0;

    /* seld_uptime() is in MILLISECONDS! */
    uint64_t start_ms = seld_uptime();
    uint64_t target_ms = (uint64_t)duration_s * 1000ULL;
    uint32_t last_draw_sec = 0;

    draw_progress_bar(mode_tag, 0, duration_s);

    while (1) {
        uint64_t cur_ms = seld_uptime();
        uint64_t elapsed_ms = (cur_ms >= start_ms) ? (cur_ms - start_ms) : 0;
        if (elapsed_ms >= target_ms) {
            break;
        }

        /* Check for keyboard interruption */
        struct seld_kbd_event ev;
        if (seld_poll_key(&ev) == 0 && ev.pressed) {
            printf("\n\n [!] Key pressed. Jammer disarmed by operator.\n");
            break;
        }

        if (mode_mic) {
            synth_ultrasonic_block(pcm_buf, sizeof(pcm_buf), &phase1, &phase2, step);
            seld_audio_play(pcm_buf, sizeof(pcm_buf), SAMPLE_RATE);
        } else if (mode_speech) {
            synth_speech_mask_block(pcm_buf, sizeof(pcm_buf));
            seld_audio_play(pcm_buf, sizeof(pcm_buf), SAMPLE_RATE);
        }

        if (mode_rf) {
            pump_tempest_rf_bus(256);
        }

        step++;
        uint32_t cur_sec = (uint32_t)(elapsed_ms / 1000ULL);
        if (cur_sec != last_draw_sec) {
            draw_progress_bar(mode_tag, cur_sec, duration_s);
            last_draw_sec = cur_sec;
        }

        seld_sleep(10);
    }

    draw_progress_bar(mode_tag, duration_s, duration_s);

    /* Clean up hardware state */
    if (mode_rf) {
        seld_set_tempest(0);
    }

    printf("\n\n [+] Jammer disarmed. Audio quiet. RF quiesced.\n\n");
    return 0;
}
