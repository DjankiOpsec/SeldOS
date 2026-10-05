/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign PC Speaker Sound Subsystem Interface
 * Direct 8254 PIT Timer 2 Square Wave Synthesis & Note Engine
 * GPLv3 Licensed.
 */

#ifndef SELD_AUDIO_H
#define SELD_AUDIO_H

#include <stdint.h>
#include <stddef.h>

#define AUDIO_DEV_NONE    0
#define AUDIO_DEV_SPEAKER (1 << 0)
#define AUDIO_DEV_SB16    0
#define AUDIO_DEV_AC97    0

struct audio_info {
    uint32_t active_devices;
    uint16_t sb16_io_base;
    uint16_t ac97_nambar;
    uint16_t ac97_nabmbar;
    uint32_t sample_rate;
};

void     audio_init(void);
struct audio_info audio_get_info(void);
void     audio_play_tone(uint32_t freq_hz);
void     audio_stop_tone(void);
void     audio_beep(uint32_t freq_hz, uint32_t duration_ms);
void     audio_snd(int8_t ona);
uint32_t audio_ona_to_freq(int8_t ona);
int8_t   audio_freq_to_ona(uint32_t freq_hz);
int      audio_play_pcm(const uint8_t* samples, size_t len, uint32_t sample_rate);
void     audio_chime_boot(void);

#endif /* SELD_AUDIO_H */
