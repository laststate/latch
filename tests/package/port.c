// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/package/port.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include "nrf_reset.h"

int main(void) {
    volatile uint32_t reset_reason = LS_NRF_RESETREAS_SREQ;
    ls_nrf_reset_port_t port;
    ls_nrf_reset_profile_nrf52(&port, &reset_reason);
    return ls_nrf_reset_info(&port).reason == LS_RESET_SOFTWARE ? 0 : 1;
}
