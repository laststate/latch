# ESP-IDF Component Manager consumer

This project consumes the adjacent Latch checkout through an ESP-IDF Component
Manager local-path dependency. It captures an explicit error and sends its LEP
envelope to the ESP-IDF log.

```sh
cd examples/esp-idf-component
idf.py set-target esp32
idf.py build flash monitor
```

`main/idf_component.yml` deliberately uses `path: ../../..` so this project
tests the checkout being edited. After Latch is published to the ESP Component
Registry, replace that dependency with its released registry coordinate and a
bounded version.

This is a normal-runtime component integration check. It does not enable the
private ESP-IDF panic-wrapper path, install a fault handler, or prove a
crash-survives-reboot flow.
