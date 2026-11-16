param(
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $MainSourcePath
$required = @(
    'CMW_CAMERA_SetExposure(IMX335_EXPOSURE_DEFAULT)',
    'bsp_i2c2_read_reg16(CAMERA_IMX335_ADDRESS, 0x3030U',
    'bsp_i2c2_read_reg16(CAMERA_IMX335_ADDRESS, 0x3058U',
    'bsp_i2c2_read_reg16(CAMERA_IMX335_ADDRESS, 0x30E8U',
    '[CAM-EXPOSURE]'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "IMX335 initial-exposure test is incomplete: $($missing -join ', ')"
}

$setExposure = $source.IndexOf('CMW_CAMERA_SetExposure(IMX335_EXPOSURE_DEFAULT)', [System.StringComparison]::Ordinal)
$start = $source.IndexOf('app_camera_nn_pipe_start(', [System.StringComparison]::Ordinal)
if (($setExposure -lt 0) -or ($setExposure -gt $start)) {
    throw 'The non-zero initial exposure must be set before sensor streaming starts.'
}

Write-Host 'PASS: IMX335 starts with a non-zero exposure and reports timing registers.'
