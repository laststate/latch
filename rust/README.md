# Rust SDK

`laststate-latch` is a `#![no_std]` binding layer over the C runtime. It adds safe wrappers for breadcrumbs, metrics, captures and RAII spans without allocating. The final firmware links this crate with the C Latch libraries for its architecture.

```rust
use core::ffi::c_str;
use laststate_latch::{breadcrumb, metric_u32, Span};

breadcrumb(c_str!("boot_complete"));
metric_u32(c_str!("battery_mv"), 3712);
let _span = Span::begin(7);
```
