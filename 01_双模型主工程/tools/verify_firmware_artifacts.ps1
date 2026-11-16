param(
    [string]$ElfPath = (Join-Path $PSScriptRoot '..\Appli\build\ExprRecognition_Appli.elf'),
    [string]$HexPath = (Join-Path $PSScriptRoot '..\Appli\build\ExprRecognition_Appli.hex'),
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c'),
    [string]$AppliCMakePath = (Join-Path $PSScriptRoot '..\Appli\CMakeLists.txt')
)

$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$message) {
    $script:failures.Add($message)
    Write-Host "[FAIL] $message" -ForegroundColor Red
}

function Add-Pass([string]$message) {
    Write-Host "[PASS] $message" -ForegroundColor Green
}

function Resolve-ArmTool([string]$name) {
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Required tool not found: $name"
    }
    return $command.Source
}

function Get-FunctionDisassembly([string[]]$lines, [string]$functionName) {
    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "^[0-9a-fA-F]+ <$([regex]::Escape($functionName))>:$") {
            $start = $i
            break
        }
    }

    if ($start -lt 0) {
        return @()
    }

    $result = [System.Collections.Generic.List[string]]::new()
    for ($i = $start; $i -lt $lines.Count; $i++) {
        if (($i -gt $start) -and ($lines[$i] -match '^[0-9a-fA-F]+ <[^>]+>:$')) {
            break
        }
        $result.Add($lines[$i])
    }
    return $result.ToArray()
}

$ElfPath = [IO.Path]::GetFullPath($ElfPath)
$HexPath = [IO.Path]::GetFullPath($HexPath)
$MainSourcePath = [IO.Path]::GetFullPath($MainSourcePath)
$AppliCMakePath = [IO.Path]::GetFullPath($AppliCMakePath)

if (-not (Test-Path -LiteralPath $ElfPath)) {
    throw "ELF not found: $ElfPath"
}

$mainSource = Get-Content -Raw -LiteralPath $MainSourcePath
$appliCMake = Get-Content -Raw -LiteralPath $AppliCMakePath
if ($appliCMake -notmatch '(?m)-u\s+_printf_float') {
    Add-Failure 'newlib-nano floating-point printf support is not linked'
} else {
    Add-Pass 'newlib-nano floating-point printf support is linked'
}

if (($mainSource -match 'wdog_deadline\s*--') -or
    ($mainSource -notmatch 'HAL_GetTick\(\)\s*-\s*wdog_started_ms')) {
    Add-Failure 'NPU watchdog is not based on elapsed HAL milliseconds'
} else {
    Add-Pass 'NPU watchdog is based on elapsed HAL milliseconds'
}

if (($mainSource -notmatch 'set_risaf_default\(RISAF8_S\)') -or
    ($mainSource -notmatch 'set_risaf_default\(RISAF15_S\)')) {
    Add-Failure 'NPU CACHEAXI RISAF8/RISAF15 access is not configured'
} else {
    Add-Pass 'NPU CACHEAXI RISAF8/RISAF15 access is configured'
}

$runtimeInitIndex = $mainSource.IndexOf('stai_runtime_init()')
$networkInitIndex = $mainSource.IndexOf('stai_network_init(net)')
if (($runtimeInitIndex -lt 0) -or
    ($networkInitIndex -lt 0) -or
    ($runtimeInitIndex -gt $networkInitIndex)) {
    Add-Failure 'STAI global runtime is not initialized before the network instance'
} else {
    Add-Pass 'STAI global runtime is initialized before the network instance'
}

if ($mainSource -notmatch '\[NPU-ERROR\] inference timeout') {
    Add-Failure 'NPU timeout error handling is missing'
} else {
    Add-Pass 'NPU timeout error handling is present'
}

$nm = Resolve-ArmTool 'arm-none-eabi-nm'
$objdump = Resolve-ArmTool 'arm-none-eabi-objdump'
$strings = Resolve-ArmTool 'arm-none-eabi-strings'

$elfStrings = & $strings -f $ElfPath
$elfStringText = $elfStrings -join "`n"
if ($elfStringText -match 'LL_ATON_PLATFORM LL_ATON_PLAT_SWEMUL') {
    Add-Failure 'ELF contains a SWEMUL ATON runtime object'
} else {
    Add-Pass 'ELF has no SWEMUL ATON runtime marker'
}

$disassembly = & $objdump -d -C $ElfPath
$waitBody = Get-FunctionDisassembly $disassembly 'LL_Streng_Wait'
if ($waitBody.Count -eq 0) {
    Add-Failure 'LL_Streng_Wait is missing from ELF'
} elseif (($waitBody -join "`n") -match '0x15000|#86016') {
    Add-Failure 'LL_Streng_Wait accesses the SW emulator register block at 0x15000'
} elseif (($waitBody -join "`n") -match 'ec_trace_unsupported') {
    Add-Failure 'LL_Streng_Wait routes register reads through ec_trace_unsupported'
} else {
    Add-Pass 'LL_Streng_Wait uses the hardware register path'
}

$symbols = & $nm -C $ElfPath
$npu0 = $symbols | Where-Object { $_ -match '\sNPU0_IRQHandler$' } | Select-Object -First 1
if ($null -eq $npu0) {
    Add-Failure 'NPU0_IRQHandler is missing from ELF'
} elseif ($npu0 -notmatch '\s[Tt]\s+NPU0_IRQHandler$') {
    Add-Failure "NPU0_IRQHandler is not a strong text symbol: $npu0"
} else {
    Add-Pass 'NPU0_IRQHandler is a strong text symbol'
}

$npu0Body = Get-FunctionDisassembly $disassembly 'NPU0_IRQHandler'
if ($npu0Body.Count -eq 0) {
    Add-Failure 'NPU0_IRQHandler disassembly is missing'
} elseif (($npu0Body -join "`n") -match 'ec_trace_unsupported') {
    Add-Failure 'NPU0_IRQHandler routes register reads through ec_trace_unsupported'
} else {
    Add-Pass 'NPU0_IRQHandler uses the hardware register path'
}

foreach ($faultName in 'HardFault', 'MemManage', 'BusFault', 'UsageFault', 'SecureFault') {
    if ($elfStringText -notmatch [regex]::Escape("[FAULT] $faultName")) {
        Add-Failure "$faultName diagnostics are missing from ELF"
    } else {
        Add-Pass "$faultName diagnostics are linked"
    }
}

if (-not (Test-Path -LiteralPath $HexPath)) {
    Add-Failure "HEX not found: $HexPath"
} else {
    $elfInfo = Get-Item -LiteralPath $ElfPath
    $hexInfo = Get-Item -LiteralPath $HexPath
    if ($hexInfo.LastWriteTimeUtc -lt $elfInfo.LastWriteTimeUtc) {
        Add-Failure 'HEX is older than ELF'
    } else {
        Add-Pass 'HEX is not older than ELF'
    }

    $hexSections = & $objdump -h $HexPath
    if (($hexSections -join "`n") -notmatch '70100400') {
        Add-Failure 'HEX does not contain the application base address 0x70100400'
    } else {
        Add-Pass 'HEX contains the application base address 0x70100400'
    }
}

if ($failures.Count -gt 0) {
    Write-Host "Artifact verification failed: $($failures.Count) check(s) failed." -ForegroundColor Red
    exit 1
}

Write-Host 'Artifact verification passed.' -ForegroundColor Green
exit 0
