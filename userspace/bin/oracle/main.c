/*
 * SeldOS - Humboldt Kernel Project
 * SNL Userland Utility: oracle - TempleOS-style Opsec Oracle
 * C99 Ring 3 Implementation for SeldOS Native Layer (SNL)
 *
 * Features:
 * - Sacred Requiem / Solemn Ethereal Aesthetic (Deep tragic weight, bittersweet sadness)
 * - Pure Minor Requiem Scale [220, 349] Hz:
 *     A3 (220 Hz) -> C4 (262 Hz) -> D4 (294 Hz) -> E4 (330 Hz) -> F4 (349 Hz)
 * - Completely uniform, steady floating pace (NO abrupt slowing down or speeding up)
 * - 100% continuous tone legato via direct audio_play_tone (zero dead silence gaps)
 * - Natural paragraph text flow (spaces between words, auto 70-col wrap)
 * - Default execution without arguments plays ~1 minute of music (160 words)
 * - Syntax: oracle [X]
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

#define ORACLE_MIN_FREQ  215
#define ORACLE_MAX_FREQ  350
#define ORACLE_MAX_COLS  70
#define ORACLE_PACE_MS   330

/*
 * Solemn Requiem Minor Scale [220, 349] Hz:
 * - 0: 220 Hz (A3 - Heavy dark root ground)
 * - 1: 262 Hz (C4 - Pure poignant minor third - eradicates 'весело')
 * - 2: 294 Hz (D4 - Solemn subdominant)
 * - 3: 330 Hz (E4 - Resonant fifth)
 * - 4: 349 Hz (F4 - Weeping minor sixth - tragic bittersweet peak)
 */
static const uint32_t s_scale[5] = {
    220, 262, 294, 330, 349
};

/* Melodic voice leading transition matrix (classical Gregorian motion, guarantees deg != prev_deg) */
static const uint8_t s_transitions[5][4] = {
    { 1, 2, 3, 1 }, /* From 0 (A3): rise to poignant 3rd (1), 4th (2), or 5th (3) */
    { 0, 2, 3, 0 }, /* From 1 (C4): fall to dark root (0), or step to 2, 3 */
    { 1, 3, 4, 0 }, /* From 2 (D4): step to 1, 3, weep at 6th (4), or fall to root */
    { 2, 4, 1, 0 }, /* From 3 (E4): step to 2, weep at 6th (4), or drop to 1, 0 */
    { 3, 2, 1, 3 }  /* From 4 (F4): sorrowful resolution downwards to 3, 2, 1 */
};

static int s_prev_deg = 0;
static uint64_t s_music_rng = 0;
static int s_line_col = 0;

/* Fallback Sacred Canon (if /oracle.txt is not on SeldFS) */
static const char* s_fallback_words[] = {
    "The", "sovereign", "mind", "requires", "defense", "in", "depth", "always.",
    "Never", "trust", "unauthenticated", "nodes,", "and", "verify", "all", "signatures.",
    "Physical", "memory", "isolation,", "ephemeral", "routing,", "and", "zero-trust", "boundaries",
    "form", "the", "sacred", "sanctuary", "of", "true", "digital", "liberty."
};
#define FALLBACK_WORD_COUNT (sizeof(s_fallback_words) / sizeof(s_fallback_words[0]))

static inline void start_tone(uint32_t freq_hz) {
    seld_syscall(SYS_BEEP, (long)freq_hz, 0, 0);
}

static inline void stop_tone(void) {
    seld_syscall(SYS_BEEP, 0, 0, 0);
}

static void utter_word(const char* word) {
    if (!word || !word[0]) return;

    size_t len = strlen(word);

    /* 1. Word entropy & PRNG evolution */
    uint32_t whash = 5381;
    int has_period = 0;
    int has_comma = 0;

    for (size_t i = 0; i < len; i++) {
        char c = word[i];
        whash = ((whash << 5) + whash) + (uint8_t)c;
        if (c == '.' || c == '!' || c == '?') has_period = 1;
        if (c == ',' || c == ';' || c == ':') has_comma = 1;
    }

    s_music_rng = s_music_rng * 6364136223846793005ULL + 1442695040888963407ULL + whash;

    /* 2. Procedural note synthesis & voice leading */
    int deg;
    uint32_t dur_ms = ORACLE_PACE_MS;

    if (has_period) {
        deg = 0;      /* Dark root resolution on A3 (220 Hz) */
        dur_ms = 370;
    } else if (has_comma) {
        deg = (s_prev_deg == 1) ? 3 : 1; /* Melancholic suspension on C4 or E4 */
        dur_ms = 345;
    } else {
        uint8_t choice = (uint8_t)((s_music_rng >> 24) % 4);
        deg = s_transitions[s_prev_deg][choice];
    }

    /* Invariant: strictly no consecutive duplicate pitch */
    if (deg == s_prev_deg) {
        deg = (deg == 0) ? 1 : 0;
    }
    s_prev_deg = deg;

    uint32_t freq = s_scale[deg];
    if (freq < ORACLE_MIN_FREQ) freq = ORACLE_MIN_FREQ;
    if (freq > ORACLE_MAX_FREQ) freq = ORACLE_MAX_FREQ;

    /* 3. Start continuous tone at the EXACT instant the word appears */
    start_tone(freq);

    /* 4. Natural text flow with column wrap */
    if (s_line_col + len + 1 > ORACLE_MAX_COLS) {
        printf("\n");
        s_line_col = 0;
    }
    printf("%s ", word);
    fflush(stdout);
    s_line_col += len + 1;

    /* 5. Fluid legato duration synchronized with word */
    seld_sleep(dur_ms);
}

static char s_io_buf[512];
static int s_io_pos = 0;
static int s_io_len = 0;

static inline int read_char(int fd) {
    if (s_io_pos >= s_io_len) {
        s_io_len = read(fd, s_io_buf, sizeof(s_io_buf));
        s_io_pos = 0;
        if (s_io_len <= 0) return -1;
    }
    return (unsigned char)s_io_buf[s_io_pos++];
}

static void speak_from_file(int target_words, uint64_t seed) {
    s_music_rng = seed;
    s_prev_deg = (int)(seed % 5);
    s_io_pos = 0;
    s_io_len = 0;

    int fd = open("/oracle.txt", 0);
    if (fd < 0) {
        fd = open("oracle.txt", 0);
    }

    if (fd < 0) {
        size_t start_idx = (size_t)(seed % FALLBACK_WORD_COUNT);
        for (int i = 0; i < target_words; i++) {
            size_t idx = (start_idx + (size_t)i) % FALLBACK_WORD_COUNT;
            utter_word(s_fallback_words[idx]);
        }
        return;
    }

    struct seld_stat st;
    uint32_t fsize = 2000000;
    if (stat("/oracle.txt", &st) == 0 && st.size > 2048) {
        fsize = st.size;
    } else if (stat("oracle.txt", &st) == 0 && st.size > 2048) {
        fsize = st.size;
    }

    /* Keep safe seek margin from end of corpus */
    long max_seek = (long)(fsize > 100000 ? (fsize - 50000) : (fsize / 2));
    long seek_target = (long)(seed % (uint64_t)max_seek);
    lseek(fd, seek_target, 0 /* SEEK_SET */);

    /* Skip partial line */
    int ch;
    while ((ch = read_char(fd)) >= 0) {
        if (ch == '\n') break;
    }

    char word_buf[64];
    int buf_pos = 0;
    int words_spoken = 0;

    while (words_spoken < target_words) {
        ch = read_char(fd);
        if (ch < 0) {
            /* If EOF reached before target_words, seamlessly loop to start of corpus */
            lseek(fd, 0, 0 /* SEEK_SET */);
            s_io_pos = 0;
            s_io_len = 0;
            continue;
        }

        char c = (char)ch;
        if (c == '\r') continue;
        if (c == '\n' || c == ' ') {
            if (buf_pos > 0) {
                word_buf[buf_pos] = '\0';
                utter_word(word_buf);
                words_spoken++;
                buf_pos = 0;
            }
        } else {
            if (buf_pos < (int)(sizeof(word_buf) - 1)) {
                word_buf[buf_pos++] = c;
            }
        }
    }

    if (buf_pos > 0 && words_spoken < target_words) {
        word_buf[buf_pos] = '\0';
        utter_word(word_buf);
    }

    stop_tone();
    close(fd);
}

int main(int argc, char* argv[]) {
    int target_words = 160; /* Default ~1 minute of steady music */
    uint64_t seed = seld_uptime();

    if (argc >= 2) {
        int parsed = atoi(argv[1]);
        if (parsed > 0) {
            target_words = parsed;
        } else {
            for (int c = 0; argv[1][c]; c++) {
                seed = (seed * 31) + (uint8_t)argv[1][c];
            }
            if (argc >= 3) {
                int p2 = atoi(argv[2]);
                if (p2 > 0) target_words = p2;
            }
        }
    }

    if (target_words > 1000) target_words = 1000;

    s_line_col = 0;
    printf("\n--- TEMPLE OF OPSEC ORACLE ---\n");
    speak_from_file(target_words, seed);
    printf("\n--- [AMEN] ---\n\n");

    return 0;
}
