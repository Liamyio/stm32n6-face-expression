param(
    [string]$MspSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\stm32n6xx_hal_msp.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $MspSourcePath
$required = @(
    'void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)',
    'RCC_PERIPHCLK_I2C2',
    'RCC_I2C2CLKSOURCE_PCLK1',
    'GPIO_PIN_4 | GPIO_PIN_14',
    'GPIO_MODE_AF_OD',
    'GPIO_AF4_I2C2',
    '__HAL_RCC_I2C2_CLK_ENABLE()'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "I2C2 MSP initialization is incomplete: $($missing -join ', ')"
}

Write-Host 'I2C2 MSP initialization checks passed.' -ForegroundColor Green
