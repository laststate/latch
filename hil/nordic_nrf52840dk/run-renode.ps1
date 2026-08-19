param(
    [Parameter(Mandatory = $true)]
    [string]$Firmware,
    [string]$Renode = "C:\Program Files\Renode\renode.exe",
    [string]$EvidenceOut = ""
)

$ErrorActionPreference = "Stop"
$firmwarePath = (Resolve-Path -LiteralPath $Firmware).Path
$renodePath = (Resolve-Path -LiteralPath $Renode).Path
$robot = Join-Path $PSScriptRoot "nrf52840dk.robot"
$renodeTest = Join-Path (Split-Path $renodePath) "renode-test.bat"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$markers = @("HIL:ARMED:USAGEFAULT", "HIL:PASS:USAGEFAULT")

if (-not (Test-Path -LiteralPath $firmwarePath -PathType Leaf)) {
    throw "Firmware ELF not found: $firmwarePath"
}

# Robot resolves ${ELF} relative to the repository build directory. Keep the
# fixture deterministic by copying the requested image to that canonical path.
$canonical = Join-Path $PSScriptRoot "..\..\build\emulator\latch-renode-nrf52840.elf"
New-Item -ItemType Directory -Force -Path (Split-Path $canonical) | Out-Null
if ([System.IO.Path]::GetFullPath($firmwarePath) -ne [System.IO.Path]::GetFullPath($canonical)) {
    Copy-Item -LiteralPath $firmwarePath -Destination $canonical -Force
}

$log = Join-Path ([System.IO.Path]::GetTempPath()) ("latch-nrf52840-renode-" + [guid]::NewGuid().ToString("N") + ".log")
Push-Location $repoRoot
try {
    & $renodeTest $robot 2>&1 | Tee-Object -FilePath $log
    $renodeExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($renodeExitCode -ne 0) { throw "Renode failed with exit code $renodeExitCode" }

$robotOutput = Join-Path $repoRoot "robot_output.xml"
if (-not (Test-Path -LiteralPath $robotOutput -PathType Leaf)) {
    throw "Renode passed but Robot output was not generated: $robotOutput"
}
$robotText = Get-Content -Raw -LiteralPath $robotOutput
foreach ($marker in $markers) {
    if (-not $robotText.Contains($marker)) {
        throw "Renode completed without marker '$marker'. Robot evidence: $robotOutput"
    }
}

if ($EvidenceOut -eq "") {
    $EvidenceOut = Join-Path $PSScriptRoot ("evidence\" + (Get-Date -Format "yyyyMMddTHHmmssZ") + ".json")
}
$evidence = [ordered]@{
    schema_version = 1
    result = "pass"
    execution = "Renode"
    board = "Nordic nRF52840 DK"
    mcu = "nRF52840 / Cortex-M4F"
    scenario = "usagefault"
    fault_source = "firmware UDF instruction"
    firmware = $firmwarePath
    firmware_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $firmwarePath).Hash.ToLowerInvariant()
    observed_at = (Get-Date).ToUniversalTime().ToString("o")
    markers = $markers
    renode_log = $log
    renode_log_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $log).Hash.ToLowerInvariant()
    robot_output = $robotOutput
    robot_output_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $robotOutput).Hash.ToLowerInvariant()
    report_html = (Join-Path $repoRoot "report.html")
    log_html = (Join-Path $repoRoot "log.html")
    caveat = "This validates Renode's nRF52840 model, vector table, UART0/EasyDMA, and UsageFault path; it is not physical-board qualification and does not claim HardFault generation."
}
New-Item -ItemType Directory -Force -Path (Split-Path $EvidenceOut) | Out-Null
$evidence | ConvertTo-Json | Set-Content -LiteralPath $EvidenceOut -Encoding utf8
$markdownOut = [System.IO.Path]::ChangeExtension($EvidenceOut, ".md")
$markdown = @"
# Renode nRF52840DK evidence

- Result: **PASS**
- Observed at (UTC): $($evidence.observed_at)
- Renode execution: `renode-test` / Robot Framework
- Board model: `$($evidence.board)`
- MCU: `$($evidence.mcu)`
- Scenario: `$($evidence.scenario)`
- Fault source: `$($evidence.fault_source)`
- Firmware ELF: `$($evidence.firmware)`
- Firmware SHA-256: `$($evidence.firmware_sha256)`

## Assertions observed

1. `$($markers[0])` — firmware booted and UART0/EasyDMA output was captured.
2. `$($markers[1])` — `UsageFault_Handler` ran after `udf #0` and the firmware confirmed `CFSR.UNDEFINSTR`.

## Raw evidence

- Robot Framework XML: `$($evidence.robot_output)`
- Robot XML SHA-256: `$($evidence.robot_output_sha256)`
- Renode log: `$($evidence.renode_log)`
- Renode log SHA-256: `$($evidence.renode_log_sha256)`
- Robot report: `$($evidence.report_html)`
- Robot log: `$($evidence.log_html)`

## Scope and limitation

$($evidence.caveat)
The installed Renode 1.16.1 does not expose synchronous HardFault injection
through the test interface, so this run must not be cited as proof that an
instruction generated HardFault. Physical Nordic silicon, power/reset,
watchdog, radio, and production linker/toolchain behavior remain unqualified.
"@
New-Item -ItemType Directory -Force -Path (Split-Path $markdownOut) | Out-Null
$markdown | Set-Content -LiteralPath $markdownOut -Encoding utf8
Write-Host "Renode HIL PASS; evidence written to $EvidenceOut and $markdownOut"
