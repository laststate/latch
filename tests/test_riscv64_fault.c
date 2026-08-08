#include <stdio.h>
#include <string.h>

#include "riscv64.h"
#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "riscv64 check failed: %s at line %d\n", #condition, __LINE__);        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t retained[50000];
static uint8_t sent[LS_MAX_EVENT_SIZE];
static size_t sent_length;

static bool available(void *context) {
    (void)context;
    return true;
}

static size_t mtu(void *context) {
    (void)context;
    return sizeof sent;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (!data || length > sizeof sent) {
        return LS_EINVAL;
    }
    memcpy(sent, data, length);
    sent_length = length;
    return LS_OK;
}

static uint64_t read_u64(const uint8_t *data) {
    uint64_t value = 0u;
    for (unsigned index = 0u; index < 8u; ++index) {
        value |= (uint64_t)data[index] << (index * 8u);
    }
    return value;
}

typedef struct {
    const ls_riscv64_context_t *expected;
    bool complete;
    bool seen;
    bool valid;
} cpu64_visit_t;

static ls_result_t visit_cpu64(void *opaque, uint16_t type, const uint8_t *value, uint16_t length) {
    cpu64_visit_t *state = (cpu64_visit_t *)opaque;

    if (type != LS_TLV_CPU64) {
        return LS_OK;
    }
    state->seen = true;
    state->valid =
        length >= 4u && value[0] == 1u && value[2] == (uint8_t)LS_ARCH_RISCV64 && value[3] == 8u;
    if (!state->valid) {
        return LS_OK;
    }

    if (!state->complete) {
        state->valid = length == 4u && value[1] == 2u;
        return LS_OK;
    }

    state->valid = value[1] == 1u && length == 4u + (32u + 4u) * 8u;
    for (unsigned index = 0u; index < 32u && state->valid; ++index) {
        state->valid = read_u64(value + 4u + index * 8u) == state->expected->x[index];
    }
    if (state->valid) {
        const uint8_t *csr = value + 4u + 32u * 8u;
        state->valid = read_u64(csr) == state->expected->mstatus &&
                       read_u64(csr + 8u) == state->expected->mcause &&
                       read_u64(csr + 16u) == state->expected->mtval &&
                       read_u64(csr + 24u) == state->expected->mepc;
    }
    return LS_OK;
}

static int expect_cpu64(const ls_riscv64_context_t *expected, bool complete, bool truncated) {
    ls_envelope_info_t info;
    cpu64_visit_t state = {.expected = expected, .complete = complete};

    CHECK(sent_length != 0u);
    CHECK(ls_envelope_validate(sent, sent_length, &info) == LS_OK);
    CHECK(((info.flags & LS_ENVELOPE_TRUNCATED) != 0u) == truncated);
    CHECK(ls_envelope_visit(sent, sent_length, visit_cpu64, &state) == LS_OK);
    CHECK(state.seen && state.valid);
    return 0;
}

static void fill_context(ls_riscv64_context_t *context) {
    *context = (ls_riscv64_context_t){0};
    for (unsigned index = 0u; index < 32u; ++index) {
        context->x[index] = UINT64_C(0x1234000000000000) + index;
    }
    context->mstatus = UINT64_C(0x8000000000001800);
    context->mcause = UINT64_C(0x800000000000000d);
    context->mtval = UINT64_C(0xfeedface00001000);
    context->mepc = UINT64_C(0xffffffff80001234);
}

int main(void) {
    ls_riscv64_context_t source;
    ls_riscv64_saved_t saved;
    ls_minimal_snapshot_t minimal;
    ls_wide_context_snapshot_t wide;
    ls_identity_t identity = {
        .project_id = "riscv64", .device_id = "host", .firmware_build_id = "rv64test"};
    ls_config_t config = {.identity = &identity, .architecture = LS_ARCH_RISCV64};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {.name = "memory",
                                    .context = &memory,
                                    .capacity = sizeof retained,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t transport = {.name = "loopback",
                                        .priority = 1u,
                                        .available = available,
                                        .send = send_data,
                                        .max_payload = mtu};

    fill_context(&source);
    saved = source;
    ls_minimal_snapshot_clear();
    ls_riscv64_init();
    ls_riscv64_capture_minimal_from_saved(0);
    CHECK(!ls_minimal_snapshot_read(&minimal));
    ls_riscv64_capture_minimal_from_saved(&saved);
    CHECK(ls_minimal_snapshot_read(&minimal));
    CHECK(minimal.version == LS_MINIMAL_SNAPSHOT_VERSION);
    CHECK(minimal.architecture == LS_ARCH_RISCV64);
    CHECK((minimal.flags & LS_MINIMAL_SNAPSHOT_WIDE_CONTEXT_VALID) != 0u);
    CHECK(minimal.fault_sequence != 0u);
    CHECK(minimal.pc == (uint32_t)source.mepc && minimal.lr == (uint32_t)source.x[1]);
    CHECK(minimal.msp == (uint32_t)source.x[2] && minimal.excvaddr == (uint32_t)source.mtval);
    CHECK(ls_wide_context_snapshot_read(&wide));
    CHECK(wide.version == LS_WIDE_CONTEXT_SNAPSHOT_VERSION);
    CHECK(wide.fault_sequence == minimal.fault_sequence);
    CHECK(wide.context.x[31] == source.x[31] && wide.context.mepc == source.mepc);

    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    CHECK(!ls_minimal_snapshot_read(&minimal));
    CHECK(!ls_wide_context_snapshot_read(&wide));
    CHECK(ls_flush() == LS_OK);
    CHECK(expect_cpu64(&source, true, false) == 0);

    sent_length = 0u;
    CHECK(ls_capture_riscv64_context(&source) == LS_OK);
    CHECK(ls_flush() == LS_OK);
    CHECK(expect_cpu64(&source, true, false) == 0);

    {
        ls_arch_context_t incomplete = {
            .architecture = LS_ARCH_RISCV64,
            .fault = LS_FAULT_TRAP,
            .pc = (uint32_t)source.mepc,
            .lr = (uint32_t)source.x[1],
            .msp = (uint32_t)source.x[2],
            .mcause = (uint32_t)source.mcause,
            .mtval = (uint32_t)source.mtval,
        };
        sent_length = 0u;
        CHECK(ls_capture_cpu_context(&incomplete) == LS_OK);
        CHECK(ls_flush() == LS_OK);
        CHECK(expect_cpu64(0, false, true) == 0);
    }

    ls_minimal_snapshot_clear();
    puts("riscv64 retained fault tests passed");
    return 0;
}
