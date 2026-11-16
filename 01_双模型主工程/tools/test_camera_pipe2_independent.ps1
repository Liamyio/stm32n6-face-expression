param(
    [string]$CameraSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\app_camera.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $CameraSourcePath
$required = @(
    'status = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE2, &cmw_dcmipp_conf, &hw_pitch)',
    'HAL_DCMIPP_PIPE_CSI_DisableShare(CMW_CAMERA_GetDCMIPPHandle(), DCMIPP_PIPE2)'
)

$missing = $required | Where-Object { $source -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "PIPE2 is still sharing PIPE1 CSI data flow: $($missing -join ', ')"
}

$configure = $source.IndexOf($required[0], [System.StringComparison]::Ordinal)
$disable = $source.IndexOf($required[1], [System.StringComparison]::Ordinal)
if ($disable -lt $configure) {
    throw 'PIPE2 sharing must be disabled after PIPE2 CSI configuration.'
}

Write-Host 'PASS: PIPE2 uses an independent CSI data flow.'
