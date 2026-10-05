/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign Audio Architecture Implementation
 * Universal Support: PC Speaker (PIT Ch2), Intel AC'97 (PCI / VirtualBox), Sound Blaster 16
 * GPLv3 Licensed.
 */

#include "audio.h"
#include "io.h"
#include "serial.h"
#include "pit.h"
#include "pci.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"

/* PC Speaker Hardware I/O Ports */
#define SPEAKER_PIT_CMD   0x43
#define SPEAKER_PIT_DATA  0x42
#define SPEAKER_PORT_B    0x61
#define PIT_FREQ_BASE     1193182

/* Sound Blaster 16 Ports */
#define SB16_BASE         0x220
#define SB16_MIXER_ADDR   (SB16_BASE + 0x04)
#define SB16_MIXER_DATA   (SB16_BASE + 0x05)
#define SB16_DSP_RESET    (SB16_BASE + 0x06)
#define SB16_DSP_READ     (SB16_BASE + 0x0A)
#define SB16_DSP_WRITE    (SB16_BASE + 0x0C)
#define SB16_DSP_STATUS   (SB16_BASE + 0x0E)

/* AC97 Register Offsets */
#define AC97_RESET        0x00
#define AC97_MASTER_VOL   0x02
#define AC97_HEADPHONE    0x04
#define AC97_MONO_VOL     0x06
#define AC97_PCM_VOL      0x18

#define AC97_PO_BDBAR     0x10
#define AC97_PO_CIV       0x14
#define AC97_PO_LVI       0x15
#define AC97_PO_SR        0x16
#define AC97_PO_CR        0x1B

/* AC97 Buffer Descriptor List (BDL) Entry */
struct ac97_bdl_entry {
    uint32_t buffer_phys;
    uint16_t sample_count; // Words (16-bit samples)
    uint16_t flags;        // 0x8000 = IOC, 0x4000 = BUP
} __attribute__((packed));

static struct audio_info s_audio_info = {0};
static struct ac97_bdl_entry* s_ac97_bdl = NULL;
static uint32_t s_ac97_bdl_phys = 0;
static uint8_t* s_ac97_pcm_buf = NULL;
static uint32_t s_ac97_pcm_buf_phys = 0;
static uint32_t s_ac97_buf_frames = 24000;
static uint32_t s_ac97_buf_bytes = 96000;
static volatile int s_ac97_tone_active = 0;

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

/* --- Sound Blaster 16 Driver --- */
static int sb16_dsp_write(uint8_t val) {
    for (int i = 0; i < 1000; i++) {
        if (!(inb(SB16_DSP_STATUS) & 0x80)) {
            outb(SB16_DSP_WRITE, val);
            return 0;
        }
    }
    return -1;
}

static int sb16_init(void) {
    outb(SB16_DSP_RESET, 1);
    for (volatile int i = 0; i < 1000; i++) { inb(0x80); }
    outb(SB16_DSP_RESET, 0);

    for (int i = 0; i < 1000; i++) {
        if (inb(SB16_DSP_STATUS) & 0x80) {
            if (inb(SB16_DSP_READ) == 0xAA) {
                // Set Master and Voice Volume to max
                outb(SB16_MIXER_ADDR, 0x22);
                outb(SB16_MIXER_DATA, 0xFF);
                outb(SB16_MIXER_ADDR, 0x04);
                outb(SB16_MIXER_DATA, 0xFF);

                s_audio_info.active_devices |= AUDIO_DEV_SB16;
                s_audio_info.sb16_io_base = SB16_BASE;
                serial_puts("[+] Audio: Sound Blaster 16 DSP detected at port 0x220\n");
                return 0;
            }
        }
    }
    return -1;
}

/* --- Intel ICH AC'97 Driver (VirtualBox / Hardware) --- */
static void ac97_init(void) {
    for (uint16_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_word((uint8_t)bus, slot, func, 0x00);
                if (vendor == 0xFFFF || vendor == 0x0000) {
                    if (func == 0) break;
                    continue;
                }

                uint32_t class_rev = pci_read_dword((uint8_t)bus, slot, func, 0x08);
                uint8_t class_code = (uint8_t)(class_rev >> 24);
                uint8_t subclass = (uint8_t)(class_rev >> 16);
                uint16_t device = pci_read_word((uint8_t)bus, slot, func, 0x02);

                // Check for AC'97 controller:
                // Vendor 0x8086 & Device 0x2415 (VirtualBox ICH AC97)
                // OR PCI Class 0x04 (Multimedia), Subclass 0x01 (Audio Controller)
                if ((vendor == 0x8086 && device == 0x2415) || (class_code == 0x04 && subclass == 0x01)) {
                    uint32_t bar0 = pci_read_dword((uint8_t)bus, slot, func, 0x10);
                    uint32_t bar1 = pci_read_dword((uint8_t)bus, slot, func, 0x14);

                    uint16_t nambar = (uint16_t)(bar0 & 0xFFFE);
                    uint16_t nabmbar = (uint16_t)(bar1 & 0xFFFE);

                    pci_enable_bus_mastering((uint8_t)bus, slot, func);

                    s_audio_info.ac97_nambar = nambar;
                    s_audio_info.ac97_nabmbar = nabmbar;

                    // Reset AC97 codec
                    outw(nambar + AC97_RESET, 0x0000);
                    for (volatile int i = 0; i < 10000; i++) inb(0x80);

                    // Unmute Master Volume (0dB attenuation)
                    outw(nambar + AC97_MASTER_VOL, 0x0000);
                    // Unmute Headphone / AUX Volume
                    outw(nambar + AC97_HEADPHONE, 0x0000);
                    // Unmute Master Mono Volume
                    outw(nambar + AC97_MONO_VOL, 0x0000);
                    // Unmute PCM Output Volume (0dB attenuation)
                    outw(nambar + AC97_PCM_VOL, 0x0000);

                    // Allocate DMA Buffer Descriptor List (32 entries * 8 bytes = 256 bytes)
                    void* bdl_frame = pmm_alloc_frame();
                    if (bdl_frame) {
                        s_ac97_bdl_phys = (uint32_t)(uint64_t)bdl_frame;
                        s_ac97_bdl = (struct ac97_bdl_entry*)phys_to_virt(s_ac97_bdl_phys);
                        memset(s_ac97_bdl, 0, 4096);
                    }

                    // Allocate 24 contiguous frames (96000 bytes = 24000 stereo frames at 48000 Hz)
                    void* pcm_frames = pmm_alloc_frames(24);
                    if (pcm_frames) {
                        s_ac97_pcm_buf_phys = (uint32_t)(uint64_t)pcm_frames;
                        s_ac97_pcm_buf = (uint8_t*)phys_to_virt(s_ac97_pcm_buf_phys);
                        s_ac97_buf_frames = 24000;
                        s_ac97_buf_bytes = 96000;
                        memset(s_ac97_pcm_buf, 0, s_ac97_buf_bytes);
                    }

                    if (s_ac97_bdl && s_ac97_pcm_buf) {
                        for (int k = 0; k < 32; k++) {
                            s_ac97_bdl[k].buffer_phys = s_ac97_pcm_buf_phys;
                            s_ac97_bdl[k].sample_count = 48000; // 48000 16-bit words
                            s_ac97_bdl[k].flags = 0;
                        }
                    }

                    // Reset PCM Out channel
                    outb(nabmbar + AC97_PO_CR, 0x02); // RR
                    for (volatile int i = 0; i < 1000; i++) inb(0x80);
                    outw(nabmbar + AC97_PO_SR, 0x001C);

                    s_audio_info.active_devices |= AUDIO_DEV_AC97;
                    s_audio_info.sample_rate = 48000;

                    serial_puts("[+] Audio: AC'97 Audio Controller detected & initialized (PCI ");
                    serial_print_hex(vendor);
                    serial_puts(":");
                    serial_print_hex(device);
                    serial_puts(", NAMBAR: ");
                    serial_print_hex(nambar);
                    serial_puts(", NABMBAR: ");
                    serial_print_hex(nabmbar);
                    serial_puts(")\n");
                    return;
                }
            }
        }
    }
}

/*
 * audio_play_tone:
 * Simultaneous tone synthesis across all active audio backends:
 * - Intel AC'97 DMA PCM (VirtualBox & PCI hardware)
 * - PC Speaker PIT Channel 2 + Port 0x61 (Universal x86)
 */
void audio_play_tone(uint32_t freq_hz) {
    if (freq_hz < 20 || freq_hz > 20000) {
        audio_stop_tone();
        return;
    }

    // 1. Synthesize square wave for AC'97 (VirtualBox & physical hardware)
    if ((s_audio_info.active_devices & AUDIO_DEV_AC97) && s_ac97_pcm_buf && s_ac97_bdl) {
        int16_t* dst = (int16_t*)s_ac97_pcm_buf;
        for (uint32_t i = 0; i < s_ac97_buf_frames; i++) {
            uint32_t phase = ((uint64_t)i * freq_hz) % 48000;
            int16_t sample = (phase < 24000) ? 16384 : -16384;
            dst[i * 2]     = sample; // Left channel
            dst[i * 2 + 1] = sample; // Right channel
        }

        uint16_t nabm = s_audio_info.ac97_nabmbar;
        // Reset PCM Out channel to start immediately from descriptor 0
        outb(nabm + AC97_PO_CR, 0x02); // RR
        for (volatile int i = 0; i < 100; i++) inb(0x80);
        outw(nabm + AC97_PO_SR, 0x001C);
        outl(nabm + AC97_PO_BDBAR, s_ac97_bdl_phys);
        outb(nabm + AC97_PO_LVI, 31);
        outb(nabm + AC97_PO_CR, 0x01); // Run Bus Master
        s_ac97_tone_active = 1;
    }

    // 2. PIT Channel 2 square wave on Port 0x61 (Standard x86 PC Speaker)
    uint32_t period = PIT_FREQ_BASE / freq_hz;
    if (period < 2) period = 2;
    if (period > 65535) period = 65535;

    // Mode 3 square wave generator on Channel 2
    outb(SPEAKER_PIT_CMD, 0xB6);
    outb(SPEAKER_PIT_DATA, (uint8_t)(period & 0xFF));
    outb(SPEAKER_PIT_DATA, (uint8_t)((period >> 8) & 0xFF));

    // Turn on Speaker Gate (bit 0) and Speaker Data (bit 1)
    uint8_t cur = inb(SPEAKER_PORT_B);
    outb(SPEAKER_PORT_B, cur | 3);
}

/*
 * audio_stop_tone:
 * Silences tone across PC Speaker and AC'97 DMA subsystem.
 */
void audio_stop_tone(void) {
    // 1. PC Speaker off
    uint8_t cur = inb(SPEAKER_PORT_B);
    outb(SPEAKER_PORT_B, cur & ~3);

    // 2. AC'97 DMA pause and buffer mute
    if (s_audio_info.active_devices & AUDIO_DEV_AC97) {
        s_ac97_tone_active = 0;
        uint16_t nabm = s_audio_info.ac97_nabmbar;
        outb(nabm + AC97_PO_CR, 0x00); // Pause Bus Master
        if (s_ac97_pcm_buf) {
            memset(s_ac97_pcm_buf, 0, s_ac97_buf_bytes);
        }
    }
}

/*
 * audio_timer_tick:
 * IRQ0 timer hook (100 Hz) to ensure seamless endless playback for continuous tones.
 */
void audio_timer_tick(void) {
    if (!s_ac97_tone_active || !(s_audio_info.active_devices & AUDIO_DEV_AC97)) {
        return;
    }
    uint16_t nabm = s_audio_info.ac97_nabmbar;
    uint8_t cr = inb(nabm + AC97_PO_CR);
    // If DMA halted because it reached LVI (e.g. after 16s), restart it seamlessly
    if (!(cr & 0x01)) {
        outb(nabm + AC97_PO_LVI, 31);
        outb(nabm + AC97_PO_CR, 0x01);
    }
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
    if (duration_ms == 0) duration_ms = 150;
    if (duration_ms > 10000) duration_ms = 10000;

    audio_play_tone(freq_hz);
    pit_sleep_ms(duration_ms);
    audio_stop_tone();
}

/*
 * audio_chime_boot:
 * Silent boot - system boots completely silently.
 */
void audio_chime_boot(void) {
    // Silent boot
}

/*
 * audio_init:
 * Initialize all audio subsystems (Speaker, AC'97, SB16).
 */
void audio_init(void) {
    memset(&s_audio_info, 0, sizeof(s_audio_info));
    s_audio_info.sample_rate = 48000;
    s_audio_info.active_devices = AUDIO_DEV_SPEAKER;

    audio_stop_tone();
    serial_puts("[+] Audio: Universal PC Speaker driver online (PIT Ch2, Port 0x61).\n");

    // Scan and initialize Intel AC'97 Controller (VirtualBox / PCI)
    ac97_init();

    // Scan and initialize Sound Blaster 16 DSP
    sb16_init();

    // Silent boot
    audio_chime_boot();
}

struct audio_info audio_get_info(void) {
    return s_audio_info;
}

/*
 * audio_play_pcm:
 * Digital PCM audio playback for userspace applications (e.g. DOOM).
 */
int audio_play_pcm(const uint8_t* samples, size_t len, uint32_t sample_rate) {
    (void)sample_rate;
    if (!samples || len == 0) return 0;

    // 1. AC'97 DMA streaming
    if ((s_audio_info.active_devices & AUDIO_DEV_AC97) && s_ac97_pcm_buf && s_ac97_bdl) {
        size_t copy_sz = len > s_ac97_buf_frames ? s_ac97_buf_frames : len;
        int16_t* dst = (int16_t*)s_ac97_pcm_buf;
        for (size_t i = 0; i < copy_sz; i++) {
            int16_t s16 = (int16_t)(((int)samples[i] - 128) << 8);
            dst[i * 2]     = s16;
            dst[i * 2 + 1] = s16;
        }

        uint16_t nabm = s_audio_info.ac97_nabmbar;
        outb(nabm + AC97_PO_CR, 0x02);
        for (volatile int i = 0; i < 100; i++) inb(0x80);
        outw(nabm + AC97_PO_SR, 0x001C);
        outl(nabm + AC97_PO_BDBAR, s_ac97_bdl_phys);
        s_ac97_bdl[0].sample_count = (uint16_t)(copy_sz * 2);
        outb(nabm + AC97_PO_LVI, 0);
        outb(nabm + AC97_PO_CR, 0x01);
    }

    // 2. Sound Blaster 16 direct DAC streaming
    if (s_audio_info.active_devices & AUDIO_DEV_SB16) {
        size_t limit = len > 256 ? 256 : len;
        for (size_t i = 0; i < limit; i++) {
            sb16_dsp_write(0x10);
            sb16_dsp_write(samples[i]);
        }
    }

    // 3. Modulate PC Speaker tone based on waveform amplitude
    uint32_t avg = 0;
    size_t sample_count = len > 64 ? 64 : len;
    for (size_t i = 0; i < sample_count; i++) {
        avg += samples[i];
    }
    avg /= sample_count;
    uint32_t tone_freq = 200 + (avg * 6);
    audio_play_tone(tone_freq);
    pit_sleep_ms(25);
    audio_stop_tone();

    return (int)len;
}
