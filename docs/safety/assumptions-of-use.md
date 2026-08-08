# Safety assumptions of use

Latch is a bounded diagnostic/evidence subsystem. It is **not** a flight-control,
navigation, collision-avoidance, propulsion, leak-detection or emergency-ascent
controller and must never be the only mechanism protecting people, property or a
vehicle.

For a commercial AUV integration, the product owner must provide and qualify:

- an independent hardware watchdog and a fail-safe/safe-state strategy outside Latch;
- verified power supervision/brownout handling and enough hold-up energy for any
  promised persistent commit;
- qualified nonvolatile memory geometry/endurance/ECC behavior for the final board;
- a signed boot chain and product bootloader that verifies images before execution;
- monotonic anti-rollback storage appropriate to the threat model;
- device-unique keys generated/provisioned with approved entropy, preferably held by
  a secure element or hardware-backed key store;
- authenticated transport and collector identity (normally mutual TLS or an
  equivalent product-specific mechanism);
- correct linker/MPU/TrustZone placement and dedicated retained/emergency memory;
- WCET, stack, interrupt-latency and memory-budget measurements made with the exact
  compiler, optimization, SDK, board revision and enabled Latch profile;
- environmental qualification appropriate to the vehicle: pressure, temperature,
  humidity/condensation, vibration/shock, EMC/EMI, power transients and long-duration
  mission cycling.

A fault record is evidence, not proof that the vehicle is safe. A missing record must
also never be interpreted as proof that no fault occurred.
