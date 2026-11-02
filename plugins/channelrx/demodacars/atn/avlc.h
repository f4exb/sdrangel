/*
 * Minimal stand-in for dumpvdl2's avlc.h, providing just the avlc_addr_t type that the
 * vendored xid.c uses (the original also carries frame queue types that depend on glib).
 * From https://github.com/szpajder/dumpvdl2, GPL-3.0,
 * Copyright (c) 2017-2026 Tomasz Lemiech <szpajder@gmail.com>.
 */
#ifndef DUMPVDL2_AVLC_COMPAT_H
#define DUMPVDL2_AVLC_COMPAT_H

#include <stdint.h>
#include "config.h"     // IS_BIG_ENDIAN

typedef union {
    uint32_t val;
    struct {
#ifdef IS_BIG_ENDIAN
        uint8_t status:1;
        uint8_t type:3;
        uint32_t addr:24;
#else
        uint32_t addr:24;
        uint8_t type:3;
        uint8_t status:1;
#endif
    } a_addr;
} avlc_addr_t;

#endif // DUMPVDL2_AVLC_COMPAT_H
