#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# hil/esp32_qemu/lsak_responder.py
#
# Minimal LSAK responder that drives the ESP32 HIL fixture over QEMU's
# TCP serial, acting as the Relay for the durable-ack path.
#
# Wire contract (stream transport, lsak-v1):
#   frame: "LS" 0x01 0x00 <len:le32> <envelope> <crc32:le32>
#   lsak:  "LSAK" <version=1> <status=ACK_STORED=1> 0x00 0x00 <event_id:le32>
#
# Event id lives at LEP header offset 12 inside the envelope (see lep.c).
import socket
import sys
import time


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: lsak_responder.py <log_path> [duration_s] [port]")
        return 2
    log_path = sys.argv[1]
    duration = float(sys.argv[2]) if len(sys.argv) > 2 else 90.0
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 5555

    s = None
    deadline = time.time() + 15.0
    while s is None:
        try:
            s = socket.create_connection(("127.0.0.1", port))
        except OSError:
            if time.time() > deadline:
                print("could not connect to QEMU tcp serial")
                return 1
            time.sleep(0.5)
    s.settimeout(0.5)

    buf = bytearray()
    raw = bytearray()
    acks = 0
    end = time.time() + duration
    while time.time() < end:
        try:
            d = s.recv(4096)
            if d:
                buf += d
                raw += d
        except socket.timeout:
            pass
        except OSError:
            break

        while True:
            if len(buf) < 8:
                break
            if buf[0] != ord("L") or buf[1] != ord("S") or buf[2] != 1 or buf[3] != 0:
                buf.pop(0)
                continue
            length = int.from_bytes(buf[4:8], "little")
            if length == 0 or length > 2048:
                buf.pop(0)
                continue
            if len(buf) < 8 + length + 4:
                break
            envelope = bytes(buf[8 : 8 + length])
            event_id = int.from_bytes(envelope[12:16], "little")
            ack = b"LSAK" + bytes([1, 1, 0, 0]) + event_id.to_bytes(4, "little")
            s.sendall(ack)
            acks += 1
            print(f"ACK event_id={event_id} len={length} total_acks={acks}", flush=True)
            del buf[: 8 + length + 4]

    s.close()
    with open(log_path, "wb") as f:
        f.write(bytes(raw))
    print(f"done; acks={acks}; log={log_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())