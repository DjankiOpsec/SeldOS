/*
 * SeldOS - Humboldt Kernel Project
 * Unified Audio Subsystem Implementation
 * Supports PC Speaker, Sound Blaster 16 (DSP), and Intel AC'97 Controller.
 * GPLv3 Licensed.
 */

#include "audio.h"
#include "io.h"
#include "serial.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* PC Speaker Ports */
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
#define AC97_PCM_VOL      0x18
#define AC97_NABMBAR_PO_BDBAR 0x10
#define AC97_NABMBAR_PO_CIV   0x14
#define AC97_NABMBAR_PO_LVI   0x15
#define AC97_NABMBAR_PO_SR    0x16
#define AC97_NABMBAR_PO_CR    0x1B

/* AC97 BDL Entry */
struct ac97_bdl_entry {
    uint32_t buffer_phys;
    uint16_t sample_count;
    uint16_t flags; // 0x8000 = IOC, 0x4000 = BUP
} __attribute__((packed));

static struct audio_info s_audio_info = {0};
static struct ac97_bdl_entry* s_ac97_bdl = NULL;
static uint32_t s_ac97_bdl_phys = 0;
static uint8_t* s_ac97_pcm_buf = NULL;
static uint32_t s_ac97_pcm_buf_phys = 0;

/* PCI Read/Write Accessors */
static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t d = pci_read_dword(bus, slot, func, offset);
    return (uint16_t)((d >> ((offset & 2) * 8)) & 0xFFFF);
}

static void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t d = inl(PCI_CONFIG_DATA);
    if ((offset & 2) == 0) {
        d = (d & 0xFFFF0000) | (uint32_t)val;
    } else {
        d = (d & 0x0000FFFF) | ((uint32_t)val << 16);
    }
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, d);
}

/* --- PC Speaker Driver --- */
void audio_play_tone(uint32_t freq_hz) {
    if (freq_hz < 20 || freq_hz > 20000) {
        audio_stop_tone();
        return;
    }

    uint32_t div = PIT_FREQ_BASE / freq_hz;
    if (div > 65535) div = 65535;
    if (div < 1) div = 1;

    // PIT Channel 2: Mode 3 (Square Wave Generator), Access lobyte/hibyte
    outb(SPEAKER_PIT_CMD, 0xB6);
    outb(SPEAKER_PIT_DATA, (uint8_t)(div & 0xFF));
    outb(SPEAKER_PIT_DATA, (uint8_t)((div >> 8) & 0xFF));

    // Enable timer 2 gate and speaker data via Port 0x61
    uint8_t cur = inb(SPEAKER_PORT_B);
    if ((cur & 3) != 3) {
        outb(SPEAKER_PORT_B, cur | 3);
    }
}

void audio_stop_tone(void) {
    uint8_t cur = inb(SPEAKER_PORT_B);
    outb(SPEAKER_PORT_B, cur & ~3);
}

void audio_beep(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0 || duration_ms == 0) return;
    audio_play_tone(freq_hz);
    pit_sleep_ms(duration_ms);
    audio_stop_tone();
}

void audio_chime_boot(void) {
    // Elegant 3-tone boot chime (C5 -> E5 -> G5)
    audio_play_tone(523);
    pit_sleep_ms(60);
    audio_play_tone(659);
    pit_sleep_ms(60);
    audio_play_tone(784);
    pit_sleep_ms(120);
    audio_stop_tone();
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
    // Reset DSP
    outb(SB16_DSP_RESET, 1);
    for (volatile int i = 0; i < 1000; i++) { inb(0x80); }
    outb(SB16_DSP_RESET, 0);

    for (int i = 0; i < 1000; i++) {
        if (inb(SB16_DSP_STATUS) & 0x80) {
            if (inb(SB16_DSP_READ) == 0xAA) {
                // Set Master & Voice Volume to Max (0xFF)
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

/* --- AC'97 Driver --- */
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

                // Class 0x04 (Multimedia), Subclass 0x01 (Audio Controller)
                if (class_code == 0x04 && (subclass == 0x01 || subclass == 0x03)) {
                    uint16_t device = pci_read_word((uint8_t)bus, slot, func, 0x02);
                    uint32_t bar0 = pci_read_dword((uint8_t)bus, slot, func, 0x10);
                    uint32_t bar1 = pci_read_dword((uint8_t)bus, slot, func, 0x14);

                    uint16_t nambar = (uint16_t)(bar0 & 0xFFFC);
                    uint16_t nabmbar = (uint16_t)(bar1 & 0xFFFC);

                    // Enable Bus Master (bit 2) and I/O Space (bit 0)
                    uint16_t cmd = pci_read_word((uint8_t)bus, slot, func, 0x04);
                    pci_write_word((uint8_t)bus, slot, func, 0x04, cmd | 0x05);

                    s_audio_info.ac97_nambar = nambar;
                    s_audio_info.ac97_nabmbar = nabmbar;

                    // Reset AC97 codec
                    outw(nambar + AC97_RESET, 0x0000);

                    // Unmute Master Volume (0dB attenuation)
                    outw(nambar + AC97_MASTER_VOL, 0x0000);

                    // Unmute PCM Output Volume (0dB attenuation)
                    outw(nambar + AC97_PCM_VOL, 0x0000);

                    // Allocate DMA Buffer Descriptor List (32 entries * 8 bytes = 256 bytes)
                    void* bdl_frame = pmm_alloc_frame();
                    if (bdl_frame) {
                        s_ac97_bdl_phys = (uint32_t)(uint64_t)bdl_frame;
                        s_ac97_bdl = (struct ac97_bdl_entry*)phys_to_virt(s_ac97_bdl_phys);
                        memset(s_ac97_bdl, 0, 4096);
                    }

                    // Allocate PCM Out Sample Buffer (4096 bytes)
                    void* pcm_frame = pmm_alloc_frame();
                    if (pcm_frame) {
                        s_ac97_pcm_buf_phys = (uint32_t)(uint64_t)pcm_frame;
                        s_ac97_pcm_buf = (uint8_t*)phys_to_virt(s_ac97_pcm_buf_phys);
                        memset(s_ac97_pcm_buf, 0, 4096);
                    }

                    if (s_ac97_bdl && s_ac97_pcm_buf) {
                        s_ac97_bdl[0].buffer_phys = s_ac97_pcm_buf_phys;
                        s_ac97_bdl[0].sample_count = 1024; // 1024 stereo 16-bit samples
                        s_ac97_bdl[0].flags = 0x8000;       // Interrupt on completion
                    }

                    s_audio_info.active_devices |= AUDIO_DEV_AC97;
                    serial_puts("[+] Audio: AC'97 Controller detected (Vendor: 0x");
                    serial_print_hex(vendor);
                    serial_puts(", Device: 0x");
                    serial_print_hex(device);
                    serial_puts(", NAMBAR: 0x");
                    serial_print_hex(nambar);
                    serial_puts(", NABMBAR: 0x");
                    serial_print_hex(nabmbar);
                    serial_puts(")\n");
                    return;
                }
            }
        }
    }
}

/* --- Public Subsystem Initialization --- */
void audio_init(void) {
    memset(&s_audio_info, 0, sizeof(s_audio_info));
    s_audio_info.sample_rate = 44100;

    // 1. PC Speaker is universally present on all x86 architectures
    audio_stop_tone();
    s_audio_info.active_devices |= AUDIO_DEV_SPEAKER;
    serial_puts("[+] Audio: Universal PC Speaker driver initialized (PIT Ch2, Port 0x61).\n");

    // 2. Scan and initialize AC'97 PCI Audio Controller
    ac97_init();

    // 3. Scan and initialize Sound Blaster 16 DSP
    sb16_init();

    // 4. Play signature Humboldt OS startup chime
    audio_chime_boot();
}

struct audio_info audio_get_info(void) {
    return s_audio_info;
}

/* --- PCM Audio Playback Dispatcher --- */
int audio_play_pcm(const uint8_t* samples, size_t len, uint32_t sample_rate) {
    (void)sample_rate;
    if (!samples || len == 0) return 0;

    // 1. If AC'97 is active, stream to PCM Out buffer
    if ((s_audio_info.active_devices & AUDIO_DEV_AC97) && s_ac97_pcm_buf && s_ac97_bdl) {
        size_t copy_sz = len > 4096 ? 4096 : len;
        // Convert 8-bit unsigned PCM to 16-bit signed stereo for AC'97
        int16_t* dst = (int16_t*)s_ac97_pcm_buf;
        for (size_t i = 0; i < copy_sz && (i * 2 < 2048); i++) {
            int16_t sample16 = (int16_t)(((int)samples[i] - 128) << 8);
            dst[i * 2]     = sample16; // Left
            dst[i * 2 + 1] = sample16; // Right
        }

        uint16_t nabm = s_audio_info.ac97_nabmbar;
        outl(nabm + AC97_NABMBAR_PO_BDBAR, s_ac97_bdl_phys);
        outb(nabm + AC97_NABMBAR_PO_LVI, 0);
        // Start PCM Out DMA transfer (Bit 0 = RPBM)
        outb(nabm + AC97_NABMBAR_PO_CR, 0x01);
    }

    // 2. If SB16 is active, stream through DSP DAC
    if (s_audio_info.active_devices & AUDIO_DEV_SB16) {
        size_t limit = len > 256 ? 256 : len;
        for (size_t i = 0; i < limit; i++) {
            sb16_dsp_write(0x10); // Direct 8-bit DAC output command
            sb16_dsp_write(samples[i]);
        }
    }

    // 3. Modulate PC Speaker tone based on waveform amplitude for tactile acoustic feedback
    uint32_t avg = 0;
    size_t sample_count = len > 64 ? 64 : len;
    for (size_t i = 0; i < sample_count; i++) {
        avg += samples[i];
    }
    avg /= sample_count;

    // Convert average amplitude to audible tone in range 200 Hz .. 2000 Hz
    uint32_t tone_freq = 200 + (avg * 6);
    audio_play_tone(tone_freq);
    pit_sleep_ms(25);
    audio_stop_tone();

    return (int)len;
}
