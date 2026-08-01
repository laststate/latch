cmake_minimum_required(VERSION 3.20)
project(latch-fetch-content-example LANGUAGES C)

include(FetchContent)
FetchContent_Declare(
  latch
  GIT_REPOSITORY https://github.com/laststate/latch.git
  GIT_TAG v0.2.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(latch)

add_executable(firmware main.c)
target_link_libraries(firmware PRIVATE laststate::latch)
enable_testing()
add_test(NAME firmware-links-latch COMMAND firmware)
