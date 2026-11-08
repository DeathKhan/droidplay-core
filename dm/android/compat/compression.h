// Android stand-in for Apple's libcompression, for nvn_capture.c: no LZFSE here, so a capture's chunks are stored raw (encode reports failure,
// which cap_put takes as "keep it uncompressed"), and an LZFSE chunk from a Mac capture does not decode (cap_next reports it corrupt)
#pragma once
#include "dm_android.h"
typedef enum { COMPRESSION_LZFSE = 0x801 } compression_algorithm;
static inline size_t compression_encode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn, void *scratch, compression_algorithm a) {
    (void)dst; (void)dn; (void)src; (void)sn; (void)scratch; (void)a; return 0; }
static inline size_t compression_decode_buffer(uint8_t *dst, size_t dn, const uint8_t *src, size_t sn, void *scratch, compression_algorithm a) {
    (void)dst; (void)dn; (void)src; (void)sn; (void)scratch; (void)a; return 0; }
