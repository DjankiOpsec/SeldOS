#ifndef SELD_KSYMS_H
#define SELD_KSYMS_H

#include <stdint.h>
#include <stddef.h>

struct kernel_symbol {
    uint64_t addr;
    const char* name;
    const char* file;
    uint32_t line;
};

const struct kernel_symbol* ksym_lookup(uint64_t rip, uint64_t* out_offset);

#endif /* SELD_KSYMS_H */
