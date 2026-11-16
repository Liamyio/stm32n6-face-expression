param(
    [string]$ClockSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\MX_DCMIPP_ClockConfig.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $ClockSourcePath
$required = @(
    'ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2',
    'ICSelection[RCC_IC17].ClockDivider = 3',
    'ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1',
    'ICSelection[RCC_IC18].ClockDivider = 40'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "Camera clock configuration differs from ATK 991 AI reference: $($missing -join ', ')"
}

Write-Host 'PASS: Camera clocks match the ATK 991 AI 800 MHz reference.'
