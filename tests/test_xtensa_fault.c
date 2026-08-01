#include <stdio.h>
#include "xtensa.h"
#include "laststate/latch.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "xtensa check failed: %s:%d\n", #x, __LINE__);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    ls_xtensa_frame_t frame = {
        .pc = 0x400d1234u, .ps = 0x60020u, .sar = 7u, .exccause = 28u, .excvaddr = 0x3ffb1234u};
    for (unsigned index = 0; index < 16u; ++index)
        frame.a[index] = 0xa000u + index;
    ls_minimal_snapshot_clear();
    ls_capture_minimal_prepare();
    ls_xtensa_capture_minimal_frame(&frame);
    ls_minimal_snapshot_t snapshot;
    CHECK(ls_minimal_snapshot_read(&snapshot));
    CHECK(snapshot.version == LS_MINIMAL_SNAPSHOT_VERSION);
    CHECK(snapshot.architecture == LS_ARCH_XTENSA);
    CHECK(snapshot.pc == frame.pc && snapshot.lr == frame.a[0]);
    CHECK(snapshot.registers[15] == frame.a[15]);
    CHECK(snapshot.exccause == frame.exccause && snapshot.excvaddr == frame.excvaddr);
    ls_minimal_snapshot_clear();
    puts("xtensa retained fault tests passed");
    return 0;
}
