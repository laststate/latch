#!/usr/bin/env python3
"""Validate packaging metadata and source-layout shims for embedded IDEs."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def properties(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        result[key] = value
    return result


def manifest_value(path: Path, key: str) -> str:
    prefix = f"{key}:"
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            return line.removeprefix(prefix).strip().strip('"')
    raise AssertionError(f"{key} is missing from {path}")


def main() -> int:
    platformio = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    arduino = properties(ROOT / "library.properties")
    component = ROOT / "idf_component.yml"

    assert arduino["name"] == platformio["name"]
    assert arduino["version"] == platformio["version"]
    assert arduino["architectures"] == "avr,esp32"
    assert platformio["frameworks"] == ["arduino", "espidf"]
    assert platformio["platforms"] == ["atmelavr", "espressif32"]
    assert arduino["includes"] == "laststate/latch.h,laststate/latch.hpp"
    assert manifest_value(component, "version") == platformio["version"]
    assert f"/tree/v{platformio['version']}/" in manifest_value(component, "documentation")
    assert 'idf: ">=5.0"' in component.read_text(encoding="utf-8")

    public_headers = sorted((ROOT / "include" / "laststate").glob("*"))
    shims = ROOT / "src" / "laststate"
    assert [header.name for header in public_headers] == [header.name for header in sorted(shims.glob("*"))]
    for header in public_headers:
        shim = shims / header.name
        assert shim.read_text(encoding="utf-8") == (
            f'#include "../../include/laststate/{header.name}"\n'
        )

    arduino_example = ROOT / "examples" / "arduino-esp32-cooperative"
    sketch = (arduino_example / "arduino_esp32_cooperative.ino").read_text(encoding="utf-8")
    assert "ls_capture_message" in sketch
    assert "esp_panic" not in sketch
    assert "abort(" not in sketch

    avr_example = ROOT / "examples" / "arduino-avr-cooperative"
    avr_sketch = (avr_example / "src" / "main.cpp").read_text(encoding="utf-8")
    avr_ini = (avr_example / "platformio.ini").read_text(encoding="utf-8")
    assert "ls_capture_message" in avr_sketch
    assert "LS_CONSTRAINED_PROFILE=1" in avr_ini
    assert "megaatmega2560" in avr_ini

    registry_workflow = (ROOT / ".github" / "workflows" / "registry-packages.yml").read_text(
        encoding="utf-8"
    )
    assert registry_workflow.count("laststate-latch-platformio.tar.gz") >= 3
    assert "--project-option=\"lib_deps=file://../../build/laststate-latch-platformio.tar.gz\"" in registry_workflow

    idf_example = ROOT / "examples" / "esp-idf-component"
    dependency = (idf_example / "main" / "idf_component.yml").read_text(encoding="utf-8")
    component_cmake = (idf_example / "main" / "CMakeLists.txt").read_text(encoding="utf-8")
    component_main = (idf_example / "main" / "main.c").read_text(encoding="utf-8")
    assert "path: ../../.." in dependency
    assert "REQUIRES latch esp_timer" in component_cmake
    assert "ls_esp_idf_reset_info" in component_main
    assert "esp_idf_panic" not in component_main
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
