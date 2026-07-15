#!/usr/bin/env python3
"""Hardware-in-the-loop runner for destructive reset and fault scenarios."""
import argparse
import json
import socket
import time
from pathlib import Path

import serial


def wait_for(port, token, timeout):
    deadline = time.monotonic() + timeout
    transcript = []
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", "replace").strip()
        if line:
            transcript.append(line)
            print(line, flush=True)
            if token in line:
                return transcript
    raise TimeoutError(f"did not receive {token!r}; transcript={transcript[-20:]}")


def scpi(endpoint, command):
    host, port = endpoint.rsplit(":", 1)
    with socket.create_connection((host, int(port)), timeout=5) as connection:
        connection.sendall((command + "\n").encode())


def run(config, scenario, timeout):
    with serial.Serial(config["serial"], config.get("baud", 115200), timeout=0.25) as port:
        port.reset_input_buffer()
        port.write(f"HIL:RUN:{scenario}\n".encode())
        wait_for(port, f"HIL:ARMED:{scenario.upper()}", timeout)
        if scenario == "brownout":
            supply = config["power_supply"]
            scpi(supply["endpoint"], f"VOLT {supply['brownout_voltage']} ")
            time.sleep(supply.get("hold_seconds", 0.25))
            scpi(supply["endpoint"], f"VOLT {supply['normal_voltage']} ")
        wait_for(port, f"HIL:PASS:{scenario.upper()}", timeout)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", required=True, type=Path)
    parser.add_argument("--scenario", required=True, choices=["brownout", "watchdog", "mpu", "trustzone", "fpu-lazy", "flash", "reset-registers"])
    parser.add_argument("--timeout", type=float, default=45)
    args = parser.parse_args()
    run(json.loads(args.board.read_text(encoding="utf-8")), args.scenario, args.timeout)


if __name__ == "__main__":
    main()
