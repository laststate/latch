# LEP public test vectors

`lep-v1-basic.hex` is a canonical 34-byte LEP v1 envelope. It contains sequence 7, event ID 9 and one TLV with type 1 and bytes `aa bb`. Both the C and Rust decoders use the same layout and `tools/check_test_vector.py` independently verifies its lengths and CRCs.

Existing vectors are immutable compatibility fixtures. A breaking wire-format change requires a new protocol version and a new file rather than rewriting an old vector.
