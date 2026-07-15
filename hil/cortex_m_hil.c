#include "cortex_m_hil.h"
#include "../arch/cortex-m/cortex_m.h"
static void emit(const ls_hil_platform_t *platform, const char *line) {
    if (platform && platform->emit)
        platform->emit(platform->context, line);
}
void ls_hil_arm_brownout(const ls_hil_platform_t *platform) {
    emit(platform, "HIL:ARMED:BROWNOUT");
}
void ls_hil_trigger_watchdog(const ls_hil_platform_t *platform, uint32_t timeout_ms) {
    if (!platform || !platform->start_watchdog)
        return;
    emit(platform, "HIL:ARMED:WATCHDOG");
    platform->start_watchdog(platform->context, timeout_ms);
    for (;;) {
    }
}
ls_result_t ls_hil_trigger_mpu(uintptr_t protected_address, uint8_t region) {
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    if (region >= 8u || !protected_address)
        return LS_EINVAL;
    *(volatile uint32_t *)(uintptr_t)0xe000ed98u = region;
    *(volatile uint32_t *)(uintptr_t)0xe000ed9cu = (uint32_t)(protected_address & ~(uintptr_t)31u);
    *(volatile uint32_t *)(uintptr_t)0xe000eda0u = (1u << 28) | (4u << 1) | 1u;
    *(volatile uint32_t *)(uintptr_t)0xe000ed94u |= 5u;
    __asm volatile("dsb 0xf\n isb 0xf" ::: "memory");
    *(volatile uint32_t *)protected_address = 0xdeadbeefu;
    return LS_EIO;
#elif defined(__ARM_ARCH_8M_MAIN__)
    if (region >= 16u || !protected_address)
        return LS_EINVAL;
    *(volatile uint32_t *)(uintptr_t)0xe000ed98u = region;
    *(volatile uint32_t *)(uintptr_t)0xe000edc0u = 0x44u;
    *(volatile uint32_t *)(uintptr_t)0xe000ed9cu =
        ((uint32_t)protected_address & ~31u) | (2u << 1) | 1u;
    *(volatile uint32_t *)(uintptr_t)0xe000eda0u =
        (((uint32_t)protected_address + 31u) & ~31u) | 1u;
    *(volatile uint32_t *)(uintptr_t)0xe000ed94u |= 5u;
    __asm volatile("dsb 0xf\n isb 0xf" ::: "memory");
    *(volatile uint32_t *)protected_address = 0xdeadbeefu;
    return LS_EIO;
#else
    (void)protected_address;
    (void)region;
    return LS_ENOTSUP;
#endif
}
ls_result_t ls_hil_trigger_trustzone(uintptr_t secure_address) {
#if defined(__ARM_ARCH_8M_MAIN__)
    *(volatile uint32_t *)secure_address = 0x51ac7e5au;
    return LS_EIO;
#else
    (void)secure_address;
    return LS_ENOTSUP;
#endif
}
void ls_hil_trigger_fpu_lazy_fault(void) {
#if defined(__ARM_FP) && (__ARM_FP != 0)
    ls_cortex_m_configure_fpu_lazy_stacking(true);
    __asm volatile("vmov s0, s1");
    *(volatile uint32_t *)(uintptr_t)0xfffffffc = 0;
#endif
}
ls_result_t ls_hil_test_flash(ls_storage_backend_t *flash, size_t offset, size_t erase_length,
                              uint32_t pattern) {
    if (!flash || !flash->erase || !flash->write || !flash->read || erase_length < sizeof pattern)
        return LS_EINVAL;
    ls_result_t result = flash->erase(flash->context, offset, erase_length);
    if (result == LS_OK)
        result = flash->write(flash->context, offset, &pattern, sizeof pattern);
    uint32_t readback = 0;
    if (result == LS_OK)
        result = flash->read(flash->context, offset, &readback, sizeof readback);
    if (result == LS_OK && readback != pattern)
        result = LS_ECORRUPT;
    if (result == LS_OK && flash->sync)
        result = flash->sync(flash->context);
    return result;
}
