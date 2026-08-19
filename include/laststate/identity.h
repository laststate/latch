// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/identity.h
//
// Device identity API. Owns the long-lived device ID, key handles,
// and certificate material used by secure storage and transport.
// Tied to provisioning.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_IDENTITY_H
#define LASTSTATE_IDENTITY_H

typedef struct {
    const char *project_id;
    const char *device_id;
    const char *product;
    const char *hardware_revision;
    const char *bom_revision;
    const char *manufacturing_batch;
    const char *firmware_version;
    const char *firmware_build_id;
    const char *bootloader_version;
    const char *git_commit;
    const char *variant;
    const char *architecture;
    const char *rtos;
    const char *region;
    const char *device_group;
} ls_identity_t;
#endif
