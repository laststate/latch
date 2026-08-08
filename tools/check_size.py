from __future__ import annotations

import argparse
import pathlib

from measure_footprint import make_report, parse_berkeley_size, run_size_tool, stack_summary

parser = argparse.ArgumentParser()
parser.add_argument("archive", type=pathlib.Path)
parser.add_argument("--tool", default="arm-none-eabi-size")
parser.add_argument("--flash-budget", type=int, default=131072)
parser.add_argument("--ram-budget", type=int, default=65536)
args = parser.parse_args()
if not args.archive.is_file():
    raise SystemExit(f"archive does not exist or is not a file: {args.archive}")
try:
    sections, rows = parse_berkeley_size(run_size_tool(args.tool, args.archive))
except (RuntimeError, ValueError) as error:
    raise SystemExit(f"size measurement failed: {error}") from error
report = make_report(args.archive, args.tool, sections, rows, stack_summary([]), None)
flash = report["flash_estimate_bytes"]
ram = report["static_ram_estimate_bytes"]
print(
    f"{report['measurement_scope']}: flash-estimate={flash} "
    f"static-ram-estimate={ram}"
)
if flash > args.flash_budget or ram > args.ram_budget:
    raise SystemExit(
        f"size budget exceeded: flash-estimate {flash}/{args.flash_budget}, "
        f"static-ram-estimate {ram}/{args.ram_budget}"
    )
