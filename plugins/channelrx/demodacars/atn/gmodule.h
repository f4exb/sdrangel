/*
 * Minimal stand-in for glib's <gmodule.h>, providing just the GByteArray subset that the
 * vendored asn1-format-icao-text.c uses (g_byte_array_new/append/free), so the SDRangel
 * build does not need glib. This directory is on the include path ahead of any system
 * glib, so this file deliberately shadows the real one for the vendored sources.
 */
#ifndef DUMPVDL2_GMODULE_COMPAT_H
#define DUMPVDL2_GMODULE_COMPAT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef struct {
    uint8_t *data;
    unsigned int len;
    unsigned int allocated;
} GByteArray;

static GByteArray *g_byte_array_new(void)
{
    GByteArray *array = (GByteArray *)calloc(1, sizeof(GByteArray));
    return array;
}

static GByteArray *g_byte_array_append(GByteArray *array, const uint8_t *data, unsigned int len)
{
    if (array->len + len > array->allocated)
    {
        array->allocated = 2 * (array->len + len);
        array->data = (uint8_t *)realloc(array->data, array->allocated);
    }
    memcpy(array->data + array->len, data, len);
    array->len += len;
    return array;
}

static uint8_t *g_byte_array_free(GByteArray *array, int free_segment)
{
    uint8_t *data = array->data;
    if (free_segment)
    {
        free(array->data);
        data = NULL;
    }
    free(array);
    return data;
}

#ifdef __cplusplus
}
#endif

#endif // DUMPVDL2_GMODULE_COMPAT_H
