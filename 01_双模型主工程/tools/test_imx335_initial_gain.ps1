param(
    [string]$MainSourcePath = (Join-Path $PSScriptRoot '..\Appli\Core\Src\main.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $MainSourcePath
$gain = 'CMW_CAMERA_SetGain(IMX335_GAIN_DEFAULT)'
$start = 'app_camera_nn_pipe_start('
if ($source -notmatch [regex]::Escape($gain)) {
    throw 'IMX335 default gain is not configured.'
}
if ($source.IndexOf($gain, [System.StringComparison]::Ordinal) -gt
    $source.IndexOf($start, [System.StringComparison]::Ordinal)) {
    throw 'IMX335 default gain must be set before streaming starts.'
}

Write-Host 'PASS: IMX335 starts with its documented default gain.'
