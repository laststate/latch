// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/build_id.h
//
// Build identity API. Exposes the compile-time build ID, git SHA,
// and a validator used to confirm a serialized record was produced
// by the expected firmware revision.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_BUILD_ID_H
#define LASTSTATE_BUILD_ID_H

#include <stdbool.h>
#include "config.h"
#if defined(LS_HAS_GENERATED_BUILD_ID)
#include "laststate/build_id_generated.h"
#endif
#ifndef LS_BUILD_ID
#define LS_BUILD_ID "development-unknown"
#endif
const char *ls_build_id(void);
bool ls_build_id_validate(const char *build_id);
#endif
