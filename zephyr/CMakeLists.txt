if(CONFIG_LATCH)
  set(LATCH_ROOT ${ZEPHYR_CURRENT_MODULE_DIR})
  zephyr_library_named(laststate_latch)
  file(GLOB LATCH_CORE CONFIGURE_DEPENDS ${LATCH_ROOT}/src/core/*.c)
  file(GLOB LATCH_CAPTURE CONFIGURE_DEPENDS ${LATCH_ROOT}/src/capture/*.c)
  file(GLOB LATCH_ENVELOPE CONFIGURE_DEPENDS ${LATCH_ROOT}/src/envelope/*.c)
  file(GLOB LATCH_SPOOL CONFIGURE_DEPENDS ${LATCH_ROOT}/src/spool/*.c)
  file(GLOB LATCH_STORAGE CONFIGURE_DEPENDS ${LATCH_ROOT}/src/storage/*.c)
  file(GLOB LATCH_TRANSPORT CONFIGURE_DEPENDS ${LATCH_ROOT}/src/transport/*.c)
  file(GLOB LATCH_METRICS CONFIGURE_DEPENDS ${LATCH_ROOT}/src/metrics/*.c)
  file(GLOB LATCH_SECURITY CONFIGURE_DEPENDS ${LATCH_ROOT}/src/security/*.c)
  zephyr_library_sources(
    ${LATCH_CORE} ${LATCH_CAPTURE} ${LATCH_ENVELOPE} ${LATCH_SPOOL}
    ${LATCH_STORAGE} ${LATCH_TRANSPORT} ${LATCH_METRICS} ${LATCH_SECURITY}
    ${LATCH_ROOT}/ports/zephyr/zephyr_latch.c)
  # Public Latch headers must also be visible to the application target. The
  # library-scoped helper only exposes them while compiling Latch itself.
  zephyr_include_directories(${LATCH_ROOT}/include ${LATCH_ROOT}/ports/zephyr)
  zephyr_library_compile_definitions(LS_MAX_EVENT_SIZE=${CONFIG_LATCH_MAX_EVENT_SIZE})
endif()
