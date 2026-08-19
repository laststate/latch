# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# hil/esp32_qemu/run-qemu.ps1
#
# Reproducible QEMU (Espressif fork) verification of the ESP32 HIL fixture.
# Builds the fixture, merges a 4 MiB flash image, boots it under QEMU and
# drives the stream-transport durable-ack path with a local LSAK responder.
# Writes machine-readable evidence (JSON + MD) into hil/esp32_qemu/evidence/.
#
# Prereqs: platformio via `python -m platformio`; esptool (platformio
# package) or `python -m esptool`; QEMU at $env:QEMU_ESP32 or
# .tools/qemu/qemu/bin/qemu-system-xtensa.exe (or $env:QEMU_XTENSA).

param(
    [string]$Fixture = (Join-Path $PSScriptRoot "..\esp32_relay"),
    [string]$OutDir = (Join-Path $PSScriptRoot "evidence"),
    [double]$Duration = 90.0,
    [int]$Port = 5555
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$Fixture = (Resolve-Path $Fixture).Path
$buildRoot = Join-Path $OutDir "build"

function Locate-Qemu {
    if ($env:QEMU_ESP32 -and (Test-Path $env:QEMU_ESP32)) { return $env:QEMU_ESP32 }
    foreach ($cand in @(
        (Join-Path $repoRoot ".tools\qemu\qemu\bin\qemu-system-xtensa.exe"),
        (Join-Path $repoRoot ".tools\qemu\bin\qemu-system-xtensa.exe")
    )) {
        if (Test-Path $cand) { return $cand }
    }
    $cmd = Get-Command qemu-system-xtensa -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "QEMU (Espressif xtensa) not found. Set QEMU_ESP32 or unpack to .tools/qemu/"
}

function Get-EsptoolInvocation {
    try {
        & python -m esptool --version 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) { return "python -m esptool" }
    } catch {}
    $pkg = Join-Path $HOME ".platformio\packages\tool-esptoolpy\esptool.py"
    if (Test-Path $pkg) { return "python $pkg" }
    throw "esptool not found (python -m esptool or platformio tool-esptoolpy)"
}

function Get-LogText([string]$Path) {
    $raw = [System.IO.File]::ReadAllBytes($Path)
    return -join ($raw | ForEach-Object {
        if ($_ -ge 32 -and $_ -le 126) { [char]$_ }
        elseif ($_ -eq 10 -or $_ -eq 13) { [char]$_ }
        else { ' ' }
    })
}

$qemu = Locate-Qemu
$qemuVersion = (& $qemu --version 2>&1 | Select-Object -First 1)
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null

# 1. Build fixture if needed.
$pioBuild = Join-Path $Fixture ".pio\build\esp32dev"
$firmware = Join-Path $pioBuild "firmware.bin"
if (-not (Test-Path $firmware)) {
    Write-Host "building fixture..."
    Push-Location $repoRoot
    try { & python -m platformio run -d $Fixture | Out-Null } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "platformio build failed" }
}
$bootloader = Join-Path $pioBuild "bootloader.bin"
$partitions = Join-Path $pioBuild "partitions.bin"

# 2. Merge a 4 MiB flash image (QEMU requires 2/4/8/16 MiB).
$flashOut = Join-Path $buildRoot "flash_image.bin"
$esptool = Get-EsptoolInvocation
Write-Host "merging flash image..."
Invoke-Expression "$esptool --chip esp32 merge_bin -o `"$flashOut`" --flash_mode dio --flash_size 4MB --fill-flash-size 4MB 0x1000 `"$bootloader`" 0x8000 `"$partitions`" 0x10000 `"$firmware`"" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "esptool merge failed" }
if (-not (Test-Path $flashOut)) { throw "flash image missing" }

# 3. Boot QEMU and drive the durable-ack path.
$uartLog = Join-Path $buildRoot "qemu-uart.log"
$qerr = Join-Path $buildRoot "qemu-stderr.log"
Remove-Item $uartLog, $qerr -ErrorAction SilentlyContinue
Write-Host "starting QEMU ($qemuVersion)..."
$qemuProc = Start-Process -FilePath $qemu `
    -ArgumentList @("-display", "none", "-machine", "esp32",
        "-drive", "file=$flashOut,if=mtd,format=raw",
        "-serial", "tcp::$Port,server") `
    -RedirectStandardError $qerr -PassThru -NoNewWindow
try {
    Write-Host "running LSAK responder..."
    $responderOut = & python (Join-Path $PSScriptRoot "lsak_responder.py") $uartLog $Duration $Port 2>&1
    if ($LASTEXITCODE -ne 0) { throw "lsak responder failed" }
} finally {
    if (-not $qemuProc.HasExited) { Stop-Process -Id $qemuProc.Id -Force }
}
$responderLog = Join-Path $buildRoot "responder.log"
$responderOut | Set-Content -Path $responderLog -Encoding UTF8
$responderOut | Write-Host

# 4. Parse markers.
$text = Get-LogText $uartLog
$markers = [ordered]@{
    phase0_capture            = ($text -match "HIL:START:phase=0" -and $text -match "HIL:PHASE0:capture-before-panic")
    panic_path                = ($text -match "abort\(\) was called|Backtrace:")
    reset_reason_decode       = ($text -match "rst:0xc" -and $text -match "HIL:START:phase=1")
    phase1_recovery           = ($text -match "HIL:PHASE1:recovered")
    durable_ack               = ($text -match "HIL:PASS:LATCH_RELAY_ESP32")
}
$acks = ($responderOut | Where-Object { $_ -match "ACK event_id=" }).Count
$pass = ($markers.Values -notcontains $false)

# 5. Evidence.
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$observed = (Get-Date).ToUniversalTime().ToString("o")
$firmwareSha = (Get-FileHash $firmware -Algorithm SHA256).Hash.ToLower()
$flashSha = (Get-FileHash $flashOut -Algorithm SHA256).Hash.ToLower()
$uartSha = (Get-FileHash $uartLog -Algorithm SHA256).Hash.ToLower()
$responderSha = (Get-FileHash $responderLog -Algorithm SHA256).Hash.ToLower()

$evidence = [ordered]@{
    schema_version   = 1
    result           = if ($pass) { "pass" } else { "fail" }
    execution        = "QEMU-Espressif"
    chip             = "esp32"
    family           = "Xtensa"
    mcu              = "ESP32 (LX6)"
    arch             = "xtensa"
    scenario         = "panic-reboot-recovery-durable-ack"
    fault_source     = "fixture abort() during capture-before-panic"
    firmware         = $firmware
    firmware_sha256  = $firmwareSha
    flash_image      = $flashOut
    flash_image_sha256 = $flashSha
    qemu             = $qemu
    qemu_version     = $qemuVersion
    observed_at      = $observed
    duration_s       = $Duration
    uart_log         = $uartLog
    uart_log_sha256  = $uartSha
    responder_log    = $responderLog
    responder_log_sha256 = $responderSha
    acks             = $acks
    markers          = $markers
    caveat           = "QEMU (Espressif fork) is an emulator, not physical silicon. It validates the ESP32 SoC model, the Xtensa abort/panic path, ESP-IDF reset-reason decode, mirrored-flash persistence across reboot, boot recovery and the stream-transport durable-ack (lsak-v1) contract. It does not validate silicon errata, analog supplies, real flash timing, RF, or physical reset registers."
}

$jsonPath = Join-Path $OutDir "$($stamp).json"
$mdPath = Join-Path $OutDir "$($stamp).md"
$evidence | ConvertTo-Json -Depth 4 | Set-Content -Path $jsonPath -Encoding UTF8

$md = New-Object System.Collections.Generic.List[string]
foreach ($line in @(
    "# QEMU evidence: esp32 (xtensa)"
    ""
    "- Result: $($evidence.result.ToUpper())"
    "- Chip: esp32 (ESP32 LX6, Xtensa)"
    "- Scenario: panic-reboot-recovery-durable-ack"
    "- Observed at (UTC): $observed"
    "- Duration: $Duration s"
    "- Durable ACKs: $acks"
    ""
    "## Assertions"
    ""
    "1. Latch boots and captures before the fixture abort() (phase 0)."
    "2. The Xtensa panic path runs and the reset reason decodes to ESP_RST_PANIC."
    "3. The envelope survives reboot and is recovered from flash (phase 1)."
    "4. The stream-transport frame is delivered and durably ACKed (lsak-v1)."
    "5. The fixture reports HIL:PASS:LATCH_RELAY_ESP32."
    ""
    "## Markers"
    ""
)) { $md.Add($line) }
foreach ($key in $markers.Keys) { $md.Add("- $key : $($markers[$key])") }
foreach ($line in @(
    ""
    "## Raw evidence"
    ""
    "- QEMU: $qemuVersion"
    "- Firmware SHA-256: $firmwareSha"
    "- Flash image SHA-256: $flashSha"
    "- UART log: $uartLog"
    "- UART log SHA-256: $uartSha"
    "- Responder log: $responderLog"
    "- Responder log SHA-256: $responderSha"
    ""
    "## Scope"
    ""
    $evidence.caveat
    ""
)) { $md.Add($line) }
$md | Set-Content -Path $mdPath -Encoding UTF8

Write-Host ""
Write-Host "RESULT: $($evidence.result.ToUpper())  (acks=$acks)"
$markers.GetEnumerator() | ForEach-Object { Write-Host "  marker $($_.Key): $($_.Value)" }
Write-Host "evidence: $jsonPath"
exit $(if ($pass) { 0 } else { 1 })