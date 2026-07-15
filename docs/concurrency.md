# Concurrency contract

Latch does not create threads or use dynamic allocation. Integrators provide critical-section callbacks when the target has preemptive threads or interrupts.

`ls_init`, registration functions, configuration setters, `ls_boot`, `ls_flush` and all capture APIs other than the architecture fault handlers are normal-runtime APIs. Call them from a serialized task context unless the configured critical-section callbacks protect their use. A transport backend is acquired for the duration of `ls_transport_send`; another sender for the same backend receives `LS_EBUSY`.

Interrupt handlers may record only data through APIs documented by the selected port as ISR-safe. They must not call storage, transport, cryptographic provisioning or normal event capture directly. Deferred work should call the normal capture APIs after returning to task context.

Fault handlers are separate from the normal runtime. The Cortex-M and RISC-V ports switch to an emergency stack, avoid spool, storage, transport and reset callbacks, and write a fixed retained snapshot. A recursive Cortex-M fault writes one recursive snapshot and then stops. Normal boot validates and persists the snapshot before clearing it.

The application owns synchronization for callback implementations, storage drivers, network stacks and secure elements. A callback must not re-enter Latch while it is executing on behalf of Latch.
