param(
    [string]$SensorSourcePath = (Join-Path $PSScriptRoot '..\Middlewares\STM32_MW_CAMERA\sensors\cmw_imx335.c')
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw -LiteralPath $SensorSourcePath
$pipe2 = [regex]::Match(
    $source,
    'case\s+DCMIPP_PIPE2\s*:(?<body>[\s\S]*?)break\s*;',
    [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
)
if (!$pipe2.Success) {
    throw 'IMX335 PIPE2 VSYNC handler was not found.'
}

$required = @(
    'ISP_IncMainFrameId(&((CMW_IMX335_t *)io_ctx)->hIsp)',
    'ISP_GatherStatistics(&((CMW_IMX335_t *)io_ctx)->hIsp)'
)
$missing = $required | Where-Object { $pipe2.Groups['body'].Value -notmatch [regex]::Escape($_) }
if ($missing) {
    throw "PIPE2-only ISP statistics bridge is incomplete: $($missing -join ', ')"
}

if ($pipe2.Groups['body'].Value -match 'ISP_IncAncillaryFrameId') {
    throw 'PIPE2 is still classified as an ancillary ISP frame.'
}

Write-Host 'PASS: PIPE2 VSYNC drives the ISP main-frame statistics path.'
