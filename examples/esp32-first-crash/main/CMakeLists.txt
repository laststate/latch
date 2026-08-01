file(GLOB_RECURSE LATCH_PORTABLE_SOURCES
    "${LATCH_ROOT}/src/core/*.c"
    "${LATCH_ROOT}/src/capture/*.c"
    "${LATCH_ROOT}/src/envelope/*.c"
    "${LATCH_ROOT}/src/spool/*.c"
    "${LATCH_ROOT}/src/storage/*.c"
    "${LATCH_ROOT}/src/transport/*.c"
    "${LATCH_ROOT}/src/metrics/*.c"
    "${LATCH_ROOT}/src/security/*.c")

idf_component_register(
    SRCS "main.c" "${LATCH_ROOT}/ports/esp-idf/esp_idf.c"
         "${LATCH_ROOT}/ports/esp-idf/esp_idf_panic.c"
         "${LATCH_ROOT}/arch/xtensa/xtensa.c" ${LATCH_PORTABLE_SOURCES}
    INCLUDE_DIRS "${LATCH_ROOT}/include" "${LATCH_ROOT}/ports/esp-idf"
                 "${LATCH_ROOT}/arch/xtensa"
    REQUIRES esp_partition esp_system esp_timer nvs_flash xtensa)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-misleading-indentation)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LS_MAX_EVENT_SIZE=768 LS_SPOOL_MAX_RECORDS=4
    LS_BREADCRUMB_CAPACITY=8 LS_METRIC_CAPACITY=8)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
