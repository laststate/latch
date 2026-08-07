# LEP public test vectors

`lep-v1-basic.hex` is a canonical 34-byte LEP v1 envelope. It contains sequence 7, event ID 9 and one TLV with type 1 and bytes `aa bb`. Both the C and Rust decoders use the same layout and `tools/check_test_vector.py` independently verifies its lengths and CRCs.

`invalid/` holds negative golden vectors (bad magic/CRC/flags/version/TLV). `tools/check_test_vector.py` asserts they fail plain validation.

Existing vectors are immutable compatibility fixtures. A breaking wire-format change requires a new protocol version and a new file rather than rewriting an old vector.

`invalid/truncated-envelope.hex` is a canonical envelope truncated to 30 bytes:
the header declares a payload longer than the file provides. Plain validation
rejects it because the declared payload length does not match the available
bytes. Decoders must report a truncated or invalid-length failure instead of
returning a partial envelope.
