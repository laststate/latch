cmake_minimum_required(VERSION 3.16)

# This sample lives in the Latch checkout so the component can compile the
# exact source under test. No global install or generated SDK is required.
set(LATCH_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE INTERNAL "Latch source root")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(latch_esp32_first_crash)
