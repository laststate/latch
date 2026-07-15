from __future__ import annotations

import argparse
import pathlib
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("archive", type=pathlib.Path)
parser.add_argument("--tool", default="arm-none-eabi-size")
parser.add_argument("--flash-budget", type=int, default=131072)
parser.add_argument("--ram-budget", type=int, default=65536)
args = parser.parse_args()
output = subprocess.check_output([args.tool, str(args.archive)], text=True)
text_total = data_total = bss_total = 0
for line in output.splitlines():
    fields = line.split()
    if len(fields) >= 6 and all(field.isdigit() for field in fields[:3]):
        text_total += int(fields[0])
        data_total += int(fields[1])
        bss_total += int(fields[2])
flash = text_total + data_total
ram = data_total + bss_total
print(f"portable archive flash={flash} ram={ram}")
if flash > args.flash_budget or ram > args.ram_budget:
    raise SystemExit(f"size budget exceeded: flash {flash}/{args.flash_budget}, ram {ram}/{args.ram_budget}")
