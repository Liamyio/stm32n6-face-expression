$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$faultSource = Get-Content -Raw -LiteralPath (Join-Path $root 'Appli\Core\Src\stm32n6xx_it.c')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'Appli\Core\Src\main.c')

$requirements = @(
    @{ Text = 'Fault_ReportStacked'; Source = $faultSource; Message = 'stacked fault reporter is missing' },
    @{ Text = 'tst lr, #4'; Source = $faultSource; Message = 'fault wrapper does not select MSP/PSP from EXC_RETURN' },
    @{ Text = 'FAULT-RAWSP='; Source = $faultSource; Message = 'fault report does not preserve the raw exception SP' },
    @{ Text = 'BASIC-PC='; Source = $faultSource; Message = 'fault report does not print the basic-frame candidate' },
    @{ Text = 'EXT-PC='; Source = $faultSource; Message = 'fault report does not print the extended-frame candidate' },
    @{ Text = 'FAULT-STACK='; Source = $faultSource; Message = 'fault report does not dump raw exception stack words' },
    @{ Text = 'stacked[6]'; Source = $faultSource; Message = 'fault reporter does not capture the basic-frame PC candidate' },
    @{ Text = 'stacked[24]'; Source = $faultSource; Message = 'fault reporter does not capture the extended-frame PC candidate' },
    @{ Text = 'g_fault_stage'; Source = $mainSource; Message = 'main-loop fault stage marker is missing' },
    @{ Text = 'FAULT-STAGE='; Source = $faultSource; Message = 'UART fault report does not print the main-loop stage' }
)

foreach ($requirement in $requirements) {
    if (-not $requirement.Source.Contains($requirement.Text)) {
        throw $requirement.Message
    }
}

Write-Output 'PASS: fault reports include the original exception PC/LR and application stage.'
