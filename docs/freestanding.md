# Freestanding contract

The portable libraries include only C freestanding headers and do not allocate, start threads, perform I/O or require an operating system. Memory copying and clearing use internal implementations so the fault path does not acquire a hidden libc dependency.

`latch-freestanding-check` compiles all portable sources with strings disabled. GCC/Clang builds use `-ffreestanding -fno-builtin -fno-stack-protector`; MSVC builds use `/Zl /GS- /Oi-`. The Windows verification additionally checks the resulting objects for unresolved CRT memory, heap and stack-cookie symbols.

Callbacks supplied in `ls_config_t` are the only platform dependencies: timestamp, reset reason, reset action and optional critical-section entry/exit. Normal-runtime capture and boot may use the configured storage backend; the minimal fault path writes only its bounded retained snapshot and does not call storage.
