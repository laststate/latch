cmake_minimum_required(VERSION 3.20)
project(latch-idf-component-layout LANGUAGES C)

# This is a configure-time contract test for the root ESP-IDF component path.
# It supplies the small subset of Component Manager CMake API that Latch uses;
# the physical ESP-IDF example remains the real SDK integration test.
set(ESP_PLATFORM ON)
set(LATCH_IDF_COMPONENT_DUMMY "${CMAKE_CURRENT_SOURCE_DIR}/component_dummy.c")

macro(idf_component_register)
  cmake_parse_arguments(IDF "" "" "SRCS;INCLUDE_DIRS;REQUIRES" ${ARGN})
  list(FIND IDF_SRCS "ports/esp-idf/esp_idf.c" has_reset_port)
  if(has_reset_port EQUAL -1)
    message(FATAL_ERROR "Latch ESP-IDF component must include esp_idf.c")
  endif()
  foreach(forbidden_source ports/esp-idf/esp_idf_panic.c arch/xtensa/xtensa.c)
    list(FIND IDF_SRCS "${forbidden_source}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
      message(FATAL_ERROR "public ESP-IDF component must not include ${forbidden_source}")
    endif()
  endforeach()
  list(FIND IDF_REQUIRES "esp_system" has_esp_system)
  if(has_esp_system EQUAL -1)
    message(FATAL_ERROR "Latch ESP-IDF component must require esp_system")
  endif()
  add_library(latch-idf-component STATIC "${LATCH_IDF_COMPONENT_DUMMY}")
  target_include_directories(latch-idf-component PRIVATE ${IDF_INCLUDE_DIRS})
  set(COMPONENT_LIB latch-idf-component)
endmacro()

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." latch-component-source)

if(NOT TARGET latch-idf-component)
  message(FATAL_ERROR "Latch did not register an ESP-IDF component")
endif()
