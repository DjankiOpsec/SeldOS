/*
 * SeldOS - Humboldt Kernel Project
 * AMD PCnet-FAST III (Am79C973) PCI Network Driver Interface
 * GPLv3 Licensed.
 */

#ifndef SELD_PCNET_H
#define SELD_PCNET_H

#include <stdint.h>
#include <stddef.h>

int            pcnet_init(void);
int            pcnet_is_active(void);
const uint8_t* pcnet_get_mac(void);
int            pcnet_send_packet(const void* data, size_t len);
int            pcnet_poll_packet(void* buf, size_t max_len);

#endif /* SELD_PCNET_H */
