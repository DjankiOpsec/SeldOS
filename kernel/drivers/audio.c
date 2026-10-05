/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign PC Speaker Audio Subsystem Implementation
 * Pure 8254 PIT Timer 2 Square Wave Synthesis & Note Engine
 * GPLv3 Licensed.
 */

#include "audio.h"
#include "io.h"
#include "serial.h"
#include "pit.h"
#include "string.h"

/* PC Speaker Hardware I/O Ports */
#define SPEAKER_PIT_CMD   0x43
#define SPEAKER_PIT_DATA  0x42
#define SPEAKER_PORT_B    0x61
#define PIT_FREQ_BASE     1193182

static struct audio_info s_audio_info = {0};

/*
 * Musical Semitone Note Frequency Table (1..127):
 * Formula: 440.0 / 32.0 * 2^(note / 12.0)
 * note 60 = standard concert pitch A4 (440 Hz)
 * note 72 = A5 (880 Hz)
 */
static const uint16_t s_ona_to_freq[128] = {
        0,    15,    15,    16,    17,    18,    19,    21, // ona 0..7
       22,    23,    24,    26,    28,    29,    31,    33, // ona 8..15
       35,    37,    39,    41,    44,    46,    49,    52, // ona 16..23
       55,    58,    62,    65,    69,    73,    78,    82, // ona 24..31
       87,    92,    98,   104,   110,   117,   123,   131, // ona 32..39
      139,   147,   156,   165,   175,   185,   196,   208, // ona 40..47
      220,   233,   247,   262,   277,   294,   311,   330, // ona 48..55
      349,   370,   392,   415,   440,   466,   494,   523, // ona 56..63
      554,   587,   622,   659,   698,   740,   784,   831, // ona 64..71
      880,   932,   988,  1047,  1109,  1175,  1245,  1319, // ona 72..79
     1397,  1480,  1568,  1661,  1760,  1865,  1976,  2093, // ona 80..87
     2217,  2349,  2489,  2637,  2794,  2960,  3136,  3322, // ona 88..95
     3520,  3729,  3951,  4186,  4435,  4699,  4978,  5274, // ona 96..103
     5588,  5920,  6272,  6645,  7040,  7459,  7902,  8372, // ona 104..111
     8870,  9397,  9956, 10548, 11175, 11840, 12544, 13290, // ona 112..119
    14080, 14917, 15804, 16744, 17740, 18795, 19912, 21096  // ona 120..127
};

uint32_t audio_ona_to_freq(int8_t ona) {
    if (ona <= 0) return 0;
    return (uint32_t)s_ona_to_freq[(uint8_t)ona];
}

int8_t audio_freq_to_ona(uint32_t freq_hz) {
    if (freq_hz == 0) return 0;
    if (freq_hz <= s_ona_to_freq[1]) return 1;
    if (freq_hz >= s_ona_to_freq[127]) return 127;

    int low = 1, high = 127;
    int best_ona = 1;
    uint32_t best_diff = 0xFFFFFFFF;

    while (low <= high) {
        int mid = (low + high) / 2;
        uint32_t mid_f = s_ona_to_freq[mid];
        uint32_t diff = (freq_hz > mid_f) ? (freq_hz - mid_f) : (mid_f - freq_hz);
        if (diff < best_diff) {
            best_diff = diff;
            best_ona = mid;
        }

        if (mid_f == freq_hz) {
            return (int8_t)mid;
        } else if (mid_f < freq_hz) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return (int8_t)best_ona;
}

/*
 * audio_play_tone:
 * Frequency register programming for PIT Channel 2 and Port 0x61.
 */
void audio_play_tone(uint32_t freq_hz) {
    if (freq_hz < 20 || freq_hz > 20000) {
        audio_stop_tone();
        return;
    }

    uint32_t period = PIT_FREQ_BASE / freq_hz;
    // Mode 3 square wave generator requires divisor >= 2. Clamp [2, 65535].
    if (period < 2) period = 2;
    if (period > 65535) period = 65535;

    // Channel 2, lobyte/hibyte, Mode 3 square wave, binary (0xB6)
    outb(SPEAKER_PIT_CMD, 0xB6);
    outb(SPEAKER_PIT_DATA, (uint8_t)(period & 0xFF));
    outb(SPEAKER_PIT_DATA, (uint8_t)((period >> 8) & 0xFF));

    // Turn on Speaker Gate (bit 0) and Speaker Data (bit 1)
    uint8_t cur = inb(SPEAKER_PORT_B);
    outb(SPEAKER_PORT_B, cur | 3);
}

/*
 * audio_stop_tone:
 * Silences PC speaker by clearing bits 0 and 1 on Port 0x61.
 */
void audio_stop_tone(void) {
    uint8_t cur = inb(SPEAKER_PORT_B);
    outb(SPEAKER_PORT_B, cur & ~3);
}

/*
 * audio_snd:
 * Continuous tone control by semitone note index.
 */
void audio_snd(int8_t ona) {
    if (ona <= 0) {
        audio_stop_tone();
    } else {
        audio_play_tone(audio_ona_to_freq(ona));
    }
}

/*
 * audio_beep:
 * Timed beep tone generator with overflow protection.
 */
void audio_beep(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0) {
        audio_stop_tone();
        return;
    }

    // Bounds checking & clamping against C overflows
    if (freq_hz < 20) freq_hz = 20;
    if (freq_hz > 20000) freq_hz = 20000;
    if (duration_ms == 0) duration_ms = 200;
    if (duration_ms > 10000) duration_ms = 10000;

    audio_play_tone(freq_hz);
    pit_sleep_ms(duration_ms);
    audio_stop_tone();
}

/*
 * audio_chime_boot:
 * Silent boot - no automatic startup tone.
 */
void audio_chime_boot(void) {
    // Silent boot
}

/*
 * audio_init:
 * Initialize PC Speaker audio subsystem.
 */
void audio_init(void) {
    memset(&s_audio_info, 0, sizeof(s_audio_info));
    s_audio_info.sample_rate = 1193182;
    s_audio_info.active_devices = AUDIO_DEV_SPEAKER;

    audio_stop_tone();
    serial_puts("[+] Audio: PC Speaker driver online (PIT Ch2, Port 0x61).\n");
}

struct audio_info audio_get_info(void) {
    return s_audio_info;
}

/*
 * audio_play_pcm:
 * Compatibility stub for userspace applications (e.g. DOOM).
 */
int audio_play_pcm(const uint8_t* samples, size_t len, uint32_t sample_rate) {
    (void)samples;
    (void)len;
    (void)sample_rate;
    return (int)len;
}
