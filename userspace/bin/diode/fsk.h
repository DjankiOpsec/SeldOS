/*
 * SeldOS - Humboldt Kernel Project
 * Sovereign Acoustic Air-Gap FSK (Frequency Shift Keying) Modulator
 * 1200 Hz (Space) / 2200 Hz (Mark) Synchronous Bitstream via PC Speaker / Sound Card
 * GPLv3 Licensed.
 */

#ifndef _FSK_H_
#define _FSK_H_

#include <stdint.h>
#include <stddef.h>

#define FSK_FREQ_SPACE     1200  /* 1200 Hz = Space (bit 0) */
#define FSK_FREQ_MARK      2200  /* 2200 Hz = Mark (bit 1) */
#define FSK_FREQ_PREAMBLE  1800  /* 1800 Hz = Preamble pilot tone */

#define FSK_SYNC_BYTE1     0x5E  /* SeldOS Humboldt sync signature */
#define FSK_SYNC_BYTE2     0x1D

#define FSK_PKT_TYPE_KEY   0x01  /* Key packet */

#define FSK_DEFAULT_SYMBOL_MS 30 /* 30 ms per bit = 33.3 baud */

#pragma pack(push, 1)
struct fsk_key_packet {
    uint8_t  sync[2];        /* 0x5E, 0x1D */
    uint8_t  pkt_type;       /* 0x01 */
    uint32_t session_id;     /* 4 bytes */
    uint32_t key_id;         /* 4 bytes */
    uint8_t  master_key[16]; /* 16 bytes (AES-128 key) */
    uint32_t crc32;          /* CRC32 of bytes from sync to end of master_key */
};
#pragma pack(pop)

/* Build the 31-byte key packet buffer */
void fsk_build_key_packet(struct fsk_key_packet* pkt, uint32_t session_id, uint32_t key_id, const uint8_t key[16]);

/* Transmit the packet over PC speaker / audio hardware using synchronous FSK modulation */
void fsk_transmit_key_packet(const struct fsk_key_packet* pkt, uint32_t symbol_ms);

#endif /* _FSK_H_ */
