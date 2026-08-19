## Overview

Adaptive replay windowing allows Latch to dynamically adjust the replay buffer size based on network conditions and system load.

## Window Parameters

- **Min Window Size:** 64 KB
- **Max Window Size:** 1 MB
- **Initial Window Size:** 256 KB
- **Growth Rate:** 2x on ACK timeout
- **Shrink Rate:** 0.5x on successful delivery

## Algorithm

1. **Initialize** with `Initial Window Size`
2. **On ACK timeout:** `window = min(window * 2, Max Window Size)`
3. **On successful delivery:** `window = max(window * 0.5, Min Window Size)`
4. **Adjust** based on packet loss rate:
   - Loss > 10%: Shrink window
   - Loss < 1%: Grow window

## Implementation

The replay window is implemented as a circular buffer in RAM:

```c
struct replay_window {
    uint8_t *buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t max_size;
    size_t min_size;
    uint32_t growth_rate;
    uint32_t shrink_rate;
};
```

## Constraints

- Window size must be power of 2
- Maximum window size limited by available RAM
- Minimum window size ensures basic replay capability

## References

- [Latch Transport Spec](transports.md)
- [Latch Encryption Spec](encryption.md)
