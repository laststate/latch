cmake_minimum_required(VERSION 3.20)
project(latch-add-subdirectory-example LANGUAGES C)

add_subdirectory(../.. latch-build)
add_executable(firmware main.c)
target_link_libraries(firmware PRIVATE laststate::latch)
enable_testing()
add_test(NAME firmware-links-latch COMMAND firmware)
