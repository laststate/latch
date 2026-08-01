cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(latch_zephyr_first_capture)
target_sources(app PRIVATE src/main.c)
