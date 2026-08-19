// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/noinit.h
//
// Cross-toolchain noinit attribute macro. The single source of
// truth for declaring .noinit storage so the capture path works
// the same way on every supported compiler.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_NOINIT_H
#define LASTSTATE_NOINIT_H
#if defined(__APPLE__)
#define LS_NOINIT __attribute__((section("__DATA,.noinit"), used, aligned(8)))
#elif defined(__GNUC__) || defined(__clang__)
#define LS_NOINIT __attribute__((section(".noinit"), used, aligned(8)))
#elif defined(__ICCARM__)
#define LS_NOINIT __no_init
#else
#define LS_NOINIT
#endif
#endif
