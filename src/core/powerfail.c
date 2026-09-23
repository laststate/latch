// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/core/powerfail.c
//
// Last-Microjoule commit: NMI-safe power-fail seal plus normal-runtime
// promotion into the spool. The NMI path touches only retained memory and
// an optional VBAT-retained window with bounded stores; promotion,
// envelope encoding, storage, and transport stay in normal runtime.
//
// Heap-free, bounded, deterministic.

#include "internal.h"
#include "laststate/noinit.h"

#if LS_ENABLE_POWERFAIL_SEAL

#define LS_POWERFAIL_MAGIC 0x46575250u
#define LS_POWERFAIL_VERSION 1u
#define LS_POWERFAIL_BACKUP_MAGIC 0x42575250u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t reason;
    uint32_t vcap_mv;
    uint32_t tier;
    uint32_t boot_id;
    uint32_t fault;
    uint32_t pc;
    uint32_t lr;
    uint32_t msp;
    uint32_t psp;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t sequence;
    uint32_t crc;
} ls_powerfail_sealed_t;

_Static_assert(sizeof(ls_powerfail_sealed_t) <= 128u, "seal must stay single-shot");

static LS_NOINIT volatile ls_powerfail_sealed_t sealed_retained;
static uint8_t *backup_window;
static size_t backup_window_size;
static volatile uint32_t seal_sequence;

static uint32_t seal_crc_update(uint32_t crc, const uint8_t *data, size_t length) {
    while (length-- != 0u) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc >> 1u) ^ (0xedb88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc;
}

static void seal_copy_to_backup(const ls_powerfail_sealed_t *snapshot) {
    uint8_t *dst;
    const uint8_t *src;
    size_t length;

    if (!backup_window || backup_window_size < sizeof(*snapshot)) {
        return;
    }
    /* Direct bounded byte copy: no callbacks, no storage backend, no locks. */
    dst = backup_window;
    src = (const uint8_t *)(const void *)snapshot;
    length = sizeof(*snapshot);
    while (length-- != 0u) {
        *dst++ = *src++;
    }
}

static bool sealed_validate_copy(const ls_powerfail_sealed_t *copy) {
    uint32_t crc;
    const uint8_t *bytes;
    size_t length;
    uint32_t computed = 0xffffffffu;
    const uint32_t magic = LS_POWERFAIL_MAGIC;

    if (!copy || copy->magic != LS_POWERFAIL_MAGIC || copy->version != LS_POWERFAIL_VERSION) {
        return false;
    }
    if (copy->reason > (uint32_t)LS_POWERFAIL_PVD ||
        copy->tier > (uint32_t)LS_POWERFAIL_TIER_BACKUP_RAM) {
        return false;
    }
    bytes = (const uint8_t *)(const void *)&magic;
    for (length = 0; length < sizeof magic; ++length) {
        computed ^= bytes[length];
        for (unsigned bit = 0; bit < 8u; ++bit) {
            computed = (computed >> 1u) ^ (0xedb88320u & (uint32_t)(-(int32_t)(computed & 1u)));
        }
    }
    bytes = (const uint8_t *)(const void *)&copy->version;
    length = offsetof(ls_powerfail_sealed_t, crc) - offsetof(ls_powerfail_sealed_t, version);
    for (size_t index = 0; index < length; ++index) {
        computed ^= bytes[index];
        for (unsigned bit = 0; bit < 8u; ++bit) {
            computed = (computed >> 1u) ^ (0xedb88320u & (uint32_t)(-(int32_t)(computed & 1u)));
        }
    }
    crc = ~computed;
    return crc == copy->crc;
}

static bool sealed_read_retained(ls_powerfail_sealed_t *out) {
    ls_powerfail_sealed_t copy;
    volatile const uint8_t *source = (volatile const uint8_t *)(const void *)&sealed_retained;
    uint8_t *destination = (uint8_t *)(void *)&copy;
    ls_powerfail_sealed_t backup_copy;
    bool backup_valid = false;

    copy.magic = 0u;
    for (size_t index = 0; index < sizeof copy; ++index) {
        destination[index] = source[index];
    }
    if (sealed_validate_copy(&copy)) {
        ls_memcpy(out, &copy, sizeof copy);
        return true;
    }
    /* Fall back to the VBAT-retained mirror when the .noinit word died
     * with the rail but backup RAM survived. */
    if (backup_window && backup_window_size >= sizeof(backup_copy)) {
        ls_memcpy(&backup_copy, backup_window, sizeof backup_copy);
        if (backup_copy.magic == LS_POWERFAIL_MAGIC ||
            backup_copy.magic == LS_POWERFAIL_BACKUP_MAGIC) {
            uint32_t saved_magic = backup_copy.magic;
            backup_copy.magic = LS_POWERFAIL_MAGIC;
            backup_valid = sealed_validate_copy(&backup_copy);
            backup_copy.magic = saved_magic;
            if (backup_valid) {
                backup_copy.magic = LS_POWERFAIL_MAGIC;
                ls_memcpy(out, &backup_copy, sizeof backup_copy);
                return true;
            }
        }
    }
    return false;
}

void ls_powerfail_install_backup(uint8_t *ram, size_t size) {
    if (ram && size >= sizeof(ls_powerfail_sealed_t)) {
        backup_window = ram;
        backup_window_size = size;
    } else {
        backup_window = 0;
        backup_window_size = 0;
    }
}

void ls_powerfail_seal(ls_powerfail_reason_t reason, uint16_t vcap_mv) {
    volatile ls_powerfail_sealed_t *seal = &sealed_retained;
    ls_minimal_snapshot_t minimal;
    bool has_minimal = false;
    ls_powerfail_sealed_t snapshot = {0};
    uint32_t sequence;

    if (reason == LS_POWERFAIL_NONE) {
        return;
    }
    if (seal->magic == LS_POWERFAIL_MAGIC) {
        /* Second NMI during the same brownout: seal once and stop so a
         * dying rail cannot torn-write the committed record. */
        return;
    }
    /* ls_minimal_snapshot_read touches only retained memory and the stack
     * with bounded loops; safe to consult from the NMI path. */
    has_minimal = ls_minimal_snapshot_read(&minimal);

    sequence = seal_sequence + 1u;
    if (sequence == 0u) {
        sequence = 1u;
    }
    seal_sequence = sequence;

    snapshot.magic = 0u;
    snapshot.version = LS_POWERFAIL_VERSION;
    snapshot.reason = (uint32_t)reason;
    snapshot.vcap_mv = (uint32_t)vcap_mv;
    snapshot.tier = (backup_window && backup_window_size >= sizeof(snapshot))
                        ? (uint32_t)LS_POWERFAIL_TIER_BACKUP_RAM
                        : (uint32_t)LS_POWERFAIL_TIER_RAM;
    snapshot.boot_id = ls_runtime.boot_count;
    snapshot.fault = has_minimal ? minimal.fault : (uint32_t)LS_FAULT_UNKNOWN;
    snapshot.pc = has_minimal ? minimal.pc : 0u;
    snapshot.lr = has_minimal ? minimal.lr : 0u;
    snapshot.msp = has_minimal ? minimal.msp : 0u;
    snapshot.psp = has_minimal ? minimal.psp : 0u;
    snapshot.cfsr = has_minimal ? minimal.cfsr : 0u;
    snapshot.hfsr = has_minimal ? minimal.hfsr : 0u;
    snapshot.sequence = sequence;
    snapshot.crc = 0u;
    {
        const uint32_t magic = LS_POWERFAIL_MAGIC;
        uint32_t crc = 0xffffffffu;
        crc = seal_crc_update(crc, (const uint8_t *)(const void *)&magic, sizeof magic);
        crc = seal_crc_update(crc, (const uint8_t *)(const void *)&snapshot.version,
                              offsetof(ls_powerfail_sealed_t, crc) -
                                  offsetof(ls_powerfail_sealed_t, version));
        snapshot.crc = ~crc;
    }
    /* Commit order: payload, CRC, magic last. A torn NMI write leaves
     * magic != MAGIC and is ignored on recovery. Magic is the first
     * word, so copy the tail first, mirror to backup RAM, then commit. */
    {
        volatile uint8_t *dst = (volatile uint8_t *)(void *)&sealed_retained;
        const uint8_t *src = (const uint8_t *)(const void *)&snapshot;
        for (size_t index = sizeof snapshot.magic; index < sizeof snapshot; ++index) {
            dst[index] = src[index];
        }
        seal_copy_to_backup(&snapshot);
        seal->magic = LS_POWERFAIL_MAGIC;
        if (backup_window && backup_window_size >= sizeof(snapshot)) {
            /* Mirror magic last in backup RAM as well. */
            backup_window[offsetof(ls_powerfail_sealed_t, magic)] =
                (uint8_t)(LS_POWERFAIL_MAGIC & 0xffu);
            backup_window[offsetof(ls_powerfail_sealed_t, magic) + 1u] =
                (uint8_t)((LS_POWERFAIL_MAGIC >> 8) & 0xffu);
            backup_window[offsetof(ls_powerfail_sealed_t, magic) + 2u] =
                (uint8_t)((LS_POWERFAIL_MAGIC >> 16) & 0xffu);
            backup_window[offsetof(ls_powerfail_sealed_t, magic) + 3u] =
                (uint8_t)((LS_POWERFAIL_MAGIC >> 24) & 0xffu);
        }
    }
}

bool ls_powerfail_sealed_read(ls_powerfail_seal_info_t *out) {
    ls_powerfail_sealed_t sealed;

    if (!out) {
        return false;
    }
    if (!sealed_read_retained(&sealed)) {
        return false;
    }
    *out = (ls_powerfail_seal_info_t){
        .reason = (ls_powerfail_reason_t)sealed.reason,
        .tier = (ls_powerfail_tier_t)sealed.tier,
        .vcap_mv = (uint16_t)sealed.vcap_mv,
        .boot_id = sealed.boot_id,
        .fault = sealed.fault,
        .sealed_ok = true,
    };
    return true;
}

void ls_powerfail_sealed_clear(void) {
    sealed_retained.magic = 0u;
    if (backup_window && backup_window_size >= sizeof(ls_powerfail_sealed_t)) {
        backup_window[0] = 0u;
    }
}

ls_powerfail_seal_info_t ls_powerfail_last_seal(void) {
    if (ls_runtime.powerfail_last_valid) {
        return ls_runtime.powerfail_last;
    }
    return (ls_powerfail_seal_info_t){0};
}

ls_result_t ls_powerfail_recover(void) {
    ls_powerfail_sealed_t sealed;
    ls_arch_context_t context;
    ls_event_t event;
    ls_result_t result;

    if (!sealed_read_retained(&sealed)) {
        return LS_OK;
    }
    /* A valid seal proves the previous boot died mid-flight. */
    ls_runtime.previous_crashed = true;
    if (!ls_runtime.storage) {
        return LS_EAGAIN;
    }
    context = (ls_arch_context_t){
        .architecture = ls_runtime.config.architecture,
        .fault = (ls_fault_kind_t)sealed.fault,
        .lr = sealed.lr,
        .pc = sealed.pc,
        .msp = sealed.msp,
        .psp = sealed.psp,
        .cfsr = sealed.cfsr,
        .hfsr = sealed.hfsr,
        .fault_address = sealed.cfsr,
    };
    ls_runtime.powerfail_last = (ls_powerfail_seal_info_t){
        .reason = (ls_powerfail_reason_t)sealed.reason,
        .tier = (ls_powerfail_tier_t)sealed.tier,
        .vcap_mv = (uint16_t)sealed.vcap_mv,
        .boot_id = sealed.boot_id,
        .fault = sealed.fault,
        .sealed_ok = true,
    };
    ls_runtime.powerfail_last_valid = true;
    event = (ls_event_t){
        .type = LS_EVENT_RESET,
        .priority = LS_PRIORITY_EMERGENCY,
        .timestamp_ms = ls_uptime_ms(),
        .fingerprint = (uint32_t)(sealed.reason ^ (sealed.vcap_mv << 8u) ^ sealed.sequence),
        .domain = "powerfail",
        .code = (int32_t)sealed.reason,
        .severity = LS_SEVERITY_FATAL,
        .message = "brownout_seal",
        .cpu = &context,
        .capture_level = LS_CAPTURE_SNAPSHOT,
    };
    result = ls_capture_event(&event);
    if (result == LS_OK) {
        ls_powerfail_sealed_clear();
    } else {
        ls_runtime.powerfail_last_valid = false;
    }
    return result;
}

#else

void ls_powerfail_install_backup(uint8_t *ram, size_t size) {
    (void)ram;
    (void)size;
}

void ls_powerfail_seal(ls_powerfail_reason_t reason, uint16_t vcap_mv) {
    (void)reason;
    (void)vcap_mv;
}

ls_powerfail_seal_info_t ls_powerfail_last_seal(void) {
    return (ls_powerfail_seal_info_t){0};
}

bool ls_powerfail_sealed_read(ls_powerfail_seal_info_t *out) {
    (void)out;
    return false;
}

void ls_powerfail_sealed_clear(void) {
}

ls_result_t ls_powerfail_recover(void) {
    return LS_OK;
}

#endif
