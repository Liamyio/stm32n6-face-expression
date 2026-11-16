param(
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c'),
    [string]$CameraSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\app_camera.c')
)

$ErrorActionPreference = 'Stop'
$main = Get-Content -Raw -LiteralPath $MainSourcePath
$camera = Get-Content -Raw -LiteralPath $CameraSourcePath
$required = @(
    'CMW_CAMERA_SetTestPattern(10)',
    'bsp_i2c2_read_reg16(CAMERA_IMX335_ADDRESS, 0x3000U',
    'bsp_i2c2_read_reg16(CAMERA_IMX335_ADDRESS, 0x3002U',
    'bsp_i2c2_read_reg16(CAMERA_IMX335_ADDRESS, 0x329EU',
    '[CAM-SENSOR]'
)

$missing = $required | Where-Object { $main -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "IMX335 stream diagnostics are incomplete: $($missing -join ', ')"
}

$pattern = $main.IndexOf('CMW_CAMERA_SetTestPattern(10)', [System.StringComparison]::Ordinal)
$start = $main.IndexOf('app_camera_nn_pipe_start(', [System.StringComparison]::Ordinal)
if (($pattern -lt 0) -or ($start -lt 0) -or ($pattern -gt $start)) {
    throw 'The IMX335 test pattern must be configured before capture starts.'
}

$sensorRead = $main.IndexOf('[CAM-SENSOR]', [System.StringComparison]::Ordinal)
if ($sensorRead -lt $start) {
    throw 'IMX335 stream registers must be read after capture starts the sensor.'
}

if ($camera -match 'HAL_DCMIPP_PIPE_CSI_DisableShare') {
    throw 'The disproved independent PIPE2 routing experiment is still enabled.'
}

Write-Host 'PASS: IMX335 stream/test-pattern diagnostics are present.'
