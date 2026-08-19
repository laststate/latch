// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/test_vendor_reset.c
//
// Vendor-specific reset tests. STM32, ESP32, Zephyr port hooks.
//
// Heap-free, bounded, deterministic.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "microchip_reset.h"
#include "nrf_reset.h"
#include "nxp_reset.h"
#include "silabs_reset.h"
#include "ti_reset.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "vendor reset check failed: %s:%d: %s\n", __FILE__, __LINE__,          \
                    #condition);                                                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int test_common_normalization(void) {
    typedef struct {
        uint32_t mask;
        ls_reset_reason_t reason;
    } reset_case_t;
    static const ls_reset_reason_masks_t masks = {
        .power_on_mask = 1u << 0,
        .pin_mask = 1u << 12,
        .software_mask = 1u << 1,
        .watchdog_mask = 1u << 2,
        .independent_watchdog_mask = 1u << 3,
        .window_watchdog_mask = 1u << 4,
        .brownout_mask = 1u << 5,
        .low_power_mask = 1u << 6,
        .lockup_mask = 1u << 7,
        .security_mask = 1u << 8,
        .bootloader_mask = 1u << 9,
        .firmware_update_mask = 1u << 10,
        .clock_failure_mask = 1u << 11,
    };
    static const reset_case_t cases[] = {
        {1u << 0, LS_RESET_POWER_ON},
        {1u << 1, LS_RESET_SOFTWARE},
        {1u << 2, LS_RESET_WATCHDOG},
        {1u << 3, LS_RESET_INDEPENDENT_WATCHDOG},
        {1u << 4, LS_RESET_WINDOW_WATCHDOG},
        {1u << 5, LS_RESET_BROWNOUT},
        {1u << 6, LS_RESET_LOW_POWER_WAKE},
        {1u << 7, LS_RESET_LOCKUP},
        {1u << 8, LS_RESET_SECURITY},
        {1u << 9, LS_RESET_BOOTLOADER},
        {1u << 10, LS_RESET_FIRMWARE_UPDATE},
        {1u << 11, LS_RESET_CLOCK_FAILURE},
        {1u << 12, LS_RESET_PIN},
    };
    ls_reset_mask_port_t port;
    volatile uint32_t status = 0u;
    volatile uint32_t clear = 0u;
    size_t index;

    CHECK(ls_reset_reason_from_masks(0x1234u, NULL).reason == LS_RESET_UNKNOWN);
    CHECK(ls_reset_reason_from_masks(0x1234u, NULL).raw_reason == 0x1234u);
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index)
        CHECK(ls_reset_reason_from_masks(cases[index].mask, &masks).reason == cases[index].reason);
    CHECK(ls_reset_reason_from_masks((1u << 1) | (1u << 5), &masks).reason == LS_RESET_BROWNOUT);
    CHECK(ls_reset_mask_port_info(NULL).reason == LS_RESET_UNKNOWN);
    port = (ls_reset_mask_port_t){
        .status_register = &status,
        .clear_register = &clear,
        .clear_mask = 0x5au,
        .masks = masks,
    };
    status = 1u << 7;
    CHECK(ls_reset_mask_port_info(&port).reason == LS_RESET_LOCKUP);
    port.status_register = NULL;
    CHECK(ls_reset_mask_port_info(&port).reason == LS_RESET_UNKNOWN);
    port.status_register = &status;
    ls_reset_mask_port_clear(NULL);
    port.clear_register = NULL;
    ls_reset_mask_port_clear(&port);
    CHECK(clear == 0u);
    port.clear_register = &clear;
    port.clear_mask = 0u;
    ls_reset_mask_port_clear(&port);
    CHECK(clear == 0u);
    port.clear_mask = 0x5au;
    ls_reset_mask_port_clear(&port);
    CHECK(clear == 0x5au);
    return 0;
}

static int test_nordic(void) {
    ls_nrf_reset_port_t port;
    volatile uint32_t resetreas = 0u;

    CHECK(ls_nrf_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_nrf_reset_profile_nrf52(NULL, &resetreas);
    ls_nrf_reset_profile_nrf52(&port, &resetreas);
    ls_nrf_reset_profile_nrf53(NULL, &resetreas);
    ls_nrf_reset_profile_nrf52(&port, NULL);
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_UNKNOWN);
    ls_nrf_reset_profile_nrf52(&port, &resetreas);
    resetreas = LS_NRF_RESETREAS_RESETPIN;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_PIN);
    resetreas = LS_NRF_RESETREAS_DOG;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_WATCHDOG);
    resetreas = LS_NRF_RESETREAS_SREQ;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_SOFTWARE);
    resetreas = LS_NRF_RESETREAS_LOCKUP;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_LOCKUP);
    resetreas = LS_NRF_RESETREAS_OFF | LS_NRF_RESETREAS_LPCOMP;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_LOW_POWER_WAKE);
    ls_nrf_reset_clear(NULL);
    ls_nrf_reset_clear(&port);
    CHECK(resetreas == (LS_NRF_RESETREAS_RESETPIN | LS_NRF_RESETREAS_DOG | LS_NRF_RESETREAS_SREQ |
                        LS_NRF_RESETREAS_LOCKUP | LS_NRF_RESETREAS_OFF | LS_NRF_RESETREAS_LPCOMP));
    ls_nrf_reset_profile_nrf53(&port, &resetreas);
    resetreas = LS_NRF53_RESETREAS_SREQ;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_SOFTWARE);
    resetreas = LS_NRF53_RESETREAS_LOCKUP;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_LOCKUP);
    resetreas = LS_NRF53_RESETREAS_DOG1;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_WATCHDOG);
    resetreas = LS_NRF53_RESETREAS_CTRLAP;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_SECURITY);
    resetreas = LS_NRF53_RESETREAS_VBUS;
    CHECK(ls_nrf_reset_info(&port).reason == LS_RESET_LOW_POWER_WAKE);
    ls_nrf_reset_clear(&port);
    CHECK(resetreas ==
          (LS_NRF53_RESETREAS_RESETPIN | LS_NRF53_RESETREAS_DOG0 | LS_NRF53_RESETREAS_CTRLAP |
           LS_NRF53_RESETREAS_SREQ | LS_NRF53_RESETREAS_LOCKUP | LS_NRF53_RESETREAS_OFF |
           LS_NRF53_RESETREAS_LPCOMP | LS_NRF53_RESETREAS_DIF | LS_NRF53_RESETREAS_LSREQ |
           LS_NRF53_RESETREAS_LLOCKUP | LS_NRF53_RESETREAS_LDOG | LS_NRF53_RESETREAS_MFORCEOFF |
           LS_NRF53_RESETREAS_NFC | LS_NRF53_RESETREAS_DOG1 | LS_NRF53_RESETREAS_VBUS |
           LS_NRF53_RESETREAS_LCTRLAP));
    return 0;
}

static int test_nxp(void) {
    ls_nxp_reset_port_t imxrt_port;
    ls_nxp_kinetis_reset_port_t kinetis_port;
    ls_reset_reason_masks_t masks = {
        .power_on_mask = 1u << 0,
        .watchdog_mask = 1u << 1,
        .clock_failure_mask = 1u << 2,
    };
    volatile uint32_t status = 0u;
    volatile uint32_t clear = 0u;
    volatile uint8_t srs0 = 0u;
    volatile uint8_t srs1 = 0u;

    CHECK(ls_nxp_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_nxp_reset_clear(NULL);
    ls_nxp_reset_port_init(NULL, &status, &clear, 0xa5u, masks);
    ls_nxp_reset_port_init(&imxrt_port, &status, &clear, 0xa5u, masks);
    status = 1u << 2;
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_CLOCK_FAILURE);
    ls_nxp_reset_clear(&imxrt_port);
    CHECK(clear == 0xa5u);
    ls_nxp_imxrt105x_reset_profile(NULL, &status);
    ls_nxp_imxrt105x_reset_profile(&imxrt_port, NULL);
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_UNKNOWN);
    ls_nxp_imxrt105x_reset_profile(&imxrt_port, &status);
    status = LS_NXP_IMXRT105X_SRSR_IPP_RESET_B;
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_PIN);
    status = LS_NXP_IMXRT105X_SRSR_JTAG_SW_RST;
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_SOFTWARE);
    status = LS_NXP_IMXRT105X_SRSR_CSU_RESET_B;
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_SECURITY);
    status = LS_NXP_IMXRT105X_SRSR_WDOG3_RST_B;
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_WATCHDOG);
    status = LS_NXP_IMXRT105X_SRSR_LOCKUP_SYSRESETREQ;
    CHECK(ls_nxp_reset_info(&imxrt_port).reason == LS_RESET_UNKNOWN);
    CHECK(ls_nxp_reset_info(&imxrt_port).raw_reason == LS_NXP_IMXRT105X_SRSR_LOCKUP_SYSRESETREQ);
    ls_nxp_reset_clear(&imxrt_port);
    CHECK(status == (LS_NXP_IMXRT105X_SRSR_IPP_RESET_B | LS_NXP_IMXRT105X_SRSR_LOCKUP_SYSRESETREQ |
                     LS_NXP_IMXRT105X_SRSR_CSU_RESET_B | LS_NXP_IMXRT105X_SRSR_IPP_USER_RESET_B |
                     LS_NXP_IMXRT105X_SRSR_WDOG_RST_B | LS_NXP_IMXRT105X_SRSR_JTAG_RST_B |
                     LS_NXP_IMXRT105X_SRSR_JTAG_SW_RST | LS_NXP_IMXRT105X_SRSR_WDOG3_RST_B));

    ls_nxp_kinetis_reset_profile(NULL, &srs0, &srs1);
    CHECK(ls_nxp_kinetis_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_nxp_kinetis_reset_profile(&kinetis_port, NULL, NULL);
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_UNKNOWN);
    ls_nxp_kinetis_reset_profile(&kinetis_port, &srs0, &srs1);
    srs0 = LS_NXP_KINETIS_SRS0_POR;
    srs1 = 0u;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_POWER_ON);
    srs0 = LS_NXP_KINETIS_SRS0_PIN;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_PIN);
    srs0 = LS_NXP_KINETIS_SRS0_LVD;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_BROWNOUT);
    srs0 = LS_NXP_KINETIS_SRS0_WDOG;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_WATCHDOG);
    srs0 = LS_NXP_KINETIS_SRS0_LOC;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_CLOCK_FAILURE);
    srs0 = 0u;
    srs1 = LS_NXP_KINETIS_SRS1_SW;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_SOFTWARE);
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).raw_reason ==
          ((uint32_t)LS_NXP_KINETIS_SRS1_SW << 8u));
    srs1 = LS_NXP_KINETIS_SRS1_LOCKUP;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_LOCKUP);
    kinetis_port.srs0_register = NULL;
    srs1 = 0u;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_UNKNOWN);
    kinetis_port.srs0_register = &srs0;
    kinetis_port.srs1_register = NULL;
    srs0 = LS_NXP_KINETIS_SRS0_LOL;
    CHECK(ls_nxp_kinetis_reset_info(&kinetis_port).reason == LS_RESET_CLOCK_FAILURE);
    return 0;
}

static int test_microchip(void) {
    ls_microchip_samd_reset_port_t samd_port;
    ls_microchip_sam_rsttyp_reset_port_t sam_port;
    volatile uint8_t rcause = 0u;
    volatile uint32_t rstc_status = 0u;

    CHECK(ls_microchip_samd_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_microchip_samd_reset_profile(NULL, &rcause);
    ls_microchip_samd_reset_profile(&samd_port, NULL);
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_UNKNOWN);
    ls_microchip_samd_reset_profile(&samd_port, &rcause);
    rcause = LS_MICROCHIP_SAMD_RCAUSE_BOD33;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_BROWNOUT);
    rcause = LS_MICROCHIP_SAMD_RCAUSE_WDT;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_WATCHDOG);
    rcause = LS_MICROCHIP_SAMD_RCAUSE_SYST;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_SOFTWARE);
    rcause = LS_MICROCHIP_SAMD_RCAUSE_EXT;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_PIN);
    rcause = LS_MICROCHIP_SAMD_RCAUSE_POR;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_POWER_ON);
    rcause = LS_MICROCHIP_SAMD_RCAUSE_BACKUP;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_LOW_POWER_WAKE);
    rcause = 1u << 3;
    CHECK(ls_microchip_samd_reset_info(&samd_port).reason == LS_RESET_UNKNOWN);

    CHECK(ls_microchip_sam_rsttyp_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_microchip_sam_rsttyp_reset_profile(NULL, &rstc_status);
    ls_microchip_sam_rsttyp_reset_profile(&sam_port, NULL);
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_UNKNOWN);
    ls_microchip_sam_rsttyp_reset_profile(&sam_port, &rstc_status);
    sam_port.reset_type_mask = 0u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_UNKNOWN);
    sam_port.reset_type_mask = 7u << 8;
    sam_port.reset_type_shift = 32u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_UNKNOWN);
    sam_port.reset_type_shift = 8u;
    rstc_status = LS_MICROCHIP_SAM_RSTTYP_BACKUP << 8u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_LOW_POWER_WAKE);
    rstc_status = LS_MICROCHIP_SAM_RSTTYP_WATCHDOG << 8u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_WATCHDOG);
    rstc_status = LS_MICROCHIP_SAM_RSTTYP_SOFTWARE << 8u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_SOFTWARE);
    rstc_status = LS_MICROCHIP_SAM_RSTTYP_USER << 8u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_PIN);
    rstc_status = LS_MICROCHIP_SAM_RSTTYP_GENERAL;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_UNKNOWN);
    rstc_status |= 1u;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_PIN);
    rstc_status = 5u << 8;
    CHECK(ls_microchip_sam_rsttyp_reset_info(&sam_port).reason == LS_RESET_UNKNOWN);
    return 0;
}

static int test_ti(void) {
    ls_ti_reset_port_t port;
    volatile uint32_t resc = 0u;
    volatile uint32_t clear = 0u;

    CHECK(ls_ti_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_ti_reset_clear(NULL);
    ls_ti_tiva_reset_profile(NULL, &resc);
    ls_ti_msp432e4_reset_profile(NULL, &resc);
    ls_ti_tiva_reset_profile(&port, NULL);
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_UNKNOWN);
    ls_ti_tiva_reset_profile(&port, &resc);
    resc = LS_TI_RESC_POR;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_POWER_ON);
    resc = LS_TI_RESC_EXT;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_PIN);
    resc = LS_TI_RESC_SW;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_SOFTWARE);
    resc = LS_TI_RESC_BOR | LS_TI_RESC_SW;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_BROWNOUT);
    resc = LS_TI_RESC_WDT1;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_WATCHDOG);
    resc = LS_TI_RESC_HIB;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_LOW_POWER_WAKE);
    ls_ti_msp432e4_reset_profile(&port, &resc);
    resc = LS_TI_RESC_MOSCFAIL;
    CHECK(ls_ti_reset_info(&port).reason == LS_RESET_CLOCK_FAILURE);
    port.clear_register = &clear;
    port.clear_mask = 0x1u;
    ls_ti_reset_clear(&port);
    CHECK(clear == 0x1u);
    return 0;
}

static int test_silabs(void) {
    ls_silabs_reset_port_t port;
    volatile uint32_t rstcause = 0u;
    volatile uint32_t command = 0u;

    CHECK(ls_silabs_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    ls_silabs_reset_clear(NULL);
    ls_silabs_efm32_series0_reset_profile(NULL, &rstcause, &command, 0x10u);
    ls_silabs_series2_reset_profile(NULL, &rstcause, &command, 0x20u);
    ls_silabs_efm32_series0_reset_profile(&port, NULL, &command, 0x10u);
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_UNKNOWN);
    ls_silabs_efm32_series0_reset_profile(&port, &rstcause, &command, 0x10u);
    rstcause = LS_SILABS_EFM32_SERIES0_PORST;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_POWER_ON);
    rstcause = LS_SILABS_EFM32_SERIES0_EXTRST;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_PIN);
    rstcause = LS_SILABS_EFM32_SERIES0_BODUNREGRST;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_BROWNOUT);
    rstcause = LS_SILABS_EFM32_SERIES0_WDOGRST;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_WATCHDOG);
    rstcause = LS_SILABS_EFM32_SERIES0_LOCKUPRST;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_LOCKUP);
    rstcause = LS_SILABS_EFM32_SERIES0_SYSREQRST;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_SOFTWARE);
    ls_silabs_reset_clear(&port);
    CHECK(command == 0x10u);
    ls_silabs_series2_reset_profile(&port, NULL, &command, 0x20u);
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_UNKNOWN);
    ls_silabs_series2_reset_profile(&port, &rstcause, &command, 0x20u);
    rstcause = LS_SILABS_SERIES2_POR;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_POWER_ON);
    rstcause = LS_SILABS_SERIES2_PIN;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_PIN);
    rstcause = LS_SILABS_SERIES2_WDOG0;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_WATCHDOG);
    rstcause = LS_SILABS_SERIES2_LOCKUP;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_LOCKUP);
    rstcause = LS_SILABS_SERIES2_SYSREQ;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_SOFTWARE);
    rstcause = LS_SILABS_SERIES2_SETAMPER;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_SECURITY);
    rstcause = LS_SILABS_SERIES2_EM4;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_LOW_POWER_WAKE);
    rstcause = LS_SILABS_SERIES2_DVDDLEBOD;
    CHECK(ls_silabs_reset_info(&port).reason == LS_RESET_BROWNOUT);
    ls_silabs_reset_clear(&port);
    CHECK(command == 0x20u);
    return 0;
}

int main(void) {
    CHECK(test_common_normalization() == 0);
    CHECK(test_nordic() == 0);
    CHECK(test_nxp() == 0);
    CHECK(test_microchip() == 0);
    CHECK(test_ti() == 0);
    CHECK(test_silabs() == 0);
    puts("vendor reset tests passed");
    return 0;
}
