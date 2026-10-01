/*
 * SeldOS - Humboldt Kernel Project
 * PS/2 & VMMouse Touchscreen/Mouse Subsystem
 * GPLv3 Licensed.
 */

#ifndef SELD_MOUSE_H
#define SELD_MOUSE_H

#include <stdint.h>
#include <stddef.h>

struct mouse_event {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    uint8_t buttons; // bit 0 = left, bit 1 = right, bit 2 = middle
};

void mouse_init(void);
void mouse_handle_irq(void);
void mouse_handle_byte(uint8_t data);
int  mouse_poll_event(struct mouse_event* ev);
void mouse_get_state(int32_t* x, int32_t* y, uint8_t* buttons);

#endif /* SELD_MOUSE_H */
