cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(latch_zephyr_native_ports C)
set(LATCH_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
target_sources(app PRIVATE
  src/main.c
  ${LATCH_ROOT}/ports/zephyr/zephyr_ble_gatt.c
  ${LATCH_ROOT}/ports/zephyr/zephyr_can.c
  ${LATCH_ROOT}/ports/zephyr/zephyr_lorawan.c
  ${LATCH_ROOT}/ports/zephyr/zephyr_tls_socket.c)
target_include_directories(app PRIVATE
  ${LATCH_ROOT}/include
  ${LATCH_ROOT}/ports/zephyr)
