param(
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $MainSourcePath
$required = @(
    'RIF_RISC_PERIPH_INDEX_I2C2',
    'HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_4 | GPIO_PIN_14',
    'HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_4 | GPIO_PIN_6'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "Camera control-plane RIF configuration is incomplete: $($missing -join ', ')"
}

Write-Host 'Camera control-plane RIF configuration checks passed.' -ForegroundColor Green
