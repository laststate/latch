// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/test_powerfail_seal.c
//
// Last-Microjoule commit tests. NMI-safe seal, torn-write rejection,
// backup-RAM mirror recovery, spool promotion, and LEP TLV 23.
//
// Heap-free, bounded, deterministic.

#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "powerfail-seal check failed: %s:%d\n", #condition, __LINE__);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t storage_bytes[60000];
static uint8_t backup_ram[256];
static unsigned sent;
static uint8_t captured[LS_MAX_EVENT_SIZE];
static size_t captured_length;
static unsigned tlv23_seen;
static uint16_t tlv23_length;

static bool available(void *context) {
    (void)context;
    return true;
}

static size_t mtu(void *context) {
    (void)context;
    return sizeof captured;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (length > sizeof captured) {
        return LS_ENOSPACE;
    }
    memcpy(captured, data, length);
    captured_length = length;
    sent++;
    return LS_OK;
}

static ls_result_t count_tlv23(void *context, uint16_t type, const uint8_t *value,
                               uint16_t length) {
    (void)context;
    (void)value;
    if (type == LS_TLV_POWERFAIL_SEAL) {
        tlv23_seen++;
        tlv23_length = length;
    }
    return LS_OK;
}

static void make_storage(ls_storage_backend_t *storage) {
    static ls_memory_storage_t memory;
    memset(storage_bytes, 0xff, sizeof(storage_bytes));
    memory = (ls_memory_storage_t){storage_bytes, sizeof(storage_bytes)};
    *storage = (ls_storage_backend_t){.name = "mem",
                                      .context = &memory,
                                      .capacity = sizeof(storage_bytes),
                                      .erase_size = 1u,
                                      .write_size = 1u,
                                      .read = ls_memory_storage_read,
                                      .write = ls_memory_storage_write,
                                      .erase = ls_memory_storage_erase};
}

static int boot_runtime(ls_storage_backend_t *storage, ls_transport_backend_t *transport) {
    static const ls_identity_t identity = {
        .project_id = "powerfail",
        .device_id = "seal-1",
        .firmware_build_id = "seal0001",
    };
    ls_config_t config = {.identity = &identity};
    if (ls_init(&config) != LS_OK) {
        return 1;
    }
    ls_powerfail_install_backup(backup_ram, sizeof(backup_ram));
    ls_storage_register(storage);
    ls_transport_register(transport);
    return ls_boot() == LS_OK ? 0 : 1;
}

int main(void) {
    ls_storage_backend_t storage;
    ls_transport_backend_t transport = {
        .name = "sink",
        .priority = 1u,
        .available = available,
        .send = send_data,
        .max_payload = mtu,
    };
    ls_powerfail_seal_info_t info;

    /* 1. No seal on a clean boot. */
    make_storage(&storage);
    memset(backup_ram, 0, sizeof(backup_ram));
    ls_powerfail_install_backup(backup_ram, sizeof(backup_ram));
    CHECK(boot_runtime(&storage, &transport) == 0);
    CHECK(!ls_powerfail_sealed_read(&info));
    CHECK(!ls_powerfail_last_seal().sealed_ok);

    /* 2. NMI seal survives ls_init (retained) and promotes on next boot. */
    ls_powerfail_seal(LS_POWERFAIL_BROWNOUT, 2100u);
    CHECK(ls_powerfail_sealed_read(&info));
    CHECK(info.reason == LS_POWERFAIL_BROWNOUT && info.vcap_mv == 2100u && info.sealed_ok);
    CHECK(boot_runtime(&storage, &transport) == 0);
    CHECK(!ls_powerfail_sealed_read(&info));
    CHECK(ls_powerfail_last_seal().sealed_ok);
    CHECK(ls_powerfail_last_seal().reason == LS_POWERFAIL_BROWNOUT);
    CHECK(ls_powerfail_last_seal().vcap_mv == 2100u);
    CHECK(ls_previous_boot_crashed());

    /* 3. Promoted envelope carries additive TLV 23; legacy skip-by-len holds. */
    sent = 0u;
    captured_length = 0u;
    CHECK(ls_flush() == LS_OK);
    CHECK(sent == 1u);
    CHECK(captured_length > 0u);
    tlv23_seen = 0u;
    tlv23_length = 0u;
    CHECK(ls_envelope_visit(captured, captured_length, count_tlv23, 0) == LS_OK);
    CHECK(tlv23_seen == 1u);
    CHECK(tlv23_length == 13u);

    /* 4. Double NMI seals once (dying rail must not torn-write). */
    make_storage(&storage);
    memset(backup_ram, 0, sizeof(backup_ram));
    ls_powerfail_install_backup(backup_ram, sizeof(backup_ram));
    CHECK(boot_runtime(&storage, &transport) == 0);
    ls_powerfail_seal(LS_POWERFAIL_PVD, 2500u);
    ls_powerfail_seal(LS_POWERFAIL_POWER_LOSS, 1000u);
    CHECK(ls_powerfail_sealed_read(&info));
    CHECK(info.reason == LS_POWERFAIL_PVD && info.vcap_mv == 2500u);

    /* 5. NONE reason never seals. */
    ls_powerfail_sealed_clear();
    ls_powerfail_seal(LS_POWERFAIL_NONE, 0u);
    CHECK(!ls_powerfail_sealed_read(&info));

    /* 6. Backup-RAM mirror recovers when .noinit died with the rail. */
    make_storage(&storage);
    memset(backup_ram, 0, sizeof(backup_ram));
    ls_powerfail_install_backup(backup_ram, sizeof(backup_ram));
    CHECK(boot_runtime(&storage, &transport) == 0);
    ls_powerfail_seal(LS_POWERFAIL_POWER_LOSS, 1800u);
    CHECK(ls_powerfail_sealed_read(&info));
    /* Simulate SRAM loss: wipe .noinit by re-sealing path is internal, so
     * simulate by clearing retained via recover+re-seal into backup only.
     * Host model: backup mirror already written; force retained invalid by
     * clearing retained magic through public clear + restoring backup. */
    {
        uint8_t saved[sizeof(backup_ram)];
        memcpy(saved, backup_ram, sizeof(saved));
        ls_powerfail_sealed_clear();
        memcpy(backup_ram, saved, sizeof(saved));
        /* Retained .noinit was cleared; backup mirror must still recover. */
        CHECK(boot_runtime(&storage, &transport) == 0);
        CHECK(ls_powerfail_last_seal().sealed_ok);
        CHECK(ls_powerfail_last_seal().reason == LS_POWERFAIL_POWER_LOSS);
    }

    /* 7. Disabled profile compiles out (host model: runtime flag path). */
    ls_boot_mark_successful();
    CHECK(!ls_powerfail_last_seal().sealed_ok);

    return 0;
}
