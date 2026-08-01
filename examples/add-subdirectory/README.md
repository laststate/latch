# CMake `add_subdirectory` consumer

This project represents firmware that vendors Latch in its source tree. From
this directory:

```sh
cmake -S . -B build
cmake --build build
./build/firmware
```

The important integration lines are:

```cmake
add_subdirectory(path/to/latch latch-build)
target_link_libraries(firmware PRIVATE laststate::latch)
```
