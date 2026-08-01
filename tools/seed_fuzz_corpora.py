from __future__ import annotations

import argparse
from pathlib import Path


def write_seed(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> None:
    parser = argparse.ArgumentParser(description="Create deterministic initial fuzz corpora.")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--vectors",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "vectors",
    )
    arguments = parser.parse_args()

    output = arguments.output.resolve()
    vectors = arguments.vectors.resolve()
    vector_count = 0
    for vector in sorted(vectors.rglob("*.hex")):
        relative = vector.relative_to(vectors).with_suffix("")
        seed_name = "__".join(relative.parts)
        try:
            data = bytes.fromhex(vector.read_text(encoding="utf-8"))
        except ValueError as error:
            raise ValueError(f"invalid hex vector {vector}: {error}") from error
        if not data:
            raise ValueError(f"empty hex vector {vector}")
        write_seed(output / "lep" / seed_name, data)
        vector_count += 1

    if vector_count == 0:
        raise ValueError(f"no .hex vectors found below {vectors}")

    write_seed(output / "compression" / "mixed-bytes", bytes(range(256)))
    write_seed(output / "compression" / "short-runs", b"\x00\x00\x00\x01\x01\xff\xff")
    write_seed(output / "aead" / "zero-length-plaintext", bytes(56))
    write_seed(output / "aead" / "aad-and-plaintext", bytes(range(96)))
    basic = bytes.fromhex((vectors / "lep-v1-basic.hex").read_text(encoding="utf-8"))
    stream = b"LS\x01\x00" + len(basic).to_bytes(4, "little") + basic
    import binascii
    stream += (binascii.crc32(basic) & 0xFFFFFFFF).to_bytes(4, "little")
    write_seed(output / "stream" / "basic-frame", stream)
    write_seed(output / "stream" / "stored-ack", b"LSAK\x01\x01\x00\x00\x09\x00\x00\x00")
    print(f"seeded {vector_count} LEP vectors and 6 codec/AEAD/stream inputs in {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        raise SystemExit(f"seed_fuzz_corpora: {error}") from error
