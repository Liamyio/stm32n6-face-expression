param(
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $MainSourcePath
$required = @(
    'CMW_CAMERA_SetTestPattern(-1)',
    'app_camera_nn_pipe_start(camera_buf, DCMIPP_MODE_CONTINUOUS)',
    '[CAM] AE/AWB warm-up',
    'CMW_CAMERA_Suspend(DCMIPP_PIPE2)',
    'SCB_InvalidateDCache_by_Addr((uint32_t *)camera_buf, camera_buf_size)',
    'CMW_CAMERA_Resume(DCMIPP_PIPE2)'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "Live camera capture flow is incomplete: $($missing -join ', ')"
}

$suspend = $source.IndexOf('CMW_CAMERA_Suspend(DCMIPP_PIPE2)', [System.StringComparison]::Ordinal)
$invalidate = $source.IndexOf('SCB_InvalidateDCache_by_Addr((uint32_t *)camera_buf, camera_buf_size)', [System.StringComparison]::Ordinal)
$resume = $source.IndexOf('CMW_CAMERA_Resume(DCMIPP_PIPE2)', [System.StringComparison]::Ordinal)
if (!(($suspend -lt $invalidate) -and ($invalidate -lt $resume))) {
    throw 'PIPE2 must be suspended before cache invalidation and resumed after frame copy.'
}

Write-Host 'PASS: Continuous capture, ISP warm-up, and frame consistency protection are present.'
