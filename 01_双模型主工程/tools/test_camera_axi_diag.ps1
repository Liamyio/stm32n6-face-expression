param(
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $MainSourcePath

$required = @(
    'first_changed_offset',
    'last_changed_offset',
    'camera_sentinel_byte',
    'row_changed_min',
    'P2DSSZR',
    'DCMIPP_MODE_CONTINUOUS',
    'P2STM0AR',
    'P2PPM0AR1',
    'P2PPM0PR',
    'P2CPPM0AR1',
    'P2CPPM0PR',
    'IPGR1',
    'IPC5R3',
    'RISAF2_S->IASR',
    'RISAF2_S->IAR[0].IAESR',
    'RISAF2_S->IAR[0].IADDR',
    'RAMCFG_SRAM1_AXI_S->CR'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "Camera AXI diagnostics are incomplete: $($missing -join ', ')"
}

Write-Host 'PASS: Camera AXI boundary diagnostics are present.'
