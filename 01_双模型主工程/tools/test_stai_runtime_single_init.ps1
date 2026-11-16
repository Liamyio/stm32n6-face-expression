$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $root 'Appli\Core\Src\main.c'
$source = Get-Content -Raw -LiteralPath $mainPath

$runtimeCalls = [regex]::Matches($source, '(?m)^\s*(?:stai_return_code\s+\w+\s*=\s*|\w+\s*=\s*)?stai_runtime_init\s*\(\s*\)\s*;')
if ($runtimeCalls.Count -ne 1) {
    throw "stai_runtime_init() must be called exactly once for the shared runtime; found $($runtimeCalls.Count)."
}

$expressionStart = $source.IndexOf('static void run_expression(', [System.StringComparison]::Ordinal)
$softmaxStart = $source.IndexOf('static void softmax7(', $expressionStart, [System.StringComparison]::Ordinal)
if (($expressionStart -lt 0) -or ($softmaxStart -lt 0)) {
    throw 'Unable to locate the run_expression() implementation.'
}

$expressionBody = $source.Substring($expressionStart, $softmaxStart - $expressionStart)
if ($expressionBody.Contains('stai_runtime_init()')) {
    throw 'run_expression() must not reinitialize the shared STAI runtime.'
}

Write-Output 'PASS: the shared STAI runtime is initialized exactly once before either network.'
