// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/freestanding_link.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

/* Link-only probe: the target is never executed. Its purpose is to force all
   portable objects through a link with the platform runtime disabled. */
#if defined(_MSC_VER)
/* Visual Studio injects debug-only RTC hooks before per-target flags can take
   effect. Link-only no-op definitions model the absent hosted debug runtime;
   they are not part of any shipped Latch library. */
void _RTC_InitBase(void) {
}
void _RTC_Shutdown(void) {
}
void _RTC_CheckStackVars(void *frame, void *descriptor) {
    (void)frame;
    (void)descriptor;
}
#endif
void ls_freestanding_entry(void) {
    for (;;) {
    }
}
