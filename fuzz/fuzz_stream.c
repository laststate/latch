// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// fuzz/fuzz_stream.c
//
// Stream transport fuzz target. Exercises framing and AEAD.
// Corpus driven.
//
// Heap-free, bounded, deterministic.

#include <stddef.h>
#include <stdint.h>
#include "laststate/latch.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ls_stream_frame_t frame;
    (void)ls_stream_frame_parse(data, size, LS_MAX_EVENT_SIZE, &frame);
    ls_lsak_t ack;
    (void)ls_lsak_parse(data, size, &ack);
    return 0;
}
