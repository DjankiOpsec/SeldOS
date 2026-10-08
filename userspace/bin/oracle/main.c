/*
 * SeldOS - Humboldt Kernel Project
 * SNL Userland Utility: oracle - TempleOS-style Opsec Oracle
 * C99 Ring 3 Implementation for SeldOS Native Layer (SNL)
 *
 * Features:
 * - Sacred Requiem / Celestial Ethereal Aesthetic (Paradise light, poignant sorrow, noble weight)
 * - Pure Sacred Dorian/Aeolian Scale [294, 659] Hz (8 degrees):
 *     D4 (294 Hz) -> E4 (330 Hz) -> F4 (349 Hz) -> A4 (440 Hz) ->
 *     Bb4 (466 Hz) -> C5 (523 Hz) -> D5 (587 Hz) -> E5 (659 Hz)
 * - Rapid seamless continuous pace (~160 ms / 6.25 words/sec) with NO pauses at punctuation
 * - 100% continuous tone legato via direct audio_play_tone (zero dead silence gaps)
 * - Natural paragraph text flow (spaces between words, auto 70-col wrap)
 * - RDTSC high-entropy corpus seek preventing repetitive text openings
 * - Anti-repetition voice leading engine preventing 2-note/3-note cyclic loops
 * - Syntax: oracle [words] [seed]
 * GPLv3 Licensed.
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "seld.h"

#define ORACLE_MIN_FREQ  290
#define ORACLE_MAX_FREQ  680
#define ORACLE_MAX_COLS  70
#define ORACLE_PACE_MS   160

/*
 * Sacred Requiem Dorian/Aeolian Scale [294, 659] Hz (8 degrees):
 * - 0: 294 Hz (D4 - Solemn foundation, noble weight / "тяжесть")
 * - 1: 330 Hz (E4 - Solemn second)
 * - 2: 349 Hz (F4 - Minor third / poignant sorrow / "грусть")
 * - 3: 440 Hz (A4 - Sacred resonant fifth / axis of purity)
 * - 4: 466 Hz (Bb4 - Weeping minor sixth / tears of sorrow)
 * - 5: 523 Hz (C5 - Minor seventh / yearning light)
 * - 6: 587 Hz (D5 - Celestial upper tonic / luminous paradise / "рай")
 * - 7: 659 Hz (E5 - Angelic high ninth / ethereal crystalline peak)
 */
static const uint32_t s_scale[8] = {
    294, 330, 349, 440, 466, 523, 587, 659
};

/* Melodic voice leading transition matrix (classical sacred chant motion) */
static const uint8_t s_transitions[8][4] = {
    { 2, 3, 5, 6 },  /* From 0 (D4): F4, A4, C5, D5 (octave leap) */
    { 0, 2, 3, 5 },  /* From 1 (E4): D4, F4, A4, C5 */
    { 1, 3, 4, 6 },  /* From 2 (F4): E4, A4, Bb4, D5 (celestial leap) */
    { 2, 4, 6, 7 },  /* From 3 (A4): F4, Bb4, D5, E5 (angelic peak) */
    { 2, 3, 5, 6 },  /* From 4 (Bb4): F4, A4, C5, D5 */
    { 3, 4, 6, 7 },  /* From 5 (C5): A4, Bb4, D5, E5 */
    { 2, 3, 5, 7 },  /* From 6 (D5): F4, A4, C5, E5 */
    { 3, 5, 6, 2 }   /* From 7 (E5): A4, C5, D5, F4 */
};

static int s_history[4] = { -1, -1, -1, -1 };
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
    int prev = (s_history[0] >= 0) ? s_history[0] : 3;
    uint8_t choice = (uint8_t)((s_music_rng >> 24) % 4);

    if (has_period) {
        /* Musical cadence resolving to upper tonic (6), sacred fifth (3), or ground (0) */
        static const uint8_t s_cadences_high[4] = { 6, 3, 5, 0 };
        static const uint8_t s_cadences_low[4]  = { 3, 6, 2, 0 };
        const uint8_t* cands = (prev >= 4) ? s_cadences_high : s_cadences_low;
        deg = cands[(s_music_rng >> 16) % 4];
    } else if (has_comma) {
        /* Melancholic suspension on poignant minor intervals */
        static const uint8_t s_suspensions[4] = { 4, 5, 2, 7 };
        deg = s_suspensions[(s_music_rng >> 16) % 4];
    } else {
        deg = s_transitions[prev][choice];
    }

    /* Invariants: strictly no consecutive duplicate pitch, no 2-note alternating trill */
    int attempts = 0;
    while ((deg == s_history[0] || deg == s_history[1] || (deg == s_history[2] && ((s_music_rng >> 8) & 1))) && attempts < 8) {
        deg = (deg + 1) % 8;
        attempts++;
    }
    if (deg == s_history[0] || deg == s_history[1]) {
        deg = (deg + 2) % 8;
    }

    s_history[3] = s_history[2];
    s_history[2] = s_history[1];
    s_history[1] = s_history[0];
    s_history[0] = deg;

    uint32_t freq = s_scale[deg];
    if (freq < ORACLE_MIN_FREQ) freq = ORACLE_MIN_FREQ;
    if (freq > ORACLE_MAX_FREQ) freq = ORACLE_MAX_FREQ;

    /* 3. Start continuous tone at the EXACT instant the word appears */
    start_tone(freq);

    /* 4. Natural text flow with column wrap (spaces between words) */
    if (s_line_col + len + 1 > ORACLE_MAX_COLS) {
        printf("\n");
        s_line_col = 0;
    }
    printf("%s ", word);
    fflush(stdout);
    s_line_col += len + 1;

    /* 5. 100% continuous legato duration without extra delays on punctuation */
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
    for (int i = 0; i < 4; i++) s_history[i] = -1;
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

static inline uint64_t rdtsc_seed(void) {
#if defined(__riscv)
    uint64_t val;
    __asm__ volatile ("rdtime %0" : "=r"(val));
    return val;
#else
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

int main(int argc, char* argv[]) {
    int target_words = 160; /* Default fast steady chant */
    uint64_t seed = rdtsc_seed() ^ ((uint64_t)seld_uptime() << 24);
    seed ^= (seed >> 13);
    seed *= 0xbf58476d1ce4e5b9ULL;
    seed ^= (seed >> 27);

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
