# Real-time and resource qualification

Host benchmarks are useful regressions but are not WCET evidence. Before a product
release, measure the exact release build on each qualified target and retain the raw
results with the HIL manifest.

Minimum measurements:

| Operation | p50 | p99 | observed max | product limit |
| --- | ---: | ---: | ---: | ---: |
| breadcrumb append | TBD | TBD | TBD | product-defined |
| metric update | TBD | TBD | TBD | product-defined |
| fault snapshot | TBD | TBD | TBD | product-defined |
| LEP encode | TBD | TBD | TBD | product-defined |
| AEAD seal/open | TBD | TBD | TBD | product-defined |
| spool append | TBD | TBD | TBD | product-defined |
| flash commit/erase | TBD | TBD | TBD | product-defined |
| post-boot recovery | TBD | TBD | TBD | product-defined |

Also retain maximum stack usage (including the emergency stack), static RAM/flash,
maximum interrupt-disabled interval, flash writes/erases per incident, worst retry
burst, and energy used for a persistent commit. Repeat whenever the compiler, flags,
SDK, linker script, MCU/flash revision or enabled Latch profile changes.
