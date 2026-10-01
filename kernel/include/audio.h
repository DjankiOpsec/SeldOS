/*
 * SeldOS - Humboldt Kernel Project
 * Unified Audio Architecture Interface
 * Supports PC Speaker (PIT Ch2), Sound Blaster 16 (DSP), and AC'97 Audio Controller.
 * GPLv3 Licensed.
 */

#ifndef SELD_AUDIO_H
#define SELD_AUDIO_H

#include <stdint.h>
#include <stddef.h>

#define AUDIO_DEV_NONE    0
#define AUDIO_DEV_SPEAKER (1 << 0)
#define AUDIO_DEV_SB16    (1 << 1)
#define AUDIO_DEV_AC97    (1 << 2)

struct audio_info {
    uint32_t active_devices;
    uint16_t sb16_io_base;
    uint16_t ac97_nambar;
    uint16_t ac97_nabmbar;
    uint32_t sample_rate;
};

void audio_init(void);
struct audio_info audio_get_info(void);
void audio_play_tone(uint32_t freq_hz);
void audio_stop_tone(void);
void audio_beep(uint32_t freq_hz, uint32_t duration_ms);
int  audio_play_pcm(const uint8_t* samples, size_t len, uint32_t sample_rate);
void audio_chime_boot(void);

#endif /* SELD_AUDIO_H */
