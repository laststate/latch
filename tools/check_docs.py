from __future__ import annotations
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
errors: list[str] = []
for path in sorted(root.rglob("*.md")):
    if "build" in path.parts or "target" in path.parts:
        continue
    text = path.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        errors.append(f"{path.relative_to(root)}: missing final newline")
    for line_no, line in enumerate(text.splitlines(), 1):
        if line.rstrip() != line:
            errors.append(f"{path.relative_to(root)}:{line_no}: trailing whitespace")
    for target in re.findall(r"\[[^]]+\]\((?!https?://|#)([^)]+)\)", text):
        candidate = (path.parent / target.split("#", 1)[0]).resolve()
        if target and not candidate.exists():
            errors.append(f"{path.relative_to(root)}: broken link {target}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print("documentation checks passed")
