if(ESP_PLATFORM)
  # ESP-IDF Component Manager evaluates the component root CMake file inside
  # an ESP-IDF project. Keep this path explicit: the public component ships
  # the portable runtime and reset normalization only. The private/versioned
  # ESP-IDF panic wrapper and Xtensa fault adapter stay opt-in in the physical
  # crash example because their ABI must be qualified per ESP-IDF release.
  set(LS_IDF_PORTABLE_SOURCES
    src/core/runtime.c src/core/util.c src/core/memory.c src/core/health.c
    src/core/peripheral.c src/core/assert.c src/core/log.c src/core/performance.c
    src/core/boot.c src/core/build_id.c src/core/policy.c src/core/blackbox.c
    src/core/mission.c src/core/time_sync.c src/core/fingerprint.c src/core/supervisor.c
    src/core/trace.c src/core/provisioning.c src/core/selftest.c src/core/fault_injection.c src/core/ota.c src/core/environment.c
    src/capture/capture.c src/envelope/lep.c src/envelope/compression.c
    src/spool/spool.c src/storage/memory_storage.c src/storage/storage_sim.c
    src/storage/flash_mirror.c src/storage/wear_level.c src/storage/secure_storage.c
    src/transport/transport.c src/transport/stream.c src/transport/network.c
    src/metrics/breadcrumbs.c src/metrics/metrics.c src/security/sha256.c
    src/security/chacha20.c src/security/poly1305.c src/security/aead.c
    src/security/provider.c src/security/secure_element.c)
  idf_component_register(
    SRCS ${LS_IDF_PORTABLE_SOURCES} ports/esp-idf/esp_idf.c
    INCLUDE_DIRS include ports/esp-idf
    REQUIRES esp_system)
  target_compile_features(${COMPONENT_LIB} PUBLIC c_std_11)
  target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-misleading-indentation)
  if(LS_COMMERCIAL_PROFILE)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER=1 LS_MIN_CRYPTO_ASSURANCE=3)
  endif()
  return()
endif()

cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0184)
  cmake_policy(SET CMP0184 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_CHECKS "$<$<BOOL:0>:StackFrameErrorCheck>")
project(latch VERSION 0.5.0 LANGUAGES C CXX)
set(LS_TOP_LEVEL_DEFAULT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(LS_TOP_LEVEL_DEFAULT ON)
endif()
option(LS_BUILD_TESTS "Build host tests" ${LS_TOP_LEVEL_DEFAULT})
option(LS_FREESTANDING_CHECK "Compile the portable runtime without hosted assumptions" ${LS_TOP_LEVEL_DEFAULT})
option(LS_BUILD_CORTEX_M "Build Cortex-M port with an ARM toolchain" OFF)
option(LS_BUILD_RISCV "Build RV32 trap port with a RISC-V toolchain" OFF)
option(LS_BUILD_RISCV64 "Build RV64 trap port with a RISC-V toolchain" OFF)
option(LS_BUILD_XTENSA "Build Xtensa frame adapter" OFF)
option(LS_BUILD_LINUX "Build Linux signal and file ports" OFF)
option(LS_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
option(LS_BUILD_TOOLS "Build host LEP tools" OFF)
option(LS_BUILD_BENCHMARKS "Build host benchmarks" OFF)
option(LS_BUILD_EXAMPLES "Build host examples" OFF)
option(LS_BUILD_MBEDTLS_PORT "Build the mbedTLS session transport adapter" OFF)
option(LS_BUILD_CRYPTOAUTHLIB_PORT "Build the Microchip CryptoAuthLib secure-element adapter" OFF)
option(LS_TEST_LIBSODIUM "Cross-check XChaCha20-Poly1305 against libsodium" OFF)
option(LS_COMMERCIAL_PROFILE "Fail closed unless an externally-audited crypto provider is installed" OFF)
option(LS_BUILD_LIBSODIUM_PROVIDER "Build the optional libsodium crypto-provider adapter" OFF)
unset(LS_TOP_LEVEL_DEFAULT)
include(GNUInstallDirs)
include(cmake/GenerateBuildId.cmake)

# Allow feature-profile values supplied with -DLS_*=... to reach every target.
# Defaults remain in config.h when a value is not provided by the integrator.
set(LS_CONFIGURABLE_DEFINITIONS
  LS_CONSTRAINED_PROFILE LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER LS_MIN_CRYPTO_ASSURANCE LS_STORE_STRINGS LS_ENABLE_BREADCRUMBS LS_ENABLE_METRICS LS_ENABLE_LOGS
  LS_ENABLE_POWER_SAMPLES LS_ENABLE_PERFORMANCE LS_ENABLE_DUMPS LS_ENABLE_ASSERTS
  LS_ENABLE_WIDE_CONTEXT
  LS_ENABLE_STACK_SNAPSHOT LS_COMPILED_MIN_LEVEL LS_MAX_EVENT_SIZE
  LS_BREADCRUMB_CAPACITY LS_BREADCRUMB_MESSAGE_MAX LS_BREADCRUMB_CATEGORY_MAX
  LS_BREADCRUMB_KV_MAX LS_METRIC_CAPACITY LS_METRIC_NAME_MAX LS_METRIC_WINDOW_SIZE
  LS_POWER_SAMPLE_CAPACITY LS_HEALTH_CAPACITY LS_SPAN_CAPACITY LS_LOG_ARG_MAX
  LS_MAX_TRANSPORTS LS_MAX_DUMP_REGIONS LS_MAX_REDACTIONS LS_DUMP_REGION_MAX_BYTES
  LS_STACK_SNAPSHOT_MAX LS_SPOOL_MAX_RECORDS LS_SPOOL_RESERVED_CRITICAL LS_SPOOL_RESERVED_EMERGENCY LS_EMERGENCY_STACK_SIZE
  LS_BOOT_LOOP_THRESHOLD LS_BOOT_LOOP_WINDOW_MS LS_BUILD_ID_MAX LS_POLICY_CAPACITY
  LS_DEDUP_CAPACITY LS_DEDUP_WINDOW_MS LS_BLACKBOX_CAPACITY LS_BLACKBOX_ENVELOPE_MAX LS_SELFTEST_CAPACITY LS_FAULT_INJECTION_CAPACITY LS_MISSION_ID_MAX LS_DIVE_ID_MAX LS_NODE_ID_MAX LS_VEHICLE_MODE_MAX)
foreach(definition IN LISTS LS_CONFIGURABLE_DEFINITIONS)
  if(DEFINED ${definition})
    if("${${definition}}" STREQUAL "ON")
      set(ls_definition_value 1)
    elseif("${${definition}}" STREQUAL "OFF")
      set(ls_definition_value 0)
    else()
      set(ls_definition_value "${${definition}}")
    endif()
    add_compile_definitions(${definition}=${ls_definition_value})
  endif()
endforeach()
unset(ls_definition_value)

if(LS_COMMERCIAL_PROFILE)
  add_compile_definitions(LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER=1 LS_MIN_CRYPTO_ASSURANCE=3)
endif()

set(LS_CORE_SOURCES src/core/runtime.c src/core/util.c src/core/memory.c src/core/health.c src/core/peripheral.c src/core/assert.c src/core/log.c src/core/performance.c src/core/boot.c src/core/build_id.c src/core/policy.c src/core/blackbox.c src/core/mission.c src/core/time_sync.c src/core/fingerprint.c src/core/supervisor.c src/core/trace.c src/core/provisioning.c src/core/selftest.c src/core/fault_injection.c src/core/ota.c src/core/environment.c)
set(LS_CAPTURE_SOURCES src/capture/capture.c)
set(LS_ENVELOPE_SOURCES src/envelope/lep.c src/envelope/compression.c)
set(LS_SPOOL_SOURCES src/spool/spool.c)
set(LS_STORAGE_SOURCES src/storage/memory_storage.c src/storage/storage_sim.c src/storage/flash_mirror.c src/storage/wear_level.c src/storage/secure_storage.c)
set(LS_TRANSPORT_SOURCES src/transport/transport.c src/transport/stream.c src/transport/network.c)
set(LS_METRICS_SOURCES src/metrics/breadcrumbs.c src/metrics/metrics.c)
set(LS_SECURITY_SOURCES src/security/sha256.c src/security/chacha20.c src/security/poly1305.c src/security/aead.c src/security/provider.c src/security/secure_element.c)
set(LS_ALL_PORTABLE_SOURCES ${LS_CORE_SOURCES} ${LS_CAPTURE_SOURCES} ${LS_ENVELOPE_SOURCES} ${LS_SPOOL_SOURCES} ${LS_STORAGE_SOURCES} ${LS_TRANSPORT_SOURCES} ${LS_METRICS_SOURCES} ${LS_SECURITY_SOURCES})

function(ls_configure_target target)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_definitions(${target} PUBLIC LS_HAS_GENERATED_BUILD_ID=1)
  target_compile_features(${target} PUBLIC c_std_11)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-misleading-indentation)
  endif()
endfunction()

function(ls_add_module name output)
  add_library(${name}_obj OBJECT ${ARGN})
  ls_configure_target(${name}_obj)
  add_library(${name} STATIC $<TARGET_OBJECTS:${name}_obj>)
  set_target_properties(${name} PROPERTIES OUTPUT_NAME ${output})
  ls_configure_target(${name})
endfunction()

function(ls_configure_port_target target port_directory)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${port_directory}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/laststate/${port_directory}>)
  ls_configure_target(${target})
endfunction()

ls_add_module(latch-core latch-core ${LS_CORE_SOURCES})
ls_add_module(latch-capture latch-capture ${LS_CAPTURE_SOURCES})
ls_add_module(latch-envelope latch-envelope ${LS_ENVELOPE_SOURCES})
ls_add_module(latch-spool latch-spool ${LS_SPOOL_SOURCES})
ls_add_module(latch-storage-memory latch-storage-memory ${LS_STORAGE_SOURCES})
ls_add_module(latch-transport latch-transport ${LS_TRANSPORT_SOURCES})
ls_add_module(latch-metrics latch-metrics ${LS_METRICS_SOURCES})
ls_add_module(latch-security latch-security ${LS_SECURITY_SOURCES})

add_library(latch STATIC
  $<TARGET_OBJECTS:latch-core_obj> $<TARGET_OBJECTS:latch-capture_obj>
  $<TARGET_OBJECTS:latch-envelope_obj> $<TARGET_OBJECTS:latch-spool_obj>
  $<TARGET_OBJECTS:latch-storage-memory_obj> $<TARGET_OBJECTS:latch-transport_obj>
  $<TARGET_OBJECTS:latch-metrics_obj> $<TARGET_OBJECTS:latch-security_obj>)
ls_configure_target(latch)
add_library(laststate::latch ALIAS latch)
add_library(latch-cpp INTERFACE)
target_include_directories(latch-cpp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(latch-cpp INTERFACE latch)
add_library(laststate::latch-cpp ALIAS latch-cpp)

if(LS_BUILD_LIBSODIUM_PROVIDER)
  find_path(SODIUM_INCLUDE_DIR sodium.h REQUIRED)
  find_library(SODIUM_LIBRARY NAMES sodium libsodium REQUIRED)
  add_library(latch-crypto-libsodium STATIC ports/libsodium/libsodium_provider.c)
  ls_configure_target(latch-crypto-libsodium)
  target_include_directories(latch-crypto-libsodium PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ports/libsodium> PRIVATE ${SODIUM_INCLUDE_DIR})
  target_link_libraries(latch-crypto-libsodium PUBLIC latch-security PRIVATE ${SODIUM_LIBRARY})
endif()

add_library(latch-port-stm32 STATIC ports/stm32/stm32_reset.c)
ls_configure_port_target(latch-port-stm32 ports/stm32)
add_library(latch-port-rp STATIC ports/rp2040/rp_reset.c)
ls_configure_port_target(latch-port-rp ports/rp2040)
add_library(latch-port-esp-idf STATIC ports/esp-idf/esp_idf.c)
ls_configure_port_target(latch-port-esp-idf ports/esp-idf)
add_library(latch-port-freertos STATIC ports/freertos/freertos_latch.c)
ls_configure_port_target(latch-port-freertos ports/freertos)
add_library(latch-port-zephyr STATIC ports/zephyr/zephyr_latch.c ports/zephyr/zephyr_ble_gatt.c ports/zephyr/zephyr_can.c ports/zephyr/zephyr_lorawan.c ports/zephyr/zephyr_tls_socket.c)
ls_configure_port_target(latch-port-zephyr ports/zephyr)
add_library(latch-port-reset-common STATIC ports/common/reset_reason.c)
ls_configure_port_target(latch-port-reset-common ports/common)
add_library(latch-port-nordic STATIC ports/nordic/nrf_reset.c)
ls_configure_port_target(latch-port-nordic ports/nordic)
target_link_libraries(latch-port-nordic PUBLIC latch-port-reset-common)
add_library(latch-port-nxp STATIC ports/nxp/nxp_reset.c)
ls_configure_port_target(latch-port-nxp ports/nxp)
target_link_libraries(latch-port-nxp PUBLIC latch-port-reset-common)
add_library(latch-port-microchip STATIC ports/microchip/microchip_reset.c)
ls_configure_port_target(latch-port-microchip ports/microchip)
add_library(latch-port-ti STATIC ports/ti/ti_reset.c)
ls_configure_port_target(latch-port-ti ports/ti)
target_link_libraries(latch-port-ti PUBLIC latch-port-reset-common)
add_library(latch-port-silabs STATIC ports/silabs/silabs_reset.c)
ls_configure_port_target(latch-port-silabs ports/silabs)
target_link_libraries(latch-port-silabs PUBLIC latch-port-reset-common)
if(LS_BUILD_MBEDTLS_PORT)
  find_package(MbedTLS QUIET)
  if(NOT TARGET MbedTLS::mbedtls)
    find_path(MBEDTLS_INCLUDE_DIR mbedtls/ssl.h REQUIRED)
    find_library(MBEDTLS_LIBRARY mbedtls REQUIRED)
    find_library(MBEDX509_LIBRARY mbedx509 REQUIRED)
    find_library(MBEDCRYPTO_LIBRARY mbedcrypto REQUIRED)
    add_library(MbedTLS::mbedtls UNKNOWN IMPORTED)
    set_target_properties(MbedTLS::mbedtls PROPERTIES IMPORTED_LOCATION "${MBEDTLS_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${MBEDTLS_INCLUDE_DIR}")
    add_library(MbedTLS::mbedx509 UNKNOWN IMPORTED)
    set_target_properties(MbedTLS::mbedx509 PROPERTIES IMPORTED_LOCATION "${MBEDX509_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${MBEDTLS_INCLUDE_DIR}")
    add_library(MbedTLS::mbedcrypto UNKNOWN IMPORTED)
    set_target_properties(MbedTLS::mbedcrypto PROPERTIES IMPORTED_LOCATION "${MBEDCRYPTO_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${MBEDTLS_INCLUDE_DIR}")
  endif()
  add_library(latch-port-mbedtls STATIC ports/mbedtls/mbedtls_transport.c)
  target_include_directories(latch-port-mbedtls PUBLIC ports/mbedtls)
  target_link_libraries(latch-port-mbedtls PUBLIC latch MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)
  ls_configure_target(latch-port-mbedtls)
endif()
if(LS_BUILD_CRYPTOAUTHLIB_PORT)
  find_package(cryptoauthlib CONFIG REQUIRED)
  add_library(latch-port-cryptoauthlib STATIC ports/cryptoauthlib/cryptoauthlib_adapter.c)
  target_include_directories(latch-port-cryptoauthlib PUBLIC ports/cryptoauthlib)
  target_link_libraries(latch-port-cryptoauthlib PUBLIC latch cryptoauth)
  ls_configure_target(latch-port-cryptoauthlib)
endif()

if(LS_BUILD_CORTEX_M)
  enable_language(ASM)
  add_library(latch-cortex-m STATIC arch/cortex-m/cortex_m.c arch/cortex-m/cortex_m_fault.S hil/cortex_m_hil.c)
  target_include_directories(latch-cortex-m PUBLIC arch/cortex-m hil)
  ls_configure_target(latch-cortex-m)
endif()
if(LS_BUILD_RISCV)
  enable_language(ASM)
  add_library(latch-riscv STATIC arch/riscv/riscv.c arch/riscv/riscv_trap.S)
  target_include_directories(latch-riscv PUBLIC arch/riscv)
  ls_configure_target(latch-riscv)
endif()
if(LS_BUILD_RISCV64)
  if(NOT LS_ENABLE_WIDE_CONTEXT)
    message(FATAL_ERROR "LS_BUILD_RISCV64 requires -DLS_ENABLE_WIDE_CONTEXT=1")
  endif()
  enable_language(ASM)
  add_library(latch-riscv64 STATIC arch/riscv64/riscv64.c arch/riscv64/riscv64_trap.S)
  target_include_directories(latch-riscv64 PUBLIC arch/riscv64)
  ls_configure_target(latch-riscv64)
endif()
if(LS_BUILD_XTENSA)
  add_library(latch-xtensa STATIC arch/xtensa/xtensa.c)
  target_include_directories(latch-xtensa PUBLIC arch/xtensa)
  ls_configure_target(latch-xtensa)
endif()
if(LS_BUILD_LINUX)
  add_library(latch-linux STATIC arch/linux/linux.c ports/embedded-linux/file_backend.c)
  target_include_directories(latch-linux PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/arch/linux>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ports/embedded-linux>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/laststate/arch/linux>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/laststate/ports/embedded-linux>)
  ls_configure_target(latch-linux)
endif()

if(LS_FREESTANDING_CHECK)
  add_library(latch-freestanding-check OBJECT ${LS_ALL_PORTABLE_SOURCES})
  ls_configure_target(latch-freestanding-check)
  target_compile_definitions(latch-freestanding-check PRIVATE LS_STORE_STRINGS=0)
  if(MSVC)
    target_compile_options(latch-freestanding-check PRIVATE /Zl /GS- /Gs9999999 /Oi- /Od)
    set_property(TARGET latch-freestanding-check PROPERTY MSVC_RUNTIME_CHECKS "$<$<BOOL:0>:StackFrameErrorCheck>")
  else()
    target_compile_options(latch-freestanding-check PRIVATE -ffreestanding -fno-builtin -fno-stack-protector -fno-profile-arcs -fno-test-coverage)
  endif()
  set(LS_CAN_LINK_FREESTANDING TRUE)
  if(CMAKE_C_FLAGS MATCHES "--coverage|-fprofile-arcs|-fsanitize" OR CMAKE_CXX_FLAGS MATCHES "--coverage|-fprofile-arcs|-fsanitize")
    set(LS_CAN_LINK_FREESTANDING FALSE)
  endif()
  if(LS_CAN_LINK_FREESTANDING AND (MSVC OR (CMAKE_SYSTEM_NAME STREQUAL "Linux")))
    add_executable(latch-freestanding-link tests/freestanding_link.c $<TARGET_OBJECTS:latch-freestanding-check>)
    target_include_directories(latch-freestanding-link PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/generated)
    if(MSVC)
      target_compile_options(latch-freestanding-link PRIVATE /Zl /GS- /Gs9999999)
      set_property(TARGET latch-freestanding-link PROPERTY MSVC_RUNTIME_CHECKS "$<$<BOOL:0>:StackFrameErrorCheck>")
      target_link_options(latch-freestanding-link PRIVATE /NODEFAULTLIB /ENTRY:ls_freestanding_entry /SUBSYSTEM:CONSOLE)
    else()
    target_compile_options(latch-freestanding-link PRIVATE -ffreestanding -fno-builtin -fno-stack-protector -fno-profile-arcs -fno-test-coverage)
    target_link_options(latch-freestanding-link PRIVATE -fno-profile-arcs -fno-test-coverage -nostdlib -no-pie -Wl,--no-undefined -Wl,-e,ls_freestanding_entry)
    endif()
  endif()
endif()

if(LS_BUILD_TESTS)
  enable_testing()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  foreach(python_test footprint release-metadata release-qualification embedded-packaging fleet-tools safety-c)
    string(REPLACE "-" "_" python_source ${python_test})
    add_test(NAME latch-${python_test}-tests
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${python_source}.py)
  endforeach()
  add_test(NAME latch-release-metadata-current
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_release_metadata.py)
  add_test(NAME latch-hil-matrix-current
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_hil_matrix.py)
  add_test(NAME latch-esp-idf-component-layout
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/tests/idf-component
      -B ${CMAKE_CURRENT_BINARY_DIR}/idf-component-layout)
  add_executable(latch-tests tests/test_latch.c)
  target_link_libraries(latch-tests PRIVATE latch)
  add_test(NAME latch-c-tests COMMAND latch-tests)
  if(NOT CMAKE_CROSSCOMPILING)
    find_package(Threads REQUIRED)
    add_executable(latch-concurrency-tests tests/test_concurrency.c)
    target_link_libraries(latch-concurrency-tests PRIVATE latch Threads::Threads)
    add_test(NAME latch-concurrency-tests COMMAND latch-concurrency-tests)
  endif()
  add_executable(latch-cpp-tests tests/test_cpp.cpp)
  target_link_libraries(latch-cpp-tests PRIVATE latch-cpp)
  target_compile_features(latch-cpp-tests PRIVATE cxx_std_17)
  add_test(NAME latch-cpp-tests COMMAND latch-cpp-tests)
  add_executable(latch-boot-tests tests/test_boot.c)
  target_link_libraries(latch-boot-tests PRIVATE latch)
  add_test(NAME latch-boot-tests COMMAND latch-boot-tests)
  add_executable(latch-core-observability-tests tests/test_core_observability.c)
  target_link_libraries(latch-core-observability-tests PRIVATE latch)
  add_test(NAME latch-core-observability-tests COMMAND latch-core-observability-tests)
  foreach(test_name security crypto-vectors crypto-negative secure-element secure-storage secure-power-loss wear-level compression flash policy transport envelope-fuzz power-loss spool-stress property storage-bounds transport-policy build-id envelope-errors spool-priority crypto-provider update defensive-paths stream-edges memory-capture spool-edges envelope-capacity runtime-edges auv-runtime auv-edges ota fault-injection-pipeline replay-property)
    string(REPLACE "-" "_" test_source ${test_name})
    add_executable(latch-${test_name}-tests tests/test_${test_source}.c)
    target_link_libraries(latch-${test_name}-tests PRIVATE latch)
    add_test(NAME latch-${test_name}-tests COMMAND latch-${test_name}-tests)
  endforeach()
  add_executable(latch-profile-minimal tests/test_profile.c ${LS_ALL_PORTABLE_SOURCES})
  ls_configure_target(latch-profile-minimal)
  target_compile_definitions(latch-profile-minimal PRIVATE LS_STORE_STRINGS=0 LS_ENABLE_BREADCRUMBS=0 LS_ENABLE_METRICS=0 LS_ENABLE_LOGS=0 LS_ENABLE_POWER_SAMPLES=0 LS_ENABLE_PERFORMANCE=0 LS_ENABLE_DUMPS=0 LS_ENABLE_STACK_SNAPSHOT=0 LS_MAX_EVENT_SIZE=768 LS_BREADCRUMB_CAPACITY=1 LS_METRIC_CAPACITY=1)
  add_test(NAME latch-profile-minimal COMMAND latch-profile-minimal)
  add_executable(latch-profile-production tests/test_profile.c ${LS_ALL_PORTABLE_SOURCES})
  ls_configure_target(latch-profile-production)
  target_compile_definitions(latch-profile-production PRIVATE LS_STORE_STRINGS=0 LS_COMPILED_MIN_LEVEL=2 LS_MAX_EVENT_SIZE=2048 LS_BREADCRUMB_CAPACITY=8 LS_METRIC_CAPACITY=8)
  add_test(NAME latch-profile-production COMMAND latch-profile-production)
  add_executable(latch-profile-constrained tests/test_constrained_profile.c ${LS_ALL_PORTABLE_SOURCES})
  ls_configure_target(latch-profile-constrained)
  target_compile_definitions(latch-profile-constrained PRIVATE LS_CONSTRAINED_PROFILE=1)
  add_test(NAME latch-profile-constrained COMMAND latch-profile-constrained)
  add_executable(latch-blackbox-wrap-tests tests/test_blackbox_wrap.c ${LS_ALL_PORTABLE_SOURCES})
  ls_configure_target(latch-blackbox-wrap-tests)
  target_compile_definitions(latch-blackbox-wrap-tests PRIVATE LS_BLACKBOX_TEST_INITIAL_SEQUENCE=4294967293u)
  add_test(NAME latch-blackbox-wrap-tests COMMAND latch-blackbox-wrap-tests)
  add_executable(latch-crypto-commercial-tests tests/test_crypto_commercial.c ${LS_ALL_PORTABLE_SOURCES})
  ls_configure_target(latch-crypto-commercial-tests)
  target_compile_definitions(latch-crypto-commercial-tests PRIVATE LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER=1 LS_MIN_CRYPTO_ASSURANCE=3)
  add_test(NAME latch-crypto-commercial-tests COMMAND latch-crypto-commercial-tests)
  if(LS_TEST_LIBSODIUM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    add_executable(latch-sodium-interop-tests tests/test_sodium_interop.c)
    target_link_libraries(latch-sodium-interop-tests PRIVATE latch PkgConfig::SODIUM)
    add_test(NAME latch-sodium-interop-tests COMMAND latch-sodium-interop-tests)
  endif()
  if(LS_BUILD_LIBSODIUM_PROVIDER)
    add_executable(latch-libsodium-provider-tests tests/test_libsodium_provider.c)
    target_link_libraries(latch-libsodium-provider-tests PRIVATE latch latch-crypto-libsodium)
    add_test(NAME latch-libsodium-provider-tests COMMAND latch-libsodium-provider-tests)
  endif()
  add_executable(latch-stm32-reset-tests tests/test_stm32_reset.c)
  target_link_libraries(latch-stm32-reset-tests PRIVATE latch-port-stm32)
  add_test(NAME latch-stm32-reset-tests COMMAND latch-stm32-reset-tests)
  add_executable(latch-vendor-reset-tests tests/test_vendor_reset.c)
  target_link_libraries(latch-vendor-reset-tests PRIVATE latch-port-nordic latch-port-nxp
    latch-port-microchip latch-port-ti latch-port-silabs)
  add_test(NAME latch-vendor-reset-tests COMMAND latch-vendor-reset-tests)
  add_executable(latch-port-adapter-tests tests/test_port_adapters.c)
  target_include_directories(latch-port-adapter-tests PRIVATE
    ports/esp-idf ports/freertos ports/rp2040 ports/zephyr)
  target_link_libraries(latch-port-adapter-tests PRIVATE latch latch-port-esp-idf
    latch-port-freertos latch-port-rp latch-port-zephyr)
  add_test(NAME latch-port-adapter-tests COMMAND latch-port-adapter-tests)
  add_executable(latch-cortex-m-fault-tests tests/test_cortex_m_fault.c arch/cortex-m/cortex_m.c)
  target_include_directories(latch-cortex-m-fault-tests PRIVATE arch/cortex-m)
  target_link_libraries(latch-cortex-m-fault-tests PRIVATE latch)
  ls_configure_target(latch-cortex-m-fault-tests)
  add_test(NAME latch-cortex-m-fault-tests COMMAND latch-cortex-m-fault-tests)
  add_executable(latch-riscv64-fault-tests tests/test_riscv64_fault.c arch/riscv64/riscv64.c
    ${LS_ALL_PORTABLE_SOURCES})
  target_include_directories(latch-riscv64-fault-tests PRIVATE arch/riscv64)
  target_compile_definitions(latch-riscv64-fault-tests PRIVATE LS_ENABLE_WIDE_CONTEXT=1)
  ls_configure_target(latch-riscv64-fault-tests)
  add_test(NAME latch-riscv64-fault-tests COMMAND latch-riscv64-fault-tests)
  add_executable(latch-linux-signal-tests tests/test_linux_signal.c arch/linux/linux.c)
  target_include_directories(latch-linux-signal-tests PRIVATE arch/linux)
  ls_configure_target(latch-linux-signal-tests)
  add_test(NAME latch-linux-signal-tests COMMAND latch-linux-signal-tests)
  if(LS_BUILD_LINUX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(latch-file-backend-tests tests/test_file_backend.c)
    target_link_libraries(latch-file-backend-tests PRIVATE latch-linux latch)
    add_test(NAME latch-file-backend-tests COMMAND latch-file-backend-tests)
  endif()
  add_executable(latch-xtensa-fault-tests tests/test_xtensa_fault.c arch/xtensa/xtensa.c)
  target_include_directories(latch-xtensa-fault-tests PRIVATE arch/xtensa)
  target_link_libraries(latch-xtensa-fault-tests PRIVATE latch)
  ls_configure_target(latch-xtensa-fault-tests)
  add_test(NAME latch-xtensa-fault-tests COMMAND latch-xtensa-fault-tests)
endif()

if(LS_BUILD_FUZZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LS_BUILD_FUZZERS requires Clang")
  endif()
  foreach(target
      latch-core_obj
      latch-capture_obj
      latch-envelope_obj
      latch-spool_obj
      latch-storage-memory_obj
      latch-transport_obj
      latch-metrics_obj
      latch-security_obj)
    target_compile_options(${target} PRIVATE
      -fsanitize=fuzzer-no-link,address,undefined
      -fno-omit-frame-pointer)
  endforeach()
  foreach(fuzzer lep compression aead stream)
    add_executable(fuzz-${fuzzer} fuzz/fuzz_${fuzzer}.c)
    target_link_libraries(fuzz-${fuzzer} PRIVATE latch)
    target_compile_options(fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer,address,undefined)
  endforeach()
endif()

if(LS_BUILD_TOOLS)
  add_executable(latch-dump tools/latch_dump.c)
  target_link_libraries(latch-dump PRIVATE latch)
  install(TARGETS latch-dump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  if(LS_BUILD_TESTS)
    add_test(NAME latch-dump-hex-tests
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_latch_dump.py
        $<TARGET_FILE:latch-dump> ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/lep-v1-basic.hex)
    add_test(NAME latch-reference-collector-tests
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_latch_collector.py
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/lep-v1-basic.hex)
  endif()
endif()
if(LS_BUILD_EXAMPLES)
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "LS_BUILD_EXAMPLES builds hosted examples and cannot be used with a cross toolchain")
  endif()
  add_executable(latch-host-example examples/host/main.c)
  target_link_libraries(latch-host-example PRIVATE latch)
  foreach(reference iot-ota critical-redaction low-power-deep-sleep)
    add_executable(latch-reference-${reference}
      examples/reference-designs/${reference}/main.c)
    target_link_libraries(latch-reference-${reference} PRIVATE latch)
  endforeach()
  if(LS_BUILD_TESTS)
    add_test(NAME latch-host-example COMMAND latch-host-example)
    set_tests_properties(latch-host-example PROPERTIES
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    if(LS_BUILD_TOOLS)
      add_test(NAME latch-host-example-dump
        COMMAND latch-dump ${CMAKE_CURRENT_BINARY_DIR}/latch-demo.lst)
      set_tests_properties(latch-host-example-dump PROPERTIES
        DEPENDS latch-host-example
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    foreach(reference iot-ota critical-redaction low-power-deep-sleep)
      add_test(NAME latch-reference-${reference} COMMAND latch-reference-${reference})
    endforeach()
  endif()
endif()
if(LS_BUILD_BENCHMARKS)
  add_executable(latch-benchmark benchmarks/benchmark.c)
  target_link_libraries(latch-benchmark PRIVATE latch)
endif()

install(TARGETS latch latch-core latch-capture latch-envelope latch-spool latch-storage-memory latch-transport latch-metrics latch-security latch-cpp
  latch-port-stm32 latch-port-rp latch-port-esp-idf latch-port-freertos latch-port-zephyr
  latch-port-reset-common latch-port-nordic latch-port-nxp latch-port-microchip latch-port-ti
  latch-port-silabs
  EXPORT LatchTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/laststate DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(TARGET latch-crypto-libsodium)
  install(TARGETS latch-crypto-libsodium ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY ports/libsodium/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/laststate/ports/libsodium FILES_MATCHING PATTERN "*.h")
endif()
install(PROGRAMS
  tools/latch_collector.py tools/latch_fleet_report.py tools/latch_symbols.py
  tools/latch_support_bundle.py
  DESTINATION ${CMAKE_INSTALL_BINDIR})
foreach(port_directory stm32 rp2040 esp-idf freertos zephyr common nordic nxp microchip ti silabs)
  install(DIRECTORY ports/${port_directory}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/laststate/ports/${port_directory}
    FILES_MATCHING PATTERN "*.h")
endforeach()
if(TARGET latch-linux)
  install(TARGETS latch-linux EXPORT LatchTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY arch/linux/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/laststate/arch/linux
    FILES_MATCHING PATTERN "*.h")
  install(DIRECTORY ports/embedded-linux/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/laststate/ports/embedded-linux
    FILES_MATCHING PATTERN "*.h")
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/laststate/build_id_generated.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/laststate)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/laststate-latch-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/laststate-latch-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/laststate-latch)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/laststate-latch-config-version.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT LatchTargets NAMESPACE laststate:: FILE laststate-latch-targets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/laststate-latch)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/laststate-latch-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/laststate-latch-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/laststate-latch)
set(CPACK_PACKAGE_NAME "laststate-latch")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Last State")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "ZIP;TGZ")
include(CPack)
