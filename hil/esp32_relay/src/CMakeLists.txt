set(LATCH_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

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
    SRCS
        "main.c"
        "${LATCH_ROOT}/ports/esp-idf/esp_idf.c"
        ${LATCH_PORTABLE_SOURCES}
    INCLUDE_DIRS
        "${LATCH_ROOT}/include"
        "${LATCH_ROOT}/ports/esp-idf"
    REQUIRES
        driver
        esp_partition
        esp_system)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wno-misleading-indentation)
