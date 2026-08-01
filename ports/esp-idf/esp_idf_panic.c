#include "sdkconfig.h"

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ARCH_XTENSA)
#include "esp_private/panic_internal.h"
#include "xtensa_context.h"

#include "xtensa.h"

void __real_esp_panic_handler(panic_info_t *info);

/* ESP-IDF 5.5 does not expose a stable application panic-hook API. The
   example opts into the GNU linker --wrap contract and pins this adapter to
   the private panic_info_t/XtExcFrame ABI. It only writes Latch's retained
   snapshot before delegating to the original ESP-IDF handler. */
void __wrap_esp_panic_handler(panic_info_t *info) {
    if (info && info->frame) {
        const XtExcFrame *source = (const XtExcFrame *)info->frame;
        ls_xtensa_frame_t frame = {
            .pc = (uint32_t)source->pc,
            .ps = (uint32_t)source->ps,
            .sar = (uint32_t)source->sar,
            .exccause = (uint32_t)source->exccause,
            .excvaddr = (uint32_t)source->excvaddr,
        };
        frame.a[0] = (uint32_t)source->a0;
        frame.a[1] = (uint32_t)source->a1;
        frame.a[2] = (uint32_t)source->a2;
        frame.a[3] = (uint32_t)source->a3;
        frame.a[4] = (uint32_t)source->a4;
        frame.a[5] = (uint32_t)source->a5;
        frame.a[6] = (uint32_t)source->a6;
        frame.a[7] = (uint32_t)source->a7;
        frame.a[8] = (uint32_t)source->a8;
        frame.a[9] = (uint32_t)source->a9;
        frame.a[10] = (uint32_t)source->a10;
        frame.a[11] = (uint32_t)source->a11;
        frame.a[12] = (uint32_t)source->a12;
        frame.a[13] = (uint32_t)source->a13;
        frame.a[14] = (uint32_t)source->a14;
        frame.a[15] = (uint32_t)source->a15;
        ls_xtensa_capture_minimal_frame(&frame);
    }
    __real_esp_panic_handler(info);
}
#endif
