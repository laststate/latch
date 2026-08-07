#!/usr/bin/env python3
"""Hardware-in-the-loop runner for destructive reset and fault scenarios."""
import argparse
import datetime as dt
import hashlib
import json
import os
import socket
import subprocess
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
    transcript = []
    with serial.Serial(config["serial"], config.get("baud", 115200), timeout=0.25) as port:
        port.reset_input_buffer()
        port.write(f"HIL:RUN:{scenario}\n".encode())
        transcript.extend(wait_for(port, f"HIL:ARMED:{scenario.upper()}", timeout))
        if scenario == "brownout":
            supply = config["power_supply"]
            scpi(supply["endpoint"], f"VOLT {supply['brownout_voltage']} ")
            time.sleep(supply.get("hold_seconds", 0.25))
            scpi(supply["endpoint"], f"VOLT {supply['normal_voltage']} ")
        transcript.extend(wait_for(port, f"HIL:PASS:{scenario.upper()}", timeout))
    return transcript


def resolve_commit(explicit):
    if explicit:
        return explicit.lower()
    if os.environ.get("GITHUB_SHA"):
        return os.environ["GITHUB_SHA"].lower()
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True, stderr=subprocess.DEVNULL
        ).strip().lower()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def write_evidence(path, config, scenario, transcript, commit):
    public_board = config.get("identity", {})
    transcript_text = "\n".join(transcript) + "\n"
    evidence = {
        "schema_version": 1,
        "result": "pass",
        "observed_at": dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z"),
        "commit": commit,
        "platform": public_board.get("platform", "unknown"),
        "mcu": public_board.get("mcu", "unknown"),
        "board": public_board.get("board", "unknown"),
        "board_revision": public_board.get("board_revision", "unknown"),
        "toolchain": public_board.get("toolchain", "unknown"),
        "features": public_board.get("features", config.get("features", [])),
        "scenarios": [scenario],
        "transcript_sha256": hashlib.sha256(transcript_text.encode()).hexdigest(),
        "transcript": transcript,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", required=True, type=Path)
    parser.add_argument(
        "--scenario",
        required=True,
        choices=[
            "brownout",
            "hardfault",
            "watchdog",
            "stack-canary",
            "mpu",
            "trustzone",
            "fpu-lazy",
            "flash",
            "reset-registers",
        ],
    )
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--commit")
    parser.add_argument("--evidence-out", type=Path)
    args = parser.parse_args()
    config = json.loads(args.board.read_text(encoding="utf-8"))
    transcript = run(config, args.scenario, args.timeout)
    if args.evidence_out:
        write_evidence(args.evidence_out, config, args.scenario, transcript,
                       resolve_commit(args.commit))


if __name__ == "__main__":
    main()
