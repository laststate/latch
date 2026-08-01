#include <stdio.h>
#include <stddef.h>
#include "xtensa.h"
#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "xtensa check failed: %s:%d\n", #x, __LINE__);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t retained[50000];

static void refresh_snapshot_crcs(ls_minimal_snapshot_t *snapshot) {
    snapshot->crc = ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, crc));
    snapshot->extension_crc =
        ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));
    snapshot->context_crc = ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, context_crc));
}

static int test_snapshot_versions(const ls_minimal_snapshot_t *source) {
    ls_minimal_snapshot_t snapshot;

    CHECK(!ls_minimal_snapshot_validate(0));
    snapshot = *source;
    snapshot.magic = 0u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));

    snapshot = *source;
    snapshot.version = 1u;
    refresh_snapshot_crcs(&snapshot);
    CHECK(ls_minimal_snapshot_validate(&snapshot));
    CHECK(snapshot.fault == 0u && snapshot.architecture == 0u && snapshot.context_crc == 0u);
    snapshot = *source;
    snapshot.version = 1u;
    refresh_snapshot_crcs(&snapshot);
    snapshot.crc ^= 1u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));

    snapshot = *source;
    snapshot.version = 2u;
    refresh_snapshot_crcs(&snapshot);
    CHECK(ls_minimal_snapshot_validate(&snapshot));
    CHECK(snapshot.fault == (uint32_t)source->fault && snapshot.architecture == 0u &&
          snapshot.context_crc == 0u);
    snapshot = *source;
    snapshot.version = 2u;
    refresh_snapshot_crcs(&snapshot);
    snapshot.crc ^= 1u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));
    snapshot = *source;
    snapshot.version = 2u;
    refresh_snapshot_crcs(&snapshot);
    snapshot.extension_crc ^= 1u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));

    snapshot = *source;
    CHECK(ls_minimal_snapshot_validate(&snapshot));
    snapshot = *source;
    snapshot.crc ^= 1u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));
    snapshot = *source;
    snapshot.extension_crc ^= 1u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));
    snapshot = *source;
    snapshot.context_crc ^= 1u;
    CHECK(!ls_minimal_snapshot_validate(&snapshot));

    snapshot = *source;
    snapshot.version = LS_MINIMAL_SNAPSHOT_VERSION + 1u;
    refresh_snapshot_crcs(&snapshot);
    CHECK(!ls_minimal_snapshot_validate(&snapshot));
    return 0;
}

int main(void) {
    ls_xtensa_frame_t frame = {
        .pc = 0x400d1234u, .ps = 0x60020u, .sar = 7u, .exccause = 28u, .excvaddr = 0x3ffb1234u};
    for (unsigned index = 0; index < 16u; ++index)
        frame.a[index] = 0xa000u + index;
    ls_minimal_snapshot_clear();
    ls_identity_t identity = {
        .project_id = "xtensa-fault", .device_id = "host", .firmware_build_id = "xtensa01"};
    ls_config_t config = {.identity = &identity, .architecture = LS_ARCH_XTENSA};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {
        .name = "memory",
        .context = &memory,
        .capacity = sizeof retained,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    CHECK(ls_boot() == LS_OK);
    ls_xtensa_capture_frame(&frame);

    ls_capture_minimal_prepare();
    CHECK(ls_capture_minimal(0) == LS_EINVAL);
    CHECK(!ls_minimal_snapshot_read(0));
    ls_capture_minimal_context_fault(0);
    ls_xtensa_capture_minimal_frame(0);
    ls_xtensa_capture_frame(0);

    ls_arch_context_t direct = {
        .architecture = LS_ARCH_XTENSA,
        .fault = LS_FAULT_TRAP,
        .pc = frame.pc,
        .lr = frame.a[0],
        .has_fpu = true,
        .fpu_lazy = true,
    };
    CHECK(ls_capture_minimal(&direct) == LS_OK);
    ls_minimal_snapshot_t snapshot;
    CHECK(ls_minimal_snapshot_read(&snapshot));
    CHECK((snapshot.flags & (LS_MINIMAL_SNAPSHOT_FPU_FRAME | LS_MINIMAL_SNAPSHOT_FPU_LAZY)) ==
          (LS_MINIMAL_SNAPSHOT_FPU_FRAME | LS_MINIMAL_SNAPSHOT_FPU_LAZY));

    ls_xtensa_capture_minimal_frame(&frame);
    CHECK(ls_minimal_snapshot_read(&snapshot));
    CHECK(snapshot.version == LS_MINIMAL_SNAPSHOT_VERSION);
    CHECK(snapshot.architecture == LS_ARCH_XTENSA);
    CHECK(snapshot.pc == frame.pc && snapshot.lr == frame.a[0]);
    CHECK(snapshot.registers[15] == frame.a[15]);
    CHECK(snapshot.exccause == frame.exccause && snapshot.excvaddr == frame.excvaddr);
    CHECK(test_snapshot_versions(&snapshot) == 0);
    CHECK(ls_capture_minimal_recover() == LS_OK);
    CHECK(!ls_minimal_snapshot_read(&snapshot));
    direct.architecture = (ls_architecture_t)(LS_ARCH_LINUX + 1u);
    CHECK(ls_capture_minimal(&direct) == LS_OK);
    CHECK(ls_capture_minimal_recover() == LS_OK);
    ls_minimal_snapshot_clear();
    puts("xtensa retained fault tests passed");
    return 0;
}
