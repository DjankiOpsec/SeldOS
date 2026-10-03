/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Linux-Style Boot Animation & Status Pipeline Header
 * GPLv3 Licensed.
 */

#ifndef SELD_BOOT_ANIM_H
#define SELD_BOOT_ANIM_H

#include <stdint.h>
#include <stddef.h>

void boot_anim_init(void);
void boot_anim_step(const char* subsystem, const char* message, int step, int total_steps);
void boot_anim_draw_logo(int x, int y);
void boot_anim_finish(void);
int boot_anim_is_active(void);

#endif /* SELD_BOOT_ANIM_H */
